#ifndef STREAMING_SINGLE_FUNCS_H
#define STREAMING_SINGLE_FUNCS_H

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


int streamingHwCompCrc(int numOperations, int bufferSize, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles){
  CpaStatus status;
  CpaDcInstanceCapabilities cap = {0};
  CpaDcOpData **opData = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  struct COMPLETION_STRUCT complete;
  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;

  Cpa16U numInstances;
  allocateDcInstances(dcInstHandles, &numInstances);

  /* Streaming submission from a single thread single inst hw*/
  prepareMultipleCompressAndCrc64InstancesAndSessionsForStreamingSubmitAndPoll(dcInstHandles, sessionHandles, numInstances, numInstances);

  CpaDcSessionHandle sessionHandle = sessionHandles[0];
  CpaInstanceHandle dcInstHandle = dcInstHandles[0];
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
      // status = icp_sal_DcPollInstance(dcInstHandle, 0);
      goto retry;
    }
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
      PRINT_ERR("Buffer not compressed/decompressed + iCRC'd correctly\n");
      return CPA_STATUS_FAIL;
    }
    if (CPA_STATUS_SUCCESS != validateCompress(srcBufferLists[i], dstBufferLists[i], dcResults[i], bufferSize)){
      PRINT_ERR("Buffer not compressed/decompressed correctly\n");
      return CPA_STATUS_FAIL;
    }

    if(CPA_STATUS_SUCCESS != validateIntegrityCrc64(srcBufferLists[i],dstBufferLists[i],dcResults[i],bufferSize,&(crcData[i]))){
      PRINT_ERR("Buffer not compressed/decompressed + iCRC'd correctly\n");
      return CPA_STATUS_FAIL;
    }
  }
}


int streamingHwCompCrcSyncLatency(int numOperations, int bufferSize, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles, Cpa32U numInstances){
  CpaStatus status;
  CpaDcInstanceCapabilities cap = {0};
  CpaDcOpData **opData = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  struct COMPLETION_STRUCT complete;
  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;

  /* Streaming submission from a single thread single inst hw*/
  prepareMultipleCompressAndCrc64InstancesAndSessionsForStreamingSubmitAndPoll(dcInstHandles, sessionHandles, numInstances, numInstances);

  CpaDcSessionHandle sessionHandle = sessionHandles[0];
  CpaInstanceHandle dcInstHandle = dcInstHandles[0];
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
    // printf("Completed %d\n", *completed);
    lastBufIdxSubmitted = *completed;
    }
    while(CPA_STATUS_SUCCESS != icp_sal_DcPollInstance(dcInstHandle, 0)){}
  }
  uint64_t endTime = sampleCoderdtsc();
  // printf("Submitted %d\n", *completed);
  printf("---\nHwAxChainCompAndCrcSync\n");
  printSyncLatencyStats(endTime, startTime, numOperations, bufferSize);
  printf("---\n");


  for(int i=0; i<numOperations; i++){
    if (CPA_STATUS_SUCCESS != validateCompressAndCrc64(srcBufferLists[i], dstBufferLists[i], bufferSize, dcResults[i], dcInstHandle, &(crcData[i]))){
      PRINT_ERR("Buffer not compressed/decompressed correctly\n");
    }
  }
}

int singleSwCompCrc(int bufferSize, int numOperations, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles){
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaInstanceHandle dcInstHandle = dcInstHandles[0];
  CpaDcInstanceCapabilities cap = {0};
  CpaDcOpData **opData = NULL;
  struct COMPLETION_STRUCT complete;
  CpaStatus status = CPA_STATUS_SUCCESS;
  COMPLETION_INIT(&complete);
  if (CPA_STATUS_SUCCESS != cpaDcQueryCapabilities(dcInstHandle, &cap)){
    PRINT_ERR("Error in querying capabilities\n");
    return CPA_STATUS_FAIL;
  }
  multiBufferTestAllocations(
    &cb_args,
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

  uint64_t startTime = sampleCoderdtsc();
  for(int i=0; i<numOperations; i++){
    z_stream strm;
    int ret;
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);

    Cpa8U *src = srcBufferLists[i]->pBuffers[0].pData;
    Cpa32U srcLen = srcBufferLists[i]->pBuffers->dataLenInBytes;
    Cpa8U *dst = dstBufferLists[i]->pBuffers[0].pData;
    Cpa32U dstLen = dstBufferLists[i]->pBuffers->dataLenInBytes;

    strm.avail_in = srcLen;
    strm.next_in = (Bytef *)src;
    strm.avail_out = dstLen;
    strm.next_out = (Bytef *)dst;
    ret = deflate(&strm, Z_FINISH);
    if(ret != Z_STREAM_END)
    {
      fprintf(stderr, "Error in deflate, ret = %d\n", ret);
      return CPA_STATUS_FAIL;
    }
    dcResults[i]->produced = strm.total_out;
    Cpa64U crc64 = crc64_be(0, Z_NULL, 0);
    crc64 = crc64_be(crc64, dst, strm.total_out);
    crcData->integrityCrc64b.oCrc = crc64;

  }
  uint64_t endTime = sampleCoderdtsc();
  for(int i=0; i<numOperations; i++){
    int rc = validateCompress(srcBufferLists[i], dstBufferLists[i], dcResults[i], bufferSize);
    if(rc != CPA_STATUS_SUCCESS){
      PRINT_ERR("Buffer not compressed/decompressed correctly\n");
    }
    rc = validateCompressAndCrc64Sw(srcBufferLists[i], dstBufferLists[i], dcResults[i], bufferSize, crcData);
    if(rc != CPA_STATUS_SUCCESS){
      PRINT_ERR("Buffer not checksumed correctly\n");
    }
  }
  printf("---\nSwCompAndCrcStream");
  printThroughputStats(endTime, startTime, numOperations, bufferSize);
  printf("---\n");
}

int singleSwCompCrcLatency(int bufferSize, int numOperations, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles){
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaInstanceHandle dcInstHandle = dcInstHandles[0];
  CpaDcInstanceCapabilities cap = {0};
  CpaDcOpData **opData = NULL;
  struct COMPLETION_STRUCT complete;
  CpaStatus status = CPA_STATUS_SUCCESS;
  COMPLETION_INIT(&complete);
  if (CPA_STATUS_SUCCESS != cpaDcQueryCapabilities(dcInstHandle, &cap)){
    PRINT_ERR("Error in querying capabilities\n");
    return CPA_STATUS_FAIL;
  }
  multiBufferTestAllocations(
    &cb_args,
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

  uint64_t startTime = sampleCoderdtsc();
  for(int i=0; i<numOperations; i++){
    z_stream strm;
    int ret;
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);

    Cpa8U *src = srcBufferLists[i]->pBuffers[0].pData;
    Cpa32U srcLen = srcBufferLists[i]->pBuffers->dataLenInBytes;
    Cpa8U *dst = dstBufferLists[i]->pBuffers[0].pData;
    Cpa32U dstLen = dstBufferLists[i]->pBuffers->dataLenInBytes;

    strm.avail_in = srcLen;
    strm.next_in = (Bytef *)src;
    strm.avail_out = dstLen;
    strm.next_out = (Bytef *)dst;
    ret = deflate(&strm, Z_FINISH);
    if(ret != Z_STREAM_END)
    {
      fprintf(stderr, "Error in deflate, ret = %d\n", ret);
      return CPA_STATUS_FAIL;
    }
    dcResults[i]->produced = strm.total_out;
    Cpa64U crc64 = crc64_be(0, Z_NULL, 0);
    crc64 = crc64_be(crc64, dst, strm.total_out);
    crcData->integrityCrc64b.oCrc = crc64;

  }
  uint64_t endTime = sampleCoderdtsc();
  for(int i=0; i<numOperations; i++){
    int rc = validateCompressAndCrc64Sw(srcBufferLists[i], dstBufferLists[i], dcResults[i], bufferSize, crcData);
    if(rc != CPA_STATUS_SUCCESS){
      PRINT_ERR("Buffer not checksumed correctly\n");
    }
  }
  printf("---\nSwCompAndCrcSyncLatency");
  printSyncLatencyStats(endTime, startTime, numOperations, bufferSize);
  printf("---\n");
}


typedef struct _strmSubCompCrcSoftChainCbArgs{
  struct task *tsk;
  struct acctest_context *ctx;
  int *bufIdx;
  CpaBufferList *srcBufferList;
  int *completed;
  CpaDcRqResults *dcResults;
} strmSubCompCrcSoftChainCbArgs;

void dcSwChainedCompCrcStreamingFwd(void *arg, CpaStatus status){



  strmSubCompCrcSoftChainCbArgs *cbArgs = (strmSubCompCrcSoftChainCbArgs *) arg;
  CpaBufferList *srcBufferList = cbArgs->srcBufferList;
  int *completed = cbArgs->completed;
  struct hw_desc * hw= NULL;
  struct task *tsk = cbArgs->tsk;
  struct acctest_context *ctx = cbArgs->ctx;
  CpaDcRqResults *dcResults = cbArgs->dcResults;

  tsk->desc->xfer_size = dcResults->produced;
  // PRINT_DBG("PRODUCED: %d\n",dcResults->produced);


  hw= tsk->desc;
  while( enqcmd(ctx->wq_reg, hw) ){PRINT_DBG("Retry\n");};
  dsa_wait_crcgen(ctx, tsk);
  (*completed)++;

}

void dcSwChainedCompCrcStreamingFwdNonBlocking(void *arg, CpaStatus status){



  strmSubCompCrcSoftChainCbArgs *cbArgs = (strmSubCompCrcSoftChainCbArgs *) arg;
  CpaBufferList *srcBufferList = cbArgs->srcBufferList;
  int *completed = cbArgs->completed;
  struct hw_desc * hw= NULL;
  struct task *tsk = cbArgs->tsk;
  struct acctest_context *ctx = cbArgs->ctx;
  CpaDcRqResults *dcResults = cbArgs->dcResults;

  tsk->desc->xfer_size = dcResults->produced;
  // PRINT_DBG("PRODUCED: %d\n",dcResults->produced);


  hw= tsk->desc;
  // if( enqcmd(ctx->wq_reg, hw) ){PRINT_ERR("Unexpected failure in dsa submission... Am I the fastest accel in the chain? Am I preceded by slow QAT?\n"); exit(-1);};
  /* noticed that we block in the callback until there is space in the shared work queue -- do we hit any retries?  */

}


CpaStatus prepareMultipleSwChainedCompressAndCrc64InstancesAndSessions(CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances, Cpa16U numSessions){
  CpaStatus status = CPA_STATUS_SUCCESS;
  for(int i=0; i<numInstances; i++){
    dcInstHandles[i] = dcInstHandles[i];
    prepareDcInst(&dcInstHandles[i]);
    sessionHandles[i] = sessionHandles[i];
    prepareDcSession(dcInstHandles[i], &sessionHandles[i], dcSwChainedCompCrcStreamingFwd);
  }
  return status;
}

CpaStatus prepareMultipleSwChainedNonBlockingCallbackCompressAndCrc64InstancesAndSessions(CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances, Cpa16U numSessions){
  CpaStatus status = CPA_STATUS_SUCCESS;
  for(int i=0; i<numInstances; i++){
    dcInstHandles[i] = dcInstHandles[i];
    prepareDcInst(&dcInstHandles[i]);
    sessionHandles[i] = sessionHandles[i];
    prepareDcSession(dcInstHandles[i], &sessionHandles[i], dcSwChainedCompCrcStreamingFwdNonBlocking);
  }
  return status;
}


int streamingSwChainCompCrc(Cpa32U numOperations, Cpa32U bufferSize, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles, Cpa16U numInstances){
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

int swChainCompCrcSync(Cpa32U numOperations, Cpa32U bufferSize, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles, Cpa16U numInstances){
  /* Sw streaming func */
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
  CpaInstanceHandle dcInstHandle = NULL;
  CpaDcSessionHandle sessionHandle = NULL;

  struct acctest_context *dsa = NULL;
  int tflags = TEST_FLAGS_BOF;
  int rc;
  int wq_type = ACCFG_WQ_SHARED;
  int dev_id = 0;
  int wq_id = 0;
  int opcode = 16;

  struct task_node *task_node = NULL;
  struct task *tsk = NULL;

  Cpa32U bListIdx = 0;

  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;

  if (!dsa)
		return -ENOMEM;

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
  acctest_alloc_multiple_tasks(dsa, numOperations);

  allocateDcInstances(dcInstHandles, &numInstances);
  if (0 == numInstances)
  {
    fprintf(stderr, "No instances found\n");
    return CPA_STATUS_FAIL;
  }

  prepareMultipleSwChainedCompressAndCrc64InstancesAndSessions(dcInstHandles, sessionHandles, numInstances, numInstances);

  sessionHandle = sessionHandles[0];
  dcInstHandle = dcInstHandles[0];
  cpaDcQueryCapabilities(dcInstHandle, &cap);

  int *completed = malloc(sizeof(int));
  *completed = 0;

  multiBufferTestAllocations(
    &cb_args,
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
    &complete
  );

  task_node = dsa->multi_task_node;
  while(task_node){
    CpaFlatBuffer *fltBuf = &(dstBufferLists[bListIdx]->pBuffers[0]);
    prepare_crc_task(task_node->tsk, dsa, fltBuf->pData, -1); /* need to update the task with the compressed buffer len from inside the cb*/
    bListIdx++;
    task_node = task_node->next;
  }
  task_node = dsa->multi_task_node;

  int lastBufIdxSubmitted = -1;


  strmSubCompCrcSoftChainCbArgs **cbArgs = NULL;
  OS_MALLOC(&cbArgs,sizeof(strmSubCompCrcSoftChainCbArgs*)*numOperations);
  for(int i=0; i<numOperations; i++){
    if(task_node == NULL){
      PRINT_ERR("Insufficient task nodes\n");
      return CPA_STATUS_FAIL;
    }
    PHYS_CONTIG_ALLOC(&(cbArgs[i]),sizeof(strmSubCompCrcSoftChainCbArgs));
    cbArgs[i]->srcBufferList = dstBufferLists[i];
    cbArgs[i]->completed = completed;
    cbArgs[i]->tsk = task_node->tsk;
    cbArgs[i]->ctx = dsa;
    cbArgs[i]->dcResults = dcResults[i];
    task_node = task_node->next;
  }

  uint64_t startTime = sampleCoderdtsc();
  while(*completed < numOperations){
retry:
  if(lastBufIdxSubmitted < *completed){
    status = cpaDcCompressData2(
      dcInstHandle,
      sessionHandle,
      srcBufferLists[*completed],     /* source buffer list */
      dstBufferLists[*completed],     /* destination buffer list */
      opData[*completed],            /* Operational data */
      dcResults[*completed],         /* results structure */
      (void *)cbArgs[*completed]);
    if(status == CPA_STATUS_RETRY){
      goto retry;
    }
    _mm_sfence();
    lastBufIdxSubmitted = *completed;
  }

    status = icp_sal_DcPollInstance(dcInstHandle, 0);
  }
  uint64_t endTime = sampleCoderdtsc();

  printf("---\nSyncSwAxChainCompAndCrc\n");
  printSyncLatencyStats(endTime, startTime, numOperations, bufferSize);
  printf("---\n");

  for(int i=0; i<numOperations; i++){
    if (CPA_STATUS_SUCCESS != validateCompress(srcBufferLists[i], dstBufferLists[i],  dcResults[i], bufferSize)){
      PRINT_ERR("Buffer not compressed/decompressed correctly\n");
    }
  }

  task_node = dsa->multi_task_node;
  bListIdx=0;
  while(task_node){
    Cpa8U *fltBuf = (dstBufferLists[bListIdx]->pBuffers[0].pData);
    tsk = task_node->tsk;
    Cpa32U dstSize = dcResults[bListIdx]->produced;
    if ( CPA_STATUS_SUCCESS != validateCrc32DSA(tsk, fltBuf, dstSize) ){
      PRINT_ERR("CRC not as expected \n");
      return CPA_STATUS_FAIL;
    }
    task_node = task_node->next;
    bListIdx++;
  }


}

int streamingSwChainCompCrcValidated(int numOperations, int bufferSize, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles){
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

  struct acctest_context *dsa = NULL;
  int tflags = TEST_FLAGS_BOF;
  int rc;
  int wq_type = ACCFG_WQ_SHARED;
  int dev_id = 0;
  int wq_id = 0;
  int opcode = 16;

  struct task_node *task_node = NULL;
  struct task *tsk = NULL;

  Cpa32U bListIdx = 0;



  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;

  if (!dsa)
		return -ENOMEM;

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
  acctest_alloc_multiple_tasks(dsa, numOperations);

  allocateDcInstances(dcInstHandles, &numInstances);
  if (0 == numInstances)
  {
    fprintf(stderr, "No instances found\n");
    return CPA_STATUS_FAIL;
  }

  prepareMultipleSwChainedNonBlockingCallbackCompressAndCrc64InstancesAndSessions(dcInstHandles, sessionHandles, numInstances, numInstances);

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

  task_node = dsa->multi_task_node;
  while(task_node){
    CpaFlatBuffer *fltBuf = &(dstBufferLists[bListIdx]->pBuffers[0]);
    prepare_crc_task(task_node->tsk, dsa, fltBuf->pData, -1); /* need to update the task with the compressed buffer len from inside the cb*/
    bListIdx++;
    task_node = task_node->next;
  }
  task_node = dsa->multi_task_node;

  int lastBufIdxSubmitted = -1;

  strmSubCompCrcSoftChainCbArgs **cbArgs = NULL;
  OS_MALLOC(&cbArgs,sizeof(strmSubCompCrcSoftChainCbArgs*)*numOperations);
  for(int i=0; i<numOperations; i++){
    if(task_node == NULL){
      PRINT_ERR("Insufficient task nodes\n");
      return CPA_STATUS_FAIL;
    }
    PHYS_CONTIG_ALLOC(&(cbArgs[i]),sizeof(strmSubCompCrcSoftChainCbArgs));
    cbArgs[i]->srcBufferList = dstBufferLists[i];
    cbArgs[i]->completed = completed;
    cbArgs[i]->tsk = task_node->tsk;
    cbArgs[i]->ctx = dsa;
    cbArgs[i]->dcResults = dcResults[i];
    task_node = task_node->next;
  }

  uint64_t startTime = sampleCoderdtsc();
  task_node = dsa->multi_task_node; /* start with the first task node and update it every time a response is found*/
  const volatile uint8_t *comp = (uint8_t *)task_node->tsk->comp;
  int bufIdx = 0; /* need to track the buffer idx to submit CPA requests for */
  int e2eCompleted = 0;
  /* We don't want to resubmit the same request every time the completion record is found unwritten
  we need a way to only submit a request for each bufIdx once

  check the comp -> yes -> update task node we check on next iteration
  poll the icp -> forward to dsa ->
  don't need to check anything unless we start overflowing the dsa, in which case we can buffer requests in memory (lots of space), but DSA is the faster ax, so we don't hit this case
   */

  while(task_node){
    if(bufIdx <numOperations){
retry:
      status = cpaDcCompressData2(
        dcInstHandle,
        sessionHandle,
        srcBufferLists[bufIdx],     /* source buffer list */
        dstBufferLists[bufIdx],     /* destination buffer list */
        opData[bufIdx],            /* Operational data */
        dcResults[bufIdx],         /* results structure */
        (void *)cbArgs[bufIdx]);
      if(status == CPA_STATUS_RETRY){
        status = icp_sal_DcPollInstance(dcInstHandle, 0); /* try to free up some space */
        goto retry;
      }
      bufIdx++;
    }
    status = icp_sal_DcPollInstance(dcInstHandle, 0); /* on success, we forwarded to DSA -- how do we integrate a freedom poll operation to give some space back to compressData? */
    if(*comp != 0){ /* found a completed dsa op */
      task_node = task_node->next;
      // PRINT_DBG("Comp found %d\n", e2eCompleted);
      if(task_node != NULL)
        comp = (uint8_t *)task_node->tsk->comp;
      e2eCompleted++;
    }
  }
  uint64_t endTime = sampleCoderdtsc();
  // printf("Submitted %d\n", *completed);
  printf("---\nSwAxChainCompAndCrcStream\n");
  printThroughputStats(endTime, startTime, numOperations, bufferSize);
  printf("---\n");
  for(int i=0; i<numOperations; i++){
    if (CPA_STATUS_SUCCESS != validateCompressAndCrc64(srcBufferLists[i], dstBufferLists[i], bufferSize, dcResults[i], dcInstHandle, &(crcData[i]))){ /* should pass since iCRC's are enabled */
      PRINT_ERR("Buffer not compressed/decompressed + iCRC'd correctly\n");
      return CPA_STATUS_FAIL;
    }

    if (CPA_STATUS_SUCCESS != validateCompress(srcBufferLists[i], dstBufferLists[i],  dcResults[i], bufferSize)){
      PRINT_ERR("Buffer not compressed/decompressed correctly\n");
      return CPA_STATUS_FAIL;
    }
  }

  task_node = dsa->multi_task_node;
  bListIdx=0;
  while(task_node){
    Cpa8U *fltBuf = (dstBufferLists[bListIdx]->pBuffers[0].pData);
    tsk = task_node->tsk;
    Cpa32U dstSize = dcResults[bListIdx]->produced;
    if ( CPA_STATUS_SUCCESS != validateCrc32DSA(tsk, fltBuf, dstSize) ){
      PRINT_ERR("CRC not as expected \n");
      return CPA_STATUS_FAIL;
    }
    task_node = task_node->next;
    bListIdx++;
  }


}

int streamingSWCompressAndCRC32Validated(int numOperations, int bufferSize, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles){
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaInstanceHandle dcInstHandle = dcInstHandles[0];
  CpaDcInstanceCapabilities cap = {0};
  CpaDcOpData **opData = NULL;
  struct COMPLETION_STRUCT complete;
  CpaStatus status = CPA_STATUS_SUCCESS;
  COMPLETION_INIT(&complete);
  Cpa16U numInstances = 0;
  CpaDcSessionHandle sessionHandle;

  allocateDcInstances(dcInstHandles, &numInstances);
  if (0 == numInstances)
  {
    fprintf(stderr, "No instances found\n");
    return CPA_STATUS_FAIL;
  }

  prepareMultipleCompressAndCrc64InstancesAndSessionsForStreamingSubmitAndPoll(dcInstHandles, sessionHandles, numInstances, numInstances);

  sessionHandle = sessionHandles[0];
  dcInstHandle = dcInstHandles[0];

  if (CPA_STATUS_SUCCESS != cpaDcQueryCapabilities(dcInstHandle, &cap)){
    PRINT_ERR("Error in querying capabilities\n");
    return CPA_STATUS_FAIL;
  }
  multiBufferTestAllocations(
    &cb_args,
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

  uint64_t startTime = sampleCoderdtsc();
  for(int i=0; i<numOperations; i++){
    z_stream strm;
    int ret;
    memset(&strm, 0, sizeof(z_stream));
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);

    Cpa8U *src = srcBufferLists[i]->pBuffers[0].pData;
    Cpa32U srcLen = srcBufferLists[i]->pBuffers->dataLenInBytes;
    Cpa8U *dst = dstBufferLists[i]->pBuffers[0].pData;
    Cpa32U dstLen = dstBufferLists[i]->pBuffers->dataLenInBytes;

    strm.avail_in = srcLen;
    strm.next_in = (Bytef *)src;
    strm.avail_out = dstLen;
    strm.next_out = (Bytef *)dst;
    ret = deflate(&strm, Z_FINISH);
    if(ret != Z_STREAM_END)
    {
      fprintf(stderr, "Error in deflate, ret = %d\n", ret);
      return CPA_STATUS_FAIL;
    }
    dcResults[i]->produced = strm.total_out;
    Cpa64U crc64 = crc64_be(0, Z_NULL, 0);
    crc64 = crc64_be(crc64, dst, strm.total_out);
    crcData->integrityCrc64b.oCrc = crc64;

  }
  uint64_t endTime = sampleCoderdtsc();

  printf("---\nSwCompAndCrcStream");
  printThroughputStats(endTime, startTime, numOperations, bufferSize);
  printf("---\n");

  for(int i=0; i<numOperations; i++){
    int rc = validateCompressAndCrc64Sw(srcBufferLists[i], dstBufferLists[i], dcResults[i], bufferSize, crcData);
    if(rc != CPA_STATUS_SUCCESS){
      PRINT_ERR("Buffer not checksumed correctly\n");
    }
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
  int nextBufIdx = 0;
  while(nextBufIdx < numOperations){
    if(nextBufIdx > lastBufIdxSubmitted){
      nextBufIdx = *completed;
retry:
      status = cpaDcCompressData2(
        dcInstHandle,
        sessionHandle,
        srcBufferLists[nextBufIdx],     /* source buffer list */
        dstBufferLists[nextBufIdx],     /* destination buffer list */
        opData[nextBufIdx],            /* Operational data */
        dcResults[nextBufIdx],         /* results structure */
        (void *)completed);
      lastBufIdxSubmitted = nextBufIdx;
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

  for(int i=0; i<numOperations; i++){
    status = validateCompress(srcBufferLists[i], dstBufferLists[i], dcResults[i], bufferSize);
    if(status != CPA_STATUS_SUCCESS){
      PRINT_ERR("Buffer not Checksum'd correctly\n");
    }
  }
}



#endif