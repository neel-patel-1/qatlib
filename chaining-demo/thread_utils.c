#include "thread_utils.h"

_Atomic int gPollingDcs[MAX_INSTANCES];
_Atomic int gPollingCrcs[MAX_DSAS];

void *dc_polling(void *args){
  thread_args *targs = (thread_args *)args;
  CpaInstanceHandle dcInstHandle = targs->dcInstHandle;
  Cpa16U id = targs->id;
  gPollingDcs[id] = 1;
  while(gPollingDcs[id]){
    icp_sal_DcPollInstance(dcInstHandle, 0);
  }
  free(targs);
  pthread_exit(NULL);
}

void *dc_crc64_polling(void *args){
  dc_crc_polling_args *dcArgs = (dc_crc_polling_args *)args;
  CpaInstanceHandle dcInstance = dcArgs->dcInstance;
  Cpa32U id = dcArgs->id;
  gPollingDcs[id] = 1;
  while(gPollingDcs[id] == 1){
    icp_sal_DcPollInstance(dcInstance, 0); /* Poll here, forward in callback */
  }
}

void *dc_multipolling(void *arg){
  mp_thread_args *args = (mp_thread_args *)arg;
  CpaInstanceHandle *dcInstHandle = args->dcInstHandle;
  Cpa32U numDcInstances = args->numDcInstances;
  Cpa16U id = args->id;
  gPollingDcs[id] = 1;
  while(gPollingDcs[id]){
    for(Cpa32U i = 0; i < numDcInstances; i++){
      icp_sal_DcPollInstance(dcInstHandle[i], 0);
    }
  }
}

CpaStatus createThread(pthread_t *thread, void *func, void *arg){
  pthread_attr_t attr;
  struct sched_param param;

  int status = pthread_attr_init(&attr);
  if(status != 0){
    PRINT_DBG( "Error initializing thread attributes\n");
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
    PRINT_DBG( "Enable SCHED_RR Policy\n");
    PRINT_DBG( "Creating thread with default attributes\n");
    pthread_create(thread, NULL, func, arg);
    pthread_detach(*thread);
  }

}

CpaStatus createThreadJoinable(pthread_t *thread, void *func, void *arg){
  pthread_attr_t attr;
  struct sched_param param;

  int status = pthread_attr_init(&attr);

  if(status != 0){
    PRINT_DBG( "Error initializing thread attributes\n");
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

void dcLatencyCallback(void *pCallbackTag, CpaStatus status){
  if(CPA_STATUS_SUCCESS != status){
    PRINT_ERR("Error in callback\n");
  }

  if(NULL != pCallbackTag){
    PRINT_DBG("Callback called\n");
    COMPLETE((struct COMPLETION_STRUCT *)pCallbackTag);
  }
}

void dcPerfCallback(void *pCallbackTag, CpaStatus status){
  if(CPA_STATUS_SUCCESS != status){
    PRINT_ERR("Error in callback\n");
  }

  if(NULL != pCallbackTag){
    callback_args *args = (callback_args *)pCallbackTag;
    Cpa32U completedOps = args->completedOperations;
    packet_stats *stats = args->stats[completedOps];

    stats->receiveTime = sampleCoderdtsc();
    args->completedOperations++;

    if(args->completedOperations == args->numOperations){
      PRINT_DBG("All operations completed\n");
      COMPLETE((struct COMPLETION_STRUCT *)(args->completion));
    }

  }
}

void dcCallback2(void *pCallbackTag, CpaStatus status){
  int *completed = (int *)pCallbackTag;
  *completed += 1;
}