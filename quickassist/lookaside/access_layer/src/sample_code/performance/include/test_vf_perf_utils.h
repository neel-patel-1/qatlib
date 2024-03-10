#ifndef _TEST_VF_PERF_UTILS
#define _TEST_VF_PERF_UTILS
#include "icp_sal_poll.h"

#define FAIL_ON(cond, args...) if (cond) { PRINT(args); return CPA_STATUS_FAIL; }

extern CpaInstanceHandle *dcInstances_g;
extern Cpa16U numDcInstances_g;
extern CpaInstanceInfo2 *instanceInfo2;


CpaStatus threadBind(sample_code_thread_t *thread, Cpa32U logicalCore)
{
    int status = 1;
    cpu_set_t cpuset;
    CHECK_POINTER_AND_RETURN_FAIL_IF_NULL(thread);
    CPU_ZERO(&cpuset);
    CPU_SET(logicalCore, &cpuset);

    status = pthread_setaffinity_np(*thread, sizeof(cpu_set_t), &cpuset);
    if (status != 0)
    {
        return CPA_STATUS_FAIL;
    }
    return CPA_STATUS_SUCCESS;
}

static void freeDcBufferList(CpaBufferList **buffListArray,
                             Cpa32U numberOfBufferList)
{
    Cpa32U i = 0, j = 0;
    Cpa32U numberOfBuffers = 0;

    i = numberOfBufferList;
    for (i = 0; i < numberOfBufferList; i++)
    {
        numberOfBuffers = buffListArray[i]->numBuffers;
        for (j = 0; j < numberOfBuffers; j++)
        {
            if (buffListArray[i]->pBuffers[j].pData != NULL)
            {
                qaeMemFreeNUMA((void **)&buffListArray[i]->pBuffers[j].pData);
                buffListArray[i]->pBuffers[j].pData = NULL;
            }
        }
        if (buffListArray[i]->pBuffers != NULL)
        {

            qaeMemFreeNUMA((void **)&buffListArray[i]->pBuffers);
            buffListArray[i]->pBuffers = NULL;
        }

        if (buffListArray[i]->pPrivateMetaData != NULL)
        {

            qaeMemFreeNUMA((void **)&buffListArray[i]->pPrivateMetaData);
        }
    }
}


void busyPollFn(){
}

void epollFn(){
    
}

CpaStatus createThreadCommon(int core, performance_func_t * pollFunc){
  printf("Creating thread on core %d\n", core);
  pthread_attr_t attr;
  pthread_t thread = core;
  struct sched_param param;
  int status = pthread_attr_init(&attr);

  if (status != 0)
  {
      PRINT_ERR("%d\n", errno);
      return CPA_STATUS_FAIL;
  }

  
  status = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  if (status != 0)
  {
      pthread_attr_destroy(&attr);
      PRINT_ERR("%d\n", errno);
      return CPA_STATUS_FAIL;
  }
  status = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  if (status != 0)
  {
      PRINT_ERR("%d\n", errno);
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }
  status = pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
  if (status != 0)
  {
      PRINT_ERR("%d\n", errno);
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }
  status = pthread_create(&thread, &attr, (void *(*)(void *))pollFunc, NULL);
  if (status != 0)
  {
      PRINT_ERR("%d\n", errno);
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }

  threadBind(&thread, core);
  /*destroy the thread attributes as they are no longer required, this does
     * not affect the created thread*/
  pthread_attr_destroy(&attr);
  return CPA_STATUS_SUCCESS;
 
}

/*
* @param instanceNum - index of instance
*/
Cpa32U getCoreAffinityFromInstanceIndex(Cpa32U instanceNum){
  Cpa32U coreAffinity = 0;
  for (int j = 0; j < CPA_MAX_CORES; j++)
  {
      if (CPA_BITMAP_BIT_TEST(instanceInfo2[j].coreAffinity, j))
      {
          coreAffinity = j;

          break;
      }
  }
  return coreAffinity;
}

/* Starts same number of Epoll Threads as there are globally registered instances*/
CpaStatus createEpollThreads(){
  int fd = -1;
  CpaInstanceInfo2 *instanceInfo2 = NULL;
  Cpa16U i = 0, j = 0, numCreatedPollingThreads = 0;
  Cpa32U coreAffinity = 0;
  CpaStatus status = CPA_STATUS_SUCCESS;
  performance_func_t *pollFnArr = NULL;
  pollFnArr = qaeMemAlloc(numDcInstances_g * sizeof(performance_func_t));
  for(i = 0; i< numDcInstances_g; i++){
    status = icp_sal_DcGetFileDescriptor(dcInstances_g[i], &fd);
    FAIL_ON(CPA_STATUS_SUCCESS != status, "Get File Descriptor Failed\n");
    pollFnArr[i] = epollFn;
    status = icp_sal_DcPutFileDescriptor(dcInstances_g[i], fd);
    if(status == CPA_STATUS_UNSUPPORTED){
      PRINT_ERR("Polling unsupported\n");
      return CPA_STATUS_FAIL;
    }
  }
  /* Create the polling threads */
  for(i = 0; i < numDcInstances_g; i++){
    coreAffinity = getCoreAffinityFromInstanceIndex(i);
    createThreadCommon(coreAffinity, pollFnArr[i]);
  }
  return CPA_STATUS_SUCCESS;
}

CpaStatus createBusyPollThreads(){
  int fd = -1;
  Cpa16U i = 0, j = 0, numCreatedPollingThreads = 0;
  Cpa32U coreAffinity = 0;
  CpaStatus status = CPA_STATUS_SUCCESS;
  performance_func_t *pollFnArr = NULL;
  pollFnArr = qaeMemAlloc(numDcInstances_g * sizeof(performance_func_t));
  for(i = 0; i< numDcInstances_g; i++){
    if(instanceInfo2[i].isPolled == CPA_TRUE){  
      pollFnArr[i] = busyPollFn;
    } else {
      PRINT_ERR("Polling unsupported\n");
      return CPA_STATUS_FAIL;
    }
  }
  /* Create the polling threads */
  for(i = 0; i < numDcInstances_g; i++){
    coreAffinity = getCoreAffinityFromInstanceIndex(i);
    createThreadCommon(coreAffinity, pollFnArr[i]);
  }
  return CPA_STATUS_SUCCESS;
}

#endif