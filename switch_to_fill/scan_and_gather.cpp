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
  #include "iaa_filter.h"
  #include "iaa_compress.h"
  #include <zlib.h>
}
#include "dsa_offloads.h"
#include "submit.hpp"
#include <algorithm>

int input_size = 100; /* sizes the feature buffer */
int num_accesses = 10; /* tells number of accesses */

void (*compute_on_input)(void *, int);

static inline void print_sorted_array(int *sorted_idxs, int num_accesses){
  for(int i = 0; i < num_accesses; i++){
    LOG_PRINT(LOG_DEBUG, "sorted_idxs[%d] = %d\n", i, sorted_idxs[i]);
  }
}

static inline void validate_feature_vec(float *feature_buf, int *indirect_array){
  int num_ents;
  num_ents = input_size / sizeof(float);
  int sorted_idxs[num_accesses];
  std::sort(indirect_array, indirect_array + num_accesses);
  int sorted_idx = 0;
  for(int i=0; i<num_ents; i++){
    if (i == indirect_array[sorted_idx]){
      if (feature_buf[i] != 1.0){
        print_sorted_array(indirect_array, num_accesses);
        LOG_PRINT(LOG_ERR, "Error in feature buf sidx: %d idx: %d ent: %f next_expected_one_hot %d\n",
          sorted_idx, i, feature_buf[i], indirect_array[sorted_idx]);
        return;
      }
      while (indirect_array[sorted_idx] == i){
        sorted_idx++;
      }
    } else if (feature_buf[i] != 0.0){
        print_sorted_array(indirect_array, num_accesses);
        LOG_PRINT(LOG_ERR, "Error in feature buf sidx: %d idx: %d ent: %f next_expected_one_hot %d\n",
          sorted_idx, i, feature_buf[i], indirect_array[sorted_idx]);
        return;

    }
  }
}

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
  }

  ts3[id] = sampleCoderdtsc();

  if(gLogLevel >= LOG_DEBUG){ /* validate code */
    validate_feature_vec(feature_buf, indirect_array);
  }

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void alloc_cpu_memcpy_and_compute_args(
  int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){

  char ***ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads =
    (char ***)malloc(total_requests * sizeof(char **));

  for(int i = 0; i < total_requests; i++){
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i] =
      (char **)malloc(2 * sizeof(char *));
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0] = (char *)malloc(input_size * sizeof(char));
    indirect_array_gen((int **)&(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][1]));
  }
  *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads =
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
}

void free_cpu_memcpy_and_compute_args(
  int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads
){
  char ***ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads =
    *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;

  for(int i = 0; i < total_requests; i++){
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0]);
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][1]);
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i]);
  }
  free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);
}

void cpu_memcpy_and_compute_stamped(
  fcontext_transfer_t arg
){
  timed_gpcore_request_args *args = (timed_gpcore_request_args *)arg.data;

  float *feature_buf = (float *)(args->inputs[0]);
  int *indirect_array = (int *)(args->inputs[1]);

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;

  int num_ents;
  num_ents = input_size / sizeof(float);

  ts0[id] = sampleCoderdtsc();
  memset((void*) feature_buf, 0x0, input_size);

  ts1[id] = sampleCoderdtsc();
  for(int i = 0; i < num_accesses; i++){
    feature_buf[indirect_array[i]] = 1.0;
  }

  ts2[id] = sampleCoderdtsc();
  if(gLogLevel >= LOG_DEBUG){ /* validate code */
    validate_feature_vec(feature_buf, indirect_array);
  }

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void prepare_iaa_filter_desc_with_preallocated_comp(
  struct hw_desc *hw, uint64_t src1, uint64_t dst1,
  uint64_t comp, uint64_t xfer_size,
  uint32_t num_inputs, uint32_t low_val, uint32_t high_val
){
  uint8_t *aecs =
    (uint8_t *)aligned_alloc(IAA_FILTER_AECS_SIZE, IAA_FILTER_AECS_SIZE);
  struct iaa_filter_aecs_t iaa_filter_aecs =
  {
    .rsvd = 0,
    .rsvd2 = 0,
    .rsvd3 = 0,
    .rsvd4 = 0,
    .rsvd5 = 0,
    .rsvd6 = 0
  };

  /* prepare aecs */
  memset(aecs, 0, IAA_FILTER_AECS_SIZE);
  iaa_filter_aecs.low_filter_param = low_val;
  iaa_filter_aecs.high_filter_param = high_val;
  memcpy(aecs, (void *)&iaa_filter_aecs, IAA_FILTER_AECS_SIZE);

  /* prepare hw */
  memset(hw, 0, sizeof(struct hw_desc));
  hw->flags |= (IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR);
  hw->flags |= IDXD_OP_FLAG_BOF;
  hw->flags |= IDXD_OP_FLAG_RD_SRC2_AECS;
  hw->opcode = IAX_OPCODE_EXTRACT;
  hw->src_addr = src1;
  hw->dst_addr = dst1;
  hw->xfer_size = xfer_size;

  /* prepare hw filter params */
  hw->iax_num_inputs = num_inputs;
  hw->iax_filter_flags = 124;
  hw->src2_addr = (uint64_t)aecs;
  hw->iax_src2_xfer_size = IAA_FILTER_AECS_SIZE;
  hw->iax_max_dst_size = IAA_FILTER_MAX_DEST_SIZE;

  /* comp */
  memset((void *)comp, 0, sizeof(ax_comp));
  hw->completion_addr = comp;
}

int gLogLevel = LOG_DEBUG;
bool gDebugParam = false;
int main(int argc, char **argv){
  int wq_id = 0;
  int dev_id = 3;
  int wq_type = SHARED;
  int rc;
  int itr = 100;
  int total_requests = 1000;
  int opt;

  while((opt = getopt(argc, argv, "t:i:r:s:q:a:")) != -1){
    switch(opt){
      case 't':
        total_requests = atoi(optarg);
        break;
      case 'i':
        itr = atoi(optarg);
        break;
      case 'q':
        input_size = atoi(optarg);
        break;
      case 'a':
        num_accesses = atoi(optarg);
        break;
      default:
        break;
    }
  }

  initialize_iaa_wq(dev_id, wq_id, wq_type);

  int input_size = 128 * 4;
  uint32_t iaa_num_inputs = input_size / sizeof(uint32_t);

    /* low val*/
  uint32_t low_val = 10;
  /* high val*/
  uint32_t high_val = iaa_num_inputs-1;
  uint32_t expected_size = high_val - low_val;
  uint32_t *src1 =
    (uint32_t *)aligned_alloc(ADDR_ALIGNMENT,
      iaa_num_inputs * sizeof(uint32_t));
  uint32_t *dst1 =
    (uint32_t *)aligned_alloc(ADDR_ALIGNMENT,
      IAA_FILTER_MAX_DEST_SIZE);
  struct iaa_filter_aecs_t aecs =
  {
    .rsvd = 0,
    .rsvd2 = 0,
    .rsvd3 = 0,
    .rsvd4 = 0,
    .rsvd5 = 0,
    .rsvd6 = 0
  };
  struct hw_desc desc; /* hw */
  ax_comp *comp =
    (ax_comp *)aligned_alloc(
      iaa->compl_size, sizeof(ax_comp));
  uint32_t iaa_filter_flags = 124;
  uint32_t sw_len = 0;

  /* seq populate*/
  for(uint32_t i = 0; i < iaa_num_inputs; i++){
    src1[i] = i;
  }

  /* sw */
  aecs.low_filter_param = low_val;
  aecs.high_filter_param = high_val;
  sw_len = iaa_do_extract((void *)dst1, (void *)src1,
    (void *)&aecs, iaa_num_inputs, iaa_filter_flags);


  /* validate */
  LOG_PRINT(LOG_DEBUG, "sw_len: %d\n", sw_len);
  // for(uint32_t i = 0; i < (sw_len/sizeof(uint32_t)); i++){
  //   LOG_PRINT(LOG_DEBUG, "dst1[%d] = %u\n", i, dst1[i]);
  // }

  /* hw */
  char *src2 = (char *)malloc(IAA_COMPRESS_AECS_SIZE);
  memcpy(src2, iaa_compress_aecs, IAA_COMPRESS_AECS_SIZE);



  prepare_iaa_filter_desc_with_preallocated_comp(
    &desc, (uint64_t)src1, (uint64_t)dst1,
    (uint64_t)comp, IAA_COMPRESS_MAX_DEST_SIZE,
    iaa_num_inputs, low_val, high_val
  );
  iaa_submit(iaa, &desc);
  while(comp->status == IAX_COMP_NONE){
    _mm_pause();
  }
  if(comp->status != IAX_COMP_SUCCESS){
    LOG_PRINT(LOG_ERR, "Error in offload: %x\n", comp->status);
    return -1;
  }

  free_iaa_wq();


}