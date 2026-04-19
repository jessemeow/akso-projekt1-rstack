#ifndef AKSO_RSTACK_RGARBAGE_COLLECTOR_H
#define AKSO_RSTACK_RGARBAGE_COLLECTOR_H

#include "rstack.h"
#include <errno.h>
#include <stdlib.h>

#define FUNCTION_FAIL (-1)
#define FUNCTION_SUCCESS 0
#define CYCLE_DETECTED 1

typedef struct garbage_collector_node garbage_collector_node_t;
typedef struct garbage_collector garbage_collector_t;

/**
 * @brief Rejestruje nowo utworzony stos w garbage collectorze.
 *
 * @param gc Wskaznik na glowna instancje garbage collectora.
 * @param rs Wskaznik na stos, ktory ma zostac zarejestrowany.
 * @return FUNCTION_SUCCESS jesli rejestracja przebiegla pomyslnie.
 * @return FUNCTION_FAIL w przypadku bledu, np. gdy rs lub gc to nullptr
 * lub przy braku pamieci.
 * Odpowiednio modyfikuje errno.
 */
int gc_push_rstack(garbage_collector_t *gc, rstack_t *rs);

/**
 * @brief Uruchamia pelny cykl zwalniania pamieci.
 * @param gc Wskaznik na instancje garbage collectora.
 * Bezpiecznie ignoruje nullptr.
 */
void gc_mark_and_sweep(garbage_collector_t *gc);

#endif //AKSO_RSTACK_RGARBAGE_COLLECTOR_H
