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


#endif //AKSO_RSTACK_RGARBAGE_COLLECTOR_H

