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

  prepareMultipleSwChainedCompressAndCrc64InstancesAndSessions(dcInstHandles, sessionHandles, numInstances, numInstances);

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
  // printf("Submitted %d\n", *completed);
  printf("---\nSwAxChainCompAndCrcStream\n");
  printThroughputStats(endTime, startTime, numOperations, bufferSize);
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
  // hwCompCrcValidatedStream(numOperations, bufferSize, dcInstHandles, sessionHandles);
  // streamingSwChainCompCrcValidated(numOperations, bufferSize, dcInstHandles, sessionHandles);
  streamingSWCompressAndCRC32Validated(numOperations, bufferSize, dcInstHandles, sessionHandles);

exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}