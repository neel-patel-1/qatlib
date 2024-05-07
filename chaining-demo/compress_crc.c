#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"

#include "dc_inst_utils.h"
#include "thread_utils.h"

int gDebugParam;
int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;
  Cpa16U numInstances = 0;
  CpaInstanceHandle dcInstHandle = NULL;
  CpaDcSessionHandle sessionHandle = NULL;
  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcSessionHandle sessionHandles[MAX_INSTANCES];
  CpaDcInstanceCapabilities cap = {0};

  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);

  allocateDcInstances(dcInstHandles, &numInstances);
  if (0 == numInstances)
  {
    fprintf(stderr, "No instances found\n");
    goto exit;
  }
  /* single instance for latency test */
  dcInstHandle = dcInstHandles[0];
  prepareDcInst(&dcInstHandle);

  CpaInstanceInfo2 info2 = {0};

  status = cpaDcInstanceGetInfo2(dcInstHandle, &info2);

  if ((status == CPA_STATUS_SUCCESS) && (info2.isPolled == CPA_TRUE))
  {
      /* Start thread to poll instance */
      pthread_t thread[MAX_INSTANCES];
      thread_args args = {
          .dcInstHandle = dcInstHandle,
          .id = 0,

      };
      createThread(&thread[0], dc_polling, (void *)&args);
  }

  sessionHandle = sessionHandles[0];
  prepareDcSession(dcInstHandle, &sessionHandle);

  Cpa8U *pBufferMetaSrc = NULL;
  Cpa8U *pBufferMetaDst = NULL;
  Cpa8U *pBufferMetaDst2 = NULL;
  Cpa32U bufferMetaSize = 0;
  CpaBufferList *pBufferListSrc = NULL;
  CpaBufferList *pBufferListDst = NULL;
  CpaBufferList *pBufferListDst2 = NULL;
  CpaFlatBuffer *pFlatBuffer = NULL;
  CpaDcOpData opData = {};
  Cpa8U *sampleData = NULL;
  Cpa32U sampleDataSize = 512;
  CpaDcHuffType huffType = CPA_DC_HT_FULL_DYNAMIC;
  if(!prepareSampleBuffer(&sampleData, sampleDataSize)){
    fprintf(stderr, "Failed to prepare sample buffer\n");
    goto exit;
  }

  Cpa32U bufferSize = sampleDataSize;
  Cpa32U dstBufferSize = bufferSize;
  Cpa32U checksum = 0;
  Cpa32U numBuffers = 1;

  Cpa32U bufferListMemSize =
        sizeof(CpaBufferList) + (numBuffers * sizeof(CpaFlatBuffer));
  Cpa8U *pSrcBuffer = NULL;
  Cpa8U *pDstBuffer = NULL;
  Cpa8U *pDst2Buffer = NULL;

  CpaDcRqResults dcResults;
  struct COMPLETION_STRUCT complete;
  INIT_OPDATA(&opData, CPA_DC_FLUSH_FINAL);


  status =
        cpaDcBufferListGetMetaSize(dcInstHandle, numBuffers, &bufferMetaSize);

  #if DC_API_VERSION_AT_LEAST(2, 5)
    status = cpaDcDeflateCompressBound(
        dcInstHandle, huffType, bufferSize, &dstBufferSize);
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("cpaDcDeflateCompressBound API failed. (status = %d)\n",
                  status);
        return CPA_STATUS_FAIL;
    }
  #endif

  if (CPA_STATUS_SUCCESS == status)
  {
      status = PHYS_CONTIG_ALLOC(&pBufferMetaSrc, bufferMetaSize);
  }
  if (CPA_STATUS_SUCCESS == status)
  {
      status = OS_MALLOC(&pBufferListSrc, bufferListMemSize);
  }
  if (CPA_STATUS_SUCCESS == status)
  {
      status = PHYS_CONTIG_ALLOC(&pSrcBuffer, bufferSize);
  }

  /* Allocate destination buffer the same size as source buffer */
  if (CPA_STATUS_SUCCESS == status)
  {
      status = PHYS_CONTIG_ALLOC(&pBufferMetaDst, bufferMetaSize);
  }
  if (CPA_STATUS_SUCCESS == status)
  {
      status = OS_MALLOC(&pBufferListDst, bufferListMemSize);
  }
  if (CPA_STATUS_SUCCESS == status)
  {
      status = PHYS_CONTIG_ALLOC(&pDstBuffer, dstBufferSize);
  }

  if(CPA_STATUS_SUCCESS == status){
    /* copy source into buffer */
    memcpy(pSrcBuffer, sampleData, sizeof(sampleData));

    /* Build source bufferList */
    pFlatBuffer = (CpaFlatBuffer *)(pBufferListSrc + 1);

    pBufferListSrc->pBuffers = pFlatBuffer;
    pBufferListSrc->numBuffers = 1;
    pBufferListSrc->pPrivateMetaData = pBufferMetaSrc;

    pFlatBuffer->dataLenInBytes = bufferSize;
    pFlatBuffer->pData = pSrcBuffer;

    /* Build destination bufferList */
    pFlatBuffer = (CpaFlatBuffer *)(pBufferListDst + 1);

    pBufferListDst->pBuffers = pFlatBuffer;
    pBufferListDst->numBuffers = 1;
    pBufferListDst->pPrivateMetaData = pBufferMetaDst;

    pFlatBuffer->dataLenInBytes = dstBufferSize;
    pFlatBuffer->pData = pDstBuffer;

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
        if (!COMPLETION_WAIT(&complete, TIMEOUT_MS))
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
            PRINT_DBG("Adler checksum 0x%x\n", dcResults.checksum);
        }
        /* To compare the checksum with decompressed output */
        checksum = dcResults.checksum;
    }
  }


  gPollingDcs[0] = 0;

exit:
  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}