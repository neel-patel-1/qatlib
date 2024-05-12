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

int prepareFlatBufferList(CpaBufferList **ppBufferList, int numBuffers, int bufferSize){
  CpaStatus status = CPA_STATUS_SUCCESS;
  CpaBufferList *pBufferList = NULL;
  CpaFlatBuffer *pFlatBuffers = NULL;

  status = OS_MALLOC(&pBufferList, sizeof(CpaBufferList));
  if (CPA_STATUS_SUCCESS == status)
  {
      status = PHYS_CONTIG_ALLOC(&pFlatBuffers, numBuffers * sizeof(CpaFlatBuffer));
      if (CPA_STATUS_SUCCESS == status)
      {
          pBufferList->pBuffers = pFlatBuffers;
          pBufferList->numBuffers = numBuffers;
          for (int i = 0; i < numBuffers; i++)
          {
              status = PHYS_CONTIG_ALLOC(&(pFlatBuffers[i].pData), bufferSize);
              if (CPA_STATUS_SUCCESS != status)
              {
                  PRINT_ERR("Failed to allocate memory for buffer %d\n", i);
                  break;
              }
              pFlatBuffers[i].dataLenInBytes = bufferSize;
          }
      }
      else
      {
          PRINT_ERR("Failed to allocate memory for flat buffers\n");
      }
  }
  else
  {
      PRINT_ERR("Failed to allocate memory for buffer list\n");
  }

  *ppBufferList = pBufferList;
  return status;
}


/* AES key, 256 bits long */
static Cpa8U sampleCipherKey[] = {
    0xEE, 0xE2, 0x7B, 0x5B, 0x10, 0xFD, 0xD2, 0x58, 0x49, 0x77, 0xF1, 0x22,
    0xD7, 0x1B, 0xA4, 0xCA, 0xEC, 0xBD, 0x15, 0xE2, 0x52, 0x6A, 0x21, 0x0B,
    0x41, 0x4C, 0x41, 0x4E, 0xA1, 0xAA, 0x01, 0x3F};

/* Initialization vector */
static Cpa8U sampleCipherIv[] = {
    0x7E, 0x9B, 0x4C, 0x1D, 0x82, 0x4A, 0xC5, 0xDF, 0x99, 0x4C, 0xA1, 0x44,
    0xAA, 0x8D, 0x37, 0x27};

static Cpa8U sampleCipherSrc[] = {
    0xD7, 0x1B, 0xA4, 0xCA, 0xEC, 0xBD, 0x15, 0xE2, 0x52, 0x6A, 0x21, 0x0B,
    0x81, 0x77, 0x0C, 0x90, 0x68, 0xF6, 0x86, 0x50, 0xC6, 0x2C, 0x6E, 0xED,
    0x2F, 0x68, 0x39, 0x71, 0x75, 0x1D, 0x94, 0xF9, 0x0B, 0x21, 0x39, 0x06,
    0xBE, 0x20, 0x94, 0xC3, 0x43, 0x4F, 0x92, 0xC9, 0x07, 0xAA, 0xFE, 0x7F,
    0xCF, 0x05, 0x28, 0x6B, 0x82, 0xC4, 0xD7, 0x5E, 0xF3, 0xC7, 0x74, 0x68,
    0xCF, 0x05, 0x28, 0x6B, 0x82, 0xC4, 0xD7, 0x5E, 0xF3, 0xC7, 0x74, 0x68,
    0x80, 0x8B, 0x28, 0x8D, 0xCD, 0xCA, 0x94, 0xB8, 0xF5, 0x66, 0x0C, 0x00,
    0x5C, 0x69, 0xFC, 0xE8, 0x7F, 0x0D, 0x81, 0x97, 0x48, 0xC3, 0x6D, 0x24};



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

CpaStatus populateAesGcmSessionSetupData(CpaCySymSessionSetupData *pSessionSetupData){
  CpaCySymSessionSetupData sessionSetupData;
  sessionSetupData.sessionPriority = CPA_CY_PRIORITY_NORMAL;
  sessionSetupData.symOperation = CPA_CY_SYM_OP_CIPHER;
  sessionSetupData.cipherSetupData.cipherAlgorithm =
      CPA_CY_SYM_CIPHER_AES_CBC;
  sessionSetupData.cipherSetupData.pCipherKey = sampleCipherKey;
  sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
      sizeof(sampleCipherKey);
  sessionSetupData.cipherSetupData.cipherDirection =
      CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;
  *pSessionSetupData = sessionSetupData;
}

CpaStatus initializeSymInstancesAndSessions(
  CpaInstanceHandle *cyInstHandles,
  CpaCySymSessionCtx *sessionCtxs,
  Cpa16U *pNumInstances,
  CpaCySymSessionSetupData sessionSetupData
  ){

  // CpaInstanceHandle cyInstHandles[MAX_INSTANCES];
  Cpa16U numInstances;
  CpaStatus status = cpaCyGetNumInstances(&numInstances);
  Cpa32U pSessionCtxSizeInBytes;
  // CpaCySymSessionCtx sessionCtxs[MAX_INSTANCES];

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

  cpaCySymSessionCtxGetDynamicSize(cyInstHandles[0],
    &sessionSetupData, &pSessionCtxSizeInBytes);

  if (CPA_STATUS_SUCCESS == status) {
      PRINT_DBG("cpaCySymInitSession\n");
      for(int i=0; i<numInstances; i++){
        status = PHYS_CONTIG_ALLOC(&sessionCtxs[i], pSessionCtxSizeInBytes);
        if(CPA_STATUS_SUCCESS == status){
          status = cpaCySymInitSession(
              cyInstHandles[i],
              symCallback, /* Callback function */
              &sessionSetupData, /* Session setup data */
              sessionCtxs[i]); /* Output of the function */
        } else{
          PRINT_ERR("Failed to allocate memory for sessionCtx: %d\n", status);
        }
    }
  }
  return status;
}

CpaStatus spawnSymPollers(pthread_t *cyPollers, cryptoPollingArgs **cyPollerArgs, CpaInstanceHandle *cyInstHandles,
  Cpa16U numInstances, cpu_set_t *coreMaps, Cpa32U numCores){

  int logical = 0;
  for(int i=0; i<numInstances; i++){
    cpu_set_t cpuset = coreMaps[i];
    for (int core = 0; core < numCores; core++) {
        if (CPU_ISSET(core, &cpuset)) {
            logical = core;
        }
    }
    OS_MALLOC(&cyPollerArgs[i], sizeof(cryptoPollingArgs));
    cyPollerArgs[i]->cyInstHandle = cyInstHandles[i];
    cyPollerArgs[i]->id = i;
    createThreadPinned(&cyPollers[i], sal_polling, cyPollerArgs[i], logical);
  }

}

int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;

  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);

  int numOperations = 1000;

  Cpa16U numInstances = 1;
  pthread_t cyPollers[numInstances];
  cryptoPollingArgs *cyPollerArgs[numInstances];
  int firstPollingLogical = 5;
  CpaCySymSessionSetupData sessionSetupData;

  CpaCySymSessionCtx sessionCtxs[MAX_INSTANCES];
  CpaInstanceHandle cyInstHandles[MAX_INSTANCES];

  CpaCySymOpData *pOpData = NULL;
  Cpa8U *pIvBuffer = NULL;

  populateAesGcmSessionSetupData(&sessionSetupData);
  status =
    initializeSymInstancesAndSessions(cyInstHandles,
    sessionCtxs, &numInstances, sessionSetupData);

  if(CPA_STATUS_SUCCESS != status){
    PRINT_ERR("Failed to initialize Sym Instances and Sessions\n");
    return status;
  }

  /* Populate Core mapping sequential */
  cpu_set_t coreMaps[numInstances];
  for(int i=0; i<numInstances; i++){
    CPU_ZERO(&coreMaps[i]);
    CPU_SET(i, &coreMaps[i]);
  }
  spawnSymPollers(cyPollers, cyPollerArgs, cyInstHandles, numInstances, coreMaps, numInstances);

  /*Generate BufferList With One Flat Buffer */
  Cpa8U *pSrcBuffer = NULL;
  CpaBufferList *pBufferList = NULL;
  status = prepareFlatBufferList(&pBufferList, 1, 4096);
  pSrcBuffer = pBufferList->pBuffers[0].pData;

  /* Setup Operational Data for encrypting a single Fltbflist with a single phys_contig flatbuf*/
  status = OS_MALLOC(&pOpData, sizeof(CpaCySymOpData));
  if (CPA_STATUS_SUCCESS != status)
  {
      PRINT_ERR("Failed to allocate memory for operational data\n");
      return status;
  }
    /*
      * Populate the structure containing the operational data needed
      * to run the algorithm:
      * - packet type information (the algorithm can operate on a full
      *   packet, perform a partial operation and maintain the state or
      *   complete the last part of a multi-part operation)
      * - the initialization vector and its length
      * - the offset in the source buffer
      * - the length of the source message
      */
    status = PHYS_CONTIG_ALLOC(&pIvBuffer, sizeof(sampleCipherIv));
    memcpy(pSrcBuffer, sampleCipherSrc, sizeof(sampleCipherSrc));

    struct COMPLETION_STRUCT complete;
    COMPLETION_INIT(&complete);

    //<snippet name="opData">
    pOpData->sessionCtx = sessionCtxs[0];
    pOpData->packetType = CPA_CY_SYM_PACKET_TYPE_FULL;
    pOpData->pIv = pIvBuffer;
    pOpData->ivLenInBytes = sizeof(sampleCipherIv);
    pOpData->cryptoStartSrcOffsetInBytes = 0;
    pOpData->messageLenToCipherInBytes = sizeof(sampleCipherSrc);
    status = cpaCySymPerformOp(
      cyInstHandles[0],
      (void *)&complete, /* data sent as is to the callback function*/
      pOpData,           /* operational data struct */
      pBufferList,       /* source buffer list */
      pBufferList,       /* same src & dst for an in-place operation*/
      NULL);
    //</snippet>

  COMPLETION_WAIT(&complete, 50);

  /* Submit Sym Requests */
  pthread_join(cyPollers[0], NULL);
  COMPLETION_DESTROY(&complete);


  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}