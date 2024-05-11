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


int gDebugParam = 0;

void allTests(int numInstances, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles ){
  int numConfigs = 3;
  int numOperations = 1000;
  int bufferSizes[] = {4096, 65536, 1*1024*1024};
  int numFlows = 10;

  if(numFlows > numInstances){
    numFlows = numInstances;
  }

  CpaInstanceHandle *dcInstHandle = dcInstHandles[0];
  for(int i=0; i<numConfigs; i++){
    multiStreamSwCompressCrc64Func(numOperations, bufferSizes[i], numFlows, dcInstHandle);
    cpaDcDsaCrcPerf(numOperations, bufferSizes[i], numFlows, dcInstHandles, sessionHandles);
    multiStreamCompressCrc64PerformanceTest(numFlows,numOperations, bufferSizes[i],dcInstHandles,sessionHandles,numInstances);
  }

  // multiStreamCompressCrc64PerformanceTestSharedMultiSwPerHwTd(
  //   4,
  //   numOperations,
  //   4096,
  //   dcInstHandles,
  //   sessionHandles,
  //   numInstances,
  //   4
  // );

  // multiStreamCompressCrc64PerformanceTestSharedSMTThreads(
  //   4,
  //   numOperations,
  //   4096,
  //   dcInstHandles,
  //   sessionHandles,
  //   numInstances
  // );

  // multiStreamCompressCrc64PerformanceTest(
  //   4,
  //   numOperations,
  //   4096,
  //   dcInstHandles,
  //   sessionHandles,
  //   numInstances
  // );
}




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

  allTests(numInstances,dcInstHandles, sessionHandles );



exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}