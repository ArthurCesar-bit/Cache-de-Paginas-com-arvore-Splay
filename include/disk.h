#ifndef DISK_H
#define DISK_H

#include <stddef.h>
#include <stdint.h>

typedef struct disk disk_t;

/* Abre (ou cria) o arquivo em 'path', com blocos de 'block_size' bytes.
 * Retorna NULL em erro. */
disk_t *disk_open(const char *path, size_t block_size);

/* Fecha o arquivo e libera o disk_t. Aceita NULL. */
void disk_close(disk_t *d);

/* Tamanho do bloco em bytes. */
size_t disk_block_size(const disk_t *d);

/* Lê o bloco 'pageno' inteiro para 'buf' (>= block_size bytes).
 * Retorna 0 em sucesso, -1 em erro. Incrementa o contador de leituras. */
int disk_read_block(disk_t *d, uint64_t pageno, void *buf);

/* Escreve 'block_size' bytes de 'buf' no bloco 'pageno'.
 * Retorna 0 em sucesso, -1 em erro. Incrementa o contador de escritas. */
int disk_write_block(disk_t *d, uint64_t pageno, const void *buf);

/* Contadores de instrumentação acumulados desde disk_open. */
uint64_t disk_reads(const disk_t *d);
uint64_t disk_writes(const disk_t *d);

#endif /* DISK_H */
