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

#endif /* SPLAY_H */
