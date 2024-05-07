#ifndef DC_INST_UTILS_H
#define DC_INST_UTILS_H

#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"
#include "cpa_sample_code_utils.h"

#include "icp_sal_poll.h"
#include "thread_utils.h"
#include <pthread.h>

#define MAX_INSTANCES 16
#define SAMPLE_MAX_BUFF 1024

extern int gDebugParam;

extern CpaDcHuffType huffmanType_g;
extern CpaStatus qaeMemInit(void);
extern void qaeMemDestroy(void);


CpaStatus allocateDcInstances(CpaInstanceHandle *dcInstHandles, Cpa16U *numInstances);

CpaStatus allocateIntermediateBuffers(CpaInstanceHandle dcInstHandle,
  CpaBufferList ***pBufferInterArray,
  Cpa16U *pNumInterBuffLists,
  Cpa32U *pBuffMetaSize);

CpaStatus prepareDcInst(CpaInstanceHandle *pDcInstHandle);
CpaStatus prepareDcSession(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle *pSessionHandle, CpaDcCallbackFn callbackFunction);

CpaStatus prepareSampleBuffer(Cpa8U **ppBuffer, Cpa32U bufferSize);
CpaStatus createDstBufferList(CpaBufferList **ppBufferList, Cpa32U bufferSize, CpaInstanceHandle dcInstHandle, CpaDcHuffType huffType);
CpaStatus createSourceBufferList(CpaBufferList **ppBufferList, Cpa8U *buffer, Cpa32U bufferSize, CpaInstanceHandle dcInstHandle, CpaDcHuffType huffType);


CpaStatus functionalCompressAndCrc64(CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle);

/* Single Flat Buffer BufferList with buffer of bufferSize */


#endif