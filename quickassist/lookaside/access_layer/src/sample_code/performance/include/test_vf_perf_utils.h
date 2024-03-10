#ifndef _TEST_VF_PERF_UTILS
#define _TEST_VF_PERF_UTILS
extern CpaInstanceHandle *dcInstances_g;
extern Cpa16U numDcInstances_g;
extern CpaInstanceInfo2 *instanceInfo2;

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


CpaStatus createEpollThreads(){
  int fd = -1;
  CpaInstanceInfo2 *instanceInfo2 = NULL;
  Cpa16U i = 0, j = 0, numCreatedPollingThreads = 0;
  Cpa32U coreAffinity = 0;
  CpaStatus status = CPA_STATUS_SUCCESS;
  status = icp_sal_DcGetFileDescriptor(dcInstances_g[i], &fd);
  if (status != CPA_STATUS_SUCCESS)
  {
      PRINT_ERR("icp_sal_DcGetFileDescriptor failed\n");
      return status;
  }
}

CpaStatus createBusyPollThreads(){
  int fd = -1;
  CpaInstanceInfo2 *instanceInfo2 = NULL;
  Cpa16U i = 0, j = 0, numCreatedPollingThreads = 0;
  Cpa32U coreAffinity = 0;
}

#endif