#ifndef HW_COMP_CRC_FUNCS_H
#define HW_COMP_CRC_FUNCS_H
#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"
#include "thread_utils.h"
#include "dc_inst_utils.h"
#include "buffer_prepare_funcs.h"
#include "validate_compress_and_crc.h"
#include "print_funcs.h"

typedef void (*dc_callback)(void *pCallbackTag, CpaStatus status);

void createCompressCrc64Submitter(
  CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  packet_stats ***pArrayPacketStatsPtrs,
  pthread_barrier_t *pthread_barrier,
  pthread_t *threadId
  );


CpaStatus prepareMultipleCompressAndCrc64InstancesAndSessions(CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances, Cpa16U numSessions);

CpaStatus prepareMultipleDcInstancesAndSessions(CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances, Cpa16U numSessions, dc_callback callback);

void createCompressCrc64Poller(CpaInstanceHandle dcInstHandle, Cpa16U id, pthread_t *threadId);

CpaStatus submitAndStamp(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle sessionHandle, CpaBufferList *pBufferListSrc,
  CpaBufferList *pBufferListDst, CpaDcOpData *opData, CpaDcRqResults *pDcResults, callback_args *cb_args, int index);


CpaStatus singleStreamOfCompressAndCrc64(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle sessionHandle,
  Cpa32U numOperations, Cpa32U bufferSize, packet_stats ***ppStats, pthread_barrier_t *barrier);

void *streamFn(void *arg);

CpaStatus doSubmissionsCompressAndCrc64AndWaitForFinal(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle sessionHandle,
  CpaBufferList **srcBufferLists, CpaBufferList **dstBufferLists, CpaDcOpData **opData, CpaDcRqResults **dcResults,
  callback_args **cb_args, Cpa32U numOperations, struct COMPLETION_STRUCT *complete);

void multiStreamCompressCrc64PerformanceTest(
  Cpa32U numFlows,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  CpaInstanceHandle *dcInstHandles,
  CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances
);

CpaStatus functionalCompressAndCrc64(CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle);

#endif