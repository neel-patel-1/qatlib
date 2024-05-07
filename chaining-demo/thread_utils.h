#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H

#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"

#include "icp_sal_poll.h"
#include <pthread.h>

#define MAX_INSTANCES 16
extern int gDebugParam;
extern _Atomic int gPollingDcs[MAX_INSTANCES];

typedef struct _thread_args{
  CpaInstanceHandle dcInstHandle;
  Cpa16U id;
} thread_args;
void *dc_polling(void *args);
CpaStatus createThread(pthread_t *thread, void *func, void *arg);
void dcLatencyCallback(void *pCallbackTag, CpaStatus status);


#endif