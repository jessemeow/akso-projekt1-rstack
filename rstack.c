#include "rstack.h"
#include "rgarbage_collector.h"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

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

extern garbage_collector_t *global_garbage_collector;

rstack_t *rstack_new() {
    rstack_t *rstack = (rstack_t *) malloc(sizeof(rstack_t));

    if (rstack == nullptr) {
        errno = ENOMEM;
        return nullptr;
    }

    if (gc_push_rstack(global_garbage_collector, rstack) == FUNCTION_FAIL) {
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
        if (current->is_visited) {
            current->is_visited = false;

            if (current->is_stack) {
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
    gc_mark_and_sweep(global_garbage_collector);
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
        errno = ENOMEM;
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

    if (node_to_be_deleted->is_stack) {
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
        if (!current->is_stack) {
            return false;
        }

        if (!current->is_visited) {
            current->is_visited = true;

            if (!rstack_empty_helper(current->value.stack_value)) {
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

    while (current != nullptr && !current->is_visited) {
        current->is_visited = true;

        if (!current->is_stack) {
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

static bool is_number(const int character) {
    return (character >= '0' && character <= '9');
}

static result_t read_number_from_file(FILE *file_ptr) {
    result_t result;
    result.flag = true;
    result.value = 0;

    uint64_t number_result = 0;

    int character = fgetc(file_ptr);

    while (character != EOF && result.flag && is_number(character)) {
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

    if (result.flag) {
        result.value = number_result;
    }

    return result;
}

static void skip_whitespace(FILE *file_ptr, int *character) {
    if (file_ptr == nullptr) {
        return;
    }

    while (isspace(*character)) {
        *character = fgetc(file_ptr);
    }
}

static int process_number(FILE *file_ptr, rstack_t *rs, int *character) {
    if (ungetc(*character, file_ptr) == EOF) {
        errno = EIO;
        fclose(file_ptr);
        rstack_delete(rs);
        return FUNCTION_FAIL;
    }

    const result_t number_result = read_number_from_file(file_ptr);

    if (number_result.flag) {
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
            errno = EINVAL;
            fclose(file_ptr);
            rstack_delete(rs);
            return nullptr;
        }
    }

    fclose(file_ptr);

    return rs;
}

static int print_num_to_file(FILE *file_ptr, const uint64_t num) {
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

    if (node->is_stack) {
        const rstack_t *current_rs = node->value.stack_value;

        if (current_rs != nullptr &&
            current_rs->head != nullptr) {
            function_result = rstack_write_helper(file_ptr, current_rs->head);
            if (function_result != FUNCTION_SUCCESS) {
                return function_result;
            }
            }
    }
    else {
        function_result = print_num_to_file(file_ptr, node->value.num_value);
        if (function_result == FUNCTION_FAIL) {
            return FUNCTION_FAIL;
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


#ifndef TEST_MACROS
#define TEST_MACROS

#include <inttypes.h>
#include <stdio.h> // IWYU pragma: keep (this stops clangd from reporting this include as unnecessary)

#define PASS 0
#define FAIL 1

#define OUTPUT_FILE "test.fout"

#define REPORT(...)                                                            \
    do {                                                                       \
        fprintf(stderr, "%s:%d (%s): ", __FILE__, __LINE__, __func__);         \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
    } while (0)

#define SIZE(x) (sizeof x / sizeof x[0])

#define ASSERT(f)                                                              \
    do {                                                                       \
        if (!(f)) {                                                            \
            REPORT("Assertion failed: %s", #f);                                \
            return FAIL;                                                       \
        }                                                                      \
    } while (0)

// If __VA_ARGS__ has a value, use it. Otherwise, fallback to 0.
#define GET_EXPECTED(...) __VA_OPT__(__VA_ARGS__) __VA_OPT__(+) 0UL

#define ASSERT_RESULT(c, f, ...)                                               \
    do {                                                                       \
        result_t r = c;                                                        \
        if (r.flag != (f)) {                                                   \
            REPORT("Result assertion failed, %s.flag is \"%s\" but should be " \
                   "\"%s\".",                                                  \
                   #c,                                                         \
                   r.flag ? "true" : "false",                                  \
                   #f);                                                        \
            return FAIL;                                                       \
        }                                                                      \
        if ((f) && r.value != __VA_ARGS__ - 0) {                               \
            REPORT("Result assertion failed, %s.value is %" PRIu64             \
                   " but should be %" PRIu64 ".",                              \
                   #c,                                                         \
                   r.value,                                                    \
                   GET_EXPECTED());                                            \
            return FAIL;                                                       \
        }                                                                      \
    } while (0)

#define NO_ERROR(f)                                                            \
    do {                                                                       \
        if ((f) != 0) {                                                        \
            REPORT(                                                            \
              "Expected %s to exit with no error but it returned %d", #f, f);  \
            return FAIL;                                                       \
        }                                                                      \
    } while (0)
#define CHECK_IF_NO_ERROR(f) NO_ERROR(f);

#define PRINT_U64(v) printf("%" PRIu64 "\n", v);

#define TEST_FILE(name) "test_" name ".fout"

#endif // TEST_MACROS
#include <assert.h>

int main(void) {
    rstack_t* rs0 = rstack_new();
    rstack_t* rs1 = rstack_new();
    rstack_t* rs2 = rstack_new();
    rstack_t* rs3 = rstack_new();
    rstack_t* rs4 = rstack_new();
    rstack_t* rs5 = rstack_new();
    rstack_t* rs6 = rstack_new();
    rstack_t* rs7 = rstack_new();
    rstack_t* rs8 = rstack_new();
    rstack_t* rs9 = rstack_new();
    NO_ERROR(rstack_push_rstack(rs3, rs6));
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    ASSERT(rstack_empty(rs4) == true);
    NO_ERROR(rstack_push_rstack(rs6, rs1));
    NO_ERROR(rstack_push_rstack(rs4, rs0));
    ASSERT_RESULT(rstack_front(rs1), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs6, rs0));
    ASSERT_RESULT(rstack_front(rs8), false, 0UL);
    ASSERT_RESULT(rstack_front(rs2), false, 0UL);
    ASSERT_RESULT(rstack_front(rs8), false, 0UL);
    NO_ERROR(rstack_write("test_16.fout", rs9));
    ASSERT_RESULT(rstack_front(rs9), false, 0UL);
    NO_ERROR(rstack_write("test_12.fout", rs6));
    ASSERT(rstack_empty(rs8) == true);
    ASSERT_RESULT(rstack_front(rs8), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs3, rs1));
    ASSERT_RESULT(rstack_front(rs3), false, 0UL);
    ASSERT_RESULT(rstack_front(rs6), false, 0UL);
    NO_ERROR(rstack_write("test_1.fout", rs6));
    ASSERT(rstack_empty(rs4) == true);
    NO_ERROR(rstack_write("test_14.fout", rs6));
    NO_ERROR(rstack_write("test_19.fout", rs0));
    NO_ERROR(rstack_push_rstack(rs7, rs9));
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs0, rs3));
    NO_ERROR(rstack_push_rstack(rs1, rs9));
    ASSERT(rstack_empty(rs9) == true);
    ASSERT_RESULT(rstack_front(rs8), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs0, rs4));
    NO_ERROR(rstack_push_rstack(rs1, rs1));
    NO_ERROR(rstack_write("test_13.fout", rs8));
    ASSERT_RESULT(rstack_front(rs1), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs0, rs9));
    ASSERT_RESULT(rstack_front(rs6), false, 0UL);
    ASSERT(rstack_empty(rs2) == true);
    ASSERT_RESULT(rstack_front(rs6), false, 0UL);
    ASSERT_RESULT(rstack_front(rs8), false, 0UL);
    ASSERT(rstack_empty(rs6) == true);
    NO_ERROR(rstack_push_rstack(rs3, rs6));
    ASSERT_RESULT(rstack_front(rs6), false, 0UL);
    NO_ERROR(rstack_write("test_10.fout", rs9));
    ASSERT_RESULT(rstack_front(rs8), false, 0UL);
    ASSERT_RESULT(rstack_front(rs7), false, 0UL);
    ASSERT_RESULT(rstack_front(rs1), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs8, rs5));
    NO_ERROR(rstack_push_rstack(rs6, rs2));
    ASSERT(rstack_empty(rs5) == true);
    NO_ERROR(rstack_push_rstack(rs8, rs9));
    NO_ERROR(rstack_push_rstack(rs9, rs4));
    NO_ERROR(rstack_push_rstack(rs7, rs7));
    NO_ERROR(rstack_write("test_2.fout", rs9));
    NO_ERROR(rstack_write("test_15.fout", rs7));
    NO_ERROR(rstack_write("test_18.fout", rs4));
    NO_ERROR(rstack_push_rstack(rs9, rs3));
    ASSERT_RESULT(rstack_front(rs1), false, 0UL);
    ASSERT_RESULT(rstack_front(rs6), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs7, rs4));
    ASSERT_RESULT(rstack_front(rs4), false, 0UL);
    ASSERT_RESULT(rstack_front(rs6), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs1, rs9));
    ASSERT_RESULT(rstack_front(rs4), false, 0UL);
    NO_ERROR(rstack_write("test_0.fout", rs1));
    ASSERT_RESULT(rstack_front(rs4), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs4, rs7));
    ASSERT_RESULT(rstack_front(rs4), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs0, rs4));
    ASSERT(rstack_empty(rs2) == true);
    ASSERT_RESULT(rstack_front(rs2), false, 0UL);
    ASSERT(rstack_empty(rs4) == true);
    NO_ERROR(rstack_push_rstack(rs9, rs6));
    ASSERT_RESULT(rstack_front(rs3), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs7, rs5));
    NO_ERROR(rstack_push_rstack(rs5, rs1));
    ASSERT(rstack_empty(rs5) == true);
    NO_ERROR(rstack_write("test_9.fout", rs2));
    ASSERT_RESULT(rstack_front(rs4), false, 0UL);
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs7, rs7));
    ASSERT(rstack_empty(rs7) == true);
    NO_ERROR(rstack_write("test_8.fout", rs9));
    ASSERT_RESULT(rstack_front(rs7), false, 0UL);
    ASSERT(rstack_empty(rs7) == true);
    NO_ERROR(rstack_write("test_5.fout", rs6));
    ASSERT(rstack_empty(rs5) == true);
    NO_ERROR(rstack_push_rstack(rs3, rs1));
    ASSERT_RESULT(rstack_front(rs1), false, 0UL);
    NO_ERROR(rstack_write("test_17.fout", rs4));
    ASSERT_RESULT(rstack_front(rs1), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs3, rs5));
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs5, rs8));
    ASSERT(rstack_empty(rs2) == true);
    ASSERT_RESULT(rstack_front(rs5), false, 0UL);
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    NO_ERROR(rstack_write("test_6.fout", rs4));
    ASSERT(rstack_empty(rs2) == true);
    ASSERT_RESULT(rstack_front(rs7), false, 0UL);
    ASSERT(rstack_empty(rs9) == true);
    ASSERT(rstack_empty(rs5) == true);
    ASSERT(rstack_empty(rs9) == true);
    ASSERT_RESULT(rstack_front(rs2), false, 0UL);
    NO_ERROR(rstack_write("test_4.fout", rs8));
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs2, rs5));
    NO_ERROR(rstack_push_rstack(rs0, rs0));
    NO_ERROR(rstack_push_rstack(rs3, rs7));
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    ASSERT(rstack_empty(rs1) == true);
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    ASSERT_RESULT(rstack_front(rs2), false, 0UL);
    ASSERT(rstack_empty(rs0) == true);
    NO_ERROR(rstack_push_rstack(rs1, rs7));
    NO_ERROR(rstack_push_rstack(rs9, rs0));
    NO_ERROR(rstack_push_rstack(rs4, rs1));
    ASSERT_RESULT(rstack_front(rs6), false, 0UL);
    ASSERT_RESULT(rstack_front(rs6), false, 0UL);
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs4, rs0));
    ASSERT_RESULT(rstack_front(rs7), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs5, rs8));
    ASSERT_RESULT(rstack_front(rs5), false, 0UL);
    ASSERT(rstack_empty(rs4) == true);
    ASSERT_RESULT(rstack_front(rs6), false, 0UL);
    ASSERT(rstack_empty(rs4) == true);
    NO_ERROR(rstack_push_rstack(rs2, rs8));
    ASSERT_RESULT(rstack_front(rs5), false, 0UL);
    ASSERT(rstack_empty(rs1) == true);
    NO_ERROR(rstack_push_rstack(rs4, rs8));
    NO_ERROR(rstack_write("test_7.fout", rs9));
    ASSERT_RESULT(rstack_front(rs9), false, 0UL);
    ASSERT_RESULT(rstack_front(rs2), false, 0UL);
    ASSERT_RESULT(rstack_front(rs6), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs4, rs0));
    NO_ERROR(rstack_push_rstack(rs6, rs4));
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs4, rs9));
    ASSERT_RESULT(rstack_front(rs3), false, 0UL);
    ASSERT(rstack_empty(rs9) == true);
    ASSERT_RESULT(rstack_front(rs3), false, 0UL);
    ASSERT(rstack_empty(rs4) == true);
    ASSERT(rstack_empty(rs4) == true);
    NO_ERROR(rstack_push_rstack(rs8, rs3));
    ASSERT_RESULT(rstack_front(rs6), false, 0UL);
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    ASSERT(rstack_empty(rs9) == true);
    ASSERT_RESULT(rstack_front(rs9), false, 0UL);
    NO_ERROR(rstack_write("test_3.fout", rs9));
    ASSERT_RESULT(rstack_front(rs0), false, 0UL);
    ASSERT_RESULT(rstack_front(rs3), false, 0UL);
    ASSERT(rstack_empty(rs6) == true);
    NO_ERROR(rstack_push_rstack(rs6, rs7));
    ASSERT(rstack_empty(rs8) == true);
    NO_ERROR(rstack_push_rstack(rs3, rs2));
    ASSERT_RESULT(rstack_front(rs7), false, 0UL);
    ASSERT_RESULT(rstack_front(rs9), false, 0UL);
    ASSERT_RESULT(rstack_front(rs6), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs7, rs4));
    ASSERT_RESULT(rstack_front(rs1), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs6, rs9));
    NO_ERROR(rstack_push_rstack(rs1, rs2));
    ASSERT(rstack_empty(rs1) == true);
    NO_ERROR(rstack_write("test_11.fout", rs1));
    NO_ERROR(rstack_push_rstack(rs9, rs0));
    ASSERT(rstack_empty(rs5) == true);
    ASSERT(rstack_empty(rs3) == true);
    ASSERT(rstack_empty(rs5) == true);
    ASSERT_RESULT(rstack_front(rs5), false, 0UL);
    NO_ERROR(rstack_push_rstack(rs2, rs3));
    ASSERT(rstack_empty(rs9) == true);
    rstack_delete(rs0);
    rstack_delete(rs1);
    rstack_delete(rs2);
    rstack_delete(rs3);
    rstack_delete(rs4);
    rstack_delete(rs5);
    rstack_delete(rs6);
    rstack_delete(rs7);
    rstack_delete(rs8);
    rstack_delete(rs9);

        return PASS;
}