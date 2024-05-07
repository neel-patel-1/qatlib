#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"

#include "icp_sal_poll.h"
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
CpaStatus prepareDcSession(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle *pSessionHandle);