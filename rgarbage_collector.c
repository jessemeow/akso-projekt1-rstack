#include "rgarbage_collector.h"

// todo: consts

typedef struct garbage_collector_node {
    rstack_t *node;
    struct garbage_collector_node *next;
    bool is_root;
} garbage_collector_node_t;

typedef struct garbage_collector {
    garbage_collector_node_t *head;
} garbage_collector_t;

typedef struct rstack_node {
    bool is_stack;
    bool is_visited;
    union {
        uint64_t num_value;
        rstack_t *stack_value;
    } value;
    struct rstack_node *next;
} rstack_node_t;

typedef struct rstack {
    uint64_t ref_count;
    uint64_t internal_ref_count;
    bool reachable;
    rstack_node_t *head;
} rstack_t;

extern garbage_collector_t *global_gc;

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
    gc_node->is_root = false;

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

bool rstack_is_root(rstack_t *rs) {
    if (rs != nullptr) {
        return (rs->ref_count > rs->internal_ref_count);
    }

    return false;
}

// todo: actually call this tho
void gc_reset(garbage_collector_t *gc) {
    if (gc == nullptr) {
        return;
    }

    garbage_collector_node_t *current = gc->head;

    while (current != nullptr) {
        current->is_root = false;

        rstack_t *rs = current->node;

        if (rs != nullptr) {
            rs->reachable = false;
        }

        current = current->next;
    }
}

// todo: result return value?
void gc_find_roots(garbage_collector_t *gc) {
    if (gc == nullptr) {
        return;
    }

    garbage_collector_node_t *current = gc->head;

    while (current != nullptr) {
        rstack_t *rs = current->node;

        if (rs != nullptr) {
            if (rstack_is_root(rs)) {
                current->is_root = true;
            }
        }

        current = current->next;
    }
}

void rstack_set_reachable(rstack_t *rs) {
    if (rs == nullptr) {
        return;
    }

    rs->reachable = true;

    rstack_node_t *current = rs->head;

    while (current != nullptr) {
        if (current->is_stack && current->value.stack_value != nullptr && current->value.stack_value->reachable == false) { // todo: simplify
            rstack_set_reachable(current->value.stack_value);
        }

        current = current->next;
    }
}

void gc_find_reachable(garbage_collector_t *gc) {
    if (gc == nullptr) {
        return;
    }

    garbage_collector_node_t *current = gc->head;

    while (current != nullptr) {
        if (current->is_root) {
            rstack_set_reachable(current->node);
        }

        current = current->next;
    }
}

void gc_mark(garbage_collector_t *gc) {
    if (gc == nullptr) {
        return;
    }

    gc_find_roots(gc);
    gc_find_reachable(gc);
}

void rstack_cleaner(rstack_t *rs) {
    if (rs != nullptr) {
        rstack_node_t *current = rs->head;

        while (current != nullptr) {
            if (current->is_stack == true) { // segfault A->B->A
                current->value.stack_value->internal_ref_count--;
                current->value.stack_value->ref_count--;
            }

            rstack_node_t *node_to_be_deleted = current;
            current = current->next;
            rs->head = current;
            free(node_to_be_deleted);
        }
    }
}

void gc_remove_rstack(garbage_collector_node_t *gc_node) {
    if (gc_node == nullptr) {
        return;
    }

    rstack_t *rs = gc_node->node;
    rstack_cleaner(rs);
    free(rs);
    free(gc_node);
}

void gc_sweep(garbage_collector_t *gc) {
    if (gc == nullptr) {
        return;
    }

    garbage_collector_node_t *current = gc->head;
    garbage_collector_node_t *previous = nullptr;

    while (current != nullptr) {
        rstack_t *rs = current->node;

        if (rs != nullptr) {
            if (rs->reachable) {
                previous = current;
                current = current->next;
            }
            else {
                if (previous != nullptr) {
                    previous->next = current->next;
                }
                else {
                    gc->head = current->next;
                }

                garbage_collector_node_t *node_to_be_deleted = current;
                current = current->next;
                gc_remove_rstack(node_to_be_deleted);
            }
        }
        else {
            current = current->next;
        }
    }
}

bool gc_is_empty(garbage_collector_t *gc) {
    if (gc == nullptr) {
        return false;
    }

    if (gc->head == nullptr) {
        return true;
    }

    return false;
}

void gc_clear(void) {
    if (global_gc != nullptr) {
        garbage_collector_node_t *current = global_gc->head;

        while (current != nullptr) {
            garbage_collector_node_t *next = current->next;
            free(current);
            current = next;
        }

        free(global_gc);
    }
}

void gc_mark_and_sweep(garbage_collector_t *gc) {
    if (gc == nullptr) {
        return;
    }

    gc_mark(gc);
    gc_sweep(gc);
    gc_reset(gc);
}

