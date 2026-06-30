#ifndef SPLAY_H
#define SPLAY_H

#include <stdint.h>

#define SPLAY_NOT_FOUND (-1)

typedef struct splay splay_t;

/* Cria uma árvore vazia. Retorna NULL se faltar memória. */
splay_t *splay_create(void);

/* Libera a árvore e todos os nós. Aceita NULL. */
void splay_destroy(splay_t *t);

/* Busca pageno.
 *   - hit : retorna o frame_id e (mais à frente) faz splay do nó até a raiz;
 *   - miss: retorna SPLAY_NOT_FOUND.
 * Se out_depth != NULL, recebe a profundidade do nó ANTES do splay — a
 * métrica de "profundidade média de acesso" que o relatório exige. */
int splay_lookup(splay_t *t, uint64_t pageno, int *out_depth);

/* Insere o mapeamento pageno -> frame_id. Se pageno já existe, atualiza o
 * frame_id. (Mais à frente: faz splay do nó inserido até a raiz.) */
void splay_insert(splay_t *t, uint64_t pageno, int frame_id);

/* Remove e retorna a página "fria" (a folha mais profunda) — a vítima do
 * despejo — devolvendo seu frame em *out_frame_id. Simétrico a
 * lru_evict_victim; é o que o page_cache chama quando o pool de frames
 * enche. Em árvore vazia retorna 0 e *out_frame_id = -1. */
uint64_t splay_evict_victim(splay_t *t, int *out_frame_id);

#endif /* SPLAY_H */
