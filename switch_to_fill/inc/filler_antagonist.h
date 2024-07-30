#ifndef FILLER_ANTAGONIST
#define FILLER_ANTAGONIST

extern "C" {
  #include "fcontext.h"
}
#include <string>
#include "print_utils.h"
#include "probe_point.h"
#include "immintrin.h"

void antagonist_interleaved(fcontext_transfer_t arg){
  volatile int glb = 0;
  int buf_size = 48 * 1024 / sizeof(int32_t);
  int32_t buffer[buf_size];

  init_probe(arg);
  while(1){
    for(int i=0; i<buf_size / sizeof(int32_t); i++){
      if(i > 0)
        buffer[i] = buffer[i-1] + 1;
      probe_point();
    }
    LOG_PRINT( LOG_VERBOSE, "Antagonist finished one loop\n");
    glb = buffer[buf_size - 1];
  }
}


#endif