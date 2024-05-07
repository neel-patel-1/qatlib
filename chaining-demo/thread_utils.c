#include "thread_utils.h"

_Atomic int gPollingDcs[MAX_INSTANCES];



void *dc_polling(void *args){
  thread_args *targs = (thread_args *)args;
  CpaInstanceHandle dcInstHandle = targs->dcInstHandle;
  Cpa16U id = targs->id;
  gPollingDcs[id] = 1;
  while(gPollingDcs[id]){
    icp_sal_DcPollInstance(dcInstHandle, 0);
  }
  pthread_exit(NULL);
}

CpaStatus createThread(pthread_t *thread, void *func, void *arg){
  pthread_attr_t attr;
  struct sched_param param;

  int status = pthread_attr_init(&attr);
  if(status != 0){
    fprintf(stderr, "Error initializing thread attributes\n");
    return CPA_STATUS_FAIL;
  }
  status = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  if(status != 0){
    fprintf(stderr, "Error setting thread scheduling inheritance\n");
    pthread_attr_destroy(&attr);
    return CPA_STATUS_FAIL;
  }

  if (pthread_attr_setschedpolicy(
        &attr, SCHED_RR) != 0)
  {
      fprintf(stderr,
              "Failed to set scheduling policy for thread!\n");
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }

  memset(&param, 0, sizeof(param));
  param.sched_priority = 15;
  if (pthread_attr_setschedparam(&attr, &param) != 0)
  {
      fprintf(stderr,
              "Failed to set the sched parameters attribute!\n");
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }

  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0)
  {
      fprintf(stderr,
              "Failed to set the dettachState attribute!\n");
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }
  if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM) != 0)
  {
      fprintf(stderr,"Failed to set the attribute!\n");
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }


  if(pthread_create(thread, &attr, func, arg) != 0){
    fprintf(stderr, "Error creating thread\n");
    fprintf(stderr, "Enable SCHED_RR Policy\n");
    fprintf(stderr, "Creating thread with default attributes\n");
    pthread_create(thread, NULL, func, arg);
    pthread_detach(*thread);
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