#ifndef WORKLOAD_H
#define WORKLOAD_H

#include <stddef.h>
#include <stdint.h>

/* Tipo de distribuição dos acessos:
 *   - WL_UNIFORM:  cada página é igualmente provável (sem localidade);
 *   - WL_ZIPFIAN:  poucas páginas concentram a maioria dos acessos
 *                  (localidade temporal forte). */
typedef enum { WL_UNIFORM, WL_ZIPFIAN } wl_kind_t;

/* Sequência de acessos pré-computada. Gerar antes de medir mantém o custo do
 * RNG/zipf FORA do cronômetro — o benchmark só replaya este vetor. */
typedef struct {
    uint64_t *pages;     /* página acessada em cada passo            */
    uint8_t  *is_write;  /* 1 = escrita, 0 = leitura                 */
    size_t    nops;      /* número de acessos                        */
    uint64_t  npages;    /* tamanho do espaço de páginas             */
    wl_kind_t kind;      /* distribuição usada                       */
    double    theta;     /* expoente da zipf (ignorado em uniforme)  */
} workload_t;

/* Gera uma carga de 'nops' acessos sobre 'npages' páginas.
 *   - write_pct: fração de escritas em [0,100];
 *   - theta:     expoente da zipf (use ~0.99 para localidade forte);
 *   - seed:      torna a sequência reprodutível.
 * Retorna NULL em erro. */
workload_t *workload_create(wl_kind_t kind, uint64_t npages, size_t nops,
                            int write_pct, double theta, uint64_t seed);

/* Libera a carga. Aceita NULL. */
void workload_destroy(workload_t *w);

/* Nome legível da distribuição ("uniform" / "zipfian"). */
const char *workload_kind_name(wl_kind_t kind);

#endif /* WORKLOAD_H */
