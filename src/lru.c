/* ===========================================================
 * lru.c — baseline LRU clássico (tabela hash + lista duplamente
 *         encadeada), exigido para o comparativo splay vs LRU
 * ===========================================================
 * Lista por recência: head = mais recente, tail = menos recente
 * (a vítima do despejo). A tabela hash (encadeamento) dá lookup
 * O(1) por pageno. Sem auto-despejo no insert: quem controla a
 * capacidade é o page_cache, que chama lru_evict_victim quando o
 * pool de frames enche — simétrico ao uso da splay.
 * =========================================================== */
#include "lru.h"

#include <stdlib.h>

typedef struct lnode {
    uint64_t      pageno;
    int           frame_id;
    struct lnode *prev;    /* vizinhos na lista de recência */
    struct lnode *next;
    struct lnode *hnext;   /* próximo no encadeamento do bucket */
} lnode;

struct lru {
    size_t   capacity;
    size_t   count;
    lnode   *head;         /* mais recente */
    lnode   *tail;         /* menos recente (vítima) */
    lnode  **buckets;
    size_t   nbuckets;
};

lru_t *lru_create(size_t capacity) {
    if (capacity == 0) return NULL;
    lru_t *l = calloc(1, sizeof(lru_t));
    if (!l) return NULL;
    l->capacity = capacity;
    l->nbuckets = capacity * 2 + 1;
    l->buckets  = calloc(l->nbuckets, sizeof(lnode *));
    if (!l->buckets) { free(l); return NULL; }
    return l;
}

void lru_destroy(lru_t *l) {
    if (!l) return;
    lnode *n = l->head;
    while (n) {
        lnode *nx = n->next;
        free(n);
        n = nx;
    }
    free(l->buckets);
    free(l);
}

static lnode *hfind(lru_t *l, uint64_t pageno) {
    lnode *n = l->buckets[pageno % l->nbuckets];
    while (n) {
        if (n->pageno == pageno) return n;
        n = n->hnext;
    }
    return NULL;
}

static void list_detach(lru_t *l, lnode *n) {
    if (n->prev) n->prev->next = n->next; else l->head = n->next;
    if (n->next) n->next->prev = n->prev; else l->tail = n->prev;
    n->prev = n->next = NULL;
}

static void list_push_front(lru_t *l, lnode *n) {
    n->prev = NULL;
    n->next = l->head;
    if (l->head) l->head->prev = n; else l->tail = n;
    l->head = n;
}

int lru_lookup(lru_t *l, uint64_t pageno, int *out_depth) {
    if (out_depth) *out_depth = 0;   /* recência, não profundidade (existe p/ simetria) */
    if (!l) return LRU_NOT_FOUND;
    lnode *n = hfind(l, pageno);
    if (!n) return LRU_NOT_FOUND;
    list_detach(l, n);               /* acerto: vira o mais recente */
    list_push_front(l, n);
    return n->frame_id;
}

void lru_insert(lru_t *l, uint64_t pageno, int frame_id) {
    if (!l) return;
    lnode *n = hfind(l, pageno);
    if (n) {                         /* já existe: atualiza e promove */
        n->frame_id = frame_id;
        list_detach(l, n);
        list_push_front(l, n);
        return;
    }
    n = calloc(1, sizeof(lnode));
    if (!n) return;
    n->pageno   = pageno;
    n->frame_id = frame_id;
    size_t b = pageno % l->nbuckets;
    n->hnext = l->buckets[b];
    l->buckets[b] = n;
    list_push_front(l, n);
    l->count++;
}

uint64_t lru_evict_victim(lru_t *l, int *out_frame_id) {
    if (!l || !l->tail) {
        if (out_frame_id) *out_frame_id = -1;
        return 0;
    }
    lnode   *v      = l->tail;        /* menos recentemente usado */
    uint64_t pageno = v->pageno;
    if (out_frame_id) *out_frame_id = v->frame_id;

    list_detach(l, v);
    lnode **pp = &l->buckets[pageno % l->nbuckets];
    while (*pp && *pp != v) pp = &(*pp)->hnext;
    if (*pp) *pp = v->hnext;
    free(v);
    l->count--;
    return pageno;
}
