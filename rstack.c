#include "rstack.h"
#include "rgarbage_collector.h"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
// todo: check libraries
// todo: reformat into files - reformat style, - add "_helper" to helper function names - const vals

typedef struct rstack rstack_t;

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

// todo: fix - DONT TREAT CYCLES AS ERRORS

rstack_t *rstack_new() {
    if (global_gc == nullptr) {
        global_gc = gc_new();

        if (global_gc == nullptr) {
            return nullptr;
        }

        //satexit(gc_clear);
    }

    rstack_t *rstack = (rstack_t*)malloc(sizeof(rstack_t));

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

void reset_visited(rstack_t *rs) {
    if (rs != nullptr) {
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
}

void rstack_delete(rstack_t *rs) {
    if (rs != nullptr) {
        rs->ref_count--;
        gc_mark_and_sweep(global_gc);
    }
}

int rstack_push_value(rstack_t *rs, uint64_t value) {
    if (rs == nullptr) {
        errno = EINVAL;
        return FUNCTION_FAIL;
    }

    rstack_node_t *new_node = (rstack_node_t*)malloc(sizeof(rstack_node_t));
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

    rstack_node_t *new_node = (rstack_node_t*)malloc(sizeof(rstack_node_t));
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
    if (rs != nullptr && rs->head != nullptr) {
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
}

bool rstack_empty_helper(rstack_t *rs) {
    if (rs == nullptr) {
        return true;
    }

    rstack_node_t *current = rs->head;

    while (current != nullptr) {
        if (current->is_stack == false) { // wartoscia wezla jest wartosc liczbowa
            return false;
        }

        if (current->is_visited == false) { // wartoscia wezla jest stos
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

result_t result_new_empty() {
    result_t result;
    result.flag = false;
    result.value = 0;
    return result;
}

result_t rstack_front(rstack_t *rs) {
    if (rs == nullptr) {
        result_t result = result_new_empty();
        return result;
    }

    rstack_node_t *current = rs->head;

    while (current != nullptr && current->is_visited == false) {
        current->is_visited = true;

        if (current->is_stack == false) {
            result_t result;
            result.flag = true;
            result.value = current->value.num_value;
            reset_visited(rs);
            return result;
        }

        result_t current_result = rstack_front(current->value.stack_value);
        if (current_result.flag == true) {
            reset_visited(rs);
            return current_result;
        }
        current = current->next;
    }

    result_t result = result_new_empty();
    reset_visited(rs);
    return result;
}

bool is_number(int character) {
    return (character >= '0' && character <= '9');
}

result_t read_number_from_file(FILE *file_ptr) {
    result_t result;
    result.flag = true;
    result.value = 0;

    uint64_t number_result = 0;

    int character = fgetc(file_ptr);

    while (character != EOF && result.flag == true && is_number(character)) {
        uint64_t digit = character - '0';

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

rstack_t* rstack_read(char const *path) {
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
        if (isspace(character)) {
            while (isspace(character)) {
                character = fgetc(file_ptr);
            }
        }
        else {
            if (is_number(character)) {
                if (ungetc(character, file_ptr) == EOF) { // Blad
                    errno = EIO;
                    fclose(file_ptr);
                    rstack_delete(rs);
                    return nullptr;
                }

                result_t number_result = read_number_from_file(file_ptr);

                if (number_result.flag == true) {
                    rstack_push_value(rs, number_result.value);
                }
                else { // Blad, errno ustawione przy wczytywaniu liczby
                    fclose(file_ptr);
                    rstack_delete(rs);
                    return nullptr;
                }
            }
            else { // Znaleziono bledny znak (Blad).
                errno = EINVAL;
                fclose(file_ptr);
                rstack_delete(rs);
                return nullptr;
            }
        }
        character = fgetc(file_ptr);
    }

    fclose(file_ptr);

    return rs;
}

int rstack_write_helper(FILE *file_ptr, rstack_t *rs) { // wrong order
    if (rs == nullptr) {
        return FUNCTION_SUCCESS;
    }

    rstack_node_t *current = rs->head;

    while (current != nullptr) {
        if (current->is_visited == true) {
            return FUNCTION_SUCCESS; // Wykryto cykl.
        }

        current->is_visited = true;

        if (current->is_stack == false) {
            if (fprintf(file_ptr, "%lu\n", current->value.num_value) < 0) {
                errno = EIO;
                return FUNCTION_FAIL;
            }
        }
        else {
            int function_result = rstack_write_helper(file_ptr, current->value.stack_value);
            if (function_result != FUNCTION_SUCCESS) {
                return function_result;
            }
        }

        current = current->next;
    }

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
        errno = ENOENT; // todo: correct errno? check for rstack_read too
        return FUNCTION_FAIL;
    }

    int function_result = rstack_write_helper(file_ptr, rs);
    if (function_result == FUNCTION_FAIL) {
        fclose(file_ptr);
        return FUNCTION_FAIL;
    }

    reset_visited(rs);

    if (fclose(file_ptr) != FUNCTION_SUCCESS) {
        return FUNCTION_FAIL;
    }

    return FUNCTION_SUCCESS;
}