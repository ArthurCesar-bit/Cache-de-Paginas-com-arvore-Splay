/* ===========================================================
 * page_cache.c — fachada estilo syscall (pc_open/close/read/write/flush)
 * ===========================================================
 * Coordena a política/índice (splay OU lru) e o pool de frames
 * (bytes + dirty bits). Só esta camada conversa com o disco.
 *
 * Concorrência fina por SHARDING: o espaço de páginas é
 * particionado em N shards independentes (shard = pageno % N),
 * cada um com sua própria árvore/lru + pool de frames + mutex.
 * Threads em shards distintos não contendem e não compartilham
 * estado mutável. O disco é compartilhado, mas usa I/O posicionado
 * (pread/pwrite) com contadores atômicos — seguro sob concorrência.
 *
 * Write-back: páginas sujas são gravadas no despejo, no pc_flush
 * e no pc_close.
 * =========================================================== */
#define _POSIX_C_SOURCE 200809L

#include "page_cache.h"
#include "splay.h"
#include "lru.h"
#include "disk.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t pageno;
    int      valid;    /* o slot guarda uma página? */
    int      dirty;    /* foi escrita desde a última gravação? */
    char    *bytes;    /* block_size bytes */
} frame_t;

typedef struct {
    pthread_mutex_t mu;
    pc_policy_t     policy;
    splay_t        *splay;     /* índice ativo conforme a política */
    lru_t          *lru;
    frame_t        *frames;
    size_t          nframes;
    size_t          used;      /* frames válidos no momento */
    uint64_t        hits;      /* métricas — só tocadas sob mu */
    uint64_t        misses;
    double          depth_sum;
} shard_t;

struct page_cache {
    disk_t  *disk;
    size_t   block_size;
    int      nshards;
    shard_t *shards;
};

/* ---- despacho de política (splay vs lru) ---- */
static int pol_lookup(shard_t *s, uint64_t pageno, int *depth) {
    return s->policy == PC_POLICY_SPLAY
        ? splay_lookup(s->splay, pageno, depth)
        : lru_lookup(s->lru, pageno, depth);
}
static void pol_insert(shard_t *s, uint64_t pageno, int fid) {
    if (s->policy == PC_POLICY_SPLAY) splay_insert(s->splay, pageno, fid);
    else                              lru_insert(s->lru, pageno, fid);
}
static uint64_t pol_evict(shard_t *s, int *fid) {
    return s->policy == PC_POLICY_SPLAY
        ? splay_evict_victim(s->splay, fid)
        : lru_evict_victim(s->lru, fid);
}

page_cache_t *pc_open(const char *path, size_t block_size,
                      size_t capacity, pc_policy_t policy, int nshards) {
    if (!path || block_size == 0 || capacity == 0 || nshards < 1) return NULL;

    disk_t *d = disk_open(path, block_size);
    if (!d) return NULL;

    page_cache_t *pc = calloc(1, sizeof(*pc));
    if (!pc) { disk_close(d); return NULL; }
    pc->disk       = d;
    pc->block_size = block_size;
    pc->nshards    = nshards;
    pc->shards     = calloc((size_t)nshards, sizeof(shard_t));
    if (!pc->shards) { disk_close(d); free(pc); return NULL; }

    size_t per = capacity / (size_t)nshards;
    if (per == 0) per = 1;             /* cada shard precisa de >= 1 frame */

    for (int i = 0; i < nshards; i++) {
        shard_t *s = &pc->shards[i];
        pthread_mutex_init(&s->mu, NULL);
        s->policy  = policy;
        s->nframes = per;
        s->frames  = calloc(per, sizeof(frame_t));
        for (size_t f = 0; f < per; f++)
            s->frames[f].bytes = malloc(block_size);
        if (policy == PC_POLICY_SPLAY) s->splay = splay_create();
        else                           s->lru   = lru_create(per);
    }
    return pc;
}

int pc_flush(page_cache_t *pc) {
    if (!pc) return -1;
    int rc = 0;
    for (int i = 0; i < pc->nshards; i++) {
        shard_t *s = &pc->shards[i];
        pthread_mutex_lock(&s->mu);
        for (size_t f = 0; f < s->nframes; f++) {
            frame_t *fr = &s->frames[f];
            if (fr->valid && fr->dirty) {
                if (disk_write_block(pc->disk, fr->pageno, fr->bytes) != 0) rc = -1;
                fr->dirty = 0;
            }
        }
        pthread_mutex_unlock(&s->mu);
    }
    return rc;
}

void pc_close(page_cache_t *pc) {
    if (!pc) return;
    pc_flush(pc);
    for (int i = 0; i < pc->nshards; i++) {
        shard_t *s = &pc->shards[i];
        for (size_t f = 0; f < s->nframes; f++) free(s->frames[f].bytes);
        free(s->frames);
        if (s->policy == PC_POLICY_SPLAY) splay_destroy(s->splay);
        else                              lru_destroy(s->lru);
        pthread_mutex_destroy(&s->mu);
    }
    free(pc->shards);
    disk_close(pc->disk);
    free(pc);
}

static int find_free(shard_t *s) {
    for (size_t i = 0; i < s->nframes; i++)
        if (!s->frames[i].valid) return (int)i;
    return -1;
}

/* Caminho comum de read/write. write=1 escreve buf na página e a marca suja;
 * write=0 lê a página para buf. Tudo sob o mutex do shard. */
static int access_page(page_cache_t *pc, uint64_t pageno, void *buf, int write) {
    shard_t *s = &pc->shards[pageno % (uint64_t)pc->nshards];
    pthread_mutex_lock(&s->mu);

    int depth = 0;
    int fid   = pol_lookup(s, pageno, &depth);
    int rc    = 0;

    if (fid >= 0) {                                  /* ---- HIT ---- */
        frame_t *fr = &s->frames[fid];
        if (write) { memcpy(fr->bytes, buf, pc->block_size); fr->dirty = 1; }
        else         memcpy(buf, fr->bytes, pc->block_size);
        s->hits++;
        s->depth_sum += depth;
        pthread_mutex_unlock(&s->mu);
        return 0;
    }

    /* ---- MISS ---- */
    s->misses++;
    int slot = find_free(s);
    if (slot < 0) {                                  /* pool cheio: despeja */
        int      vfid  = -1;
        uint64_t vpage = pol_evict(s, &vfid);
        frame_t *vf    = &s->frames[vfid];
        if (vf->dirty && disk_write_block(pc->disk, vpage, vf->bytes) != 0) rc = -1;
        vf->valid = 0;
        s->used--;
        slot = vfid;
    }

    frame_t *fr = &s->frames[slot];
    if (write) {                                     /* escrita cobre o bloco todo */
        memcpy(fr->bytes, buf, pc->block_size);
        fr->dirty = 1;
    } else {                                         /* leitura busca do disco */
        if (disk_read_block(pc->disk, pageno, fr->bytes) != 0) rc = -1;
        memcpy(buf, fr->bytes, pc->block_size);
        fr->dirty = 0;
    }
    fr->pageno = pageno;
    fr->valid  = 1;
    pol_insert(s, pageno, slot);
    s->used++;

    pthread_mutex_unlock(&s->mu);
    return rc;
}

int pc_read(page_cache_t *pc, uint64_t pageno, void *buf) {
    if (!pc || !buf) return -1;
    return access_page(pc, pageno, buf, 0);
}

int pc_write(page_cache_t *pc, uint64_t pageno, const void *buf) {
    if (!pc || !buf) return -1;
    return access_page(pc, pageno, (void *)buf, 1);
}

double pc_hit_ratio(const page_cache_t *pc) {
    if (!pc) return 0.0;
    page_cache_t *p = (page_cache_t *)pc;
    uint64_t h = 0, m = 0;
    for (int i = 0; i < p->nshards; i++) {
        shard_t *s = &p->shards[i];
        pthread_mutex_lock(&s->mu);
        h += s->hits;
        m += s->misses;
        pthread_mutex_unlock(&s->mu);
    }
    uint64_t total = h + m;
    return total ? (double)h / (double)total : 0.0;
}

double pc_avg_depth(const page_cache_t *pc) {
    if (!pc) return 0.0;
    page_cache_t *p = (page_cache_t *)pc;
    uint64_t h  = 0;
    double   ds = 0.0;
    for (int i = 0; i < p->nshards; i++) {
        shard_t *s = &p->shards[i];
        pthread_mutex_lock(&s->mu);
        h  += s->hits;
        ds += s->depth_sum;
        pthread_mutex_unlock(&s->mu);
    }
    return h ? ds / (double)h : 0.0;
}
