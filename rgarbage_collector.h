#ifndef AKSO_RSTACK_RGARBAGE_COLLECTOR_H
#define AKSO_RSTACK_RGARBAGE_COLLECTOR_H
#include "rstack.h"
#include <errno.h>
#include <stdlib.h>

// TODO MAKE GLOBAL
#define FUNCTION_FAIL (-1)
#define FUNCTION_SUCCESS 0

typedef struct garbage_collector_node garbage_collector_node_t;
typedef struct garbage_collector garbage_collector_t;

garbage_collector_t *gc_new();
garbage_collector_node_t *gc_new_node(rstack_t *rs);
int gc_push_rstack(garbage_collector_t *gc, rstack_t *rs);
int *gc_find_roots(garbage_collector_t *gc);

#endif //AKSO_RSTACK_RGARBAGE_COLLECTOR_H

