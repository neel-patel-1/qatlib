#ifndef SMT_THREAD_EXPS
#define SMT_THREAD_EXPS

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

/* Hw duplicates for Sharing SMT threads */
void multiStreamCompressCrc64PerformanceTestSharedMultiSwPerHwTd(
  Cpa32U numFlows,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  CpaInstanceHandle *dcInstHandles,
  CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances,
  int numTdsPerHwTd
);

CpaStatus createThreadPinned(pthread_t *thread, void *func, void *arg, int coreId);

void createCompressCrc64SubmitterPinned(
  CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  packet_stats ***pArrayPacketStatsPtrs,
  pthread_barrier_t *pthread_barrier,
  pthread_t *threadId,
  int coreId
  );

void createCompressCrc64PollerPinned(CpaInstanceHandle dcInstHandle, Cpa16U id,
pthread_t *threadId, int coreId);


void multiStreamCompressCrc64PerformanceTestSharedSMTThreads(
  Cpa32U numFlows,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  CpaInstanceHandle *dcInstHandles,
  CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances
);

void multiStreamCompressCrc64PerformanceTestSharedMultiSwPerHwTd(
  Cpa32U numFlows,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  CpaInstanceHandle *dcInstHandles,
  CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances,
  int numTdsPerHwTd
);

/* Sw chain duplicates for sharing SMT threads*/

void create_dc_polling_thread_pinned(int flowId,
  pthread_t *tid, CpaInstanceHandle dcInstHandle, Cpa32U coreId);


void create_crc_polling_thread_pinned(struct acctest_context *dsa, int id, two_stage_packet_stats **stats, pthread_t *tid, Cpa32U coreId);

CpaStatus compCrcStreamSharedPhysSubmitPollDcPollCrc(Cpa32U numOperations,
  Cpa32U bufferSize,
  struct acctest_context *ogDsa, int tflags,
  CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle,
  CpaDcInstanceCapabilities cap,
  int flowId,
  two_stage_packet_stats ***pStats,
  pthread_barrier_t *barrier,
  Cpa32U coreId);

void *compCrcStreamSharedSubmitPollCrcPollDcThreadFn(void *args);

/*hardcoded start logical to pass core ID to the ThreadFn, inside the stream ThreadFn, we ___*/
CpaStatus cpaDcDsaCrcPerfSharedSubmitPollCrcPollDcPerf(
  Cpa32U numOperations,
  Cpa32U bufferSize ,
  int numFlows,
  CpaInstanceHandle *dcInstHandles,
  CpaDcSessionHandle* sessionHandles
);


#endif