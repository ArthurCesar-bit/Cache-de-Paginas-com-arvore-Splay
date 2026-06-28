#include "disk.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define BS 64  /* tamanho de bloco usado no teste */

static void test_write_then_read_roundtrip(void) {
    disk_t *d = disk_open("build/test_disk.dat", BS);
    assert(d != NULL);
    assert(disk_block_size(d) == BS);

    /* recém-aberto: nenhum I/O ainda */
    assert(disk_reads(d) == 0);
    assert(disk_writes(d) == 0);

    /* escreve um bloco e lê de volta: os bytes têm de bater */
    char out[BS];
    memset(out, 0xAB, sizeof(out));
    assert(disk_write_block(d, 3, out) == 0);

    char in[BS];
    memset(in, 0, sizeof(in));
    assert(disk_read_block(d, 3, in) == 0);
    assert(memcmp(in, out, BS) == 0);

    /* instrumentação contou 1 escrita e 1 leitura */
    assert(disk_writes(d) == 1);
    assert(disk_reads(d) == 1);

    disk_close(d);
}

int main(void) {
    test_write_then_read_roundtrip();
    printf("test_disk: OK\n");
    return 0;
}
