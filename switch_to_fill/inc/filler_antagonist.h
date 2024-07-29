#ifndef FILLER_ANTAGONIST
#define FILLER_ANTAGONIST

extern "C" {
  #include "fcontext.h"
}
#include <string>
#include "print_utils.h"
#include "probe_point.h"
#include "immintrin.h"

void pollution_kernel(ax_comp *comp, fcontext_t parent){
  volatile int glb = 0;
  int buf_size = 48 * 1024 / sizeof(int32_t);
  int32_t buffer[buf_size];
  while(1){
    for(int i=0; i<buf_size / sizeof(int32_t); i++){
      if(i > 0)
        buffer[i] = buffer[i-1] + 1;
      /* check signal */
      if(comp->status == 1){
        PRINT_DBG("Filler preempted\n");
        fcontext_swap(parent, NULL);
      }
    }
    glb = buffer[buf_size - 1];
  }
}

void antagonist_interleaved(fcontext_transfer_t arg){
  volatile int glb = 0;
  int buf_size = 48 * 1024 / sizeof(int32_t);
  int32_t buffer[buf_size];

  ax_comp *p_sig = (ax_comp *)arg.data;
  fcontext_transfer_t parent_pointer;
  while(1){
    for(int i=0; i<buf_size / sizeof(int32_t); i++){
      if(i > 0)
        buffer[i] = buffer[i-1] + 1;
      probe_point(p_sig, arg.prev_context);
    }
  }
  LOG_PRINT( LOG_DEBUG, "Dummy interleaved saw comp\n");
}


#endif