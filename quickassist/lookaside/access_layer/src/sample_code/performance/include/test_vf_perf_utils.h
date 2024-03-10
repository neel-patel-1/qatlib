#ifndef _TEST_VF_PERF_UTILS
#define _TEST_VF_PERF_UTILS
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

#endif