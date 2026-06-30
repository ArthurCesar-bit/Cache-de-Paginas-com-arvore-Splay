/* ===========================================================
 * workload.c — gerador de cargas de acesso (uniforme e zipfiana)
 * ===========================================================
 * Produz, de forma reprodutível, a sequência de páginas que o
 * benchmark vai replayar. A zipfiana segue o gerador clássico
 * de Gray et al. (o mesmo do YCSB): poucas páginas "quentes"
 * concentram a maioria dos acessos, o que cria a localidade
 * temporal em que a splay deve levar vantagem sobre o LRU.
 *
 * As páginas quentes saem embaralhadas por um hash (FNV-1a) para
 * não ficarem grudadas nos pagenos baixos — assim elas se espalham
 * pelos shards e o comparativo de concorrência fica honesto.
 * =========================================================== */
#include "workload.h"

#include <math.h>
#include <stdlib.h>

/* ---- RNG: splitmix64 (rápido, reprodutível, bom o bastante) ---- */
static uint64_t sm_next(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* Double em [0,1) a partir dos 53 bits altos. */
static double sm_double(uint64_t *s) {
    return (double)(sm_next(s) >> 11) * (1.0 / 9007199254740992.0);
}

/* FNV-1a 64 bits — embaralha a "posição de popularidade" -> pageno real. */
static uint64_t fnv1a(uint64_t x) {
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < 8; i++) {
        h ^= (x & 0xff);
        h *= 1099511628211ULL;
        x >>= 8;
    }
    return h;
}

/* ---- Zipf: pré-cálculo das constantes (uma vez por carga) ---- */
typedef struct {
    uint64_t n;
    double   theta;
    double   alpha;
    double   zetan;
    double   eta;
    double   half_pow;   /* 0.5^theta, usado no caso especial */
} zipf_t;

static double zeta(uint64_t n, double theta) {
    double sum = 0.0;
    for (uint64_t i = 1; i <= n; i++) {
        sum += 1.0 / pow((double)i, theta);
    }
    return sum;
}

static void zipf_init(zipf_t *z, uint64_t n, double theta) {
    z->n        = n;
    z->theta    = theta;
    z->alpha    = 1.0 / (1.0 - theta);
    z->zetan    = zeta(n, theta);
    double z2   = 1.0 + pow(0.5, theta);            /* zeta(2, theta) */
    z->eta      = (1.0 - pow(2.0 / (double)n, 1.0 - theta)) /
                  (1.0 - z2 / z->zetan);
    z->half_pow = pow(0.5, theta);
}

/* Sorteia uma "posição de popularidade" em [0, n): 0 é a mais quente. */
static uint64_t zipf_next(const zipf_t *z, uint64_t *rng) {
    double u  = sm_double(rng);
    double uz = u * z->zetan;
    if (uz < 1.0)               return 0;
    if (uz < 1.0 + z->half_pow) return 1;
    double r = pow(z->eta * u - z->eta + 1.0, z->alpha);
    uint64_t ret = (uint64_t)((double)z->n * r);
    if (ret >= z->n) ret = z->n - 1;                /* guarda de borda */
    return ret;
}

workload_t *workload_create(wl_kind_t kind, uint64_t npages, size_t nops,
                            int write_pct, double theta, uint64_t seed) {
    if (npages == 0 || nops == 0) return NULL;
    if (write_pct < 0)   write_pct = 0;
    if (write_pct > 100) write_pct = 100;
    /* theta == 1.0 estoura o alpha = 1/(1-theta); afasta um pouco. */
    if (theta >= 1.0) theta = 0.9999;
    if (theta < 0.0)  theta = 0.0;

    workload_t *w = calloc(1, sizeof(*w));
    if (!w) return NULL;
    w->pages    = malloc(nops * sizeof(*w->pages));
    w->is_write = malloc(nops * sizeof(*w->is_write));
    if (!w->pages || !w->is_write) {
        workload_destroy(w);
        return NULL;
    }
    w->nops   = nops;
    w->npages = npages;
    w->kind   = kind;
    w->theta  = theta;

    uint64_t rng = seed ? seed : 0x1234567890ABCDEFULL;
    zipf_t z;
    if (kind == WL_ZIPFIAN) zipf_init(&z, npages, theta);

    for (size_t i = 0; i < nops; i++) {
        uint64_t page;
        if (kind == WL_ZIPFIAN) {
            uint64_t rank = zipf_next(&z, &rng);
            page = fnv1a(rank) % npages;            /* espalha as quentes */
        } else {
            page = sm_next(&rng) % npages;
        }
        w->pages[i]    = page;
        w->is_write[i] = (uint8_t)((sm_next(&rng) % 100) < (uint64_t)write_pct);
    }
    return w;
}

void workload_destroy(workload_t *w) {
    if (!w) return;
    free(w->pages);
    free(w->is_write);
    free(w);
}

const char *workload_kind_name(wl_kind_t kind) {
    return kind == WL_ZIPFIAN ? "zipfian" : "uniform";
}
