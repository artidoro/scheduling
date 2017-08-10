#include <assert.h>
#include <limits.h>
#include <time.h>
#include <stdio.h>

#include "binheap.h"
#include "bitmap.h"
#include "dag.h"
#include "schedule.h"
#include "bbsearch.h"

static int do_timeout;
static clock_t end_time;

int fujita_bound(schedule *s) {
    dag *g = schedule_dag(s);
    unsigned delta = 1;
    while (1) {
        /* printf("loop1\n"); */
        schedule_build(s, dag_level(g, dag_source(g)) + delta);
        int min_m = schedule_machine_bound(s);
        /* printf("delta: %d, min_m: %d, m: %d\n", delta, min_m, schedule_m(s)); */
        if (min_m <= schedule_m(s)) {
            break;
        }
        delta = delta * 2;
        assert(delta != 0);
    }
    int low_time = dag_level(g, dag_source(g)) + delta / 2;
    int high_time = dag_level(g, dag_source(g)) + delta;
    int best_time = high_time;
    while (1) {
        int cur_time = (high_time - low_time) / 2 + low_time;
        if (cur_time == low_time) {
            break;
        }
        schedule_build(s, cur_time);
        int min_m = schedule_machine_bound(s);
        if (min_m <= schedule_m(s)) {
            high_time = cur_time;
            best_time = (best_time < cur_time) ? best_time : cur_time;
        }
        else {
            low_time = cur_time;
        }
    }
    return best_time;
}

int bb(schedule *s, bitmap *ready_set, unsigned best_soln) {
    assert(s != NULL);
    if (do_timeout && clock() >= end_time) {
        return -2;
    }
    dag *g = schedule_dag(s);
    if (schedule_build(s, 0) != 0) {
        return -1;
    }
    if (schedule_size(s) == dag_size(g)) {
        unsigned sched_len = schedule_length(s);
        return (best_soln < sched_len) ? best_soln : sched_len;
    }
#ifdef FUJITA
#ifdef FB
    unsigned fb = schedule_fernandez_bound(s);
    if (fb >= best_soln) {
        return best_soln;
    }
#else // no FB
    unsigned mb = fujita_bound(s);
    if (mb >= best_soln) {
        return best_soln;
    }
#endif // FB
#endif // FUJITA
    binheap *sorter = binheap_create();
    if (sorter == NULL) {
        return -1;
    }
    for (size_t i = 0; i < dag_size(g); i++) {
        if (bitmap_get(ready_set, i) == 1) {
            if (binheap_put(sorter, i, dag_level(g, i)) != 0) {
                binheap_destroy(sorter);
                return -1;
            }
        }
    }
    idx_vec new_ready;
    idx_vec_init(&new_ready, 0);
    while (binheap_size(sorter) > 0) {
        unsigned new_idx = binheap_get(sorter);
        schedule_add(s, new_idx);

        size_t nsuccs = dag_nsuccs(g, new_idx);
        unsigned succs[nsuccs];
        dag_succs(g, new_idx, succs);
        for (size_t i = 0; i < nsuccs; i++) {
            size_t npreds = dag_npreds(g, succs[i]);
            unsigned preds[npreds];
            dag_preds(g, succs[i], preds);
            int all_scheduled = 1;
            for (size_t i = 0; i < npreds; i++) {
                if (!schedule_contains(s, preds[i])) {
                    all_scheduled = 0;
                    break;
                }
            }
            if (all_scheduled) {
                idx_vec_push(&new_ready, succs[i]);
                bitmap_set(ready_set, succs[i], 1);
            }
        }

        bitmap_set(ready_set, new_idx, 0);
        int soln = bb(s, ready_set, best_soln);
        bitmap_set(ready_set, new_idx, 1);
        if (soln < 0) {
            binheap_destroy(sorter);
            return soln;
        }
        best_soln = (best_soln < soln) ? best_soln : soln;
        while (new_ready.size > 0) {
            unsigned r;
            idx_vec_pop(&new_ready, &r);
            bitmap_set(ready_set, r, 0);
        }

        schedule_pop(s);
    }
    idx_vec_destroy(&new_ready);
    binheap_destroy(sorter);
    return best_soln;
}

int bbsearch(dag *g, unsigned m, int timeout) {
    assert(g != NULL);
    schedule *s = schedule_create(g, m);
    if (s == NULL) {
        return -1;
    }
    schedule_add(s, dag_source(g));
    bitmap *ready_set = bitmap_create(0);
    if (ready_set == NULL) {
        schedule_destroy(s);
        return -1;
    }

    if (timeout < 0) {
        do_timeout = 0;
    }
    else {
        do_timeout = 1;
        end_time = clock() + timeout * CLOCKS_PER_SEC;
    }

    size_t nsuccs = dag_nsuccs(g, dag_source(g));
    unsigned succs[nsuccs];
    dag_succs(g, dag_source(g), succs);
    for (size_t i = 0; i < nsuccs; i++) {
        bitmap_set(ready_set, succs[i], 1);
    }
    int result = bb(s, ready_set, UINT_MAX);
    bitmap_destroy(ready_set);
    schedule_destroy(s);
    return result;
}
