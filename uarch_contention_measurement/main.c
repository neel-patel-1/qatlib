#include "idxd.h"
#include "cpa_sample_utils.h"

typedef struct completion_record ax_comp;
ax_comp *blocked_record;
typedef struct _desc{
  int32_t id;
  uint8_t rsvd[4];
} desc;
typedef struct _ax_setup_args{
  uint64_t offload_time;
  bool *running;
  desc **pp_desc; /*pointer to the single desc submission allowed - switch to NULL once ax accepted */
} ax_setup_args;

void blocking_emul_ax(void *arg){
  ax_setup_args *args = (ax_setup_args *)arg;
  bool *running = args->running;
  bool offload_in_flight = false, offload_pending = false;
  uint64_t start_time = 0;
  uint64_t offload_time = args->offload_time;
  desc **pp_desc = args->pp_desc;
  int offloader_id = -1;

  while(*running){
    if(offload_in_flight){
      uint64_t wait_until = start_time + offload_time;
      while(sampleCoderdtsc() < wait_until){ }
      PRINT_DBG("Request id: %d completed in %ld\n", offloader_id, sampleCoderdtsc() - start_time);
      offload_in_flight = false;
    }
    offload_pending = sub_desc != NULL;
    if(offload_pending){
      start_time = sampleCoderdtsc();
      offloader_id = sub_desc->id;
      offload_in_flight = true;
      *pp_desc = NULL;
    }
  }
}


int main(int argc, char **argv){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;
  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);
  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcSessionHandle sessionHandles[MAX_INSTANCES];

  int tflags = TEST_FLAGS_BOF;
	int wq_id = 0;
	int dev_id = 2;
  int opcode = DSA_OPCODE_MEMMOVE;
  int wq_type = ACCFG_WQ_SHARED;
  int rc;

  int num_offload_requests = 1;
  dsa = acctest_init(tflags);

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
  if (rc < 0)
    return -ENOMEM;

  desc *sub_desc = NULL; /* this is implicitly the portal, any assignments notify the accelerator*/
  bool running_signal = true;

  pthread_t ax_td;
  ax_setup_args ax_args;
  ax_args.offload_time = 2100;
  ax_args.running = &running_signal;
  ax_args.pp_desc = &sub_desc;

  createThreadPinned(&ax_td, blocking_emul_ax, &ax_args, 20);

  desc off_desc;
  off_desc.id = 0;
  sub_desc = &off_desc;

  /* turn off ax */
  ax_args.running = false;
  pthread_join(ax_td, NULL);


  do_offered_load_test(argc, argv);

  acctest_free_task(dsa);
  acctest_free(dsa);
exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}