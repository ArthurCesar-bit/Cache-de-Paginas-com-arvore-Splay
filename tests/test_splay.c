#include "splay.h"

#include <assert.h>
#include <stdio.h>

/* Ciclo 1: o básico — criar, buscar vazio, inserir, achar, destruir. */
static void test_create_lookup_insert(void) {
    splay_t *t = splay_create();
    assert(t != NULL);

    int depth = -1;

    /* árvore vazia: toda busca é miss */
    assert(splay_lookup(t, 42, &depth) == SPLAY_NOT_FOUND);

    /* insere a página 42 -> frame 7 e depois encontra */
    splay_insert(t, 42, 7);
    assert(splay_lookup(t, 42, &depth) == 7);
    assert(depth >= 0);            /* profundidade do nó (métrica do relatório) */

    /* outra página continua sendo miss */
    assert(splay_lookup(t, 99, &depth) == SPLAY_NOT_FOUND);

    splay_destroy(t);
}

int main(void) {
    test_create_lookup_insert();
    printf("test_splay: OK\n");
    return 0;
}
