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


int streamingHwCompCrc(int numOperations, int bufferSize, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles, Cpa32U numInstances){
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
    int rc = validateCompress(srcBufferLists[i], dstBufferLists[i], dcResults[i], bufferSize);
    if(rc != CPA_STATUS_SUCCESS){
      PRINT_ERR("Buffer not compressed/decompressed correctly\n");
    }
    rc = validateCompressAndCrc64Sw(srcBufferLists[i], dstBufferLists[i], dcResults[i], bufferSize, crcData);
    if(rc != CPA_STATUS_SUCCESS){
      PRINT_ERR("Buffer not checksumed correctly\n");
    }
  }
  printf("---\nSwCompAndCrcSyncLatency");
  printSyncLatencyStats(endTime, startTime, numOperations, bufferSize);
  printf("---\n");
}


#endif