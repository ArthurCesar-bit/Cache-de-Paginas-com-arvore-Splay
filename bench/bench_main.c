/* ===========================================================
 * bench_main.c — driver do comparativo splay vs LRU
 * ===========================================================
 * Dois experimentos, ambos sobre a fachada pública (pc_*):
 *
 *   1) COMPARATIVO  (1 thread): para cada política {splay, lru}
 *      e cada carga {uniform, zipfian}, replaya a sequência e
 *      mede hit ratio, profundidade média e vazão.
 *
 *   2) ESCALABILIDADE (multithread): sob carga zipfiana, mede a
 *      vazão de splay e lru com 1/2/4/8/16 threads (nshards =
 *      nthreads), para ver o ganho do sharding.
 *
 * Saídas:
 *   - tabela legível em stdout;
 *   - build/bench_compare.csv  e  build/bench_threads.csv
 *     (consumidos por bench/gen_plots.py).
 *
 * Parâmetros têm default modesto (roda em segundos) e podem ser
 * ajustados por flags: --npages --capacity --nops --write-pct
 * --theta --block --seed.
 * =========================================================== */
#define _POSIX_C_SOURCE 200809L

#include "page_cache.h"
#include "workload.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------- */
typedef struct {
    uint64_t npages;
    size_t   capacity;
    size_t   nops;
    int      write_pct;
    double   theta;
    size_t   block_size;
    uint64_t seed;
} config_t;

static const config_t DEFAULTS = {
    .npages     = 20000,
    .capacity   = 2000,     /* ~10% do working set cabe na RAM */
    .nops       = 500000,
    .write_pct  = 20,
    .theta      = 0.99,
    .block_size = 4096,
    .seed       = 1,
};

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static const char *policy_name(pc_policy_t p) {
    return p == PC_POLICY_SPLAY ? "splay" : "lru";
}

/* ---- replay de 1 thread sobre uma fatia [start, start+count) ---- */
typedef struct {
    page_cache_t   *pc;
    const workload_t *w;
    size_t          start;
    size_t          count;
    size_t          block_size;
} replay_arg_t;

static void replay_slice(replay_arg_t *a) {
    char *buf = malloc(a->block_size);
    if (!buf) return;
    memset(buf, 0xAB, a->block_size);
    const workload_t *w = a->w;
    size_t end = a->start + a->count;
    for (size_t i = a->start; i < end; i++) {
        uint64_t page = w->pages[i];
        if (w->is_write[i]) {
            pc_write(a->pc, page, buf);
        } else {
            pc_read(a->pc, page, buf);
        }
    }
    free(buf);
}

static void *replay_thread(void *p) {
    replay_slice((replay_arg_t *)p);
    return NULL;
}

/* ---- métricas de uma execução ---- */
typedef struct {
    double hit_ratio;
    double avg_depth;
    double seconds;
    double ops_per_sec;
} result_t;

/* Roda uma config com 'nthreads' threads e 'nshards' shards. */
static result_t run_one(const config_t *cfg, pc_policy_t policy,
                        const workload_t *w, int nthreads, int nshards) {
    result_t r = {0};
    const char *path = "build/bench.dat";
    page_cache_t *pc = pc_open(path, cfg->block_size, cfg->capacity,
                               policy, nshards);
    if (!pc) {
        fprintf(stderr, "pc_open falhou\n");
        return r;
    }

    pthread_t   th[64];
    replay_arg_t args[64];
    if (nthreads < 1)  nthreads = 1;
    if (nthreads > 64) nthreads = 64;

    size_t per = w->nops / (size_t)nthreads;
    double t0 = now_sec();
    for (int t = 0; t < nthreads; t++) {
        args[t].pc         = pc;
        args[t].w          = w;
        args[t].start      = (size_t)t * per;
        args[t].count      = (t == nthreads - 1) ? (w->nops - (size_t)t * per)
                                                  : per;
        args[t].block_size = cfg->block_size;
    }
    if (nthreads == 1) {
        replay_slice(&args[0]);                 /* sem overhead de thread */
    } else {
        for (int t = 0; t < nthreads; t++)
            pthread_create(&th[t], NULL, replay_thread, &args[t]);
        for (int t = 0; t < nthreads; t++)
            pthread_join(th[t], NULL);
    }
    double t1 = now_sec();

    r.seconds     = t1 - t0;
    r.ops_per_sec = r.seconds > 0 ? (double)w->nops / r.seconds : 0.0;
    r.hit_ratio   = pc_hit_ratio(pc);
    r.avg_depth   = pc_avg_depth(pc);

    pc_close(pc);
    remove(path);                                /* não acumula lixo */
    return r;
}

/* ============================================================ */
static void experiment_compare(const config_t *cfg, FILE *csv) {
    printf("\n== Experimento 1: comparativo (1 thread) ==\n");
    printf("%-8s %-8s %10s %10s %10s %12s\n",
           "carga", "politica", "hit_ratio", "avg_depth", "segundos", "ops/s");

    fprintf(csv, "workload,policy,hit_ratio,avg_depth,seconds,ops_per_sec\n");

    wl_kind_t kinds[]   = { WL_UNIFORM, WL_ZIPFIAN };
    pc_policy_t pols[]  = { PC_POLICY_SPLAY, PC_POLICY_LRU };

    for (size_t ki = 0; ki < 2; ki++) {
        workload_t *w = workload_create(kinds[ki], cfg->npages, cfg->nops,
                                        cfg->write_pct, cfg->theta, cfg->seed);
        if (!w) { fprintf(stderr, "workload_create falhou\n"); return; }
        for (size_t pi = 0; pi < 2; pi++) {
            result_t r = run_one(cfg, pols[pi], w, 1, 1);
            printf("%-8s %-8s %10.4f %10.3f %10.3f %12.0f\n",
                   workload_kind_name(kinds[ki]), policy_name(pols[pi]),
                   r.hit_ratio, r.avg_depth, r.seconds, r.ops_per_sec);
            fprintf(csv, "%s,%s,%.6f,%.6f,%.6f,%.2f\n",
                    workload_kind_name(kinds[ki]), policy_name(pols[pi]),
                    r.hit_ratio, r.avg_depth, r.seconds, r.ops_per_sec);
        }
        workload_destroy(w);
    }
}

static void experiment_threads(const config_t *cfg, FILE *csv) {
    printf("\n== Experimento 2: escalabilidade (zipfian) ==\n");
    printf("%-8s %8s %8s %10s %12s\n",
           "politica", "threads", "shards", "segundos", "ops/s");

    fprintf(csv, "policy,threads,shards,seconds,ops_per_sec\n");

    int counts[]       = { 1, 2, 4, 8, 16 };
    pc_policy_t pols[] = { PC_POLICY_SPLAY, PC_POLICY_LRU };

    workload_t *w = workload_create(WL_ZIPFIAN, cfg->npages, cfg->nops,
                                    cfg->write_pct, cfg->theta, cfg->seed);
    if (!w) { fprintf(stderr, "workload_create falhou\n"); return; }

    for (size_t pi = 0; pi < 2; pi++) {
        for (size_t ci = 0; ci < sizeof(counts) / sizeof(counts[0]); ci++) {
            int n = counts[ci];
            result_t r = run_one(cfg, pols[pi], w, n, n);
            printf("%-8s %8d %8d %10.3f %12.0f\n",
                   policy_name(pols[pi]), n, n, r.seconds, r.ops_per_sec);
            fprintf(csv, "%s,%d,%d,%.6f,%.2f\n",
                    policy_name(pols[pi]), n, n, r.seconds, r.ops_per_sec);
        }
    }
    workload_destroy(w);
}

/* ============================================================ */
static void usage(const char *prog) {
    printf("uso: %s [opcoes]\n"
           "  --npages N     espaco de paginas      (%llu)\n"
           "  --capacity N   frames em RAM          (%zu)\n"
           "  --nops N       numero de acessos      (%zu)\n"
           "  --write-pct N  %% de escritas [0..100] (%d)\n"
           "  --theta F      expoente zipf          (%.2f)\n"
           "  --block N      bytes por bloco        (%zu)\n"
           "  --seed N       semente do RNG         (%llu)\n",
           prog,
           (unsigned long long)DEFAULTS.npages, DEFAULTS.capacity,
           DEFAULTS.nops, DEFAULTS.write_pct, DEFAULTS.theta,
           DEFAULTS.block_size, (unsigned long long)DEFAULTS.seed);
}

static int parse_args(int argc, char **argv, config_t *cfg) {
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        int has_val = (i + 1 < argc);
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(argv[0]); return 1; }
        else if (!strcmp(a, "--npages")   && has_val) cfg->npages     = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(a, "--capacity") && has_val) cfg->capacity   = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(a, "--nops")     && has_val) cfg->nops       = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(a, "--write-pct")&& has_val) cfg->write_pct  = atoi(argv[++i]);
        else if (!strcmp(a, "--theta")    && has_val) cfg->theta      = atof(argv[++i]);
        else if (!strcmp(a, "--block")    && has_val) cfg->block_size = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(a, "--seed")     && has_val) cfg->seed       = strtoull(argv[++i], NULL, 10);
        else { fprintf(stderr, "argumento desconhecido: %s\n", a); usage(argv[0]); return -1; }
    }
    return 0;
}

int main(int argc, char **argv) {
    config_t cfg = DEFAULTS;
    int pr = parse_args(argc, argv, &cfg);
    if (pr != 0) return pr < 0 ? 1 : 0;

    printf("minicache — benchmark\n");
    printf("npages=%llu capacity=%zu nops=%zu write_pct=%d theta=%.2f block=%zu seed=%llu\n",
           (unsigned long long)cfg.npages, cfg.capacity, cfg.nops,
           cfg.write_pct, cfg.theta, cfg.block_size,
           (unsigned long long)cfg.seed);

    FILE *csv_cmp = fopen("build/bench_compare.csv", "w");
    FILE *csv_thr = fopen("build/bench_threads.csv", "w");
    if (!csv_cmp || !csv_thr) {
        fprintf(stderr, "nao consegui abrir CSV em build/ (rode 'make bench')\n");
        if (csv_cmp) fclose(csv_cmp);
        if (csv_thr) fclose(csv_thr);
        return 1;
    }

    experiment_compare(&cfg, csv_cmp);
    experiment_threads(&cfg, csv_thr);

    fclose(csv_cmp);
    fclose(csv_thr);
    printf("\nCSVs: build/bench_compare.csv, build/bench_threads.csv\n");
    printf("graficos: python3 bench/gen_plots.py  (ou 'make plots')\n");
    return 0;
}
