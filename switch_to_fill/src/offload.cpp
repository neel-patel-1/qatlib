#include "offload.h"
#include "print_utils.h"
#include "emul_ax.h"
#include "status.h"
#include <x86intrin.h>
int submit_offload(ax_comp *comp, char *dst_payload){
  int num_retries = 3, retries_remaining = num_retries;
  comp->status = COMP_STATUS_PENDING;
  compl_addr = (uint64_t)comp;
  submit_flag = OFFLOAD_REQUESTED;
  p_dst_buf = (uint64_t)dst_payload;



retry:
  while(submit_flag.load() == OFFLOAD_REQUESTED){
    _mm_pause();
  }

  if(submit_status == SUBMIT_FAIL && retries_remaining > 0){
    retries_remaining --;
    goto retry;
  } else if(retries_remaining == 0){
    PRINT_DBG("Offload request failed after %d retries\n", num_retries);
    return STATUS_FAIL;
  }
  retries_remaining = num_retries;

  return STATUS_SUCCESS;
}

void allocate_crs(int total_requests, ax_comp **p_comps){
  *p_comps = (ax_comp *)aligned_alloc(64, sizeof(ax_comp) * total_requests);
  if(*p_comps == NULL){
    PRINT_DBG("Error allocating completion records\n");
    exit(1);
  }
  for(int i=0; i<total_requests; i++){
    (*p_comps)[i].status = COMP_STATUS_PENDING;
  }
}

void allocate_offload_requests(int total_requests, offload_request_args ***p_off_args, ax_comp *comps, char **dst_bufs){
  *p_off_args = (offload_request_args **)malloc(sizeof(offload_request_args *) * total_requests);
  for(int i=0; i<total_requests; i++){
    (*p_off_args)[i] = (offload_request_args *)malloc(sizeof(offload_request_args));
    (*p_off_args)[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));
    (*p_off_args)[i]->comp = &(comps[i]);
    (*p_off_args)[i]->dst_payload = dst_bufs[i];
    (*p_off_args)[i]->id = i;
  }
}

void allocate_timed_offload_requests(int total_requests,
  timed_offload_request_args ***p_off_args,
  ax_comp *comps, char **dst_bufs,
  uint64_t *ts0, uint64_t *ts1, uint64_t *ts2, uint64_t *ts3
  ){
  *p_off_args = (timed_offload_request_args **)malloc(sizeof(timed_offload_request_args *) * total_requests);
  for(int i=0; i<total_requests; i++){
    (*p_off_args)[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));
    (*p_off_args)[i]->comp = &(comps[i]);
    (*p_off_args)[i]->dst_payload = dst_bufs[i];
    (*p_off_args)[i]->id = i;
    (*p_off_args)[i]->ts0 = ts0;
    (*p_off_args)[i]->ts1 = ts1;
    (*p_off_args)[i]->ts2 = ts2;
    (*p_off_args)[i]->ts3 = ts3;
  }
}