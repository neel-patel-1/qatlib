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
#include "sw_comp_crc_funcs.h"

#include "print_funcs.h"

#include <zlib.h>


#include "idxd.h"
#include "dsa.h"

#include "dsa_funcs.h"

#include "validate_compress_and_crc.h"

#include "accel_test.h"

#include "sw_chain_comp_crc_funcs.h"
#include "smt-thread-exps.h"

#include "tests.h"

int gDebugParam = 0;






int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;
  Cpa16U numInstances = 0;
  CpaInstanceHandle dcInstHandle = NULL;
  CpaDcSessionHandle sessionHandle = NULL;
  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcSessionHandle sessionHandles[MAX_INSTANCES];

  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);

  allocateDcInstances(dcInstHandles, &numInstances);
  if (0 == numInstances)
  {
    fprintf(stderr, "No instances found\n");
    return CPA_STATUS_FAIL;
  }

  int numOperations = 1000;
  int bufferSize = 4096;

  CpaDcInstanceCapabilities cap = {0};
    CpaDcOpData **opData = NULL;
    CpaDcRqResults **dcResults = NULL;
    CpaCrcData *crcData = NULL;
    struct COMPLETION_STRUCT complete;
    callback_args **cb_args = NULL;
    packet_stats **stats = NULL;
    CpaBufferList **srcBufferLists = NULL;
    CpaBufferList **dstBufferLists = NULL;

  /* Streaming submission from a single thread single inst hw*/
  prepareMultipleCompressAndCrc64InstancesAndSessions(dcInstHandles, sessionHandles, numInstances, numInstances);
      cpaDcQueryCapabilities(dcInstHandle, &cap);

  sessionHandle = sessionHandles[0];
  dcInstHandle = dcInstHandles[0];

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

  for(int i=0; i< numOperations; i++){
    status = cpaDcCompressData2(
      dcInstHandle,
      sessionHandle,
      srcBufferLists[i],     /* source buffer list */
      dstBufferLists[i],     /* destination buffer list */
      opData[i],            /* Operational data */
      dcResults[i],         /* results structure */
      (void *)cb_args);
  }

exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}