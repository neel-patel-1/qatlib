#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"

#include "dc_inst_utils.h"
#include "thread_utils.h"

#include "buffer_prepare_funcs.h"
#include "hw_comp_crc_funcs.h"

#include "print_funcs.h"

int gDebugParam = 1;

#include <zlib.h>

CpaStatus validateCompressAndCrc64Sw(
  CpaBufferList *srcBufferList,
  CpaBufferList *dstBufferList,
  CpaDcRqResults *dcResults,
  Cpa32U bufferSize,
  CpaCrcData *crcData)
{
  CpaStatus status = CPA_STATUS_SUCCESS;
  /* Check the Buffer matches*/
  z_stream *stream;
  stream = malloc(sizeof(z_stream));
  stream->zalloc = Z_NULL;
  stream->zfree = Z_NULL;
  stream->opaque = Z_NULL;
  status = inflateInit(stream);
  if (status != Z_OK)
  {
      PRINT_ERR("Error in zlib_inflateInit2, ret = %d\n", status);
      return CPA_STATUS_FAIL;
  }

  Bytef *tmpBuf = NULL;
  OS_MALLOC(&tmpBuf, bufferSize);

  stream->next_in = (Bytef *)dstBufferList->pBuffers[0].pData;
  stream->avail_in = dcResults->produced;
  stream->next_out = (Bytef *)tmpBuf;
  stream->avail_out = bufferSize;
  int ret = inflate(stream, Z_FULL_FLUSH);

  inflateEnd(stream);

  ret = memcmp(srcBufferList->pBuffers[0].pData, tmpBuf, bufferSize);
  if(ret != 0){
    PRINT_ERR("Buffer data does not match\n");
    status = CPA_STATUS_FAIL;
  }

  uint64_t crc64 = crc64_be(0L, Z_NULL, 0);
  crc64 = crc64_be(crc64, dstBufferList->pBuffers[0].pData, dcResults->produced);
  uint64_t crc64_orig = crcData->integrityCrc64b.oCrc;
  ret = memcmp(&crc64, &crc64_orig, sizeof(uint64_t));
  if(ret != 0){
    PRINT_ERR("Dst CRC64 does not match\n");
    status = CPA_STATUS_FAIL;
  }

  return status;
}

/* resets the stream after each compression */
CpaStatus deflateCompressAndTimestamp(
  CpaBufferList *pBufferListSrc,
  CpaBufferList *pBufferListDst,
  CpaDcRqResults *pDcResults,
  int index,
  callback_args *cb_args,
  CpaCrcData *crcData
){
  z_stream strm;
  int ret;
  memset(&strm, 0, sizeof(z_stream));
  ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
  CpaStatus status = CPA_STATUS_SUCCESS;
  packet_stats *stats = cb_args->stats[index];

  Cpa64U submitTime = sampleCoderdtsc();
  stats->submitTime = submitTime;

  Cpa8U *src = pBufferListSrc->pBuffers[0].pData;
  Cpa32U srcLen = pBufferListSrc->pBuffers->dataLenInBytes;
  Cpa8U *dst = pBufferListDst->pBuffers[0].pData;
  Cpa32U dstLen = pBufferListDst->pBuffers->dataLenInBytes;

  strm.avail_in = srcLen;
  strm.next_in = (Bytef *)src;
  strm.avail_out = dstLen;
  strm.next_out = (Bytef *)dst;
  ret = deflate(&strm, Z_FINISH);
  if(ret != Z_STREAM_END)
  {
    fprintf(stderr, "Error in deflate, ret = %d\n", ret);
    return CPA_STATUS_FAIL;
  }
  pDcResults->produced = strm.total_out;

  Cpa64U crc64 = crc64_be(0, Z_NULL, 0);
  crc64 = crc64_be(crc64, dst, strm.total_out);
  crcData->integrityCrc64b.oCrc = crc64;

  stats->receiveTime = sampleCoderdtsc();

  return status;
}

typedef struct _sw_comp_args{
  Cpa32U numOperations;
  Cpa32U bufferSize;
  CpaInstanceHandle dcInstHandle; /*Preprepared DCInstHandle for getting hw-provided compress bounds -- may not be accurate for sw deflate, but no issues throughout testing*/
  pthread_barrier_t *startSync;
} sw_comp_args;

void syncSwComp(void *args){
  sw_comp_args *sw_args = (sw_comp_args *)args;
  Cpa32U numOperations = sw_args->numOperations;
  Cpa32U bufferSize = sw_args->bufferSize;
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaInstanceHandle dcInstHandle = sw_args->dcInstHandle;
  CpaDcInstanceCapabilities cap = {0};
  CpaDcOpData **opData = NULL;
  struct COMPLETION_STRUCT complete;
  CpaStatus status = CPA_STATUS_SUCCESS;

  COMPLETION_INIT(&complete);
  if (CPA_STATUS_SUCCESS != cpaDcQueryCapabilities(dcInstHandle, &cap)){
    PRINT_ERR("Error in querying capabilities\n");
    return;
  }

  multiBufferTestAllocations(
    &cb_args,
    &stats,
    &opData,
    &dcResults,
    &crcData,
    numOperations,
    bufferSize,
    cap,
    &srcBufferLists,
    &dstBufferLists,
    dcInstHandle,
    &complete);

  pthread_barrier_wait(sw_args->startSync);
  for(int i=0; i<numOperations; i++){
    if(CPA_STATUS_SUCCESS != deflateCompressAndTimestamp(
      srcBufferLists[i], dstBufferLists[i], dcResults[i], i, cb_args[i], &crcData[i])){
      PRINT_ERR("Error in compress data on %d'th packet\n", i);
    }
  }

  printStats(stats, numOperations, bufferSize);

  for(int i=0; i<numOperations; i++){
    if (CPA_STATUS_SUCCESS != validateCompressAndCrc64Sw(srcBufferLists[i], dstBufferLists[i], dcResults[i], bufferSize, &(crcData[i])))
    {
      PRINT_ERR("Buffer not compressed/decompressed correctly\n");
    }
  }

  return;
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
    return CPA_STATUS_FAIL;
  }

  /* Keep a dcinst handle for obtaining buffers with hw compress bounds and compatibility/ function reuse*/
  dcInstHandle = dcInstHandles[0];
  prepareDcInst(&dcInstHandles[0]);

  Cpa32U numOperations = 10000;
  Cpa32U bufferSize = 1024;
  Cpa32U numStreams = 2;

  pthread_barrier_t barrier;
  pthread_t threads[numStreams];

  pthread_barrier_init(&barrier, NULL, numStreams);
  for(int i=0; i<numStreams; i++){
    sw_comp_args *args = NULL;
    OS_MALLOC(&args, sizeof(sw_comp_args));
    args->numOperations = numOperations;
    args->bufferSize = bufferSize;
    args->dcInstHandle = dcInstHandle;
    args->startSync = &barrier;
    createThreadJoinable(&threads[i], syncSwComp, (void *)args);
  }

  for(int i=0; i<numStreams; i++){
    pthread_join(threads[i], NULL);
  }


exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}