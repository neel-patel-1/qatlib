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




  gPollingDcs[0] = 0;

exit:
  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}