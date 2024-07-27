#include "test_harness.h"
#include "print_utils.h"
#include "router_request_args.h"
#include "iaa_offloads.h"
#include "ch3_hash.h"
#include "runners.h"
#include "gpcore_compress.h"

extern "C" {
  #include "fcontext.h"
  #include "iaa.h"
  #include "accel_test.h"
  #include "iaa_compress.h"
  #include <zlib.h>
}
#include "dsa_offloads.h"
#include "submit.hpp"
#include <algorithm>

int input_size = 16384; /* sizes the feature buffer */
int num_accesses = 10; /* tells number of accesses */

void (*compute_on_input)(void *, int);

static inline void indirect_array_gen(int **p_indirect_array){
  int num_feature_ent = input_size / sizeof(float);
  int max_val = num_feature_ent - 1;
  int min_val = 0;

  int *indirect_array = (int *)malloc(num_accesses * sizeof(int));

  for(int i=0; i<num_accesses; i++){
    int idx = (rand() % (max_val - min_val + 1)) + min_val;
    indirect_array[i] = (float)idx;
  }
  *p_indirect_array = (int *)indirect_array;
}

void alloc_offload_memfill_and_gather_args( /* is this scatter ?*/
  int total_requests,
  offload_request_args *** p_off_args,
  ax_comp *comps
){

  offload_request_args **off_args =
    (offload_request_args **)malloc(total_requests * sizeof(timed_offload_request_args *));
  for(int i = 0; i < total_requests; i++){
    off_args[i] = (offload_request_args *)malloc(sizeof(offload_request_args));
    off_args[i]->src_payload = (char *)malloc(input_size * sizeof(char));
    indirect_array_gen((int **)&(off_args[i]->dst_payload)); /* use dst buf to house the indirect array*/
    off_args[i]->src_size =  input_size;
    off_args[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));
    off_args[i]->comp = &comps[i];
    off_args[i]->id = i;
  }

  *p_off_args = off_args;
}
void free_offload_memfill_and_gather_args(
  int total_requests,
  offload_request_args *** p_off_args
){
  offload_request_args **off_args = *p_off_args;
  for(int i = 0; i < total_requests; i++){
    free(off_args[i]->src_payload);
    free(off_args[i]->dst_payload);
    free(off_args[i]->desc);
    free(off_args[i]);
  }
  free(off_args);
}

void blocking_memcpy_and_compute(
  fcontext_transfer_t arg
){
  offload_request_args *args = (offload_request_args *)arg.data;

  char *src = args->src_payload;
  char *dst = args->dst_payload;
  int id = args->id;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;

  prepare_dsa_memfill_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)args->comp, (uint64_t)input_size
  );
  if (!dsa_submit(dsa, desc)){
    LOG_PRINT(LOG_ERR, "Error submitting request\n");
    return;
  }

  while(comp->status == IAX_COMP_NONE)
  {
    _mm_pause();
  }
  if(comp->status != IAX_COMP_SUCCESS){
    LOG_PRINT(LOG_ERR, "Error in offload\n");
    return;
  }

  if(gLogLevel == LOG_DEBUG){
    LOG_PRINT(LOG_DEBUG, "Request %d completed\n", id);
  }

  compute_on_input(dst, input_size);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}


int gLogLevel = LOG_DEBUG;
bool gDebugParam = false;
int main(int argc, char **argv){
  int wq_id = 0;
  int dev_id = 2;
  int wq_type = SHARED;
  int rc;

  int sorted_idxs[num_accesses];
  int num_ents;

  initialize_dsa_wq(dev_id, wq_id, wq_type);

  input_size = 1024;
  num_ents = input_size / sizeof(float);

  float *feature_buf = (float *)malloc(input_size * sizeof(float));
  int *indirect_array = (int *)malloc(num_accesses * sizeof(int));
  indirect_array_gen(&indirect_array);
  memset(feature_buf, 0, input_size * sizeof(float));

  /* populate feature buf using indrecet array*/
  for(int i = 0; i < num_accesses; i++){
    feature_buf[indirect_array[i]] = 1.0;
    sorted_idxs[i] = indirect_array[i];
  }
  std::sort(sorted_idxs, sorted_idxs + num_accesses);


  /* validate feature buf entries */
  int sorted_idx = 0;
  for(int i=0; i<num_ents; i++){
    if (i == sorted_idxs[sorted_idx]){
      if (feature_buf[i] != 1.0){
        LOG_PRINT(LOG_ERR, "Error in feature buf\n");
        return -1;
      }
      sorted_idx++;
    } else {
      if (feature_buf[i] != 0.0){
        LOG_PRINT(LOG_ERR, "Error in feature buf\n");
        return -1;
      }
    }
  }

}