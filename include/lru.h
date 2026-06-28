#ifndef LRU_H
#define LRU_H

#include <stddef.h>
#include <stdint.h>

/* Retorno de lru_lookup quando a página não está no cache. */
#define LRU_NOT_FOUND (-1)

typedef struct lru lru_t;

/* Cria um LRU com capacidade para 'capacity' páginas. NULL em erro. */
lru_t *lru_create(size_t capacity);

/* Libera o LRU e todas as estruturas. Aceita NULL. */
void lru_destroy(lru_t *l);

/* Busca pageno. Em hit, marca como o mais recente e retorna o frame_id;
 * em miss, retorna LRU_NOT_FOUND. */
int lru_lookup(lru_t *l, uint64_t pageno, int *out_depth);

/* Insere/atualiza pageno -> frame_id, marcando-o como o mais recente. */
void lru_insert(lru_t *l, uint64_t pageno, int frame_id);

/* Remove e retorna o pageno menos recentemente usado (a vítima do despejo),
 * devolvendo seu frame em *out_frame_id. */
uint64_t lru_evict_victim(lru_t *l, int *out_frame_id);

#endif /* LRU_H */
