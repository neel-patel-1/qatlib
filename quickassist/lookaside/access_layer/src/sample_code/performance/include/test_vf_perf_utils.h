#ifndef _TEST_VF_PERF_UTILS
#define _TEST_VF_PERF_UTILS
#include "icp_sal_poll.h"

#define FAIL_ON(cond, args...) if (cond) { PRINT(args); return CPA_STATUS_FAIL; }
#define DEFAULT_POLL_INTERVAL_NSEC (2100)

extern CpaInstanceHandle *dcInstances_g;
extern pthread_t *dcPollingThread_g;
extern Cpa16U numDcInstances_g;
extern CpaInstanceInfo2 *instanceInfo2;
extern volatile CpaBoolean dc_service_started_g;
extern CpaBufferList *** pInterBuffList_g;
extern Cpa32U expansionFactor_g;
extern Cpa32U coreLimit_g;

struct pollParams_t
{
    CpaInstanceHandle instanceHandle;

};

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


void busyPollFn(CpaInstanceHandle instanceHandle_in){
  CpaStatus status = CPA_STATUS_FAIL;
  struct timespec reqTime, remTime;
  reqTime.tv_sec = 0;
  reqTime.tv_nsec = DEFAULT_POLL_INTERVAL_NSEC;
  while(dc_service_started_g == CPA_TRUE){
    status = icp_sal_DcPollInstance(instanceHandle_in, 0);
    if (CPA_STATUS_SUCCESS == status || CPA_STATUS_RETRY == status)
    {
    }
    else {
        PRINT_ERR("ERROR icp_sal_DcPollInstance returned status %d\n",
                  status);
        break;
    }
    nanosleep(&reqTime, &remTime);
  }
}

void epollFn(){
    
}

CpaStatus sampleCodeThreadCreate(sample_code_thread_t *thread,
                                 sample_code_thread_attr_t *threadAttr,
                                 performance_func_t function,
                                 void *params)
{
  pthread_attr_t attr;
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
  status = pthread_create(&thread, &attr, (void *(*)(void *))function, params);
  if (status != 0)
  {
      PRINT_ERR("%d\n", errno);
      pthread_attr_destroy(&attr);
      return CPA_STATUS_FAIL;
  }

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
    sampleCodeThreadCreate(
        &dcPollingThread_g[numCreatedPollingThreads],
        NULL,
        pollFnArr[i],
        dcInstances_g[i]
    );
    coreAffinity = getCoreAffinityFromInstanceIndex(i);
    threadBind(&dcPollingThread_g[i], coreAffinity);
    numCreatedPollingThreads++;
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
  dcPollingThread_g =
            qaeMemAlloc(numDcInstances_g * sizeof(sample_code_thread_t));
  /* Create the polling threads */
  for(i = 0; i < numDcInstances_g; i++){
    coreAffinity = getCoreAffinityFromInstanceIndex(i);
    status = sampleCodeThreadCreate(
                    &dcPollingThread_g[numCreatedPollingThreads],
                    NULL,
                    pollFnArr[i],
                    dcInstances_g[i]);
    CHECK_POINTER_AND_RETURN_FAIL_IF_NULL(&dcPollingThread_g[numCreatedPollingThreads]);   
    numCreatedPollingThreads++;             
  }
  return CPA_STATUS_SUCCESS;
}
CpaStatus setCoreLimit(Cpa32U limit)
{
    Cpa32U nProcessorsOnline = sampleCodeGetNumberOfCpus();
    if (limit > nProcessorsOnline)
    {
        PRINT_ERR("exceeds number of cores (%u) on system\n",
                  nProcessorsOnline);
        return CPA_STATUS_FAIL;
    }
    coreLimit_g = limit;
    return CPA_STATUS_SUCCESS;
}
CpaStatus sampleCodeDcGetNode(CpaInstanceHandle instanceHandle, Cpa32U *node)
{
    CpaStatus status = CPA_STATUS_FAIL;
    CpaInstanceInfo2 pInstanceInfo2;
    status = cpaDcInstanceGetInfo2(instanceHandle, &pInstanceInfo2);
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to get Node affinity\n");
        return status;
    }
    *node = pInstanceInfo2.nodeAffinity;
    return status;
}
Cpa32U sampleCodeGetNumberOfCpus(void)
{
    int numCpus = 0;
    numCpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (numCpus < 0)
    {
        PRINT_ERR("Failed to get online processors count %d\n", errno);
        numCpus = 0;
    }
    return numCpus;
}

CpaStatus startDcServices(Cpa32U buffSize, Cpa32U numBuffs){
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U size = 0;
    Cpa32U i = 0, k = 0;
    Cpa32U nodeId = 0;
    Cpa32U nProcessorsOnline = 0;
    Cpa16U numBuffers = 0;
    CpaBufferList **tempBufferList = NULL;

    /*if the service started flag is false*/
    if (dc_service_started_g == CPA_FALSE)
    {
        /* Get the number of DC Instances */
        status = cpaDcGetNumInstances(&numDcInstances_g);
        /* Check the status */
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to Get Number of DC instances\n");
            return CPA_STATUS_FAIL;
        }
        /* Check if at least one DC instance are present */
        if (0 == numDcInstances_g)
        {
            PRINT_ERR(" DC Instances are not present\n");
            return CPA_STATUS_FAIL;
        }
        /* Allocate memory for all the instances */
        dcInstances_g =
            qaeMemAlloc(sizeof(CpaInstanceHandle) * numDcInstances_g);
        /* Check For NULL */
        if (NULL == dcInstances_g)
        {
            PRINT_ERR(" Unable to allocate memory for Instances \n");
            return CPA_STATUS_FAIL;
        }

        /* Get DC Instances */
        status = cpaDcGetInstances(numDcInstances_g, dcInstances_g);
        /* Check Status */
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to Get DC instances\n");
            qaeMemFree((void **)&dcInstances_g);
            return CPA_STATUS_FAIL;
        }

        /* Allocate the buffer list pointers to the number of Instances
         * this buffer list list is used only in case of dynamic
         * compression
         */
        pInterBuffList_g = (CpaBufferList ***)qaeMemAlloc(
            numDcInstances_g * sizeof(CpaBufferList **));
        /* Check For NULL */
        if (NULL == pInterBuffList_g)
        {
            PRINT_ERR("Unable to allocate dynamic buffer List\n");
            qaeMemFree((void **)&dcInstances_g);
            return CPA_STATUS_FAIL;
        }

        /* Initialize memory for buffer lists */
        memset(
            pInterBuffList_g, 0, numDcInstances_g * sizeof(CpaBufferList **));

        /* Start the Loop to create Buffer List for each instance*/
        for (i = 0; i < numDcInstances_g; i++)
        {
            /* get the Node ID for each instance Handle */
            status = sampleCodeDcGetNode(dcInstances_g[i], &nodeId);
            if (CPA_STATUS_SUCCESS != status)
            {
                PRINT_ERR("Unable to get NodeId\n");
                qaeMemFree((void **)&dcInstances_g);
                qaeMemFree((void **)&pInterBuffList_g);
                return CPA_STATUS_FAIL;
            }
            status =
                cpaDcGetNumIntermediateBuffers(dcInstances_g[i], &numBuffers);
            if (CPA_STATUS_SUCCESS != status)
            {
                PRINT_ERR("Unable to allocate Memory for Dynamic Buffer\n");
                qaeMemFree((void **)&dcInstances_g);
                qaeMemFree((void **)&pInterBuffList_g);
                return CPA_STATUS_FAIL;
            }
            if (numBuffers > 0)
            {
                /* allocate the buffer list memory for the dynamic Buffers
                 * only applicable for CPM prior to gen4 as it is done in HW */
                pInterBuffList_g[i] =
                    qaeMemAllocNUMA(sizeof(CpaBufferList *) * numBuffers,
                                    nodeId,
                                    BYTE_ALIGNMENT_64);
                if (NULL == pInterBuffList_g[i])
                {
                    PRINT_ERR("Unable to allocate Memory for Dynamic Buffer\n");
                    qaeMemFree((void **)&dcInstances_g);
                    qaeMemFree((void **)&pInterBuffList_g);
                    return CPA_STATUS_FAIL;
                }

                /* get the size of the Private meta data
                 * needed to create Buffer List
                 */
                status = cpaDcBufferListGetMetaSize(
                    dcInstances_g[i], numBuffers, &size);
                if (CPA_STATUS_SUCCESS != status)
                {
                    PRINT_ERR("Get Meta Size Data Failed\n");
                    qaeMemFree((void **)&dcInstances_g);
                    qaeMemFree((void **)&pInterBuffList_g);
                    return CPA_STATUS_FAIL;
                }
            }
            tempBufferList = pInterBuffList_g[i];
            for (k = 0; k < numBuffers; k++)
            {
                tempBufferList[k] = (CpaBufferList *)qaeMemAllocNUMA(
                    sizeof(CpaBufferList), nodeId, BYTE_ALIGNMENT_64);
                if (NULL == tempBufferList[k])
                {
                    PRINT(" %s:: Unable to allocate memory for "
                          "tempBufferList\n",
                          __FUNCTION__);
                    qaeMemFree((void **)&dcInstances_g);
                    freeDcBufferList(tempBufferList, k + 1);
                    qaeMemFree((void **)&pInterBuffList_g);
                    return CPA_STATUS_FAIL;
                }
                tempBufferList[k]->pPrivateMetaData =
                    qaeMemAllocNUMA(size, nodeId, BYTE_ALIGNMENT_64);
                if (NULL == tempBufferList[k]->pPrivateMetaData)
                {
                    PRINT(" %s:: Unable to allocate memory for "
                          "pPrivateMetaData\n",
                          __FUNCTION__);
                    qaeMemFree((void **)&dcInstances_g);
                    freeDcBufferList(tempBufferList, k + 1);
                    qaeMemFree((void **)&pInterBuffList_g);
                    return CPA_STATUS_FAIL;
                }
                tempBufferList[k]->numBuffers = ONE_BUFFER_DC;
                /* allocate flat buffers */
                tempBufferList[k]->pBuffers = qaeMemAllocNUMA(
                    (sizeof(CpaFlatBuffer)), nodeId, BYTE_ALIGNMENT_64);
                if (NULL == tempBufferList[k]->pBuffers)
                {
                    PRINT_ERR("Unable to allocate memory for pBuffers\n");
                    qaeMemFree((void **)&dcInstances_g);
                    freeDcBufferList(tempBufferList, k + 1);
                    qaeMemFree((void **)&pInterBuffList_g);
                    return CPA_STATUS_FAIL;
                }

                tempBufferList[k]->pBuffers[0].pData = qaeMemAllocNUMA(
                    (size_t)expansionFactor_g * EXTRA_BUFFER * buffSize,
                    nodeId,
                    BYTE_ALIGNMENT_64);
                if (NULL == tempBufferList[k]->pBuffers[0].pData)
                {
                    PRINT_ERR("Unable to allocate Memory for pBuffers\n");
                    qaeMemFree((void **)&dcInstances_g);
                    freeDcBufferList(tempBufferList, k + 1);
                    qaeMemFree((void **)&pInterBuffList_g);
                    return CPA_STATUS_FAIL;
                }
                tempBufferList[k]->pBuffers[0].dataLenInBytes =
                    expansionFactor_g * EXTRA_BUFFER * buffSize;
            }

            /* When starting the DC Instance, the API expects that the
             * private meta data should be greater than the dataLength
             */
            /* Configure memory Configuration Function */
            status = cpaDcSetAddressTranslation(
                dcInstances_g[i], (CpaVirtualToPhysical)qaeVirtToPhysNUMA);
            if (CPA_STATUS_SUCCESS != status)
            {
                PRINT_ERR("Error setting memory config for instance\n");
                qaeMemFree((void **)&dcInstances_g);
                freeDcBufferList(pInterBuffList_g[i], numBuffers);
                qaeMemFreeNUMA((void **)&pInterBuffList_g[i]);
                qaeMemFree((void **)&pInterBuffList_g);
                return CPA_STATUS_FAIL;
            }
            /* Start DC Instance */
            status = cpaDcStartInstance(
                dcInstances_g[i], numBuffers, pInterBuffList_g[i]);
            if (CPA_STATUS_SUCCESS != status)
            {
                PRINT_ERR("Unable to start DC Instance\n");
                qaeMemFree((void **)&dcInstances_g);
                freeDcBufferList(pInterBuffList_g[i], numBuffers);
                qaeMemFreeNUMA((void **)&pInterBuffList_g[i]);
                qaeMemFree((void **)&pInterBuffList_g);
                return CPA_STATUS_FAIL;
            }
        }
        /*set the started flag to true*/
        dc_service_started_g = CPA_TRUE;
    }

    /*determine number of cores on system and limit the number of cores to be
     * used to be the smaller of the numberOf Instances or the number of cores*/
    nProcessorsOnline = sampleCodeGetNumberOfCpus();
    if (nProcessorsOnline > numDcInstances_g)
    {
        setCoreLimit(numDcInstances_g);
    }
    return status;
}

#endif