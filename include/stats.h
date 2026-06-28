#ifndef STATS_H
#define STATS_H

#include <stdint.h>

typedef struct stats stats_t;

/* Cria um coletor zerado. Retorna NULL se faltar memória. */
stats_t *stats_create(void);

/* Libera o coletor. Aceita NULL. */
void stats_destroy(stats_t *s);

/* Registra um acerto de cache, informando a profundidade do nó acessado. */
void stats_record_hit(stats_t *s, int depth);

/* Registra uma falha de cache (miss). */
void stats_record_miss(stats_t *s);

/* Razão de acertos = hits / (hits + misses). Retorna 0.0 se não houve acesso. */
double stats_hit_ratio(const stats_t *s);

/* Profundidade média entre os acertos. Retorna 0.0 se não houve acerto. */
double stats_avg_depth(const stats_t *s);

#endif /* STATS_H */
