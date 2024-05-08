#ifndef BUFFER_PREPARE_FUNCS_H
#define BUFFER_PREPARE_FUNCS_H
#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"
#include "thread_utils.h"
#include "dc_inst_utils.h"

CpaStatus prepareTestBufferLists(CpaBufferList ***pSrcBufferLists, CpaBufferList ***pDstBufferLists,
  CpaInstanceHandle dcInstHandle, Cpa32U bufferSize, Cpa32U numBufferLists);

CpaStatus multiBufferTestAllocations(callback_args ***pcb_args, packet_stats ***pstats,
  CpaDcOpData ***popData, CpaDcRqResults ***pdcResults, CpaCrcData **pCrcData,
  Cpa32U numOperations, Cpa32U bufferSize, CpaDcInstanceCapabilities cap,
  CpaBufferList ***pSrcBufferLists, CpaBufferList ***pDstBufferLists,
  CpaInstanceHandle dcInstHandle, struct COMPLETION_STRUCT *complete);

CpaStatus prepareSampleBuffer(Cpa8U **ppBuffer, Cpa32U bufferSize);
CpaStatus createDstBufferList(CpaBufferList **ppBufferList, Cpa32U bufferSize, CpaInstanceHandle dcInstHandle, CpaDcHuffType huffType);
CpaStatus createSourceBufferList(CpaBufferList **ppBufferList, Cpa8U *buffer, Cpa32U bufferSize, CpaInstanceHandle dcInstHandle, CpaDcHuffType huffType);


#endif