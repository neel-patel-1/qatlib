#include "buffer_prepare_funcs.h"

CpaStatus prepareTestBufferLists(CpaBufferList ***pSrcBufferLists, CpaBufferList ***pDstBufferLists,
  CpaInstanceHandle dcInstHandle, Cpa32U bufferSize, Cpa32U numBufferLists){

  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;

  Cpa8U *pBuffers = NULL;
  CpaDcHuffType huffType = CPA_DC_HT_FULL_DYNAMIC;

  prepareSampleBuffer(&pBuffers, bufferSize);

  OS_MALLOC(&srcBufferLists, numBufferLists * sizeof(CpaBufferList *));
  OS_MALLOC(&dstBufferLists, numBufferLists * sizeof(CpaBufferList *));
  for(int i=0; i<numBufferLists; i++){
    createSourceBufferList(&srcBufferLists[i], pBuffers, bufferSize, dcInstHandle, huffType);
    createDstBufferList(&dstBufferLists[i], bufferSize, dcInstHandle, huffType);
  }
  *pSrcBufferLists = srcBufferLists;
  *pDstBufferLists = dstBufferLists;
}

CpaStatus multiBufferTestAllocations(callback_args ***pcb_args, packet_stats ***pstats,
  CpaDcOpData ***popData, CpaDcRqResults ***pdcResults, CpaCrcData **pCrcData,
  Cpa32U numOperations, Cpa32U bufferSize, CpaDcInstanceCapabilities cap,
  CpaBufferList ***pSrcBufferLists, CpaBufferList ***pDstBufferLists,
  CpaInstanceHandle dcInstHandle, struct COMPLETION_STRUCT *complete){

  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaDcOpData **opData = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;


  PHYS_CONTIG_ALLOC(&cb_args, sizeof(callback_args*) * numOperations);
  PHYS_CONTIG_ALLOC(&stats, sizeof(packet_stats*) * numOperations);
  PHYS_CONTIG_ALLOC(&opData, sizeof(CpaDcOpData*) * numOperations);
  PHYS_CONTIG_ALLOC(&dcResults, sizeof(CpaDcRqResults*) * numOperations);
  PHYS_CONTIG_ALLOC(&crcData, sizeof(CpaCrcData) * numOperations);

  for(int i=0; i<numOperations; i++){
    /* Per packet callback arguments */
    PHYS_CONTIG_ALLOC(&(cb_args[i]), sizeof(callback_args));
    memset(cb_args[i], 0, sizeof(struct _callback_args));
    cb_args[i]->completion = complete;
    cb_args[i]->stats = stats;
    /* here we assume that the i'th callback invocation will be processing the i'th packet*/
    cb_args[i]->completedOperations = i;
    cb_args[i]->numOperations = numOperations;

    /* Per Packet packet stats */
    PHYS_CONTIG_ALLOC(&(stats[i]), sizeof(packet_stats));
    memset(stats[i], 0, sizeof(packet_stats));

    /* Per packet CrC Datas */
    memset(&crcData[i], 0, sizeof(CpaDcOpData));

    /* Per packet opDatas*/
    PHYS_CONTIG_ALLOC(&(opData[i]), sizeof(CpaDcOpData));
    memset(opData[i], 0, sizeof(CpaDcOpData));
    INIT_OPDATA(opData[i], CPA_DC_FLUSH_FINAL);
    if(cap.integrityCrcs64b == CPA_TRUE){
      opData[i]->integrityCrcCheck = CPA_TRUE;
      opData[i]->pCrcData = &(crcData[i]);
    }

    /* Per packet results */
    PHYS_CONTIG_ALLOC(&(dcResults[i]), sizeof(CpaDcRqResults));
    memset(dcResults[i], 0, sizeof(CpaDcRqResults));
  }
  prepareTestBufferLists(&srcBufferLists, &dstBufferLists, dcInstHandle, bufferSize, numOperations);
  *pcb_args = cb_args;
  *pstats = stats;
  *popData = opData;
  *pdcResults = dcResults;
  *pCrcData = crcData;
  *pSrcBufferLists = srcBufferLists;
  *pDstBufferLists = dstBufferLists;


  return CPA_STATUS_SUCCESS;

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


void prepareZeroSampleBuffer(Cpa8U *pBuffer, Cpa32U bufferSize){
  memset(pBuffer, 0, bufferSize);
}

CpaStatus generateBufferList(CpaBufferList **ppBufferList, Cpa32U bufferSize, Cpa32U bufferListMetaSize, Cpa32U numBuffers, buffer_gen genBuffer){
  Cpa8U *pBufferMetaSrc = NULL;
  Cpa8U *pSrcBuffer = NULL;
  CpaFlatBuffer *pFlatBuffer = NULL;
  CpaBufferList *pBufferListSrc = NULL;
  CpaStatus status = CPA_STATUS_SUCCESS;


  status = OS_MALLOC(&pBufferListSrc, sizeof(CpaBufferList));
  PHYS_CONTIG_ALLOC(&pBufferListSrc->pBuffers, numBuffers * sizeof(CpaFlatBuffer));

  if (CPA_STATUS_SUCCESS == status)
  {
    if(bufferListMetaSize > 0){
      status = PHYS_CONTIG_ALLOC(&pBufferMetaSrc, bufferListMetaSize);
      pBufferListSrc->pPrivateMetaData = pBufferMetaSrc;
    } else {
      pBufferListSrc->pPrivateMetaData = NULL;
    }
  }
  if (CPA_STATUS_SUCCESS == status)
  {

    for(int i=0; i<numBuffers; i++){
      status = PHYS_CONTIG_ALLOC(&pSrcBuffer, bufferSize);
      if(CPA_STATUS_SUCCESS != status){
        PRINT_ERR("Failed to allocate buffer for buffer list\n");
        return status;
      }
      genBuffer(pSrcBuffer, bufferSize);
      pBufferListSrc->pBuffers[i].pData = pSrcBuffer;
      pBufferListSrc->pBuffers[i].dataLenInBytes = bufferSize;
    }

  }
  pBufferListSrc->numBuffers = numBuffers;
  *ppBufferList = pBufferListSrc;

  return CPA_STATUS_SUCCESS;
}