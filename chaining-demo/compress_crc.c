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

/* resets the stream after each compression */
CpaStatus deflateCompressAndTimestamp(
  CpaBufferList *pBufferListSrc,
  CpaBufferList *pBufferListDst, CpaDcRqResults *pDcResults, int index, callback_args *cb_args
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

  stats->receiveTime = sampleCoderdtsc();

  return status;
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

  /* Get test buffers and perf stats setup */
  Cpa32U numOperations = 10000;
  Cpa32U bufferSize = 1024;
  CpaDcOpData **opData = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  struct COMPLETION_STRUCT complete;
  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;
  cpaDcQueryCapabilities(dcInstHandle, &cap);
  COMPLETION_INIT(&complete);
  prepareDcInst(&dcInstHandle);

  multiBufferTestAllocations(&cb_args,
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


  multiStreamCompressCrc64PerformanceTest(
    1,
    numOperations,
    bufferSize,
    dcInstHandles,
    sessionHandles,
    numInstances
  );


  for(int i=0; i<numOperations; i++){
    if(CPA_STATUS_SUCCESS != deflateCompressAndTimestamp(
      srcBufferLists[i], dstBufferLists[i], dcResults[i], i, cb_args[i])){
      PRINT_ERR("Error in compress data on %d'th packet\n", i);
    }
  }

  printStats(stats, numOperations, bufferSize);

  for(int i=0; i<numOperations; i++){
    if (CPA_STATUS_SUCCESS != validateCompress(srcBufferLists[i], dstBufferLists[i], dcResults[i], bufferSize))
    {
      PRINT_ERR("Buffer not compressed/decompressed correctly\n");
    }
  }


exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}