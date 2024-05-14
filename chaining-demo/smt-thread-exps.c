#include "smt-thread-exps.h"

CpaStatus createThreadPinned(pthread_t *thread, void *func, void *arg, int coreId){
  pthread_attr_t attr;
  cpu_set_t cpuset;
  struct sched_param param;

  int status = pthread_attr_init(&attr);

  if(status != 0){
    PRINT_DBG( "Error initializing thread attributes\n");
    return CPA_STATUS_FAIL;
  }

  CPU_ZERO(&cpuset);
  CPU_SET(coreId, &cpuset);
  status = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
  if(status != 0){
    PRINT_DBG( "Error setting thread affinity\n");
    pthread_attr_destroy(&attr);
    return CPA_STATUS_FAIL;
  }

  status = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  if(status != 0){
    PRINT_DBG( "Error setting thread scheduling inheritance\n");
    pthread_attr_destroy(&attr);
    return CPA_STATUS_FAIL;
  }

  if (pthread_attr_setschedpolicy(
        &attr, SCHED_RR) != 0)
  {
      PRINT_DBG(
              "Failed to set scheduling policy for thread!\n");
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }

  memset(&param, 0, sizeof(param));
  param.sched_priority = 15;
  if (pthread_attr_setschedparam(&attr, &param) != 0)
  {
      PRINT_DBG(
              "Failed to set the sched parameters attribute!\n");
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }

  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0)
  {
      PRINT_DBG(
              "Failed to set the dettachState attribute!\n");
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }
  if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM) != 0)
  {
      PRINT_DBG("Failed to set the attribute!\n");
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }
  if(pthread_create(thread, &attr, func, arg) != 0){
    PRINT_DBG( "Error creating thread\n");
    PRINT_DBG( "Creating thread with NULL attributes\n");
    pthread_create(thread, NULL, func, arg);
  }
}

CpaStatus createThreadPinnedNotJoinable(pthread_t *thread, void *func, void *arg, int coreId){
  pthread_attr_t attr;
  cpu_set_t cpuset;
  struct sched_param param;

  int status = pthread_attr_init(&attr);

  if(status != 0){
    PRINT_DBG( "Error initializing thread attributes\n");
    return CPA_STATUS_FAIL;
  }

  CPU_ZERO(&cpuset);
  CPU_SET(coreId, &cpuset);
  status = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
  if(status != 0){
    PRINT_DBG( "Error setting thread affinity\n");
    pthread_attr_destroy(&attr);
    return CPA_STATUS_FAIL;
  }

  status = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  if(status != 0){
    PRINT_DBG( "Error setting thread scheduling inheritance\n");
    pthread_attr_destroy(&attr);
    return CPA_STATUS_FAIL;
  }

  if (pthread_attr_setschedpolicy(
        &attr, SCHED_RR) != 0)
  {
      PRINT_DBG(
              "Failed to set scheduling policy for thread!\n");
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }

  memset(&param, 0, sizeof(param));
  param.sched_priority = 15;
  if (pthread_attr_setschedparam(&attr, &param) != 0)
  {
      PRINT_DBG(
              "Failed to set the sched parameters attribute!\n");
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }

  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0)
  {
      PRINT_DBG(
              "Failed to set the dettachState attribute!\n");
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }
  if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM) != 0)
  {
      PRINT_DBG("Failed to set the attribute!\n");
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }
  if(pthread_create(thread, &attr, func, arg) != 0){
    PRINT_DBG( "Error creating thread\n");
    PRINT_DBG( "Creating thread with NULL attributes\n");
    pthread_create(thread, NULL, func, arg);
  }
}



void createCompressCrc64SubmitterPinned(
  CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  packet_stats ***pArrayPacketStatsPtrs,
  pthread_barrier_t *pthread_barrier,
  pthread_t *threadId,
  int coreId
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
  createThreadPinned(threadId, streamFn, (void *)submitterArgs, coreId);
}

void createCompressCrc64PollerPinned(CpaInstanceHandle dcInstHandle, Cpa16U id,
pthread_t *threadId, int coreId){
  CpaInstanceInfo2 info2 = {0};
  CpaStatus status = cpaDcInstanceGetInfo2(dcInstHandle, &info2);
  if ((status == CPA_STATUS_SUCCESS) && (info2.isPolled == CPA_TRUE))
  {
      /* Start thread to poll instance */
      thread_args *args = NULL;
      args = (thread_args *)malloc(sizeof(thread_args));
      args->dcInstHandle = dcInstHandle;
      args->id = id;
      createThreadPinned(threadId, dc_polling, (void *)args, coreId);
  }
}

void multiStreamCompressCrc64PerformanceTestSharedSMTThreads(
  Cpa32U numFlows,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  CpaInstanceHandle *dcInstHandles,
  CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances
)
{
  int startSubmitLogical=5;
  int startPollLogical=45;
  /* multiple instance for latency test */
  prepareMultipleCompressAndCrc64InstancesAndSessions(dcInstHandles, sessionHandles, numInstances, numFlows);

  /* Create Polling Threads */
  pthread_t thread[numFlows];
  for(int i=0; i<numFlows; i++){
    createCompressCrc64PollerPinned(dcInstHandles[i], i, &(thread[i]), startPollLogical + i);
  }

  /* Create submitting threads */
  pthread_t subThreads[numFlows];
  packet_stats **arrayOfPacketStatsArrayPointers[numFlows];
  pthread_barrier_t *pthread_barrier = NULL;
  pthread_barrier = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
  pthread_barrier_init(pthread_barrier, NULL, numFlows);
  for(int i=0; i<numFlows; i++){
    createCompressCrc64SubmitterPinned(
      dcInstHandles[i],
      sessionHandles[i],
      numOperations,
      bufferSize,
      &(arrayOfPacketStatsArrayPointers[i]),
      pthread_barrier,
      &(subThreads[i]),
      startSubmitLogical + i
    );
  }

  /* Join Submitting Threads -- and terminate associated polling thread busy wait loops*/
  for(int i=0; i<numFlows; i++){
    pthread_join(subThreads[i], NULL);
    gPollingDcs[i] = 0;
  }

  /*  Print Stats */
  printf("------------\nHW Offload Shared SMT Performance Test");
  printMultiThreadStats(arrayOfPacketStatsArrayPointers, numFlows, numOperations, bufferSize);
}

void multiStreamCompressCrc64PerformanceTestSharedMultiSwPerHwTd(
  Cpa32U numFlows,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  CpaInstanceHandle *dcInstHandles,
  CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances,
  int numTdsPerHwTd
)
{
  int startSubmitLogical=5;
  int startPollLogical=45;
  /* multiple instance for latency test */
  prepareMultipleCompressAndCrc64InstancesAndSessions(dcInstHandles, sessionHandles, numInstances, numFlows);

  /* Create Polling Threads */
  pthread_t thread[numFlows];
  for(int i=0; i<numFlows; i++){
    int logCore = startPollLogical + (i/numTdsPerHwTd);
    printf("Assigning Poller to %d\n",logCore);
    createCompressCrc64PollerPinned(dcInstHandles[i], i, &(thread[i]), logCore);
  }

  /* Create submitting threads */
  pthread_t subThreads[numFlows];
  packet_stats **arrayOfPacketStatsArrayPointers[numFlows];
  pthread_barrier_t *pthread_barrier = NULL;
  pthread_barrier = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
  pthread_barrier_init(pthread_barrier, NULL, numFlows);
  for(int i=0; i<numFlows; i++){
    int logCore = startSubmitLogical + i/numTdsPerHwTd;
    printf("Assigning Submitter to %d\n",logCore);
    createCompressCrc64SubmitterPinned(
      dcInstHandles[i],
      sessionHandles[i],
      numOperations,
      bufferSize,
      &(arrayOfPacketStatsArrayPointers[i]),
      pthread_barrier,
      &(subThreads[i]),
      logCore
    );
  }

  /* Join Submitting Threads -- and terminate associated polling thread busy wait loops*/
  for(int i=0; i<numFlows; i++){
    pthread_join(subThreads[i], NULL);
    gPollingDcs[i] = 0;
  }

  /*  Print Stats */
  printf("------------\nHW Offload Shared Num Kernel Threads Per SMT Thread:%d ", numTdsPerHwTd);
  printMultiThreadStats(arrayOfPacketStatsArrayPointers, numFlows, numOperations, bufferSize);
}


CpaStatus compCrcStreamSharedPhysSubmitPollDcPollCrc(Cpa32U numOperations,
  Cpa32U bufferSize,
  struct acctest_context *ogDsa, int tflags,
  CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle,
  CpaDcInstanceCapabilities cap,
  int flowId,
  two_stage_packet_stats ***pStats,
  pthread_barrier_t *barrier,
  Cpa32U coreId)
{
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  callback_args **cb_args = NULL;
  CpaDcOpData **opData = NULL;
  packet_stats **dummyStats = NULL; /* to appeaase multiBufferTestAllocations*/
  struct acctest_context *dsa = NULL;
  struct COMPLETION_STRUCT complete;

  two_stage_packet_stats **stats2Phase = NULL;
  dc_crc_polling_args *dcCrcArgs = NULL;
  crc_polling_args *crcArgs = NULL;
  dsa_fwder_args **args;

  pthread_t crcTid, dcToCrcTid;
  struct task_node *waitTaskNode;

  dsa = acctest_init(tflags);
  if(NULL == dsa){
    return CPA_STATUS_FAIL;
  }
  int rc = acctest_duplicate_context(dsa, ogDsa);

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

    /* Create args, stats packet stats, and dsa descs for the callback function to submit*/
  create_tsk_nodes_for_stage2_offload(srcBufferLists, numOperations, dsa);
  rc =alloc_crc_test_packet_stats(
    dsa,
    &stats2Phase,
    numOperations
  );
  if(rc != CPA_STATUS_SUCCESS){
    return CPA_STATUS_FAIL;
  }
  rc = prep_crc_test_cb_fwd_args(
    &args,
    dsa,
    stats2Phase,
    numOperations
  );
  if(rc != CPA_STATUS_SUCCESS){
    return CPA_STATUS_FAIL;
  }
  /* Create polling thread for DSA */
  create_crc_polling_thread_pinned(dsa, flowId, stats2Phase, &crcTid, coreId);

  /* Create intermediate polling thread for forwarding */
  create_dc_polling_thread_pinned( flowId, &dcToCrcTid, dcInstHandle, coreId);

  pthread_barrier_wait(barrier);

  rc = submit_all_comp_crc_requests(
    numOperations,
    dcInstHandle,
    sessionHandle,
    srcBufferLists,
    dstBufferLists,
    opData,
    dcResults,
    args);

  pthread_join(crcTid, NULL);
  gPollingDcs[flowId] = 0;

  /* verify all crcs */
  rc = verifyCrcTaskNodes(dsa->multi_task_node, srcBufferLists, bufferSize);

  /* print stats */
  threadStats2P *thrStats = NULL;
  populate2PhaseThreadStats(stats2Phase, &thrStats, numOperations, bufferSize, flowId);

  char filename[256];
  sprintf(filename, "sw-managed-hw-deflate-hw-crc32_bufsize_%d", bufferSize);
  logLatencies2Phase(stats2Phase, numOperations, filename );
  COMPLETION_DESTROY(&complete);

  if( CPA_STATUS_SUCCESS != rc ){
    PRINT_ERR("Invalid CRC\n");
  }
  *pStats = stats2Phase;
}


void *compCrcStreamSharedSubmitPollCrcPollDcThreadFn(void *args){
  compCrcStreamThreadArgs *threadArgs = (compCrcStreamThreadArgs *)args;
  compCrcStreamSharedPhysSubmitPollDcPollCrc(threadArgs->numOperations, threadArgs->bufferSize,
    threadArgs->dsa, threadArgs->tflags, threadArgs->dcInstHandle,
    threadArgs->sessionHandle, threadArgs->cap, threadArgs->flowId,
    threadArgs->pStats, threadArgs->barrier,
    threadArgs->coreId);
}

CpaStatus cpaDcDsaCrcPerfSharedSubmitPollCrcPollDcPerf(
  Cpa32U numOperations,
  Cpa32U bufferSize ,
  int numFlows,
  CpaInstanceHandle *dcInstHandles,
  CpaDcSessionHandle* sessionHandles
){

  int startLogical = 5; /* start logical */
  /* one time shared DSA setup */
  /* sudo ..//setup_dsa.sh -d dsa0 -w1 -ms -e4 */
  struct acctest_context *dsa = NULL;
  int tflags = TEST_FLAGS_BOF;
  int rc;
  int wq_type = ACCFG_WQ_SHARED;
  int dev_id = 0;
  int wq_id = 0;
  int opcode = 16;
  struct task *tsk;

  CpaDcInstanceCapabilities cap = {0};

  /* Arrays of packetstat array pointers for access to per-stream stats */
  two_stage_packet_stats **streamStats[numFlows];

  /* setup the shared dsa */
  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;

  if (!dsa)
		return -ENOMEM;

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
	if (rc < 0)
		return -ENOMEM;

  cpaDcQueryCapabilities(dcInstHandles[0], &cap);
  pthread_barrier_t barrier;
  pthread_t streamTds[numFlows];
  pthread_barrier_init(&barrier, NULL, numFlows);
  for(int flowId=0; flowId<numFlows; flowId++){
    /* prepare per flow dc inst/sess*/
    prepareDcInst(&dcInstHandles[flowId]);
    prepareDcSession(dcInstHandles[flowId], &sessionHandles[flowId], dcDsaCrcCallback);

    compCrcStreamThreadArgs *args;
    OS_MALLOC(&args, sizeof(compCrcStreamThreadArgs));
    populateCrcStreamThreadArgs(args, numOperations, bufferSize,
      dsa, tflags, dcInstHandles[flowId], sessionHandles[flowId], cap,
      flowId, &(streamStats[flowId]), &barrier);

    /* start the comp streams */
    createThreadPinned(&streamTds[flowId], compCrcStreamSharedSubmitPollCrcPollDcThreadFn, args, startLogical + flowId);

  }
  for(int flowId=0; flowId<numFlows; flowId++){
    pthread_join(streamTds[flowId], NULL);
  }


  printf("------------\nSw AxChain Offload Performance Test\n");
  printTwoPhaseMultiThreadStatsSummary(streamStats, numFlows, numOperations, bufferSize, CPA_FALSE);
}

void create_crc_polling_thread_pinned(struct acctest_context *dsa, int id, two_stage_packet_stats **stats, pthread_t *tid, Cpa32U coreId){
  crc_polling_args *crcArgs;
  OS_MALLOC(&crcArgs, sizeof(crc_polling_args));
  crcArgs->dsa=dsa;
  crcArgs->id = id;
  crcArgs->stats = stats;
  createThreadPinned(tid,crc_polling, crcArgs, coreId);
}

void create_dc_polling_thread_pinned(int flowId,
  pthread_t *tid, CpaInstanceHandle dcInstHandle, Cpa32U coreId)
{
  dc_crc_polling_args *dcCrcArgs = NULL;
  OS_MALLOC(&dcCrcArgs, sizeof(dc_crc_polling_args));
  dcCrcArgs->dcInstance = dcInstHandle;
  dcCrcArgs->id = flowId;
  createThreadPinned(tid, dc_crc64_polling, dcCrcArgs, coreId);
}