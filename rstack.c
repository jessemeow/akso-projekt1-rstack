#include "rstack.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
// todo: check libraries
// todo: reformat into files - reformat style, - add "_helper" to helper function names - const vals

#define CYCLE_DETECTED 1
#define FUNCTION_FAIL (-1)
#define FUNCTION_SUCCESS 0

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
    long ref_count;
    rstack_node_t *head;
} rstack_t;


[[nodiscard]]rstack_t *rstack_new() { // todo: czy mozna uzywac atrybutow?
    rstack_t *rstack = (rstack_t*)malloc(sizeof(rstack_t));
    if (rstack == nullptr) {
        errno = ENOMEM;
        return nullptr;
    }

    rstack->head = nullptr;
    rstack->ref_count = 1;
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

// todo: change function name
void rstack_cleaner(rstack_t *rs) {
    if (rs != nullptr) {
        rstack_node_t *current = rs->head;

        while (current != nullptr) {
            if (current->is_stack == true) {
                rstack_delete(current->value.stack_value);
            }

            rstack_node_t *node_to_be_deleted = current;
            current = current->next;
            rs->head = current;
            free(node_to_be_deleted);
        }
    }
}

void rstack_delete(rstack_t *rs) {
    if (rs != nullptr) {
        rs->ref_count--;

        if (rs->ref_count == 0) {
            rstack_cleaner(rs);
            free(rs);
        }
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

        if (node_to_be_deleted->is_stack == true) {
            rstack_delete(node_to_be_deleted->value.stack_value);
        }

        free(node_to_be_deleted);

        rs->head = next_node;
    }
}

bool rstack_empty(rstack_t *rs) {
    if (rs == nullptr) {
        return true;
    }

    rstack_node_t *current = rs->head;

    while (current != nullptr && current->is_visited == false) {
        current->is_visited = true;

        if (current->is_stack == false) {
            reset_visited(rs);
            return false;
        }

        if (rstack_empty(current->value.stack_value) == false) {
            reset_visited(rs);
            return false;
        }

        current = current->next;
    }

    reset_visited(rs);
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

    result_t result = result_empty_new();
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

int rstack_write_helper(FILE *file_ptr, rstack_t *rs) {
    if (rs == nullptr) {
        return FUNCTION_SUCCESS;
    }

    rstack_node_t *current = rs->head;

    while (current != nullptr) {
        if (current->is_visited == true) {
            return CYCLE_DETECTED;
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

// todo: error if cycle detected? - "a" mode, create new file if path doesnt exist?
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
    if (function_result != FUNCTION_SUCCESS) {
        return FUNCTION_FAIL; // fails if cycle detected
    }

    reset_visited(rs);

    if (fclose(file_ptr) != FUNCTION_SUCCESS) {
        return FUNCTION_FAIL;
    }

    return FUNCTION_SUCCESS;
}


////
// To są możliwe wyniki testu.
#define PASS 0
#define FAIL 1
#define WRONG_TEST 2

// Oblicza liczbę elementów tablicy x.
#define SIZE(x) (sizeof x / sizeof x[0])

#define ASSERT(f)            \
do {                       \
if (!(f))                \
return FAIL;           \
} while (0)

#define ASSERT_RESULT(c, f, ...)          \
do {                                    \
result_t r = c;                       \
if (r.flag != (f))                    \
return FAIL;                        \
if ((f) && r.value != __VA_ARGS__ -0) \
return FAIL;                        \
} while (0)

#define CHECK_IF_NO_ERROR(f) \
do {                       \
if ((f) != 0)            \
return FAIL;           \
} while (0)

#define V(code, where) (((unsigned long)code) << (3 * where))
////
////
static int zero(void) {
    rstack_t *rs0 = rstack_new();
    assert(rs0);

    ASSERT(rstack_empty(rs0) == true);
    ASSERT_RESULT(rstack_front(rs0), false);
    CHECK_IF_NO_ERROR(rstack_write("file_zero.out", rs0));
    rstack_delete(rs0);

    return PASS;
}
////


int main() {
    zero();
    return FUNCTION_SUCCESS;
}