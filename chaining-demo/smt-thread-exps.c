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