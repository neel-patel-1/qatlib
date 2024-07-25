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
#include "decompress_and_hash_request.hpp"

int input_size = 16384;

struct acctest_context *dsa;
void initialize_dsa_wq(int dev_id, int wq_id, int wq_type){
  int tflags = TEST_FLAGS_BOF;
  int rc;

  dsa = acctest_init(tflags);
  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
  if(rc != ACCTEST_STATUS_OK){
    LOG_PRINT( LOG_ERR, "Error allocating work queue\n");
    return;
  }
}

void free_dsa_wq(){
  acctest_free(dsa);
}

static inline bool dsa_submit(struct acctest_context *dsa,
  struct hw_desc *desc){
  if (enqcmd(dsa->wq_reg, desc) ){
    return false;
  }
  return true;
}

static inline void prepare_dsa_memcpy_desc_with_preallocated_comp(
  struct hw_desc *hw, uint64_t src,
  uint64_t dst, uint64_t comp, uint64_t xfer_size){

  memset(hw, 0, sizeof(struct hw_desc));
  hw->flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_BOF;
  hw->src_addr = src;
  hw->dst_addr = dst;
  hw->xfer_size = xfer_size;
  hw->opcode = DSA_OPCODE_MEMMOVE;

  memset((void *)comp, 0, sizeof(ax_comp));
  hw->completion_addr = comp;

}

static inline void input_gen(char **p_buf){
  char *buf = (char *)malloc(input_size);
  for(int i = 0; i < input_size; i++){
    buf[i] = rand() % 256;
  }
  *p_buf = buf;
}


static inline void compute(void *buf, int size){
  int i;
  char *p = (char *)buf;
  for(i = 0; i < size; i++){
    p[i] = p[i] + 1;
  }
}

void alloc_cpu_memcpy_and_compute_args(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){
    char ***ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = (char ***)malloc(total_requests * sizeof(char **));
    for(int i = 0; i < total_requests; i++){
      ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i] = (char **)malloc(3 * sizeof(char *));
      input_gen(&ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0]);
      ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][1] = (char *)malloc(input_size * sizeof(char));
      ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2] = (char *)malloc(sizeof(int));
      *((int *) ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2]) = input_size;
    }

    *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
}

void free_cpu_memcpy_and_compute_args(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){
    char *** ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
    for(int i = 0; i < total_requests; i++){
      free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0]);
      free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][1]);
      free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2]);
      free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i]);
    }
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);
}

void cpu_memcpy_and_compute_stamped(fcontext_transfer_t arg){
  timed_gpcore_request_args* args = (timed_gpcore_request_args *)arg.data;

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;

  char *src1 = args->inputs[0];
  char *dst1 = args->inputs[1];
  int buf_size = *((int *) args->inputs[2]);

  ts0[id] = sampleCoderdtsc();
  memcpy(dst1, src1, buf_size);
  ts1[id] = sampleCoderdtsc();

  compute(dst1, buf_size);
  ts2[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void alloc_offload_memcpy_and_compute_args(
  int total_requests,
  timed_offload_request_args*** p_off_args,
  ax_comp *comps,
  uint64_t *ts0,
  uint64_t *ts1,
  uint64_t *ts2,
  uint64_t *ts3
){

  timed_offload_request_args **off_args = (timed_offload_request_args **)malloc(total_requests * sizeof(timed_offload_request_args *));
  for(int i = 0; i < total_requests; i++){
    off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));
    off_args[i]->src_payload = (char *)malloc(input_size * sizeof(char));
    off_args[i]->dst_payload = (char *)malloc(input_size * sizeof(char));
    off_args[i]->src_size =  input_size;
    off_args[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));
    off_args[i]->comp = &comps[i];
    off_args[i]->ts0 = ts0;
    off_args[i]->ts1 = ts1;
    off_args[i]->ts2 = ts2;
    off_args[i]->ts3 = ts3;
    off_args[i]->id = i;
  }

  *p_off_args = off_args;
}

void free_offload_memcpy_and_compute_args(
  int total_requests,
  timed_offload_request_args*** p_off_args
){
  timed_offload_request_args **off_args = *p_off_args;
  for(int i = 0; i < total_requests; i++){
    free(off_args[i]->src_payload);
    free(off_args[i]->dst_payload);
    free(off_args[i]);
  }
  free(off_args);
}

void blocking_memcpy_and_compute_stamped(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;

  char *src = args->src_payload;
  char *dst = args->dst_payload;
  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;

  ts0[id] = sampleCoderdtsc();
  prepare_dsa_memcpy_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
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

  ts2[id] = sampleCoderdtsc();
  compute(dst, input_size);
  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void yielding_memcpy_and_compute_stamped(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;

  char *src = args->src_payload;
  char *dst = args->dst_payload;
  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;

  ts0[id] = sampleCoderdtsc();
  prepare_dsa_memcpy_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)args->comp, (uint64_t)input_size
  );
  if (!dsa_submit(dsa, desc)){
    LOG_PRINT(LOG_ERR, "Error submitting request\n");
    return;
  }
  ts1[id] = sampleCoderdtsc();
  fcontext_swap(arg.prev_context, NULL);

  ts2[id] = sampleCoderdtsc();
  compute(dst, input_size);
  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void yielding_request_offered_load(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx
)
{
  using namespace std;
  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;

  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];


  requests_completed = 0;

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  /* allocate request args */
  offload_args_allocator(total_requests, &off_args, comps);

  /* Pre-create the contexts */
  offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * total_requests);
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, request_fn);

  execute_yielding_requests_closed_system_with_sampling(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    offload_req_xfer, off_req_state, self,
    exetime, idx);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);

  offload_args_free(total_requests, &off_args);

  fcontext_destroy(self);
}

void blocking_request_offered_load(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx
)
{
  using namespace std;
  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;

  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];


  requests_completed = 0;

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  /* allocate request args */
  offload_args_allocator(total_requests, &off_args, comps);

  /* Pre-create the contexts */
  offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * total_requests);
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, request_fn);

  execute_blocking_requests_closed_system_with_sampling(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    offload_req_xfer, off_req_state, self,
    exetime, idx);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);

  offload_args_free(total_requests, &off_args);

  fcontext_destroy(self);
}


void alloc_offload_memcpy_and_compute_args(
  int total_requests,
  offload_request_args *** p_off_args,
  ax_comp *comps
){

  offload_request_args **off_args = (offload_request_args **)malloc(total_requests * sizeof(timed_offload_request_args *));
  for(int i = 0; i < total_requests; i++){
    off_args[i] = (offload_request_args *)malloc(sizeof(offload_request_args));
    off_args[i]->src_payload = (char *)malloc(input_size * sizeof(char));
    off_args[i]->dst_payload = (char *)malloc(input_size * sizeof(char));
    off_args[i]->src_size =  input_size;
    off_args[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));
    off_args[i]->comp = &comps[i];
    off_args[i]->id = i;
  }

  *p_off_args = off_args;
}

void free_offload_memcpy_and_compute_args(
  int total_requests,
  offload_request_args *** p_off_args
){
  offload_request_args **off_args = *p_off_args;
  for(int i = 0; i < total_requests; i++){
    free(off_args[i]->src_payload);
    free(off_args[i]->dst_payload);
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

  prepare_dsa_memcpy_desc_with_preallocated_comp(
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

  compute(dst, input_size);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void yielding_memcpy_and_compute(
  fcontext_transfer_t arg
){
  offload_request_args *args = (offload_request_args *)arg.data;

  char *src = args->src_payload;
  char *dst = args->dst_payload;
  int id = args->id;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;

  prepare_dsa_memcpy_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)args->comp, (uint64_t)input_size
  );
  if (!dsa_submit(dsa, desc)){
    LOG_PRINT(LOG_ERR, "Error submitting request\n");
    return;
  }

  fcontext_swap(arg.prev_context, NULL);

  compute(dst, input_size);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void cpu_memcpy_and_compute(
  fcontext_transfer_t arg
){
  gpcore_request_args *args = (gpcore_request_args *)arg.data;

  int id = args->id;
  char *src1 = args->inputs[0];
  char *dst1 = args->inputs[1];
  int buf_size = *((int *) args->inputs[2]);

  memcpy(dst1, src1, buf_size);
  compute(dst1, buf_size);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}


int gLogLevel = LOG_PERF;
bool gDebugParam = false;
int main(){


  int wq_id = 0;
  int dev_id = 2;
  int wq_type = SHARED;
  int rc;
  int itr = 100;
  int total_requests = 1000;
  initialize_dsa_wq(dev_id, wq_id, wq_type);

  run_gpcore_request_brkdown(
    cpu_memcpy_and_compute_stamped,
    alloc_cpu_memcpy_and_compute_args,
    free_cpu_memcpy_and_compute_args,
    itr,
    total_requests
  );
  run_blocking_offload_request_brkdown(
    blocking_memcpy_and_compute_stamped,
    alloc_offload_memcpy_and_compute_args,
    free_offload_memcpy_and_compute_args,
    itr,
    total_requests
  );
  run_yielding_request_brkdown(
    yielding_memcpy_and_compute_stamped,
    alloc_offload_memcpy_and_compute_args,
    free_offload_memcpy_and_compute_args,
    itr,
    total_requests
  );


  /* some vars needed for yield*/
  int num_exe_time_samples_per_run = 10; /* 10 samples per iter*/
  int num_requests_before_stamp = total_requests / num_exe_time_samples_per_run;

  int total_exe_time_samples = itr * num_exe_time_samples_per_run;
  uint64_t exetime[total_exe_time_samples];


  /*
  void run_gpcore_request_brkdown(
    fcontext_fn_t req_fn,
    void (*payload_alloc)(int,char****),
    void (*payload_free)(int,char****),
    int iter, int total_requests){
    // this function executes all requests from start to finish only taking stamps at beginning and end
  */
  for(int i = 0; i < itr; i++){
    gpcore_closed_loop_test(
      cpu_memcpy_and_compute,
      alloc_cpu_memcpy_and_compute_args,
      free_cpu_memcpy_and_compute_args,
      total_requests,
      total_requests,
      exetime,
      i
    );
  }

  mean_median_stdev_rps(
    exetime, itr, total_requests, "GPCoreRPS"
  );
  /*}*/

  /*
  void run_blocking_offered_load(
    fcontext_fn_t request_fn,
    void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
    void (* offload_args_free)(int, offload_request_args***),
    int total_requests){

    // this function executes all requests from start to finish only taking stamps at beginning and end
  */

  for(int i = 0; i < itr; i++){
    blocking_request_offered_load(
      blocking_memcpy_and_compute,
      alloc_offload_memcpy_and_compute_args,
      free_offload_memcpy_and_compute_args,
      total_requests,
      total_requests,
      exetime,
      i
    );
  }

  mean_median_stdev_rps(
    exetime, itr, total_requests, "BlockingOffloadRPS"
  );
  /*}*/


  /*
  void run_yielding_offered_load(int num_exe_time_samples_per_run
    fcontext_fn_t request_fn,
    void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
    void (* offload_args_free)(int, offload_request_args***),
    int total_requests){
    we take exe time samples periodically as a fixed number of requests complete
    This enables discarding results collected during the latter phase of the test
    where the system has ran out of work to execute during blocking stalls
  */

  /* this function takes multiple samples */
  for(int i = 0; i < itr; i++){
    yielding_request_offered_load(
      yielding_memcpy_and_compute,
      alloc_offload_memcpy_and_compute_args,
      free_offload_memcpy_and_compute_args,
      num_requests_before_stamp,
      total_requests,
      exetime,
      i * num_exe_time_samples_per_run
    );
  }

  for(int i=0; i<total_exe_time_samples; i++){
    LOG_PRINT(LOG_DEBUG, "ExeTime: %lu\n", exetime[i]);
  }
  mean_median_stdev_rps(
    exetime, total_exe_time_samples, num_requests_before_stamp, "SwitchToFillRPS"
  );
  /*}*/


  free_dsa_wq();
}