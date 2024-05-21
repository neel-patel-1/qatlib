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

#include <xmmintrin.h>


int gDebugParam = 1;




int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;

  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcSessionHandle sessionHandles[MAX_INSTANCES];

  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);

  int numOperations = 1000;

  int bufferSizes[] = {4096, 65536, 1024*1024};
  int numOperationss[] = {10000, 10000, 1000};

  for(int i=0; i<1; i++){
  int bufferSize = bufferSizes[i];
  int numOperations = numOperationss[i];

  // streamingSWCompressAndCRC32Validated(numOperations, bufferSize, dcInstHandles, sessionHandles);
  streamingSwChainCompCrcValidated(numOperations, bufferSize, dcInstHandles, sessionHandles);
  // hwCompCrcValidatedStream(numOperations, bufferSize, dcInstHandles, sessionHandles);
  streamingHwCompCrc(numOperations, bufferSize, dcInstHandles, sessionHandles);

  // singleSwCompCrcLatency(bufferSize, numOperations, dcInstHandles, sessionHandles);
  // swChainCompCrcSync(numOperations, bufferSize, dcInstHandles, sessionHandles,1);
  // streamingHwCompCrcSyncLatency(numOperations, bufferSize, dcInstHandles, sessionHandles, 1);
  }
exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}