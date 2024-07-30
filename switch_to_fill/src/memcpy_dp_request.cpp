#include "memcpy_dp_request.h"
#include "wait.h"

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
    free(item_buf);
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

  compute_on_input(dst1, buf_size);
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
  input_populate((char **)&item_buf);
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
  free(item_buf);
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
  blocking_dsa_submit(dsa, desc);
  ts1[id] = sampleCoderdtsc();
  spin_on(comp);

  ts2[id] = sampleCoderdtsc();
  compute_on_input(dst, input_size);
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
  blocking_dsa_submit(dsa, desc);
  ts1[id] = sampleCoderdtsc();
  fcontext_swap(arg.prev_context, NULL);

  ts2[id] = sampleCoderdtsc();
  compute_on_input(dst, input_size);
  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
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
  blocking_dsa_submit(dsa, desc);

  while(comp->status == IAX_COMP_NONE)
  {
    _mm_pause();
  }
  if(comp->status != IAX_COMP_SUCCESS){
    LOG_PRINT(LOG_ERR, "Error in offload\n");
    return;
  }

  compute_on_input(dst, input_size);

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
  blocking_dsa_submit(dsa, desc);

  fcontext_swap(arg.prev_context, NULL);

  compute_on_input(dst, input_size);

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
  compute_on_input(dst1, buf_size);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}



void alloc_cpu_memcpy_and_compute_args(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){
    input_populate((char **)&item_buf);

    char ***ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = (char ***)malloc(total_requests * sizeof(char **));
    for(int i = 0; i < total_requests; i++){
      ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i] = (char **)malloc(3 * sizeof(char *));
      input_populate(&ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0]);
      ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][1] = (char *)malloc(input_size * sizeof(char));
      ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2] = (char *)malloc(sizeof(int));
      *((int *) ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2]) = input_size;
    }

    *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
}
