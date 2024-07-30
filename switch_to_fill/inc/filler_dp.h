#ifndef FILLER_DP
#define FILLER_DP

extern "C" {
  #include "fcontext.h"
}
#include "print_utils.h"
#include "probe_point.h"
#include "immintrin.h"
#include "emul_ax.h"

extern float *item_buf; /* defined in mlp.cpp */
extern float *user_filler_buf; /* defined in mlp.cpp */
extern int input_size;

void dotproduct_interleaved(fcontext_transfer_t arg){
  ax_comp *p_sig = (ax_comp *)arg.data;
  fcontext_transfer_t parent_pointer;

  float sum = 0;
  float *v1 = (float *)item_buf;
  float *v2 = (float *)user_filler_buf;
  int size = input_size / sizeof(float);

  init_probe(arg);

  while(1){
    for(int i=0; i < size; i++){
      probe_point();
      sum += v1[i] * v2[i];
    }
  }
  LOG_PRINT(LOG_VERBOSE, "Dot Product: %f\n", sum);

}


#endif