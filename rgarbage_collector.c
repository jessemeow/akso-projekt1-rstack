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

garbage_collector_t global_gc_instance = {.head = nullptr};
garbage_collector_t *global_gc = &global_gc_instance;

typedef struct rstack_node {
    bool is_stack;
    bool is_visited;
    struct rstack_node *next;

    union {
        uint64_t num_value;
        rstack_t *stack_value;
    } value;
} rstack_node_t;

typedef struct rstack {
    bool reachable;
    rstack_node_t *head;
    uint64_t ref_count;
    uint64_t internal_ref_count;
} rstack_t;


static garbage_collector_node_t *gc_new_node(rstack_t *rs) {
    if (rs == nullptr) {
        errno = EINVAL;
        return nullptr;
    }

    garbage_collector_node_t *gc_node = (garbage_collector_node_t *) malloc(sizeof(garbage_collector_node_t));

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

static bool rstack_is_root(const rstack_t *rs) {
    if (rs != nullptr) {
        return (rs->ref_count > rs->internal_ref_count);
    }

    return false;
}

// todo: actually call this tho
static void gc_reset(const garbage_collector_t *gc) {
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
static void gc_find_roots(const garbage_collector_t *gc) {
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

static void rstack_set_reachable(rstack_t *rs) {
    if (rs == nullptr) {
        return;
    }

    rs->reachable = true;

    rstack_node_t *current = rs->head;

    while (current != nullptr) {
        if (current->is_stack &&
            current->value.stack_value != nullptr &&
            current->value.stack_value->reachable == false) {
            // todo: simplify
            rstack_set_reachable(current->value.stack_value);
        }

        current = current->next;
    }
}

static void gc_find_reachable(const garbage_collector_t *gc) {
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

static void gc_mark(const garbage_collector_t *gc) {
    if (gc == nullptr) {
        return;
    }

    gc_find_roots(gc);
    gc_find_reachable(gc);
}

static void rstack_cleaner(rstack_t *rs) {
    if (rs == nullptr) {
        return;
    }

    rstack_node_t *current = rs->head;

    while (current != nullptr) {
        if (current->is_stack == true) {
            current->value.stack_value->internal_ref_count--;
            current->value.stack_value->ref_count--;
        }

        rstack_node_t *node_to_be_deleted = current;
        current = current->next;
        rs->head = current;
        free(node_to_be_deleted);
    }
}

static void gc_clean_node(const garbage_collector_node_t *gc_node) {
    if (gc_node == nullptr) {
        return;
    }

    rstack_t *rs = gc_node->node;
    rstack_cleaner(rs);
}

static void gc_rstack_cleaner(const garbage_collector_t *gc) {
    if (gc == nullptr) {
        return;
    }

    garbage_collector_node_t *current = gc->head;

    while (current != nullptr) {
        rstack_t *rs = current->node;

        if (rs != nullptr && rs->reachable == false) {
            gc_clean_node(current);
        }

        current = current->next;
    }
}

static void gc_rstack_removal(garbage_collector_t *gc) {
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
                rstack_t *stack_to_be_deleted = current->node;

                current = current->next;

                free(node_to_be_deleted);

                if (stack_to_be_deleted != nullptr) {
                    free(stack_to_be_deleted);
                }
            }
        }
        else {
            current = current->next;
        }
    }
}

static void gc_sweep(garbage_collector_t *gc) {
    if (gc == nullptr) {
        return;
    }

    gc_rstack_cleaner(gc);
    gc_rstack_removal(gc);
}

void gc_mark_and_sweep(garbage_collector_t *gc) {
    if (gc == nullptr) {
        return;
    }

    gc_mark(gc);
    gc_sweep(gc);
    gc_reset(gc);
}
