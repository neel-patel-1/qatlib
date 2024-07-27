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

int input_size = 10; /* sizes the feature buffer */
int num_accesses = 1; /* tells number of accesses */

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
  timed_offload_request_args *** p_off_args,
  ax_comp *comps, uint64_t *ts0,
  uint64_t *ts1, uint64_t *ts2,
  uint64_t *ts3
){

  timed_offload_request_args **off_args =
    (timed_offload_request_args **)malloc(total_requests * sizeof(timed_offload_request_args *));
  for(int i = 0; i < total_requests; i++){
    off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));
    off_args[i]->src_payload = (char *)malloc(input_size * sizeof(char));
    indirect_array_gen((int **)&(off_args[i]->dst_payload)); /* use dst buf to house the indirect array*/
    off_args[i]->src_size =  input_size;
    off_args[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));
    off_args[i]->comp = &comps[i];
    off_args[i]->id = i;
    off_args[i]->ts0 = ts0;
    off_args[i]->ts1 = ts1;
    off_args[i]->ts2 = ts2;
    off_args[i]->ts3 = ts3;
  }

  *p_off_args = off_args;
}
void free_offload_memfill_and_gather_args(
  int total_requests,
  timed_offload_request_args *** p_off_args
){
  timed_offload_request_args **off_args = *p_off_args;
  for(int i = 0; i < total_requests; i++){
    free(off_args[i]->src_payload);
    free(off_args[i]->dst_payload);
    free(off_args[i]->desc);
    free(off_args[i]);
  }
  free(off_args);
}

void blocking_memcpy_and_compute_stamped(
  fcontext_transfer_t arg
){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;

  float *feature_buf = (float *)(args->src_payload);
  int *indirect_array = (int *)(args->dst_payload);
  int id = args->id;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;
  int sorted_idxs[num_accesses];

  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;

  int num_ents;
  num_ents = input_size / sizeof(float);

  ts0[id] = sampleCoderdtsc();
  prepare_dsa_memfill_desc_with_preallocated_comp(
    desc, 0x0, (uint64_t)feature_buf,
    (uint64_t)args->comp, (uint64_t)input_size
  );
  if (!dsa_submit(dsa, desc)){
    LOG_PRINT(LOG_ERR, "Error submitting request\n");
    return;
  }

  ts1[id] = sampleCoderdtsc();
  while(comp->status == IAX_COMP_NONE)
  {
    _mm_pause();
  }
  if(comp->status != IAX_COMP_SUCCESS){
    LOG_PRINT(LOG_ERR, "Error in offload\n");
    return;
  }

  ts2[id] = sampleCoderdtsc();
  /* populate feature buf using indrecet array*/
  for(int i = 0; i < num_accesses; i++){
    feature_buf[indirect_array[i]] = 1.0;
    LOG_PRINT( LOG_DEBUG, "feature_buf[%d] = %f\n",
      indirect_array[i], feature_buf[indirect_array[i]]);
    sorted_idxs[i] = indirect_array[i];
  }

  ts3[id] = sampleCoderdtsc();
  if(gLogLevel == LOG_DEBUG){
    std::sort(sorted_idxs, sorted_idxs + num_accesses);
    int sorted_idx = 0;
    for(int i=0; i<num_ents; i++){
      // LOG_PRINT( LOG_DEBUG, "feature_buf[%d] = %f\n",
      //   i, feature_buf[i]);
      if (i == sorted_idxs[sorted_idx]){
        if (feature_buf[i] != 1.0){
          LOG_PRINT(LOG_ERR, "Error in feature buf sidx: %d idx: %d ent: %f next_expected_one_hot %d\n",
            sorted_idx, i, feature_buf[i], sorted_idxs[sorted_idx]);
          return;
        }
        sorted_idx++;
      } else if (feature_buf[i] != 0.0){
          LOG_PRINT(LOG_ERR, "Error in feature buf sidx: %d idx: %d ent: %f next_expected_one_hot %d\n",
            sorted_idx, i, feature_buf[i], sorted_idxs[sorted_idx]);
          return;

      }
    }
  }

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
  int itr = 100;
  int total_requests = 1000;

  initialize_dsa_wq(dev_id, wq_id, wq_type);

  input_size = 1024;

  run_blocking_offload_request_brkdown(
    blocking_memcpy_and_compute_stamped,
    alloc_offload_memfill_and_gather_args,
    free_offload_memfill_and_gather_args,
    itr, total_requests
  );


}