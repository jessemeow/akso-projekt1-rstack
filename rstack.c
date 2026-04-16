#include "rstack.h"
#include "rgarbage_collector.h"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
// todo: reformat into files - reformat style, - const vals

typedef struct rstack rstack_t;

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

extern garbage_collector_t *global_gc;

rstack_t *rstack_new() {
    rstack_t *rstack = (rstack_t *) malloc(sizeof(rstack_t));

    if (rstack == nullptr) {
        errno = ENOMEM;
        return nullptr;
    }

    if (gc_push_rstack(global_gc, rstack) == FUNCTION_FAIL) {
        free(rstack);
        errno = ENOMEM;
        return nullptr;
    }

    rstack->head = nullptr;
    rstack->ref_count = 1;
    rstack->internal_ref_count = 0;
    rstack->reachable = false;
    return rstack;
}

static void reset_visited(const rstack_t *rs) {
    if (rs == nullptr) {
        return;
    }

    rstack_node_t *current = rs->head;

    while (current != nullptr) {
        if (current->is_visited == true) {
            current->is_visited = false;

            if (current->is_stack == true) {
                reset_visited(current->value.stack_value);
            }
        }

        current = current->next;
    }
}

void rstack_delete(rstack_t *rs) {
    if (rs == nullptr) {
        return;
    }

    rs->ref_count--;
    gc_mark_and_sweep(global_gc);
}

int rstack_push_value(rstack_t *rs, uint64_t value) {
    if (rs == nullptr) {
        errno = EINVAL;
        return FUNCTION_FAIL;
    }

    rstack_node_t *new_node = (rstack_node_t *) malloc(sizeof(rstack_node_t));
    if (new_node == nullptr) {
        errno = ENOMEM;
        return FUNCTION_FAIL;
    }

    new_node->is_stack = false;
    new_node->is_visited = false;
    new_node->value.num_value = value;
    new_node->next = rs->head;
    rs->head = new_node;
    return FUNCTION_SUCCESS;
}

int rstack_push_rstack(rstack_t *rs1, rstack_t *rs2) {
    if (rs1 == nullptr || rs2 == nullptr) {
        errno = EINVAL;
        return FUNCTION_FAIL;
    }

    rstack_node_t *new_node = (rstack_node_t *) malloc(sizeof(rstack_node_t));
    if (new_node == nullptr) {
        errno = ENOMEM; // todo: errno already set?
        return FUNCTION_FAIL;
    }

    rs2->ref_count++;
    rs2->internal_ref_count++;

    new_node->is_stack = true;
    new_node->is_visited = false;
    new_node->value.stack_value = rs2;
    new_node->next = rs1->head;

    rs1->head = new_node;

    return FUNCTION_SUCCESS;
}

void rstack_pop(rstack_t *rs) {
    if (rs == nullptr || rs->head == nullptr) {
        return;
    }

    rstack_node_t *node_to_be_deleted = rs->head;
    rstack_node_t *next_node = node_to_be_deleted->next;

    if (node_to_be_deleted == next_node) {
        next_node = nullptr;
    }

    rs->head = next_node;

    if (node_to_be_deleted->is_stack == true) {
        node_to_be_deleted->value.stack_value->internal_ref_count--;
        rstack_delete(node_to_be_deleted->value.stack_value);
    }

    free(node_to_be_deleted);
}

static bool rstack_empty_helper(const rstack_t *rs) {
    if (rs == nullptr) {
        return true;
    }

    rstack_node_t *current = rs->head;

    while (current != nullptr) {
        if (current->is_stack == false) {
            // Wartoscia wezla jest wartosc liczbowa.
            return false;
        }

        if (current->is_visited == false) {
            // Wartoscia wezla jest stos.
            current->is_visited = true;

            if (rstack_empty_helper(current->value.stack_value) == false) {
                return false;
            }
        }

        current = current->next;
    }

    return true;
}

bool rstack_empty(rstack_t *rs) {
    bool result = rstack_empty_helper(rs);
    reset_visited(rs);
    return result;
}

static result_t result_new_empty() {
    result_t result;
    result.flag = false;
    result.value = 0;
    return result;
}

static result_t result_new(const rstack_node_t *node) {
    result_t result = result_new_empty();

    if (node == nullptr || node->is_stack) {
        return result;
    }

    result.flag = true;
    result.value = node->value.num_value;

    return result;
}

result_t rstack_front(rstack_t *rs) {
    result_t result = result_new_empty();

    if (rs == nullptr) {
        return result;
    }

    rstack_node_t *current = rs->head;

    while (current != nullptr && current->is_visited == false) {
        current->is_visited = true;

        if (current->is_stack == false) {
            result = result_new(current);
            reset_visited(rs);
            return result;
        }

        const result_t temp_result = rstack_front(current->value.stack_value);
        if (temp_result.flag == true) {
            reset_visited(rs);
            return temp_result;
        }
        current = current->next;
    }

    reset_visited(rs);
    return result;
}

static bool is_number(int character) {
    return (character >= '0' && character <= '9');
}

static result_t read_number_from_file(FILE *file_ptr) {
    result_t result;
    result.flag = true;
    result.value = 0;

    uint64_t number_result = 0;

    int character = fgetc(file_ptr);

    while (character != EOF && result.flag == true && is_number(character)) {
        const uint64_t digit = character - '0';

        if (number_result > (UINT64_MAX - digit) / 10) {
            result.flag = false;
            errno = ERANGE;
        }
        else {
            number_result *= 10;
            number_result += digit;
            character = fgetc(file_ptr);
        }
    }

    if (character != EOF) {
        ungetc(character, file_ptr);
    }

    if (result.flag == true) {
        result.value = number_result;
    }

    return result;
}

static void skip_whitespace(FILE* file_ptr, int *character) {
    if (file_ptr == nullptr) {
        return;
    }

    while (isspace(*character)) {
        *character = fgetc(file_ptr);
    }
}

static int process_number(FILE* file_ptr, rstack_t *rs, int *character) {
    if (ungetc(*character, file_ptr) == EOF) {
        errno = EIO;
        fclose(file_ptr);
        rstack_delete(rs);
        return FUNCTION_FAIL;
    }

    const result_t number_result = read_number_from_file(file_ptr);

    if (number_result.flag == true) {
        if (rstack_push_value(rs, number_result.value) == FUNCTION_FAIL) {
            fclose(file_ptr);
            rstack_delete(rs);
            return FUNCTION_FAIL;
        }

        *character = fgetc(file_ptr);
    }
    else {
        // Blad przy wczytywaniu liczby.
        fclose(file_ptr);
        rstack_delete(rs);
        return FUNCTION_FAIL;
    }

    return FUNCTION_SUCCESS;
}

rstack_t *rstack_read(char const *path) {
    if (path == nullptr) {
        errno = EINVAL;
        return nullptr;
    }

    FILE *file_ptr = fopen(path, "r");
    if (file_ptr == nullptr) {
        errno = ENOENT;
        return nullptr;
    }

    rstack_t *rs = rstack_new();
    if (rs == nullptr) {
        fclose(file_ptr);
        return nullptr;
    }

    int character = fgetc(file_ptr);
    while (character != EOF) {
        skip_whitespace(file_ptr, &character);

        if (character == EOF) {
            break;
        }

        if (is_number(character)) {
            if (process_number(file_ptr, rs, &character) == FUNCTION_FAIL) {
                return nullptr;
            }
        }
        else {
            // Znaleziono bledny znak (Blad).
            errno = EINVAL;
            fclose(file_ptr);
            rstack_delete(rs);
            return nullptr;
        }
    }

    fclose(file_ptr);

    return rs;
}

static int print_num_to_file(FILE *file_ptr, uint64_t num) {
    if (fprintf(file_ptr, "%lu\n", num) < 0) {
        errno = EIO;
        return FUNCTION_FAIL;
    }

    return FUNCTION_SUCCESS;
}

static int rstack_write_helper(FILE *file_ptr, rstack_node_t *node) {
    if (node == nullptr) {
        return FUNCTION_SUCCESS;
    }

    if (file_ptr == nullptr) {
        errno = ENOENT;
        return FUNCTION_FAIL;
    }

    if (node->is_visited) {
        return CYCLE_DETECTED;
    }

    int function_result = rstack_write_helper(file_ptr, node->next);
    if (function_result != FUNCTION_SUCCESS) {
        return function_result;
    }

    node->is_visited = true;

    if (node->is_stack == false) {
        function_result = print_num_to_file(file_ptr, node->value.num_value);
        if (function_result == FUNCTION_FAIL) {
            node->is_visited = false;
            return FUNCTION_FAIL;
        }
    }
    else {
        const rstack_t *current_rs = node->value.stack_value;

        if (current_rs != nullptr &&
            current_rs->head != nullptr) {
            function_result = rstack_write_helper(file_ptr, current_rs->head);
            if (function_result != FUNCTION_SUCCESS) {
                node->is_visited = false;
                return function_result;
            }
        }
    }

    node->is_visited = false;

    return FUNCTION_SUCCESS;
}

int rstack_write(char const *path, rstack_t *rs) {
    if (rs == nullptr) {
        errno = EINVAL;
        return FUNCTION_FAIL;
    }

    if (path == nullptr) {
        errno = ENOENT;
        return FUNCTION_FAIL;
    }

    FILE *file_ptr = fopen(path, "w");
    if (file_ptr == nullptr) {
        errno = ENOENT;
        return FUNCTION_FAIL;
    }

    const int function_result = rstack_write_helper(file_ptr, rs->head);

    reset_visited(rs);

    if (function_result == FUNCTION_FAIL) {
        fclose(file_ptr);
        return FUNCTION_FAIL;
    }

    if (fclose(file_ptr) != FUNCTION_SUCCESS) {
        return FUNCTION_FAIL;
    }

    return FUNCTION_SUCCESS;
}


