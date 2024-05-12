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


static void symCallback(void *pCallbackTag,
                        CpaStatus status,
                        const CpaCySymOp operationType,
                        void *pOpData,
                        CpaBufferList *pDstBuffer,
                        CpaBoolean verifyResult)
{
    PRINT_DBG("Callback called with status = %d.\n", status);

    if (NULL != pCallbackTag) {
        /* Indicate that the function has been called */
        COMPLETE((struct COMPLETION_STRUCT *)pCallbackTag);
    }
}

typedef struct _cryptoPollingArgs{
    CpaInstanceHandle cyInstHandle;
    Cpa32U id;
} cryptoPollingArgs ;

static void sal_polling(void *args)
{
  cryptoPollingArgs *pollingArgs = (cryptoPollingArgs *)args;
  CpaInstanceHandle cyInstHandle = pollingArgs->cyInstHandle;
  Cpa32U id = pollingArgs->id;
  gPollingCys[id] = 1;
  PRINT_DBG("CyPoller: %d started\n", id);
  while (gPollingCys[id])
  {
      icp_sal_CyPollInstance(cyInstHandle, 0);
  }
  sampleThreadExit();
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

  // allTests(numInstances,dcInstHandles, sessionHandles );

  CpaInstanceHandle cyInstHandles[MAX_INSTANCES];
  CpaCySymSessionCtx sessionCtxs[MAX_INSTANCES];
  pthread_t cyPollers[numInstances];
  cryptoPollingArgs *cyPollerArgs[numInstances];
  int firstPollingLogical = 5;

  status = cpaCyGetNumInstances(&numInstances);
  if ((status == CPA_STATUS_SUCCESS) && (numInstances > 0))
  {
      status = cpaCyGetInstances(numInstances, cyInstHandles);
      PRINT_DBG("Number of instances found = %d\n", numInstances);
  }

  if (0 == numInstances) //sudo sed -i 's/ServicesEnabled=.*/ServicesEnabled=sym;dc/g' /etc/sysconfig/qat
  {
      PRINT_ERR("No instances found for 'SSL'\n");
      PRINT_ERR("Please check your section names");
      PRINT_ERR(" in the config file.\n");
      PRINT_ERR("Also make sure to use config file version 2.\n");
  }

  for(int i=0; i<numInstances; i++){
    status = cpaCyStartInstance(cyInstHandles[i]);
    if (status != CPA_STATUS_SUCCESS)
    {
        PRINT_ERR("cpaCyStartInstance failed, status=%d\n", status);
        return status;
    }
  }
  if (CPA_STATUS_SUCCESS == status)
  {
      /*
        * Set the address translation function for the instance
        */
      //<snippet name="virt2phys">
    for(int i=0; i<numInstances; i++){
      status = cpaCySetAddressTranslation(cyInstHandles[i], sampleVirtToPhys);
    }
  }

  OS_MALLOC(&cyPollerArgs, numInstances * sizeof(cryptoPollingArgs));

  for(int i=0; i<numInstances; i++){
    OS_MALLOC(&cyPollerArgs[i], sizeof(cryptoPollingArgs));
    cyPollerArgs[i]->cyInstHandle = cyInstHandles[i];
    cyPollerArgs[i]->id = i;
    createThreadPinned(&cyPollers[i], sal_polling, cyPollerArgs[i], firstPollingLogical + i);
  }



  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}