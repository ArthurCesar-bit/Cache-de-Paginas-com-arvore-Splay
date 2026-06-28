#include "page_cache.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define BS 64  /* tamanho de bloco do teste */

static void fill(char *b, int v) { memset(b, v, BS); }

static void test_roundtrip_hit_and_writeback(void) {
    const char *path = "build/test_cache.dat";
    char buf[BS], a[BS], b[BS], c[BS];
    fill(a, 0xA1);
    fill(b, 0xB2);
    fill(c, 0xC3);

    /* capacidade 2 frames, política splay, 1 shard */
    page_cache_t *pc = pc_open(path, BS, 2, PC_POLICY_SPLAY, 1);
    assert(pc != NULL);

    /* escreve duas páginas e relê: os bytes têm de bater */
    assert(pc_write(pc, 0, a) == 0);
    assert(pc_write(pc, 1, b) == 0);
    assert(pc_read(pc, 0, buf) == 0);
    assert(memcmp(buf, a, BS) == 0);

    /* reler a mesma página é hit -> hit ratio sobe acima de zero */
    assert(pc_read(pc, 0, buf) == 0);
    assert(pc_hit_ratio(pc) > 0.0);

    /* terceira página excede a capacidade (2) e força um despejo;
       o flush garante o write-back das páginas sujas no disco */
    assert(pc_write(pc, 2, c) == 0);
    assert(pc_flush(pc) == 0);
    pc_close(pc);

    /* reabrindo do zero, os dados sujos têm de estar persistidos */
    pc = pc_open(path, BS, 2, PC_POLICY_SPLAY, 1);
    assert(pc != NULL);
    assert(pc_read(pc, 0, buf) == 0);
    assert(memcmp(buf, a, BS) == 0);
    assert(pc_read(pc, 2, buf) == 0);
    assert(memcmp(buf, c, BS) == 0);
    pc_close(pc);
}

int main(void) {
    test_roundtrip_hit_and_writeback();
    printf("test_cache: OK\n");
    return 0;
}
