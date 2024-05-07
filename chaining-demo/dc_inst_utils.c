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