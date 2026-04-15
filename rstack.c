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

// todo: to powinna byc funkcja rekurencyjna, ale czy nie bedzie stack overflow?
bool rstack_empty_helper(rstack_t *rs) {
    if (rs == nullptr) {
        return true;
    }

    rstack_node_t *current = rs->head;

    while (current != nullptr) {
        if (current->is_stack == false) { // Wartoscia wezla jest wartosc liczbowa.
            return false;
        }

        if (current->is_visited == false) { // Wartoscia wezla jest stos.
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
                    if (rstack_push_value(rs, number_result.value) == FUNCTION_FAIL) {
                        fclose(file_ptr);
                        rstack_delete(rs);
                        return nullptr;
                    }
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

int rstack_push_to_iterator(rstack_t *it, rstack_t *rs) { // succeed if pushed stack is null, immediately stops on detecting cycle
    if (it == nullptr) {
        return FUNCTION_FAIL;
    }

    if (rs == nullptr) {
        return FUNCTION_SUCCESS;
    }

    rstack_node_t *current = rs->head;

    while (current != nullptr && current->is_visited == false) {
        current->is_visited = true;

        if (current->is_stack == false) {
            if (rstack_push_value(it, current->value.num_value) == FUNCTION_FAIL) {
                return FUNCTION_FAIL;
            }
        }
        else if (current->value.stack_value != nullptr) {
            if (rstack_push_rstack(it, current->value.stack_value) == FUNCTION_FAIL) {
                return FUNCTION_FAIL;
            }
        }

        current = current->next;
    }

    if (current != nullptr && current->is_visited) {
        return CYCLE_DETECTED; // todo: use this somewhere else too idk
    }

    return FUNCTION_SUCCESS;
}

int rstack_fetch_next_from_iterator(rstack_t *it, result_t* result) {
    result->flag = false;
    result->value = 0;

    if (it == nullptr || result == nullptr) {
        return FUNCTION_FAIL;
    }

    while (it->head != nullptr && it->head->is_stack) {
        rstack_t *rs = it->head->value.stack_value;
        if (rs != nullptr) {
            rstack_pop(it);

            int function_push_result = rstack_push_to_iterator(it, rs);
            if (function_push_result != FUNCTION_SUCCESS) {
                return function_push_result;
            }
        }
    }

    if (it->head == nullptr) {
        return FUNCTION_SUCCESS;
    }

    result->flag = true;
    result->value = it->head->value.num_value;

    return FUNCTION_SUCCESS;
}

// todo: reset subnodes
int rstack_write_helper(FILE *file_ptr, rstack_t *rs) {
    if (rs == nullptr) {
        return FUNCTION_SUCCESS;
    }

    if (file_ptr == nullptr) {
        errno = ENOENT;
        return FUNCTION_FAIL;
    }

    rstack_t *it = rstack_new();
    if (it == nullptr) {
        return FUNCTION_FAIL;
    }

    if (rstack_push_to_iterator(it, rs) == FUNCTION_FAIL) {
        rstack_delete(it);
        return FUNCTION_FAIL;
    }

    result_t current_val = result_new_empty();

    int function_result = rstack_fetch_next_from_iterator(it, &current_val);

    if (function_result == FUNCTION_FAIL) {
        rstack_delete(it);
        return FUNCTION_FAIL;
    }

    if (function_result == CYCLE_DETECTED) {
        rstack_delete(it);
        return FUNCTION_SUCCESS;
    }

    while (current_val.flag == true) {
        rstack_pop(it);

        uint64_t current_num_value = current_val.value;
        if (fprintf(file_ptr, "%lu\n", current_num_value) < 0) {
            errno = EIO;
            rstack_delete(it);
            return FUNCTION_FAIL;
        }

        function_result = rstack_fetch_next_from_iterator(it, &current_val);

        if (function_result == FUNCTION_FAIL) {
            rstack_delete(it);
            return FUNCTION_FAIL;
        }

        if (function_result == CYCLE_DETECTED) {
            rstack_delete(it);
            return FUNCTION_SUCCESS;
        }
    }

    rstack_delete(it);

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

    if (rstack_write_helper(file_ptr, rs) == FUNCTION_FAIL) {
        reset_visited(rs); 
        fclose(file_ptr); // Function fails regardless of fclose fail.
        return FUNCTION_FAIL;
    }

    reset_visited(rs); //todo ?

    if (fclose(file_ptr) != FUNCTION_SUCCESS) {
        return FUNCTION_FAIL;
    }

    return FUNCTION_SUCCESS;
}


static int wojtekmal_0(void) {
    rstack_t *rs0 = rstack_new();
    rstack_t *rs1 = rstack_new();
    rstack_t *rs2 = rstack_new();

    rstack_push_value(rs0, 3);
    rstack_push_value(rs0, 2);
    rstack_push_value(rs0, 1);
    rstack_push_rstack(rs0, rs1);

    rstack_push_value(rs1, 5);
    rstack_push_rstack(rs1, rs2);
    rstack_push_value(rs1, 4);

    rstack_push_rstack(rs2, rs1);
    rstack_push_value(rs2, 6);

    rstack_write("w.out", rs0);

    rstack_delete(rs0);
    rstack_delete(rs1);
    rstack_delete(rs2);

    return 0;
}

void test1() {
    rstack_t *A = rstack_new();
    rstack_t *B = rstack_new();
    rstack_t *C = rstack_new();
    rstack_t *D = rstack_new();
    rstack_t *E = rstack_new();
    rstack_t *F = rstack_new();
    rstack_t *G = rstack_new();
    rstack_t *H = rstack_new();
    rstack_t *I = rstack_new();
    rstack_t *J = rstack_new();
    rstack_t *K = rstack_new();
    rstack_t *L = rstack_new();
    rstack_t *M = rstack_new();
    rstack_t *N = rstack_new();
    rstack_t *P = rstack_new();
    rstack_t *Q = rstack_new();
    rstack_t *R = rstack_new();
    rstack_t *S = rstack_new();

    rstack_push_rstack(A, B);
    rstack_push_rstack(B, C);
    rstack_push_rstack(C, D);
    rstack_push_rstack(D, E);
    rstack_push_rstack(E, F);
    rstack_push_rstack(F, G);
    rstack_push_rstack(G, H);
    rstack_push_rstack(H, I);
    rstack_push_rstack(I, J);
    rstack_push_rstack(J, K);
    rstack_push_rstack(K, L);
    rstack_push_rstack(L, M);
    rstack_push_rstack(M, N);
    rstack_push_rstack(N, P);
    rstack_push_rstack(P, Q);
    rstack_push_rstack(Q, R);
    rstack_push_rstack(R, S);

    printf("%d\n", rstack_empty(A));

    rstack_delete(A);
    rstack_delete(B);
    rstack_delete(C);
    rstack_delete(D);
    rstack_delete(E);
    rstack_delete(F);
    rstack_delete(G);
    rstack_delete(H);
    rstack_delete(I);
    rstack_delete(J);
    rstack_delete(K);
    rstack_delete(L);
    rstack_delete(M);
    rstack_delete(N);
    rstack_delete(P);
    rstack_delete(Q);
    rstack_delete(R);
    rstack_delete(S);
}

int main() {
    wojtekmal_0();
}