#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"
#include "dc_inst_utils.h"


int gDebugParam;
int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;
  Cpa16U numInstances = 0;
  CpaInstanceHandle dcInstHandle = NULL;
  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
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
      createThread(&thread[0], dc_polling, (void *)dcInstHandle);
  }

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

exit:
  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}