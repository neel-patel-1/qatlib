#include "dc_inst_utils.h"

CpaStatus allocateDcInstances(CpaInstanceHandle *dcInstHandles, Cpa16U *numInstances)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa16U numInstancesLocal = 0;

    status = cpaDcGetNumInstances(&numInstancesLocal);
    if (numInstancesLocal >= MAX_INSTANCES)
    {
        numInstancesLocal = MAX_INSTANCES;
    }
    if ((status == CPA_STATUS_SUCCESS) && (numInstancesLocal > 0))
    {
        status = cpaDcGetInstances(numInstancesLocal, dcInstHandles);
    }
    *numInstances = numInstancesLocal;
    return status;
}

CpaStatus allocateIntermediateBuffers(CpaInstanceHandle dcInstHandle,
  CpaBufferList ***pBufferInterArray,
  Cpa16U *pNumInterBuffLists,
  Cpa32U *pBuffMetaSize)
{
  Cpa16U numInterBuffLists = 0;
  Cpa16U bufferNum = 0;
  Cpa32U buffMetaSize = 0;
  CpaBufferList **bufferInterArray = NULL;
  CpaStatus status = cpaDcBufferListGetMetaSize(dcInstHandle, 1, &buffMetaSize);

  if (CPA_STATUS_SUCCESS == status)
  {
      status = cpaDcGetNumIntermediateBuffers(dcInstHandle,
                                              &numInterBuffLists);
  }
  if (CPA_STATUS_SUCCESS == status && 0 != numInterBuffLists)
  {
      status = PHYS_CONTIG_ALLOC(
          &bufferInterArray, numInterBuffLists * sizeof(CpaBufferList *));
  }
  for (bufferNum = 0; bufferNum < numInterBuffLists; bufferNum++)
  {
      if (CPA_STATUS_SUCCESS == status)
      {
          status = PHYS_CONTIG_ALLOC(&bufferInterArray[bufferNum],
                                      sizeof(CpaBufferList));
      }
      if (CPA_STATUS_SUCCESS == status)
      {
          status = PHYS_CONTIG_ALLOC(
              &bufferInterArray[bufferNum]->pPrivateMetaData,
              buffMetaSize);
      }
      if (CPA_STATUS_SUCCESS == status)
      {
          status =
              PHYS_CONTIG_ALLOC(&bufferInterArray[bufferNum]->pBuffers,
                                sizeof(CpaFlatBuffer));
      }
      if (CPA_STATUS_SUCCESS == status)
      {
          /* Implementation requires an intermediate buffer approximately
                      twice the size of the output buffer */
          status = PHYS_CONTIG_ALLOC(
              &bufferInterArray[bufferNum]->pBuffers->pData,
              2 * SAMPLE_MAX_BUFF);
          bufferInterArray[bufferNum]->numBuffers = 1;
          bufferInterArray[bufferNum]->pBuffers->dataLenInBytes =
              2 * SAMPLE_MAX_BUFF;
      }

  } /* End numInterBuffLists */
  *pBufferInterArray = bufferInterArray;
  *pNumInterBuffLists = numInterBuffLists;
  *pBuffMetaSize = buffMetaSize;
  return status;
}

CpaStatus prepareDcInst(CpaInstanceHandle *pDcInstHandle){
  CpaInstanceHandle dcInstHandle = *pDcInstHandle;
  CpaDcInstanceCapabilities cap = {0};
  CpaBufferList **bufferInterArray = NULL;
  Cpa16U numInterBuffLists = 0;
  Cpa32U buffMetaSize = 0;
  CpaStatus status = cpaDcQueryCapabilities(dcInstHandle, &cap);
  if (!cap.statelessDeflateCompression ||
      !cap.statelessDeflateDecompression || !cap.checksumAdler32 ||
      !cap.dynamicHuffman)
  {
      fprintf(stderr, "Capabilities not supported\n");
      return CPA_STATUS_FAIL;
  }
  if (cap.dynamicHuffmanBufferReq)
  {
      allocateIntermediateBuffers(dcInstHandle, &bufferInterArray, &numInterBuffLists, &buffMetaSize);
  }
  if(cap.integrityCrcs64b){
    PRINT_DBG("Integrity CRC is enabled\n");
  }
  if (CPA_STATUS_SUCCESS == status)
  {
      /*
        * Set the address translation function for the instance
        */
      status = cpaDcSetAddressTranslation(dcInstHandle, qaeVirtToPhysNUMA);
  }

  if (CPA_STATUS_SUCCESS == status)
  {
      /* Start DataCompression component */
      PRINT_DBG("cpaDcStartInstance\n");
      status = cpaDcStartInstance(
          dcInstHandle, numInterBuffLists, bufferInterArray);
  }


}

CpaStatus prepareDcSession(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle *pSessionHandle){
  CpaStatus status = CPA_STATUS_SUCCESS;
  CpaDcSessionSetupData sd = {0};
  Cpa32U sess_size = 0;
  Cpa32U ctx_size = 0;
  CpaDcSessionHandle sessionHdl = NULL;
  sd.compLevel = CPA_DC_L6;
  sd.compType = CPA_DC_DEFLATE;
  sd.huffType = CPA_DC_HT_FULL_DYNAMIC;
  sd.autoSelectBestHuffmanTree = CPA_DC_ASB_ENABLED;
  sd.sessDirection = CPA_DC_DIR_COMBINED;
  sd.sessState = CPA_DC_STATELESS;
  status = cpaDcGetSessionSize(dcInstHandle, &sd, &sess_size, &ctx_size);
  if (CPA_STATUS_SUCCESS == status)
  {
      /* Allocate session memory */
      status = PHYS_CONTIG_ALLOC(&sessionHdl, sess_size);
  }
  /* Initialize the Stateless session */
  if (CPA_STATUS_SUCCESS == status)
  {
      status = cpaDcInitSession(
          dcInstHandle,
          sessionHdl, /* session memory */
          &sd,        /* session setup data */
          NULL, /* pContexBuffer not required for stateless operations */
          dcLatencyCallback); /* callback function */
  }
  *pSessionHandle = sessionHdl;
}

CpaStatus prepareSampleBuffer(Cpa8U **ppBuffer, Cpa32U bufferSize){
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa8U *pBuffer = NULL;
    status = PHYS_CONTIG_ALLOC(&pBuffer, bufferSize);
    Cpa32U i = 0;
    for (i = 0; i < bufferSize; i++)
    {
        pBuffer[i] = i % 256;
    }
    *ppBuffer = pBuffer;
    return status;
}


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

CpaStatus functionalCompressAndCrc64(CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle){
  CpaStatus status = CPA_STATUS_SUCCESS;
  Cpa8U *pBufferMetaDst2 = NULL;
  Cpa32U bufferMetaSize = 0;
  CpaBufferList *pBufferListSrc = NULL;
  CpaBufferList *pBufferListDst = NULL;
  CpaBufferList *pBufferListDst2 = NULL;
  CpaDcOpData opData = {};
  Cpa8U *sampleData = NULL;
  Cpa32U sampleDataSize = 512;
  CpaDcInstanceCapabilities cap = {0};
  CpaDcHuffType huffType = CPA_DC_HT_FULL_DYNAMIC;
  if(CPA_STATUS_SUCCESS != prepareSampleBuffer((Cpa8U **)&sampleData, sampleDataSize)){
    fprintf(stderr, "Failed to prepare sample buffer\n");
    return CPA_STATUS_FAIL;;
  }

  Cpa32U bufferSize = sampleDataSize;
  Cpa32U dstBufferSize = bufferSize;
  Cpa64U checksum = 0;

  CpaDcRqResults dcResults;
  struct COMPLETION_STRUCT complete;
  INIT_OPDATA(&opData, CPA_DC_FLUSH_FINAL);

  status =
    createSourceBufferList(&pBufferListSrc, sampleData, sampleDataSize, dcInstHandle, huffType);

  status =
    createDstBufferList(&pBufferListDst, sampleDataSize, dcInstHandle, huffType);

  if(CPA_STATUS_SUCCESS != status){
    fprintf(stderr, "Failed to create source buffer list\n");
    return CPA_STATUS_FAIL;;
  }

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
  COMPLETION_DESTROY(&complete);


}