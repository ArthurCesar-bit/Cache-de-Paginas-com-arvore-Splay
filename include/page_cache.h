#ifndef PAGE_CACHE_H
#define PAGE_CACHE_H

#include <stddef.h>
#include <stdint.h>

/* Qual política de cache usar (para o comparativo splay vs LRU). */
typedef enum {
    PC_POLICY_SPLAY,
    PC_POLICY_LRU
} pc_policy_t;

typedef struct page_cache page_cache_t;

/* Abre um page cache sobre o arquivo 'path':
 *   - block_size: tamanho do bloco/página em bytes;
 *   - capacity:   número de frames (páginas) que cabem em RAM;
 *   - policy:     splay ou lru;
 *   - nshards:    número de shards de concorrência (1 = sem sharding).
 * Retorna NULL em erro. */
page_cache_t *pc_open(const char *path, size_t block_size,
                      size_t capacity, pc_policy_t policy, int nshards);

/* Faz flush das páginas sujas, fecha o arquivo e libera tudo. Aceita NULL. */
void pc_close(page_cache_t *pc);

/* Lê o bloco 'pageno' para 'buf' (>= block_size bytes). Serve da RAM em hit,
 * busca no disco em miss (despejando uma vítima se preciso). 0 ok, -1 erro. */
int pc_read(page_cache_t *pc, uint64_t pageno, void *buf);

/* Escreve 'block_size' bytes de 'buf' no bloco 'pageno' e o marca como sujo
 * (write-back acontece no despejo, no flush ou no close). 0 ok, -1 erro. */
int pc_write(page_cache_t *pc, uint64_t pageno, const void *buf);

/* Grava no disco todas as páginas sujas, sem fechar o cache. 0 ok, -1 erro. */
int pc_flush(page_cache_t *pc);

/* Métricas acumuladas (para o relatório). */
double pc_hit_ratio(const page_cache_t *pc);
double pc_avg_depth(const page_cache_t *pc);

#endif /* PAGE_CACHE_H */
