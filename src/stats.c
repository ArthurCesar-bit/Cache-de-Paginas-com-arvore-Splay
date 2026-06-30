/* ===========================================================
 * stats.c — coletor de métricas (hit ratio + profundidade média)
 * ===========================================================
 * Módulo isolado e testável (test_stats). Guarda os acertos com
 * a soma das profundidades para reportar a profundidade média de
 * acesso — a métrica central do relatório da splay. Sem divisão
 * por zero quando não houve acessos.
 * =========================================================== */
#include "stats.h"

#include <stdlib.h>

struct stats {
    uint64_t hits;
    uint64_t misses;
    int64_t  depth_sum;   /* soma das profundidades dos acertos */
};

stats_t *stats_create(void) {
    return calloc(1, sizeof(stats_t));
}

void stats_destroy(stats_t *s) {
    free(s);
}

void stats_record_hit(stats_t *s, int depth) {
    if (!s) return;
    s->hits++;
    s->depth_sum += depth;
}

void stats_record_miss(stats_t *s) {
    if (!s) return;
    s->misses++;
}

double stats_hit_ratio(const stats_t *s) {
    if (!s) return 0.0;
    uint64_t total = s->hits + s->misses;
    return total ? (double)s->hits / (double)total : 0.0;
}

double stats_avg_depth(const stats_t *s) {
    if (!s || s->hits == 0) return 0.0;
    return (double)s->depth_sum / (double)s->hits;
}
