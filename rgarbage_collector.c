#include "rgarbage_collector.h"

typedef struct garbage_collector_node {
    rstack_t *node;
    struct garbage_collector_node *next;
    bool reachable;
} garbage_collector_node_t;

typedef struct garbage_collector {
    garbage_collector_node_t *head;
} garbage_collector_t;


// garbage collector TODO!

garbage_collector_t *gc_new() {
    garbage_collector_t *gc = (garbage_collector_t*)malloc(sizeof(garbage_collector_t));

    if (gc == nullptr) {
        errno = ENOMEM;
        return nullptr;
    }

    gc->head = nullptr;

    return gc;
}

garbage_collector_node_t *gc_new_node(rstack_t *rs) {
    if (rs == nullptr) {
        errno = EINVAL;
        return nullptr;
    }

    garbage_collector_node_t *gc_node = (garbage_collector_node_t*)malloc(sizeof(garbage_collector_node_t));

    if (gc_node == nullptr) {
        errno = ENOMEM;
        return nullptr;
    }

    gc_node->node = rs;
    gc_node->next = nullptr;
    gc_node->reachable = false; // todo: ???

    return gc_node;
}

int gc_push_rstack(garbage_collector_t *gc, rstack_t *rs) {
    if (rs == nullptr || gc == nullptr) {
        errno = EINVAL;
        return FUNCTION_FAIL;
    }

    garbage_collector_node_t *gc_node = gc_new_node(rs);

    if (gc_node == nullptr) {
        return FUNCTION_FAIL;
    }

    garbage_collector_node_t *current = gc->head;
    gc_node->next = current;
    gc->head = gc_node;

    return FUNCTION_SUCCESS;
}

// todo: result return value?
int *gc_find_roots(garbage_collector_t *gc) {
    if (gc == nullptr) {
        return FUNCTION_SUCCESS; //todo: ???
    }

    garbage_collector_node_t *current = gc->head;

    while (current != nullptr) {

    }

    return FUNCTION_SUCCESS;
}