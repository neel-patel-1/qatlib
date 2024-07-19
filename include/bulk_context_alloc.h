#ifndef BULK_CONTEXT_ALLOC_H
#define BULK_CONTEXT_ALLOC_H
#include "fcontext.h"

fcontext_transfer_t *bulk_alloc_fcontext_xfers(int num_xfers);
fcontext_state_t **bulk_alloc_fcontext_state_ptrs(int num_states);

#endif