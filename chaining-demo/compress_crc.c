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

CpaStatus prepareTestBufferLists(CpaBufferList ***pSrcBufferLists, CpaBufferList ***pDstBufferLists,
  CpaInstanceHandle dcInstHandle, Cpa32U bufferSize){

  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;


  Cpa16U numBufferLists = 1;
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
  prepareDcSession(dcInstHandle, &sessionHandle, dcPerfCallback);

  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;

  prepareTestBufferLists(&srcBufferLists, &dstBufferLists, dcInstHandles[0], 1024);

  CpaBufferList *pBufferListSrc = srcBufferLists[0];
  CpaBufferList *pBufferListDst = dstBufferLists[0];
  CpaDcOpData opData = {};
  CpaDcRqResults dcResults;
  CpaDcRqResults *pDcResults = &dcResults;

  struct COMPLETION_STRUCT complete;
  callback_args *cb_args = NULL;
  packet_stats *stats = NULL;

  COMPLETION_INIT(&complete);
  INIT_OPDATA(&opData, CPA_DC_FLUSH_FINAL);
  PHYS_CONTIG_ALLOC(&cb_args, sizeof(struct _callback_args));
  memset(cb_args, 0, sizeof(struct _callback_args));

  PHYS_CONTIG_ALLOC(&stats, sizeof(packet_stats));
  memset(stats, 0, sizeof(packet_stats));
  cb_args->completion = &complete;
  cb_args->stats = stats;

  stats->submitTime = sampleCoderdtsc();
  status = cpaDcCompressData2(
    dcInstHandle,
    sessionHandle,
    pBufferListSrc,     /* source buffer list */
    pBufferListDst,     /* destination buffer list */
    &opData,            /* Operational data */
    &dcResults,         /* results structure */
    (void *)cb_args); /* data sent as is to the callback function*/

  if(status != CPA_STATUS_SUCCESS){
    fprintf(stderr, "Error in compress data\n");
    goto exit;
  }
  if (!COMPLETION_WAIT(&complete, 5000))
  {
      PRINT_ERR("timeout or interruption in cpaDcCompressData2\n");
      status = CPA_STATUS_FAIL;
  }
  if (CPA_STATUS_SUCCESS == status)
  {
    if (dcResults.status != CPA_DC_OK)
    {
        PRINT_ERR("Results status not as expected (status = %d)\n",
                  dcResults.status);
        status = CPA_STATUS_FAIL;
    }
  }
  COMPLETION_DESTROY(&complete);


  // functionalCompressAndCrc64(dcInstHandle, sessionHandle);


exit:


  gPollingDcs[0] = 0;


  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}