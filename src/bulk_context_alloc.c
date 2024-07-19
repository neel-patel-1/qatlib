#include "bulk_context_alloc.h"

fcontext_transfer_t *bulk_alloc_fcontext_xfers(int num_xfers){
  fcontext_transfer_t *xfers = (fcontext_transfer_t *)malloc(num_xfers * sizeof(fcontext_transfer_t));
  return xfers;
}

fcontext_state_t **bulk_alloc_fcontext_state(int num_states){
  fcontext_state_t **states = (fcontext_state_t **)malloc(num_states * sizeof(fcontext_state_t *));
  return states;
}