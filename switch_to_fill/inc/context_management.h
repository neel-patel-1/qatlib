#ifndef CONTEXT_MANAGE
#define CONTEXT_MANAGE

extern "C"{
#include "fcontext.h"
}

void create_contexts(fcontext_state_t **states, int num_contexts, void (*func)(fcontext_transfer_t));

void free_contexts(fcontext_state_t **states, int num_contexts);

#endif