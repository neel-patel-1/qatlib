#ifndef SW_COMP_CRC_FUNCS_H
#define SW_COMP_CRC_FUNCS_H
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

#include "print_funcs.h"



typedef struct _sw_comp_args{
  Cpa32U numOperations;
  Cpa32U bufferSize;
  CpaInstanceHandle dcInstHandle; /*Preprepared DCInstHandle for getting hw-provided compress bounds -- may not be accurate for sw deflate, but no issues throughout testing*/
  pthread_barrier_t *startSync;
  packet_stats ***pCallerStatsArrayPtrIndex; /* Pointer to the packet stats array entry to populate at completion */
} sw_comp_args;

void syncSwComp(void *args);

CpaStatus deflateCompressAndTimestamp(
  CpaBufferList *pBufferListSrc,
  CpaBufferList *pBufferListDst,
  CpaDcRqResults *pDcResults,
  int index,
  callback_args *cb_args,
  CpaCrcData *crcData
);

void multiStreamSwCompressCrc64Func(Cpa32U numOperations, Cpa32U bufferSize,
  Cpa32U numStreams, CpaInstanceHandle dcInstHandle);

#endif