#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "vector.h"
#include "bitmap.h"
#include "binheap.h"
#include "dag.h"

// holds all node-specific data
typedef struct node {
    int weight;
    idx_vec preds;
    idx_vec succs;
    unsigned level;
} node;

int node_init(node *n, int weight) {
    assert(n != NULL);
    if (idx_vec_init(&n->preds, 0) != 0) {
        return -1;
    }
    if (idx_vec_init(&n->succs, 0) != 0) {
        idx_vec_destroy(&n->preds);
        return -1;
    }
    n->weight = weight;
    n->level = 0;
    return 0;
}

void node_destroy(node *n) {
    assert(n != NULL);
    idx_vec_destroy(&n->preds);
    idx_vec_destroy(&n->succs);
}

DECLARE_VECTOR(node_vec, node);
DEFINE_VECTOR(node_vec, node);

struct dag {
    node_vec nodes;
    int built;
};

dag *dag_create(void) {
    dag *g = malloc(sizeof(*g));
    if (g == NULL) {
        return NULL;
    }
    if (node_vec_init(&g->nodes, 1) != 0) {
        goto err1;
    }
    // create source node
    node s;
    if (node_init(&s, 0) != 0) {
        goto err2;
    }
    if (node_vec_push(&g->nodes, s) != 0) {
        goto err3;
    }
    g->built = 0;
    return g;
 err3:
    node_destroy(&s);
 err2:
    node_vec_destroy(&g->nodes);
 err1:
    free(g);
    return NULL;
}

void dag_destroy(dag *g) {
    assert(g != NULL);
    for (size_t i = 0, size = dag_size(g); i < size; i++) {
        node_destroy(&g->nodes.data[i]);
    }
    node_vec_destroy(&g->nodes);
    free(g);
}

size_t dag_size(dag *g) {
    assert(g != NULL);
    return g->nodes.size;
}

unsigned dag_vertex(dag *g, int weight, size_t n_deps, unsigned *deps) {
    assert(g != NULL);
    assert(n_deps == 0 || deps != NULL);
    node n;
    unsigned idx = g->nodes.size;
    if (node_init(&n, weight) != 0) {
        return (unsigned) -1;
    }
    unsigned source = 0;
    if (n_deps == 0) {
        n_deps = 1;
        deps = &source;
    }
    for (size_t i = 0; i < n_deps; i++) {
        assert(deps[i] < dag_size(g));
        if (idx_vec_push(&n.preds, deps[i]) != 0) {
            return (unsigned) -1;
        }
        if (idx_vec_push(&g->nodes.data[deps[i]].succs, idx) != 0) {
            return (unsigned) -1;
        }
    }
    if (node_vec_push(&g->nodes, n) != 0) {
        return (unsigned) -1;
    }
    return idx;
}

// calculate lvl
static void lvl_visit(dag *g, unsigned idx, idx_vec *lvl_ready,
                      bitmap *lvl_finished) {
    size_t npreds = dag_npreds(g, idx);
    unsigned preds[npreds];
    dag_preds(g, idx, preds);
    for (size_t i = 0; i < npreds; i++) {
        unsigned pred = preds[i];
        size_t nsuccs = dag_nsuccs(g, pred);
        unsigned succs[nsuccs];
        dag_succs(g, pred, succs);
        int succs_complete = 1;
        unsigned max_level = 0;
        for (size_t j = 0; j < nsuccs; j++) {
            if (bitmap_get(lvl_finished, succs[j]) != 1) {
                succs_complete = 0;
                break;
            }
            max_level = (g->nodes.data[succs[j]].level > max_level) ?
                g->nodes.data[succs[j]].level : max_level;
        }
        // all successors have calculated levels
        if (succs_complete) {
            g->nodes.data[pred].level = dag_weight(g, pred) + max_level;
            bitmap_set(lvl_finished, pred, 1);
            idx_vec_push(lvl_ready, pred);
        }
    }
}

int dag_build(dag *g) {
    assert(g != NULL);
    if (!g->built) {
        // find exit nodes
        idx_vec exit_nodes;
        idx_vec_init(&exit_nodes, 0);
        for (size_t i = 0; i < g->nodes.size; i++) {
            if (g->nodes.data[i].succs.size == 0) {
                idx_vec_push(&exit_nodes, i);
            }
        }
        // construct sink node
        dag_vertex(g, 0, exit_nodes.size, exit_nodes.data);
        idx_vec_destroy(&exit_nodes);

        // calculate level of each vertex
        idx_vec lvl_ready;
        if (idx_vec_init(&lvl_ready, 0) != 0) {
            return -1;
        }
        bitmap *lvl_finished = bitmap_create(dag_size(g));
        if (lvl_finished == NULL) {
            return -1;
        }
        idx_vec_push(&lvl_ready, dag_sink(g));
        bitmap_set(lvl_finished, dag_sink(g), 1);
        while (lvl_ready.size > 0) {
            unsigned idx;
            idx_vec_pop(&lvl_ready, &idx);
            lvl_visit(g, idx, &lvl_ready, lvl_finished);
        }
        idx_vec_destroy(&lvl_ready);
        bitmap_destroy(lvl_finished);
    }
    g->built = 1;
    return 0;
}

unsigned dag_source(dag *g) {
    assert(g != NULL);
    return 0;
}

unsigned dag_sink(dag *g) {
    assert(g != NULL);
    return g->nodes.size - 1;
}

size_t dag_nsuccs(dag *g, unsigned id) {
    assert(g != NULL);
    assert(id < dag_size(g));
    return g->nodes.data[id].succs.size;
}

size_t dag_npreds(dag *g, unsigned id) {
    assert(g != NULL);
    assert(id < dag_size(g));
    return g->nodes.data[id].preds.size;
}

void dag_succs(dag *g, unsigned id, unsigned *buf) {
    assert(g != NULL);
    assert(buf != NULL);
    assert(id < dag_size(g));
    memcpy(buf, g->nodes.data[id].succs.data,
           dag_nsuccs(g, id) * sizeof(unsigned));
}

void dag_preds(dag *g, unsigned id, unsigned *buf) {
    assert(g != NULL);
    assert(buf != NULL);
    assert(id < dag_size(g));
    memcpy(buf, g->nodes.data[id].preds.data,
           dag_npreds(g, id) * sizeof(unsigned));
}

int dag_weight(dag *g, unsigned id) {
    assert(g != NULL);
    assert(id < dag_size(g));
    return g->nodes.data[id].weight;
}

int dag_level(dag *g, unsigned id) {
    assert(g != NULL);
    assert(id < dag_size(g));
    return g->nodes.data[id].level;
}
