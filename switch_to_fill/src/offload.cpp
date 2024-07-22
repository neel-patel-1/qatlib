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