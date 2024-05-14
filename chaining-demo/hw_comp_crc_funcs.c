#include "hw_comp_crc_funcs.h"

void createCompressCrc64Submitter(
  CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  packet_stats ***pArrayPacketStatsPtrs,
  pthread_barrier_t *pthread_barrier,
  pthread_t *threadId
  )
{
  submitter_args *submitterArgs = NULL;
  OS_MALLOC(&submitterArgs, sizeof(submitter_args));
  submitterArgs->dcInstHandle = dcInstHandle;
  submitterArgs->sessionHandle = sessionHandle;
  submitterArgs->numOperations = numOperations;
  submitterArgs->bufferSize = bufferSize;
  submitterArgs->pArrayPacketStatsPtrs = pArrayPacketStatsPtrs;
  submitterArgs->pthread_barrier = pthread_barrier;
  createThreadJoinable(threadId, streamFn, (void *)submitterArgs);
}

CpaStatus prepareMultipleCompressAndCrc64InstancesAndSessions(CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances, Cpa16U numSessions){
  CpaStatus status = CPA_STATUS_SUCCESS;
  for(int i=0; i<numInstances; i++){
    dcInstHandles[i] = dcInstHandles[i];
    prepareDcInst(&dcInstHandles[i]);
    sessionHandles[i] = sessionHandles[i];
    prepareDcSession(dcInstHandles[i], &sessionHandles[i], dcPerfCallback);
  }
  return status;
}

CpaStatus prepareMultipleCompressAndCrc64InstancesAndSessionsForStreamingSubmitAndPoll(CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances, Cpa16U numSessions){
  CpaStatus status = CPA_STATUS_SUCCESS;
  for(int i=0; i<numInstances; i++){
    dcInstHandles[i] = dcInstHandles[i];
    prepareDcInst(&dcInstHandles[i]);
    sessionHandles[i] = sessionHandles[i];
    prepareDcSession(dcInstHandles[i], &sessionHandles[i], dcCallback2);
  }
  return status;
}


void createCompressCrc64Poller(CpaInstanceHandle dcInstHandle, Cpa16U id, pthread_t *threadId){
  CpaInstanceInfo2 info2 = {0};
  CpaStatus status = cpaDcInstanceGetInfo2(dcInstHandle, &info2);
  if ((status == CPA_STATUS_SUCCESS) && (info2.isPolled == CPA_TRUE))
  {
      /* Start thread to poll instance */
      thread_args *args = NULL;
      args = (thread_args *)malloc(sizeof(thread_args));
      args->dcInstHandle = dcInstHandle;
      args->id = id;
      createThread(threadId, dc_polling, (void *)args);
  }
}

CpaStatus submitAndStamp(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle sessionHandle, CpaBufferList *pBufferListSrc,
  CpaBufferList *pBufferListDst, CpaDcOpData *opData, CpaDcRqResults *pDcResults, callback_args *cb_args, int index){

  CpaStatus status = CPA_STATUS_SUCCESS;
  packet_stats *stats = cb_args->stats[index];
  Cpa64U submitTime = sampleCoderdtsc();
  stats->submitTime = submitTime;
  status = cpaDcCompressData2(
    dcInstHandle,
    sessionHandle,
    pBufferListSrc,     /* source buffer list */
    pBufferListDst,     /* destination buffer list */
    opData,            /* Operational data */
    pDcResults,         /* results structure */
    (void *)cb_args); /* data sent as is to the callback function*/
  return status;

}




CpaStatus doSubmissionsCompressAndCrc64AndWaitForFinal(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle sessionHandle,
  CpaBufferList **srcBufferLists, CpaBufferList **dstBufferLists, CpaDcOpData **opData, CpaDcRqResults **dcResults,
  callback_args **cb_args, Cpa32U numOperations, struct COMPLETION_STRUCT *complete){

  CpaStatus status = CPA_STATUS_SUCCESS;
  for(int i=0; i<numOperations; i++){
retry:
    status = submitAndStamp(
      dcInstHandle,
      sessionHandle,
      srcBufferLists[i],
      dstBufferLists[i],
      (opData[i]),
      dcResults[i],
      cb_args[i],
      i
    );
    if(status != CPA_STATUS_SUCCESS){
      // fprintf(stderr, "Error in compress data on %d'th packet\n", i);
      goto retry;
    }
  }
  if(!COMPLETION_WAIT(complete, 5000)){
    PRINT_ERR("timeout or interruption in cpaDcCompressData2\n");
    status = CPA_STATUS_FAIL;
  }
  return status;
}

CpaStatus singleStreamOfCompressAndCrc64(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle sessionHandle,
  Cpa32U numOperations, Cpa32U bufferSize, packet_stats ***ppStats, pthread_barrier_t *barrier
  ){
    CpaDcInstanceCapabilities cap = {0};
    CpaDcOpData **opData = NULL;
    CpaDcRqResults **dcResults = NULL;
    CpaCrcData *crcData = NULL;
    struct COMPLETION_STRUCT complete;
    callback_args **cb_args = NULL;
    packet_stats **stats = NULL;
    CpaBufferList **srcBufferLists = NULL;
    CpaBufferList **dstBufferLists = NULL;
    cpaDcQueryCapabilities(dcInstHandle, &cap);
    COMPLETION_INIT(&complete);

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

    pthread_barrier_wait(barrier);
    CpaStatus status = doSubmissionsCompressAndCrc64AndWaitForFinal(
      dcInstHandle,
      sessionHandle,
      srcBufferLists,
      dstBufferLists,
      opData,
      dcResults,
      cb_args,
      numOperations,
      &complete
    );
    /* validate all results */
    for(int i=0; i<numOperations; i++){
      if (CPA_STATUS_SUCCESS != validateCompressAndCrc64(srcBufferLists[i], dstBufferLists[i], bufferSize, dcResults[i], dcInstHandle, &(crcData[i]))){
        PRINT_ERR("Buffer not compressed/decompressed correctly\n");
      }
    }
    *ppStats = stats;
    char filename[256];
    sprintf(filename, "hw-deflate-crc64_bufsize_%d", bufferSize);
    logLatencies(stats, numOperations, filename );
    COMPLETION_DESTROY(&complete);
}


void *streamFn(void *arg){
  submitter_args *args = (submitter_args *)arg;
  singleStreamOfCompressAndCrc64(
    args->dcInstHandle,
    args->sessionHandle,
    args->numOperations,
    args->bufferSize,
    args->pArrayPacketStatsPtrs, /*For communicating stats to caller */
    args->pthread_barrier /* for synchronizing before starting submissions */
  );
}


void multiStreamCompressCrc64PerformanceTest(
  Cpa32U numFlows,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  CpaInstanceHandle *dcInstHandles,
  CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances
)
{
  /* multiple instance for latency test */
  prepareMultipleCompressAndCrc64InstancesAndSessions(dcInstHandles, sessionHandles, numInstances, numFlows);

  /* Create Polling Threads */
  pthread_t thread[numFlows];
  for(int i=0; i<numFlows; i++){
    createCompressCrc64Poller(dcInstHandles[i], i, &(thread[i]));
  }

  /* Create submitting threads */
  pthread_t subThreads[numFlows];
  packet_stats **arrayOfPacketStatsArrayPointers[numFlows];
  pthread_barrier_t *pthread_barrier = NULL;
  pthread_barrier = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
  pthread_barrier_init(pthread_barrier, NULL, numFlows);
  for(int i=0; i<numFlows; i++){
    createCompressCrc64Submitter(
      dcInstHandles[i],
      sessionHandles[i],
      numOperations,
      bufferSize,
      &(arrayOfPacketStatsArrayPointers[i]),
      pthread_barrier,
      &(subThreads[i])
    );
  }

  /* Join Submitting Threads -- and terminate associated polling thread busy wait loops*/
  for(int i=0; i<numFlows; i++){
    pthread_join(subThreads[i], NULL);
    gPollingDcs[i] = 0;
  }

  /*  Print Stats */
  printf("------------\nHW Offload Performance Test");
  printMultiThreadStats(arrayOfPacketStatsArrayPointers, numFlows, numOperations, bufferSize);
}

void multiStreamCompressCrc64PerformanceTestMultiPoller(
  Cpa32U numFlows,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  CpaInstanceHandle *dcInstHandles,
  CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances
)
{
  /* multiple instance for latency test */
  prepareMultipleCompressAndCrc64InstancesAndSessions(dcInstHandles, sessionHandles, numInstances, numFlows);

  /* Create Polling Threads */
  pthread_t thread;
  mp_thread_args *args = NULL;
  args = (mp_thread_args *)malloc(sizeof(mp_thread_args));
  args->dcInstHandle = dcInstHandles;
  args->numDcInstances = numInstances;
  args->id = 0;
  createThreadPinned(&thread,dc_multipolling, (void *)args, 5);

  /* Create submitting threads */
  pthread_t subThreads[numFlows];
  packet_stats **arrayOfPacketStatsArrayPointers[numFlows];
  pthread_barrier_t *pthread_barrier = NULL;
  pthread_barrier = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
  pthread_barrier_init(pthread_barrier, NULL, numFlows);
  for(int i=0; i<numFlows; i++){
    createCompressCrc64Submitter(
      dcInstHandles[i],
      sessionHandles[i],
      numOperations,
      bufferSize,
      &(arrayOfPacketStatsArrayPointers[i]),
      pthread_barrier,
      &(subThreads[i])
    );
  }

  /* Join Submitting Threads -- and terminate associated polling thread busy wait loops*/
  for(int i=0; i<numFlows; i++){
    pthread_join(subThreads[i], NULL);
    gPollingDcs[i] = 0;
  }

  /*  Print Stats */
  printf("------------\nHW Offload Performance Test (Polling for Responses from a Single Physical Core)");
  printMultiThreadStats(arrayOfPacketStatsArrayPointers, numFlows, numOperations, bufferSize);
}

CpaStatus functionalCompressAndCrc64(CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle){
  CpaStatus status = CPA_STATUS_SUCCESS;
  Cpa8U *pBufferMetaDst2 = NULL;
  Cpa32U bufferMetaSize = 0;
  CpaBufferList *pBufferListSrc = NULL;
  CpaBufferList *pBufferListDst = NULL;
  CpaBufferList *pBufferListDst2 = NULL;
  CpaDcOpData opData = {};
  Cpa8U *sampleData = NULL;
  Cpa32U sampleDataSize = 512;
  CpaDcInstanceCapabilities cap = {0};
  CpaDcHuffType huffType = CPA_DC_HT_FULL_DYNAMIC;
  if(CPA_STATUS_SUCCESS != prepareSampleBuffer((Cpa8U **)&sampleData, sampleDataSize)){
    fprintf(stderr, "Failed to prepare sample buffer\n");
    return CPA_STATUS_FAIL;;
  }

  Cpa32U bufferSize = sampleDataSize;
  Cpa32U dstBufferSize = bufferSize;
  Cpa64U checksum = 0;

  CpaDcRqResults dcResults;
  struct COMPLETION_STRUCT complete;
  INIT_OPDATA(&opData, CPA_DC_FLUSH_FINAL);

  status =
    createSourceBufferList(&pBufferListSrc, sampleData, sampleDataSize, dcInstHandle, huffType);

  status =
    createDstBufferList(&pBufferListDst, sampleDataSize, dcInstHandle, huffType);

  if(CPA_STATUS_SUCCESS != status){
    fprintf(stderr, "Failed to create source buffer list\n");
    return CPA_STATUS_FAIL;;
  }

  if(CPA_STATUS_SUCCESS == status){

  /*
    * Now, we initialize the completion variable which is used by the
    * callback
    * function to indicate that the operation is complete.  We then perform
    * the
    * operation.
    */
  PRINT_DBG("cpaDcCompressData2\n");

  //<snippet name="perfOp">
  COMPLETION_INIT(&complete);

  /* enable integrityCrcCheck */
  cpaDcQueryCapabilities(dcInstHandle, &cap);
  CpaCrcData crcData = {0};
  if(cap.integrityCrcs64b == CPA_TRUE ){
    PRINT_DBG("Integrity CRC is enabled\n");
    opData.integrityCrcCheck = CPA_TRUE;
    opData.pCrcData = &crcData;
  }

  status = cpaDcCompressData2(
      dcInstHandle,
      sessionHandle,
      pBufferListSrc,     /* source buffer list */
      pBufferListDst,     /* destination buffer list */
      &opData,            /* Operational data */
      &dcResults,         /* results structure */
      (void *)&complete); /* data sent as is to the callback function*/
                              //</snippet>

  if (CPA_STATUS_SUCCESS != status)
  {
      PRINT_ERR("cpaDcCompressData2 failed. (status = %d)\n", status);
  }

  /*
    * We now wait until the completion of the operation.  This uses a macro
    * which can be defined differently for different OSes.
    */
  if (CPA_STATUS_SUCCESS == status)
  {
      if (!COMPLETION_WAIT(&complete, 5000))
      {
          PRINT_ERR("timeout or interruption in cpaDcCompressData2\n");
          status = CPA_STATUS_FAIL;
      }
  }

  /*
    * We now check the results
    */
  if (CPA_STATUS_SUCCESS == status)
  {
    if (dcResults.status != CPA_DC_OK)
    {
        PRINT_ERR("Results status not as expected (status = %d)\n",
                  dcResults.status);
        status = CPA_STATUS_FAIL;
    }
    else
    {
        PRINT_DBG("Data consumed %d\n", dcResults.consumed);
        PRINT_DBG("Data produced %d\n", dcResults.produced);
        if(cap.integrityCrcs64b == CPA_TRUE){
          PRINT_DBG("Uncompressed CRC64 0x%lx\n", crcData.integrityCrc64b.iCrc);
          PRINT_DBG("Compressed CRC64 0x%lx\n", crcData.integrityCrc64b.oCrc);
        } else {
          PRINT_DBG("Adler checksum 0x%x\n", dcResults.checksum);
        }
    }
    /* To compare the checksum with decompressed output */
    checksum = crcData.integrityCrc64b.iCrc;
  }
}
/*
    * We now ensure we can decompress to the original buffer.
    */
  if (CPA_STATUS_SUCCESS == status)
  {
      /* Dst is now the Src buffer - update the length with amount of
          compressed data added to the buffer */
      pBufferListDst->pBuffers->dataLenInBytes = dcResults.produced;

      /* Allocate memory for new destination bufferList Dst2, we can use
        * stateless decompression here because in this scenario we know
        * that all transmitted data before compress was less than some
        * max size */
      status = createDstBufferList(&pBufferListDst2, dstBufferSize, dcInstHandle, huffType);


      if (CPA_STATUS_SUCCESS == status)
      {


          PRINT_DBG("cpaDcDecompressData2\n");

          CpaCrcData dCrcData = {0};

          if(cap.integrityCrcs64b == CPA_TRUE ){
            opData.integrityCrcCheck = CPA_TRUE;
            opData.pCrcData = &dCrcData;
          }


          //<snippet name="perfOpDecomp">
          status = cpaDcDecompressData2(
              dcInstHandle,
              sessionHandle,
              pBufferListDst,  /* source buffer list */
              pBufferListDst2, /* destination buffer list */
              &opData,
              &dcResults, /* results structure */
              (void
                    *)&complete); /* data sent as is to the callback function*/
                                  //</snippet>

          if (CPA_STATUS_SUCCESS != status)
          {
              PRINT_ERR("cpaDcDecompressData2 failed. (status = %d)\n",
                        status);
          }

          /*
            * We now wait until the completion of the operation.  This uses a
            * macro
            * which can be defined differently for different OSes.
            */
          if (CPA_STATUS_SUCCESS == status)
          {
              if (!COMPLETION_WAIT(&complete, TIMEOUT_MS))
              {
                  PRINT_ERR(
                      "timeout or interruption in cpaDcDecompressData2\n");
                  status = CPA_STATUS_FAIL;
              }
          }

          /*
            * We now check the results
            */
          if (CPA_STATUS_SUCCESS == status)
          {
              if (dcResults.status != CPA_DC_OK)
              {
                  PRINT_ERR(
                      "Results status not as expected decomp (status = %d)\n",
                      dcResults.status);
                  status = CPA_STATUS_FAIL;
              }
              else
              {
                  PRINT_DBG("Data consumed %d\n", dcResults.consumed);
                  PRINT_DBG("Data produced %d\n", dcResults.produced);
                  if(cap.integrityCrcs64b == CPA_TRUE){
                    PRINT_DBG("Uncompressed CRC64 0x%lx\n", dCrcData.integrityCrc64b.oCrc);
                    PRINT_DBG("Compressed CRC64 0x%lx\n", dCrcData.integrityCrc64b.iCrc);
                  } else {
                    PRINT_DBG("Adler checksum 0x%x\n", dcResults.checksum);
                  }
              }

              /* Compare with original Src buffer */
              if (0 == memcmp(pBufferListDst2->pBuffers[0].pData, pBufferListSrc->pBuffers[0].pData, sampleDataSize))
              {
                  PRINT_DBG("Output matches expected output\n");
              }
              else
              {
                  PRINT_ERR("Output does not match expected output\n");
                  status = CPA_STATUS_FAIL;
              }
              if (checksum == dCrcData.integrityCrc64b.oCrc)
              {
                  PRINT_DBG("Checksums match after compression and "
                            "decompression\n");
              }
              else
              {
                  PRINT_ERR("Checksums does not match after compression and "
                            "decompression\n");
                  status = CPA_STATUS_FAIL;
              }
          }
      }
  }
  COMPLETION_DESTROY(&complete);


}