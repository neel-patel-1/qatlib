#include "memfill_gather.h"

void alloc_offload_memfill_and_gather_args_timed( /* is this scatter ?*/
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
void alloc_offload_memfill_and_gather_args( /* is this scatter ?*/
  int total_requests,
  offload_request_args *** p_off_args,
  ax_comp *comps
){

  offload_request_args **off_args =
    (offload_request_args **)malloc(total_requests * sizeof(offload_request_args *));
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

void blocking_memcpy_and_compute_stamped(
  fcontext_transfer_t arg
){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;

  float *scatter_array = (float *)(args->src_payload);
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
    desc, 0x0, (uint64_t)scatter_array,
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
    scatter_array[indirect_array[i]] = 1.0;
  }

  ts3[id] = sampleCoderdtsc();

  if(gLogLevel >= LOG_DEBUG){ /* validate code */
    validate_scatter_array(scatter_array, indirect_array);
  }

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}



void blocking_memcpy_and_compute(
  fcontext_transfer_t arg
){
  offload_request_args *args = (offload_request_args *)arg.data;

  float *scatter_array = (float *)(args->src_payload);
  int *indirect_array = (int *)(args->dst_payload);
  int id = args->id;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;

  int num_ents;
  num_ents = input_size / sizeof(float);

  prepare_dsa_memfill_desc_with_preallocated_comp(
    desc, 0x0, (uint64_t)scatter_array,
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


  /* populate feature buf using indrecet array*/
  for(int i = 0; i < num_accesses; i++){
    scatter_array[indirect_array[i]] = 1.0;
  }



  if(gLogLevel >= LOG_DEBUG){ /* validate code */
    validate_scatter_array(scatter_array, indirect_array);
  }

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void yielding_memcpy_and_compute(
  fcontext_transfer_t arg
){
  offload_request_args *args = (offload_request_args *)arg.data;

  float *scatter_array = (float *)(args->src_payload);
  int *indirect_array = (int *)(args->dst_payload);
  int id = args->id;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;

  int num_ents;
  num_ents = input_size / sizeof(float);

  prepare_dsa_memfill_desc_with_preallocated_comp(
    desc, 0x0, (uint64_t)scatter_array,
    (uint64_t)args->comp, (uint64_t)input_size
  );
  if (!dsa_submit(dsa, desc)){
    LOG_PRINT(LOG_ERR, "Error submitting request\n");
    return;
  } else{
    fcontext_swap(arg.prev_context, NULL);
  }

  if(comp->status != IAX_COMP_SUCCESS){
    LOG_PRINT(LOG_ERR, "Error in offload\n");
    return;
  }


  /* populate feature buf using indrecet array*/
  for(int i = 0; i < num_accesses; i++){
    scatter_array[indirect_array[i]] = 1.0;
  }



  if(gLogLevel >= LOG_DEBUG){ /* validate code */
    validate_scatter_array(scatter_array, indirect_array);
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

  float *scatter_array = (float *)(args->inputs[0]);
  int *indirect_array = (int *)(args->inputs[1]);

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;

  int num_ents;
  num_ents = input_size / sizeof(float);

  ts0[id] = sampleCoderdtsc();
  memset((void*) scatter_array, 0x0, input_size);

  ts1[id] = sampleCoderdtsc();
  for(int i = 0; i < num_accesses; i++){
    scatter_array[indirect_array[i]] = 1.0;
  }

  ts2[id] = sampleCoderdtsc();
  if(gLogLevel >= LOG_DEBUG){ /* validate code */
    validate_scatter_array(scatter_array, indirect_array);
  }

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void cpu_memcpy_and_compute(
  fcontext_transfer_t arg
){
  gpcore_request_args *args = (gpcore_request_args *)arg.data;

  float *scatter_array = (float *)(args->inputs[0]);
  int *indirect_array = (int *)(args->inputs[1]);

  int id = args->id;

  int num_ents;
  num_ents = input_size / sizeof(float);

  memset((void*) scatter_array, 0x0, input_size);

  for(int i = 0; i < num_accesses; i++){
    scatter_array[indirect_array[i]] = 1.0;
  }

  if(gLogLevel >= LOG_DEBUG){ /* validate code */
    validate_scatter_array(scatter_array, indirect_array);
  }

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

