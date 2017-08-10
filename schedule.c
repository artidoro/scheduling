#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "vector.h"
#include "bitmap.h"
#include "binheap.h"
#include "schedule.h"

struct schedule {
    idx_vec order;
    bitmap* contents;
    dag *g;
    unsigned m;
    unsigned length;
    unsigned *max_starts;
    unsigned *min_ends;
};

schedule *schedule_create(dag *g, unsigned m) {
    assert(g != NULL);
    assert(m > 0);
    schedule *s = malloc(sizeof(*s));
    if (s == NULL) {
        return NULL;
    }
    if (idx_vec_init(&s->order, 0) != 0) {
        free(s);
        return NULL;
    }
    s->contents = bitmap_create(0);
    if (s->contents == NULL) {
        idx_vec_destroy(&s->order);
        return NULL;
    }
    s->g = g;
    s->m = m;
    s->length = 0;
    s->max_starts = NULL;
    s->min_ends = NULL;
    return s;
}

void schedule_destroy(schedule *s) {
    assert(s != NULL);
    idx_vec_destroy(&s->order);
    free(s->max_starts);
    free(s->min_ends);
    free(s);
}

dag *schedule_dag(schedule *s) {
    assert(s != NULL);
    return s->g;
}

unsigned schedule_m(schedule *s) {
    assert(s != NULL);
    return s->m;
}

unsigned schedule_get(schedule *s, unsigned idx) {
    assert(s != NULL);
    assert(idx < schedule_size(s));
    return s->order.data[idx];
}

unsigned schedule_contains(schedule *s, unsigned idx) {
    assert(s != NULL);
    assert(idx < dag_size(s->g));
    return bitmap_get(s->contents, idx);
}

int schedule_add(schedule *s, unsigned idx) {
    assert(s != NULL);
    assert(idx < dag_size(s->g));
    assert(s->order.size < dag_size(s->g));
    if (bitmap_set(s->contents, idx, 1) != 0) {
        bitmap_set(s->contents, idx, 0);
        return -1;
    }
    if (idx_vec_push(&s->order, idx) != 0) {
        bitmap_set(s->contents, idx, 0);
        return -1;
    }
    return 0;
}

int schedule_pop(schedule *s) {
    assert(s != NULL);
    assert(s->order.size > 0);
    bitmap_set(s->contents, s->order.data[s->order.size - 1], 0);
    return idx_vec_pop(&s->order, NULL);
}

size_t schedule_size(schedule *s) {
    assert(s != NULL);
    return s->order.size;
}

int schedule_is_complete(schedule *s) {
    assert(s != NULL);
    return s->order.size == dag_size(s->g);
}

int schedule_is_valid(schedule *s) {
    assert(s != NULL);
    assert(s->g != NULL);
    size_t size = schedule_size(s);
    bitmap* prev_jobs = bitmap_create(size);
    for (unsigned i = 0; i < size; i++) {
        unsigned idx = s->order.data[i];
        size_t npreds = dag_npreds(s->g, idx);
        unsigned preds[npreds];
        dag_preds(s->g, idx, preds);
        for (unsigned j = 0; j < npreds; j++) {
            if (bitmap_get(prev_jobs, preds[j]) != 1) {
                bitmap_destroy(prev_jobs);
                return 0;
            }
        }
        bitmap_set(prev_jobs, idx, 1);
    }
    bitmap_destroy(prev_jobs);
    return 1;
}

static int schedule_compute(schedule *s, unsigned *task_ends) {
    assert(s != NULL);
    assert(task_ends != NULL);
    unsigned assignments[dag_size(s->g)];
    unsigned end_times[s->m];
    unsigned cur_items[s->m];
    memset(task_ends, 0, schedule_size(s) * sizeof(*task_ends));
    memset(assignments, -1, dag_size(s->g) * sizeof(*assignments));
    memset(end_times, 0, s->m * sizeof(unsigned));
    memset(cur_items, 0, s->m * sizeof(unsigned));
    for (size_t i = 0; i < s->order.size; i++) {
        unsigned cur_time = UINT_MAX;
        unsigned cur_m = 0;
        for (size_t i = 0; i < s->m; i++) {
            if (end_times[i] < cur_time) {
                cur_time = end_times[i];
                cur_m = i;
            }
        }
        unsigned idx = s->order.data[i];
        size_t npreds = dag_npreds(s->g, idx);
        unsigned preds[npreds];
        dag_preds(s->g, idx, preds);
        unsigned max_pred_end = 0;
        unsigned max_pred_m = 0;
        for (size_t i = 0; i < npreds; i++) {
            if (task_ends[preds[i]] > max_pred_end) {
                max_pred_end = task_ends[preds[i]];
                max_pred_m = assignments[preds[i]];
            }
        }
        if (max_pred_end > cur_time) {
            cur_m = max_pred_m;
            cur_time = max_pred_end;
        }
        assignments[idx] = cur_m;
        task_ends[idx] = cur_time + dag_weight(s->g, idx);
        end_times[cur_m] = cur_time + dag_weight(s->g, idx);
    }
    unsigned final_time = 0;
    for (size_t i = 0; i < s->m; i++) {
        final_time = (end_times[i] > final_time) ? end_times[i] : final_time;
    }
    return final_time;
}

// calculate min_end
static void end_visit(dag *g, unsigned idx, idx_vec *end_ready,
                      bitmap *end_finished, unsigned *min_ends) {
    size_t nsuccs = dag_nsuccs(g, idx);
    unsigned succs[nsuccs];
    dag_succs(g, idx, succs);
    for (size_t i = 0; i < nsuccs; i++) {
        unsigned succ = succs[i];
        if (bitmap_get(end_finished, succ)) {
            continue;
        }
        size_t npreds = dag_npreds(g, succ);
        unsigned preds[npreds];
        dag_preds(g, succ, preds);
        int preds_complete = 1;
        unsigned max_min_end = 0;
        for (size_t j = 0; j < npreds; j++) {
            if (bitmap_get(end_finished, preds[j]) != 1) {
                preds_complete = 0;
                break;
            }
            max_min_end = (min_ends[preds[j]] > max_min_end) ?
                min_ends[preds[j]] : max_min_end;
        }
        // all predecessors have calculated min_ends
        if (preds_complete) {
            min_ends[succ] = dag_weight(g, succ) + max_min_end;
            bitmap_set(end_finished, succ, 1);
            idx_vec_push(end_ready, succ);
        }
    }
}

// calculate max_start
static void start_visit(dag *g, unsigned idx, idx_vec *start_ready,
                        bitmap *start_finished, unsigned *max_starts,
                        unsigned total_time) {
    size_t npreds = dag_npreds(g, idx);
    unsigned preds[npreds];
    dag_preds(g, idx, preds);
    for (size_t i = 0; i < npreds; i++) {
        unsigned pred = preds[i];
        if (bitmap_get(start_finished, pred)) {
            continue;
        }
        size_t nsuccs = dag_nsuccs(g, pred);
        unsigned succs[nsuccs];
        dag_succs(g, pred, succs);
        int succs_complete = 1;
        unsigned min_max_start = INT_MAX;
        for (size_t j = 0; j < nsuccs; j++) {
            if (bitmap_get(start_finished, succs[j]) != 1) {
                succs_complete = 0;
                break;
            }
            min_max_start = (max_starts[succs[j]] < min_max_start) ?
                max_starts[succs[j]] : min_max_start;
        }
        // all successors have calculated max_starts
        if (succs_complete) {
            max_starts[pred] =
                (min_max_start - dag_weight(g, pred) < total_time) ?
                min_max_start - dag_weight(g, pred) : total_time;
            bitmap_set(start_finished, pred, 1);
            idx_vec_push(start_ready, pred);
        }
    }
}

static int schedule_min_ends(schedule *s, unsigned *min_ends,
                             unsigned *sched_ends) {
    assert(s != NULL);
    assert(min_ends != NULL);
    idx_vec end_ready;
    if (idx_vec_init(&end_ready, 0) != 0) {
        return -1;
    }
    bitmap *end_finished = bitmap_create(dag_size(s->g));
    if (end_finished == NULL) {
        return -1;
    }
    for (size_t i = 0, nodes = schedule_size(s); i < nodes; i++) {
        unsigned idx = s->order.data[i];
        bitmap_set(end_finished, idx, 1);
        min_ends[idx] = sched_ends[idx];
        idx_vec_push(&end_ready, idx);
    }
    while (end_ready.size > 0) {
        unsigned idx;
        idx_vec_pop(&end_ready, &idx);
        end_visit(s->g, idx, &end_ready, end_finished, min_ends);
    }
    idx_vec_destroy(&end_ready);
    bitmap_destroy(end_finished);
    return 0;
}

static int schedule_max_starts(schedule *s, unsigned *max_starts,
                               unsigned total_time, unsigned *sched_ends) {
    assert(s != NULL);
    assert(max_starts != NULL);
    idx_vec start_ready;
    if (idx_vec_init(&start_ready, 0) != 0) {
        return -1;
    }
    bitmap *start_finished = bitmap_create(dag_size(s->g));
    if (start_finished == NULL) {
        return -1;
    }
    for (size_t i = 0, nodes = schedule_size(s); i < nodes; i++) {
        unsigned idx = s->order.data[i];
        bitmap_set(start_finished, idx, 1);
        max_starts[idx] = sched_ends[idx] - dag_weight(s->g, idx);
    }

    max_starts[dag_sink(s->g)] = total_time;
    bitmap_set(start_finished, dag_sink(s->g), 1);
    idx_vec_push(&start_ready, dag_sink(s->g));
    while (start_ready.size > 0) {
        unsigned idx;
        idx_vec_pop(&start_ready, &idx);
        start_visit(s->g, idx, &start_ready, start_finished, max_starts,
                    total_time);
    }
    idx_vec_destroy(&start_ready);
    bitmap_destroy(start_finished);

    int diff = total_time - dag_level(s->g, dag_source(s->g));
    for (size_t i = 0; i < dag_size(s->g); i++) {
        max_starts[i] += diff;
    }
    return 0;
}

int schedule_build(schedule *s, unsigned total_time) {
    assert(s != NULL);
    if (total_time == 0) {
        total_time = dag_level(s->g, dag_source(s->g));
    }
    unsigned sched_ends[dag_size(s->g)];
    s->length = schedule_compute(s, sched_ends);
#ifdef FUJITA
    if (s->max_starts == NULL || s->min_ends == NULL) {
        s->max_starts = malloc(sizeof(*s->max_starts) * dag_size(s->g));
        s->min_ends = malloc(sizeof(*s->min_ends) * dag_size(s->g));
        if (s->max_starts == NULL || s->min_ends == NULL) {
            free(s->max_starts);
            free(s->min_ends);
            return -1;
        }
    }
    if (schedule_max_starts(s, s->max_starts, total_time, sched_ends) != 0) {
        return -1;
    }
    if (schedule_min_ends(s, s->min_ends, sched_ends) != 0) {
        return -1;
    }
#endif
    return 0;
}

unsigned schedule_length(schedule *s) {
    assert(s != NULL);
    return s->length;
}

unsigned schedule_max_start(schedule *s, unsigned id) {
    assert(s != NULL);
    assert(id < dag_size(s->g));
    return s->max_starts[id];
}

unsigned schedule_min_end(schedule *s, unsigned id) {
    assert(s != NULL);
    assert(id < dag_size(s->g));
    return s->min_ends[id];
}

static void get_comp_list(schedule *s, idx_vec *comp_list) {
    binheap *sorter = binheap_create();
    for (size_t i = 0; i < dag_size(s->g); i++) {
        unsigned max_start = schedule_max_start(s, i);
        unsigned min_end = schedule_min_end(s, i);
        binheap_put(sorter, max_start, -((int) max_start));
        binheap_put(sorter, min_end, -((int) min_end));
    }
    idx_vec_push(comp_list, binheap_get(sorter));
    while (binheap_size(sorter) > 0) {
        unsigned c = binheap_get(sorter);
        if (c != comp_list->data[comp_list->size - 1]) {
            idx_vec_push(comp_list, c);
        }
    }
    binheap_destroy(sorter);
}

static int work_density(schedule *s, unsigned ci, unsigned cj) {
    assert(s != NULL);
    int work_density = 0;
    // TODO: Compute this is constant time
    for (size_t k = 0, n_nodes = dag_size(s->g); k < n_nodes; k++) {
        if (schedule_max_start(s, k) < cj &&
            schedule_min_end(s, k) > ci) {
            int case1 = schedule_min_end(s, k) - ci;
            int case2 = dag_weight(s->g, k);
            int case3 = cj - schedule_max_start(s, k);
            int case4 = cj - ci;
            int min1 = (case1 < case2) ? case1 : case2;
            int min2 = (case3 < case4) ? case3 : case4;
            work_density += (min1 < min2) ? min1 : min2;
        }
    }
    return work_density;
}

int schedule_fernandez_bound(schedule *s) {
    assert(s != NULL);
    idx_vec comp_list;
    idx_vec_init(&comp_list, 0);
    get_comp_list(s, &comp_list);

    int max_q = INT_MIN;
    for (size_t i = 0; i < comp_list.size - 1; i++) {
        for (size_t j = i + 1; j < comp_list.size; j++) {
            int w_density = work_density(s, comp_list.data[i],
                                         comp_list.data[j]);
            int cur_q = (comp_list.data[i] - comp_list.data[j]) +
                w_density / s->m + (w_density % s->m != 0);
            max_q = (cur_q > max_q) ? cur_q : max_q;
        }
    }
    idx_vec_destroy(&comp_list);
    int crit_path = dag_level(s->g, dag_source(s->g));
    return (max_q > 0) ? crit_path + max_q : crit_path;
}

int schedule_machine_bound(schedule *s) {
    assert(s != NULL);
    idx_vec comp_list;
    idx_vec_init(&comp_list, 0);
    get_comp_list(s, &comp_list);

    int max_m = INT_MIN;
    for (size_t i = 0; i < comp_list.size - 1; i++) {
        for (size_t j = i + 1; j < comp_list.size; j++) {
            int w_density = work_density(s, comp_list.data[i],
                                         comp_list.data[j]);
            int interval = (comp_list.data[j] - comp_list.data[i]);
            int cur_m = w_density / interval + (w_density % interval != 0);
            max_m = (cur_m > max_m) ? cur_m : max_m;
        }
    }
    idx_vec_destroy(&comp_list);
    return max_m;
}
