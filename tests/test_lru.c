#include "lru.h"

#include <assert.h>
#include <stdio.h>

static void test_recency_governs_eviction(void) {
    lru_t *l = lru_create(2);   /* cabem 2 páginas */
    assert(l != NULL);

    int depth = -1;

    /* cache vazio: miss */
    assert(lru_lookup(l, 1, &depth) == LRU_NOT_FOUND);

    /* insere 1 e 2 (2 vira o mais recente) */
    lru_insert(l, 1, 10);
    lru_insert(l, 2, 20);

    /* acessar a 1 a torna a mais recente -> a vítima passa a ser a 2 */
    assert(lru_lookup(l, 1, &depth) == 10);

    int victim_frame = -1;
    uint64_t victim = lru_evict_victim(l, &victim_frame);
    assert(victim == 2);
    assert(victim_frame == 20);

    lru_destroy(l);
}

int main(void) {
    test_recency_governs_eviction();
    printf("test_lru: OK\n");
    return 0;
}
