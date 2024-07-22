#include "context_management.h"
extern "C"{
#include "fcontext.h"
}

void create_contexts(fcontext_state_t **states, int num_contexts, void (*func)(fcontext_transfer_t)){
  for(int i=0; i<num_contexts; i++){
    states[i] = fcontext_create(func);
  }
}

void free_contexts(fcontext_state_t **states, int num_contexts){
  for(int i=0; i<num_contexts; i++){
    fcontext_destroy(states[i]);
  }
}
