#ifndef TESTS
#define TESTS
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
#include "streaming-single-funcs.h"

void chainingDeflateAndCrcComparison( CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles ){
  int bufferSizes[] = {4096, 65536, 1024*1024};
  int numOperationss[] = {10000, 10000, 1000};

  for(int i=0; i<3; i++){
  int bufferSize = bufferSizes[i];
  int numOperations = numOperationss[i];

  streamingSWCompressAndCRC32Validated(numOperations, bufferSize, dcInstHandles, sessionHandles); /* just don't submit and go to next task node regardless of comp */
  streamingSwChainCompCrcValidated(numOperations, bufferSize, dcInstHandles, sessionHandles);
  streamingHwCompCrc(numOperations, bufferSize, dcInstHandles, sessionHandles);

  singleSwCompCrcLatency(bufferSize, numOperations, dcInstHandles, sessionHandles);
  swChainCompCrcSync(numOperations, bufferSize, dcInstHandles, sessionHandles,1);
  streamingHwCompCrcSyncLatency(numOperations, bufferSize, dcInstHandles, sessionHandles, 1);
  }

}

void threadCoschedulingTest(int numInstances, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles ){
  int numConfigs = 3;
  int numOperations = 10000;
  int bufferSizes[] = {4096, 65536, 1*1024*1024};
  int numFlows = 10;
  multiStreamCompressCrc64PerformanceTestSharedMultiSwPerHwTd(
    4,
    numOperations,
    4096,
    dcInstHandles,
    sessionHandles,
    numInstances,
    4
  );

  multiStreamCompressCrc64PerformanceTestSharedSMTThreads(
    4,
    numOperations,
    4096,
    dcInstHandles,
    sessionHandles,
    numInstances
  );

  multiStreamCompressCrc64PerformanceTest(
    4,
    numOperations,
    4096,
    dcInstHandles,
    sessionHandles,
    numInstances
  );

  multiStreamCompressCrc64PerformanceTestMultiPoller(
    4,
    numOperations,
    4096,
    dcInstHandles,
    sessionHandles,
    numInstances);

}


#endif