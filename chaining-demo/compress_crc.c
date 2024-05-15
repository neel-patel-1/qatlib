#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"

#include "dc_inst_utils.h"
#include "thread_utils.h"

#include "buffer_prepare_funcs.h"
#include "hw_comp_crc_funcs.h"
#include "sw_comp_crc_funcs.h"

#include "print_funcs.h"

#include <zlib.h>


#include "idxd.h"
#include "dsa.h"

#include "dsa_funcs.h"

#include "validate_compress_and_crc.h"

#include "accel_test.h"

#include "sw_chain_comp_crc_funcs.h"
#include "smt-thread-exps.h"

#include "tests.h"

#include <xmmintrin.h>

#include "streaming-single-funcs.h"

int gDebugParam = 1;

int streamingSwChainCompCrcValidated(
  Cpa32U numOperations, Cpa32U bufferSize, CpaInstanceHandle *dcInstHandles,
  CpaDcSessionHandle *sessionHandles, Cpa16U numInstances
  ){
  /* Sw streaming func */
  struct acctest_context *dsa = NULL;
  int tflags = TEST_FLAGS_BOF;
  int rc;
  int wq_type = ACCFG_WQ_SHARED;
  int dev_id = 0;
  int wq_id = 0;
  int opcode = 16;

  CpaStatus status = CPA_STATUS_SUCCESS;
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  callback_args **cb_args = NULL;
  CpaDcOpData **opData = NULL;
  packet_stats **dummyStats = NULL; /* to appeaase multiBufferTestAllocations*/
  struct COMPLETION_STRUCT complete;

  CpaInstanceHandle dcInstHandle = NULL;
  CpaDcSessionHandle sessionHandle = NULL;


  two_stage_packet_stats **stats2Phase = NULL;
  dc_crc_polling_args *dcCrcArgs = NULL;
  crc_polling_args *crcArgs = NULL;
  dsa_fwder_args **args;

  pthread_t crcTid, dcToCrcTid;
  struct task_node *waitTaskNode;


  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;

  if (!dsa)
		return -ENOMEM;

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
	if (rc < 0)
		return -ENOMEM;

  prepareMultipleSwChainedCompressAndCrc64InstancesAndSessions(dcInstHandles, sessionHandles, numInstances, numInstances);
  CpaDcInstanceCapabilities cap = {0};
  cpaDcQueryCapabilities(dcInstHandles[0], &cap);

  sessionHandle = sessionHandles[0];
  dcInstHandle = dcInstHandles[0];

  multiBufferTestAllocations(
    &cb_args,
    &dummyStats,
    &opData,
    &dcResults,
    &crcData,
    numOperations,
    bufferSize,
    cap,
    &srcBufferLists,
    &dstBufferLists,
    dcInstHandle,
    &complete
  );

  int bufIdx = 0;
  create_tsk_nodes_for_stage2_offload(srcBufferLists, numOperations, dsa);
  strmSubCompCrcSoftChainCbArgs cbArgs[numOperations];// = malloc(sizeof(strmSubCompCrcSoftChainCbArgs) * numOperations);
  struct task_node *task_node = dsa->multi_task_node;
  for(int i=0; i<numOperations; i++){
    cbArgs[i].tsk = task_node->tsk;
    cbArgs[i].ctx = dsa;
    cbArgs[i].bufIdx = &bufIdx;
    task_node = task_node->next;
  }

  int lastBufIdxSubmitted = -1;
  /* if the callback does not increment the bufIdx,
    we should not submit another compression request for a bufIdx we
    already submitted  */
  task_node = dsa->multi_task_node;
  struct completion_record *comp = task_node->tsk->comp;

  uint64_t startTime = sampleCoderdtsc();
  while(task_node){
    comp = task_node->tsk->comp;
  if(bufIdx > lastBufIdxSubmitted && bufIdx < numOperations){
retry_comp_crc:
    status = cpaDcCompressData2(
      dcInstHandle,
      sessionHandle,
      srcBufferLists[bufIdx],     /* source buffer list */
      dstBufferLists[bufIdx],     /* destination buffer list */
      opData[bufIdx],            /* Operational data */
      dcResults[bufIdx],         /* results structure */
      (void *)&(cbArgs[bufIdx])); /* data sent as is to the callback function*/
    if(status == CPA_STATUS_RETRY){
      goto retry_comp_crc;
    }
    lastBufIdxSubmitted = bufIdx;
    // PRINT_DBG("Submitted %d\n", bufIdx);
  }

    status = icp_sal_DcPollInstance(dcInstHandle, 0);
    _mm_sfence();

    /* poll for crc completion and increment if completed */

    if(comp->status != 0){
      task_node = task_node->next;
    }

    rc = validateCompress(srcBufferLists[0], dstBufferLists[0], dcResults[0], bufferSize);
    if (rc != CPA_STATUS_SUCCESS){
      PRINT_ERR("Buffer not Checksum'd correctly\n");
    }
  }

  uint64_t endTime = sampleCoderdtsc();

  printf("---\nSwAxChainCompAndCrcStream\n");
  printThroughputStats(endTime, startTime, numOperations, bufferSize);
  printf("---\n");

  rc = verifyCrcTaskNodes(dsa->multi_task_node,srcBufferLists,bufferSize);
  if (rc != CPA_STATUS_SUCCESS){
    PRINT_ERR("Buffer not Checksum'd correctly\n");
  }
}

int hwCompCrcValidatedStream(int numOperations, int bufferSize, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles){

  CpaStatus status = CPA_STATUS_FAIL;
  CpaDcInstanceCapabilities cap = {0};
  CpaDcOpData **opData = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  struct COMPLETION_STRUCT complete;
  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;
  Cpa16U numInstances = 0;
  CpaInstanceHandle dcInstHandle = NULL;
  CpaDcSessionHandle sessionHandle = NULL;

  allocateDcInstances(dcInstHandles, &numInstances);
  if (0 == numInstances)
  {
    fprintf(stderr, "No instances found\n");
    return CPA_STATUS_FAIL;
  }

  prepareMultipleCompressAndCrc64InstancesAndSessionsForStreamingSubmitAndPoll(dcInstHandles, sessionHandles, numInstances, numInstances);

  sessionHandle = sessionHandles[0];
  dcInstHandle = dcInstHandles[0];
    cpaDcQueryCapabilities(dcInstHandle, &cap);

  int *completed = malloc(sizeof(int));
  *completed = 0;

  multiBufferTestAllocations(&cb_args,
      &stats,
      &opData,
      &dcResults,
      &crcData,
      numOperations,
      bufferSize,
      cap,
      &srcBufferLists,
      &dstBufferLists,
      dcInstHandle,
      &complete);

    int lastBufIdxSubmitted = -1;

  uint64_t startTime = sampleCoderdtsc();
  while(*completed < numOperations){
    if(*completed > lastBufIdxSubmitted){
retry:
    status = cpaDcCompressData2(
      dcInstHandle,
      sessionHandle,
      srcBufferLists[*completed],     /* source buffer list */
      dstBufferLists[*completed],     /* destination buffer list */
      opData[*completed],            /* Operational data */
      dcResults[*completed],         /* results structure */
      (void *)completed);
    if(status == CPA_STATUS_RETRY){
      goto retry;
    }
    _mm_sfence();
    printf("Completed %d\n", *completed);
    lastBufIdxSubmitted = *completed;
    }
    status = icp_sal_DcPollInstance(dcInstHandle, 0);
  }
  uint64_t endTime = sampleCoderdtsc();
  // printf("Submitted %d\n", *completed);
  printf("---\nHwAxChainCompAndCrcStream\n");
  printThroughputStats(endTime, startTime, numOperations, bufferSize);
  printf("---\n");
  for(int i=0; i<numOperations; i++){
    if (CPA_STATUS_SUCCESS != validateCompressAndCrc64(srcBufferLists[i], dstBufferLists[i], bufferSize, dcResults[i], dcInstHandle, &(crcData[i]))){
      PRINT_ERR("Buffer not compressed/decompressed correctly\n");
    }
  }

  status = validateCompressAndCrc64(srcBufferLists[0], dstBufferLists[0], bufferSize,  dcResults[0], dcInstHandle, &crcData[0]);
  if(status != CPA_STATUS_SUCCESS){
    PRINT_ERR("Buffer not Checksum'd correctly\n");
  }
  for(int i=0; i<numOperations; i++){
    status = validateCompress(srcBufferLists[i], dstBufferLists[i], dcResults[i], bufferSize);
    if(status != CPA_STATUS_SUCCESS){
      PRINT_ERR("Buffer not Checksum'd correctly\n");
    }
  }
}


int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;

  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcSessionHandle sessionHandles[MAX_INSTANCES];

  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);



  int bufferSizes[] = {4096, 65536, 1024*1024};
  int bufferSize = 4096;
  int numOperations = 1000;
  hwCompCrcValidatedStream(numOperations, bufferSize, dcInstHandles, sessionHandles);
  streamingSwChainCompCrcValidated(numOperations, bufferSize, dcInstHandles, sessionHandles, 1);


exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}