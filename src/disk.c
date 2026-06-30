/* ===========================================================
 * disk.c — camada de blocos de tamanho fixo, instrumentada
 * ===========================================================
 * Vê o arquivo como uma sequência de blocos de 'block_size'
 * bytes endereçados por 'pageno' (offset = pageno * block_size).
 * Usa pread/pwrite (I/O posicionado e atômico) para que vários
 * shards possam ler/escrever o MESMO arquivo concorrentemente
 * sem um lock global. Os contadores de I/O são atômicos, logo
 * o módulo é thread-safe e limpo no ThreadSanitizer.
 * =========================================================== */
#define _POSIX_C_SOURCE 200809L

#include "disk.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

struct disk {
    int                  fd;
    size_t               block_size;
    atomic_uint_fast64_t reads;
    atomic_uint_fast64_t writes;
};

disk_t *disk_open(const char *path, size_t block_size) {
    if (!path || block_size == 0) return NULL;
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return NULL;
    disk_t *d = calloc(1, sizeof(*d));
    if (!d) { close(fd); return NULL; }
    d->fd = fd;
    d->block_size = block_size;
    atomic_init(&d->reads, 0);
    atomic_init(&d->writes, 0);
    return d;
}

void disk_close(disk_t *d) {
    if (!d) return;
    close(d->fd);
    free(d);
}

size_t disk_block_size(const disk_t *d) {
    return d->block_size;
}

int disk_read_block(disk_t *d, uint64_t pageno, void *buf) {
    if (!d || !buf) return -1;
    off_t  off   = (off_t)pageno * (off_t)d->block_size;
    size_t total = 0;
    char  *p     = buf;
    while (total < d->block_size) {
        ssize_t n = pread(d->fd, p + total, d->block_size - total,
                          off + (off_t)total);
        if (n < 0) return -1;
        if (n == 0) {                 /* buraco/EOF: zera o restante */
            memset(p + total, 0, d->block_size - total);
            break;
        }
        total += (size_t)n;
    }
    atomic_fetch_add(&d->reads, 1);
    return 0;
}

int disk_write_block(disk_t *d, uint64_t pageno, const void *buf) {
    if (!d || !buf) return -1;
    off_t       off   = (off_t)pageno * (off_t)d->block_size;
    size_t      total = 0;
    const char *p     = buf;
    while (total < d->block_size) {
        ssize_t n = pwrite(d->fd, p + total, d->block_size - total,
                           off + (off_t)total);
        if (n < 0) return -1;
        total += (size_t)n;
    }
    atomic_fetch_add(&d->writes, 1);
    return 0;
}

uint64_t disk_reads(const disk_t *d) {
    disk_t *m = (disk_t *)d;   /* atomic_load exige lvalue não-const */
    return (uint64_t)atomic_load(&m->reads);
}

uint64_t disk_writes(const disk_t *d) {
    disk_t *m = (disk_t *)d;
    return (uint64_t)atomic_load(&m->writes);
}
