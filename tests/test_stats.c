#include "stats.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

/* compara doubles com tolerância (evita armadilha de igualdade exata) */
static int close_to(double a, double b) {
    return fabs(a - b) < 1e-9;
}

static void test_hit_ratio_and_avg_depth(void) {
    stats_t *s = stats_create();
    assert(s != NULL);

    /* sem acessos: tudo zero, sem divisão por zero */
    assert(close_to(stats_hit_ratio(s), 0.0));
    assert(close_to(stats_avg_depth(s), 0.0));

    /* 3 hits (profundidades 0, 2, 4) e 1 miss */
    stats_record_hit(s, 0);
    stats_record_hit(s, 2);
    stats_record_hit(s, 4);
    stats_record_miss(s);

    /* hit ratio = 3 / 4 = 0.75 */
    assert(close_to(stats_hit_ratio(s), 0.75));
    /* profundidade média = (0 + 2 + 4) / 3 = 2.0 */
    assert(close_to(stats_avg_depth(s), 2.0));

    stats_destroy(s);
}

int main(void) {
    test_hit_ratio_and_avg_depth();
    printf("test_stats: OK\n");
    return 0;
}
