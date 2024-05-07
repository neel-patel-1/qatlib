#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"

#include "dc_inst_utils.h"
#include "thread_utils.h"

int gDebugParam = 1;

/* Single Flat Buffer BufferList with buffer of bufferSize */
CpaStatus createDstBufferList(CpaBufferList **ppBufferList, Cpa32U bufferSize, CpaInstanceHandle dcInstHandle, CpaDcHuffType huffType){
  Cpa32U numBuffers = 1;
  Cpa8U *pBufferMetaSrc = NULL;
  Cpa8U *pSrcBuffer = NULL;
  CpaFlatBuffer *pFlatBuffer = NULL;
  CpaBufferList *pBufferListSrc = NULL;
  CpaStatus status = CPA_STATUS_SUCCESS;
  Cpa32U bufferListMemSize =
        sizeof(CpaBufferList) + (numBuffers * sizeof(CpaFlatBuffer));
  Cpa32U bufferMetaSize = 0;
  Cpa32U dstBufferSize = 0;

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
  if(CPA_STATUS_SUCCESS == status){
    /* Build source bufferList */
    pFlatBuffer = (CpaFlatBuffer *)(pBufferListSrc + 1);

    pBufferListSrc->pBuffers = pFlatBuffer;
    pBufferListSrc->numBuffers = 1;
    pBufferListSrc->pPrivateMetaData = pBufferMetaSrc;

    pFlatBuffer->dataLenInBytes = dstBufferSize;
    pFlatBuffer->pData = pSrcBuffer;
  }

  *ppBufferList = pBufferListSrc;

  return CPA_STATUS_SUCCESS;
}

/* Single Flat Buffer BufferList with buffer of bufferSize */
CpaStatus createSourceBufferList(CpaBufferList **ppBufferList, Cpa8U *buffer, Cpa32U bufferSize, CpaInstanceHandle dcInstHandle, CpaDcHuffType huffType){
  Cpa32U numBuffers = 1;
  Cpa8U *pBufferMetaSrc = NULL;
  Cpa8U *pSrcBuffer = NULL;
  CpaFlatBuffer *pFlatBuffer = NULL;
  CpaBufferList *pBufferListSrc = NULL;
  CpaStatus status = CPA_STATUS_SUCCESS;
  Cpa32U bufferListMemSize =
        sizeof(CpaBufferList) + (numBuffers * sizeof(CpaFlatBuffer));
  Cpa32U bufferMetaSize = 0;

  status =
        cpaDcBufferListGetMetaSize(dcInstHandle, numBuffers, &bufferMetaSize);

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
  if(CPA_STATUS_SUCCESS == status){
    /* copy source into buffer */
    memcpy(pSrcBuffer, buffer, bufferSize);

    /* Build source bufferList */
    pFlatBuffer = (CpaFlatBuffer *)(pBufferListSrc + 1);

    pBufferListSrc->pBuffers = pFlatBuffer;
    pBufferListSrc->numBuffers = 1;
    pBufferListSrc->pPrivateMetaData = pBufferMetaSrc;

    pFlatBuffer->dataLenInBytes = bufferSize;
    pFlatBuffer->pData = pSrcBuffer;
  }

  *ppBufferList = pBufferListSrc;

  return CPA_STATUS_SUCCESS;
}

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
      thread_args *args = NULL;
      args = (thread_args *)malloc(sizeof(thread_args));
      args->dcInstHandle = dcInstHandle;
      args->id = 0;
      createThread(&thread[0], dc_polling, (void *)args);
  }

  sessionHandle = sessionHandles[0];
  prepareDcSession(dcInstHandle, &sessionHandle);

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
  if(CPA_STATUS_SUCCESS != prepareSampleBuffer((Cpa8U **)&sampleData, sampleDataSize)){
    fprintf(stderr, "Failed to prepare sample buffer\n");
    goto exit;
  }

  Cpa32U bufferSize = sampleDataSize;
  Cpa32U dstBufferSize = bufferSize;
  Cpa64U checksum = 0;
  Cpa32U numBuffers = 1;

  Cpa32U bufferListMemSize =
        sizeof(CpaBufferList) + (numBuffers * sizeof(CpaFlatBuffer));
  Cpa8U *pDstBuffer = NULL;
  Cpa8U *pDst2Buffer = NULL;

  CpaDcRqResults dcResults;
  struct COMPLETION_STRUCT complete;
  INIT_OPDATA(&opData, CPA_DC_FLUSH_FINAL);

  status =
    createSourceBufferList(&pBufferListSrc, sampleData, sampleDataSize, dcInstHandle, huffType);

  status =
    createDstBufferList(&pBufferListDst, sampleDataSize, dcInstHandle, huffType);

  if(CPA_STATUS_SUCCESS != status){
    fprintf(stderr, "Failed to create source buffer list\n");
    goto exit;
  }

  status =
    cpaDcBufferListGetMetaSize(dcInstHandle, numBuffers, &bufferMetaSize);


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

exit:


  gPollingDcs[0] = 0;

  COMPLETION_DESTROY(&complete);

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}