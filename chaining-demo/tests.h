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


void chainingDeflateAndCrcComparisonSingleCore(int numOperations, int bufferSize, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles){

  multiStreamSwCompressCrc64Func(numOperations,bufferSize,1, dcInstHandles[0]);
  cpaDcDsaCrcPerf(numOperations, bufferSize,1,dcInstHandles,sessionHandles);
  multiStreamCompressCrc64PerformanceTest(1,numOperations, bufferSize,dcInstHandles,sessionHandles,1);
}

void chainingDeflateAndCrcComparisonUnboundedPhys(int numInstances, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles ){
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

}



/*This is the new baseline comparison -- we sweep different number of polling cores and allow CPU to get 1 phys CPU*/
void chainingDeflateAndCrcComparison(int numInstances, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles ){
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


    cpaDcDsaCrcPerfSharedSubmitPollCrcPollDcPerf( /*Sw with varying number of thread oversubscription */
      numOperations,
      bufferSizes[i],
      numFlows,
      dcInstHandles,
      sessionHandles);


    multiStreamCompressCrc64PerformanceTestSharedSMTThreads( /*HW with varying number of thread oversubscription */
      4,
      numOperations,
      4096,
      dcInstHandles,
      sessionHandles,
      numInstances
    );
  }

}

void chainingDeflateAndCrcComparisonSinglePhys(int numInstances, CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles ){
  int numConfigs = 3;
  int numOperations = 1000;
  int bufferSizes[] = {4096, 65536, 1*1024*1024};
  int numFlows = 10;
  int i=0;
  multiStreamCompressCrc64PerformanceTestMultiPoller(numFlows,numOperations, bufferSizes[i],dcInstHandles,sessionHandles,numInstances);
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