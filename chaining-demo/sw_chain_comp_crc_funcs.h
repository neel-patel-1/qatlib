#ifndef SW_CHAIN_COMP_CRC_FUNCS
#define SW_CHAIN_COMP_CRC_FUNCS
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


typedef struct _crc_polling_args{
  struct acctest_context *dsa;
  struct _two_stage_packet_stats **stats;
  int id;
} crc_polling_args;

 typedef struct _multi_idxd_poller_args {

  int numCtxs;
  struct acctest_context **ctxs;

  int id;

 } multi_idxd_poller_args;

typedef struct _dsaFwderCbArgs {
  Cpa32U packetId;
  struct acctest_context *dsa;
  struct task *toSubmit;
  struct _two_stage_packet_stats **stats;
} dsa_fwder_args;

typedef struct _compCrcStreamThreadArgs{
  Cpa32U numOperations;
  Cpa32U bufferSize;
  struct acctest_context *dsa;
  int tflags;
  CpaInstanceHandle dcInstHandle;
  CpaDcSessionHandle sessionHandle;
  CpaDcInstanceCapabilities cap;
  int flowId;
  two_stage_packet_stats ***pStats;
  pthread_barrier_t *barrier;
  int coreId;
} compCrcStreamThreadArgs;

typedef struct _two_stage_packet_stats two_stage_packet_stats;

void *crc_polling(void *args);
void *crc_multi_polling(void *args);


void dcDsaCrcCallback(void *pCallbackTag, CpaStatus status);

CpaStatus submitAndStampBeforeDSAFwdingCb(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle sessionHandle, CpaBufferList *pBufferListSrc,
  CpaBufferList *pBufferListDst, CpaDcOpData *opData, CpaDcRqResults *pDcResults, dsa_fwder_args *cb_args, int index);

CpaStatus prep_crc_test_cb_fwd_args(
  dsa_fwder_args *** pArrayCbArgsPtrs,
  struct acctest_context * dsa,
  two_stage_packet_stats **arrayPktStatsPtrs,
  int numOperations
);

CpaStatus alloc_crc_test_packet_stats(
  struct acctest_context * dsa,
  two_stage_packet_stats ***pArrayPktStatsPtrs,
  int numOperations);

void create_crc_polling_thread(struct acctest_context *dsa, int id, two_stage_packet_stats **stats, pthread_t *tid);

void create_dc_polling_thread(int flowId,
  pthread_t *tid, CpaInstanceHandle dcInstHandle);

CpaStatus submit_all_comp_crc_requests(
  int numOperations,
  CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle,
  CpaBufferList **srcBufferLists,
  CpaBufferList **dstBufferLists,
  CpaDcOpData **opData,
  CpaDcRqResults **dcResults,
  dsa_fwder_args **args
);

CpaStatus compCrcStream(Cpa32U numOperations,
  Cpa32U bufferSize,
  struct acctest_context *ogDsa, int tflags,
  CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle,
  CpaDcInstanceCapabilities cap,
  int flowId,
  two_stage_packet_stats ***pStats,
  pthread_barrier_t *barrier);

void populateCrcStreamThreadArgs(compCrcStreamThreadArgs *args,
  Cpa32U numOperations,
  Cpa32U bufferSize,
  struct acctest_context *dsa,
  int tflags,
  CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle,
  CpaDcInstanceCapabilities cap,
  int flowId,
  two_stage_packet_stats ***pStats,
  pthread_barrier_t *barrier);

void *compCrcStreamThreadFn(void *args);

CpaStatus cpaDcDsaCrcPerf(
  Cpa32U numOperations,
  Cpa32U bufferSize ,
  int numFlows,
  CpaInstanceHandle *dcInstHandles,
  CpaDcSessionHandle* sessionHandles
);

#endif