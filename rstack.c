#include "rstack.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct rstack rstack_t;

typedef struct rstack_node {
    bool is_stack;
    union {
        uint64_t num_value;
        rstack_t *stack_value;
    } value;
    struct rstack_node *next;
} rstack_node_t;

typedef struct rstack {
    long ref_count;
    rstack_node_t *head;
} rstack_t;


rstack_t *rstack_new() {
    rstack_t *rstack = (rstack_t*)malloc(sizeof(rstack_t));
    if (rstack == nullptr) {
        errno = ENOMEM;
        return nullptr;
    }

    rstack->head = nullptr;
    rstack->ref_count = 1; // todo: co jesli wartosc funkcji nie jest przypisana do zmiennej?
    return rstack;
}

// todo: change function name
void rstack_cleaner(rstack_t *rs) {
    if (rs != nullptr) {
        rstack_node_t *current = rs->head;
        while (current != nullptr) {
            if (current->is_stack == true) {
                rstack_t *current_stack = current->value.stack_value;
                (current_stack->ref_count)--;

                if (current_stack->ref_count <= 0) {
                    rstack_delete(current_stack);
                }

                current = current->next;
            }
            else { // Numerical value
                rstack_node_t *node_to_be_deleted = current;
                current = current->next;
                free(node_to_be_deleted);
            }
        }
    }
}

// todo: handle cycles
void rstack_delete(rstack_t *rs) {
    printf("i\n");
    if (rs != nullptr) {
        rs->ref_count--;
        rstack_cleaner(rs);
        if (rs != nullptr) { // rs może zostać usunięty w rstack_cleaner
            free(rs);
        }
    }
}

int rstack_push_value(rstack_t *rs, uint64_t value) {
    if (rs == nullptr) {
        errno = EINVAL;
        return -1;
    }

    rstack_node_t *new_node = (rstack_node_t*)malloc(sizeof(rstack_node_t));
    if (new_node == nullptr) {
        errno = ENOMEM;
        return -1;
    }

    new_node->is_stack = false;
    new_node->value.num_value = value;
    new_node->next = rs->head;
    rs->head = new_node;
    return 0;
}

int rstack_push_rstack(rstack_t *rs1, rstack_t *rs2) {
    if (rs1 == nullptr || rs2 == nullptr) {
        errno = EINVAL;
        return -1;
    }

    rstack_node_t *new_node = (rstack_node_t*)malloc(sizeof(rstack_node_t));
    if (new_node == nullptr) {
        errno = ENOMEM;
        return -1;
    }

    rs2->ref_count++;

    new_node->is_stack = true;
    new_node->value.stack_value = rs2;
    new_node->next = rs1->head;

    rs1->head = new_node;

    return 0;
}

void rstack_pop(rstack_t *rs) {
}

bool rstack_empty(rstack_t *rs) {
    if (rs == nullptr) {
        return true;
    }

    rstack_node_t *current = rs->head;

    while (current != nullptr) {
        if (current->is_stack == false) {
            return false;
        }

        if (rstack_empty(current->value.stack_value) == false) {
            return false;
        }

        current = current->next;
    }

    return true;
}

result_t result_empty_new() {
    result_t result;
    result.flag = false;
    result.value = 0;
    return result;
}

result_t rstack_front(rstack_t *rs) {
    if (rs == nullptr) {
        result_t result = result_empty_new();
        return result;
    }

    rstack_node_t *current = rs->head;
    while (current != nullptr) {
        if (current->is_stack == false) {
            result_t result;
            result.flag = true;
            result.value = current->value.num_value;
            return result;
        }

        result_t current_result = rstack_front(current->value.stack_value);
        if (current_result.flag == true) {
            return current_result;
        }
        current = current->next;
    }

    result_t result = result_empty_new();
    return result;
}

rstack_t* rstack_read(char const *path);

int rstack_write(char const *path, rstack_t *rs);


int main(void) {
    rstack_t *rs1 = rstack_new();
    rstack_push_rstack(rs1, rstack_new());
    rstack_delete(rs1);

}