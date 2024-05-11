#include "sw_comp_crc_funcs.h"

void multiStreamSwCompressCrc64Func(Cpa32U numOperations, Cpa32U bufferSize,
  Cpa32U numStreams, CpaInstanceHandle dcInstHandle){

  pthread_barrier_t barrier;
  pthread_t threads[numStreams];

  packet_stats **stats[numStreams];
  pthread_barrier_init(&barrier, NULL, numStreams);
  for(int i=0; i<numStreams; i++){
    sw_comp_args *args = NULL;
    OS_MALLOC(&args, sizeof(sw_comp_args));
    args->numOperations = numOperations;
    args->bufferSize = bufferSize;
    args->dcInstHandle = dcInstHandle;
    args->startSync = &barrier;
    args->pCallerStatsArrayPtrIndex = &stats[i];
    createThreadJoinable(&threads[i], syncSwComp, (void *)args);
  }
  for(int i=0; i<numStreams; i++){
    pthread_join(threads[i], NULL);
  }
  printf("------------\nSw Deflate + Sw Crc64 Offload Performance Test");
  printMultiThreadStats(stats, numStreams, numOperations, bufferSize);
}

void syncSwComp(void *args){
  sw_comp_args *sw_args = (sw_comp_args *)args;
  Cpa32U numOperations = sw_args->numOperations;
  Cpa32U bufferSize = sw_args->bufferSize;
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaInstanceHandle dcInstHandle = sw_args->dcInstHandle;
  CpaDcInstanceCapabilities cap = {0};
  CpaDcOpData **opData = NULL;
  struct COMPLETION_STRUCT complete;
  CpaStatus status = CPA_STATUS_SUCCESS;

  COMPLETION_INIT(&complete);
  if (CPA_STATUS_SUCCESS != cpaDcQueryCapabilities(dcInstHandle, &cap)){
    PRINT_ERR("Error in querying capabilities\n");
    return;
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

  pthread_barrier_wait(sw_args->startSync);
  for(int i=0; i<numOperations; i++){
    if(CPA_STATUS_SUCCESS != deflateCompressAndTimestamp(
      srcBufferLists[i], dstBufferLists[i], dcResults[i], i, cb_args[i], &crcData[i])){
      PRINT_ERR("Error in compress data on %d'th packet\n", i);
    }
  }

  *(sw_args->pCallerStatsArrayPtrIndex) = stats;

  for(int i=0; i<numOperations; i++){
    if (CPA_STATUS_SUCCESS != validateCompressAndCrc64Sw(srcBufferLists[i], dstBufferLists[i], dcResults[i], bufferSize, &(crcData[i])))
    {
      PRINT_ERR("Buffer not compressed/decompressed correctly\n");
    }
  }

  char filename[256];
  sprintf(filename, "sw-deflate-sw-crc_bufsize_%d", bufferSize);
  logLatencies(stats, numOperations, filename );

  return;
}

/* resets the stream after each compression */
CpaStatus deflateCompressAndTimestamp(
  CpaBufferList *pBufferListSrc,
  CpaBufferList *pBufferListDst,
  CpaDcRqResults *pDcResults,
  int index,
  callback_args *cb_args,
  CpaCrcData *crcData
){
  z_stream strm;
  int ret;
  memset(&strm, 0, sizeof(z_stream));
  ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
  CpaStatus status = CPA_STATUS_SUCCESS;
  packet_stats *stats = cb_args->stats[index];

  Cpa64U submitTime = sampleCoderdtsc();
  stats->submitTime = submitTime;

  Cpa8U *src = pBufferListSrc->pBuffers[0].pData;
  Cpa32U srcLen = pBufferListSrc->pBuffers->dataLenInBytes;
  Cpa8U *dst = pBufferListDst->pBuffers[0].pData;
  Cpa32U dstLen = pBufferListDst->pBuffers->dataLenInBytes;

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
  pDcResults->produced = strm.total_out;

  Cpa64U crc64 = crc64_be(0, Z_NULL, 0);
  crc64 = crc64_be(crc64, dst, strm.total_out);
  crcData->integrityCrc64b.oCrc = crc64;

  stats->receiveTime = sampleCoderdtsc();

  return status;
}