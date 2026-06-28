#include "page_cache.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BS               64
#define NTHREADS         8
#define PAGES_PER_THREAD 50

typedef struct {
    page_cache_t *pc;
    int           id;
} worker_arg_t;

/* Cada thread escreve nas SUAS páginas e depois relê verificando byte a byte.
 * O working set (8*50 = 400 páginas) excede a capacidade, então há despejo e
 * releitura do disco em concorrência — exatamente o cenário a estressar. */
static void *worker(void *p) {
    worker_arg_t *w = p;
    char buf[BS];
    char val = (char)(w->id + 1);

    for (int i = 0; i < PAGES_PER_THREAD; i++) {
        uint64_t page = (uint64_t)w->id * PAGES_PER_THREAD + (uint64_t)i;
        memset(buf, val, BS);
        assert(pc_write(w->pc, page, buf) == 0);
    }
    for (int i = 0; i < PAGES_PER_THREAD; i++) {
        uint64_t page = (uint64_t)w->id * PAGES_PER_THREAD + (uint64_t)i;
        assert(pc_read(w->pc, page, buf) == 0);
        for (int k = 0; k < BS; k++) {
            assert(buf[k] == val);
        }
    }
    return NULL;
}

static void test_concurrent_disjoint_pages(void) {
    /* capacidade 64 frames, splay, 8 shards (concorrência fina) */
    page_cache_t *pc = pc_open("build/test_concurrency.dat", BS, 64,
                               PC_POLICY_SPLAY, 8);
    assert(pc != NULL);

    pthread_t    th[NTHREADS];
    worker_arg_t args[NTHREADS];

    for (int t = 0; t < NTHREADS; t++) {
        args[t].pc = pc;
        args[t].id = t;
        assert(pthread_create(&th[t], NULL, worker, &args[t]) == 0);
    }
    for (int t = 0; t < NTHREADS; t++) {
        assert(pthread_join(th[t], NULL) == 0);
    }

    pc_close(pc);
}

int main(void) {
    test_concurrent_disjoint_pages();
    printf("test_concurrency: OK\n");
    return 0;
}
