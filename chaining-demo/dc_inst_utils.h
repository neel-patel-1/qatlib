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
extern _Atomic int gPollingDcs[MAX_INSTANCES];

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

typedef struct _thread_args{
  CpaInstanceHandle dcInstHandle;
  Cpa16U id;
} thread_args;
void *dc_polling(void *args);
CpaStatus createThread(pthread_t *thread, void *func, void *arg);
void dcLatencyCallback(void *pCallbackTag, CpaStatus status);
