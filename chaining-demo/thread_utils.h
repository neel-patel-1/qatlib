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

typedef struct _packet_stats{
  Cpa32U packetId;
  Cpa64U submitTime;
  Cpa64U receiveTime;
} packet_stats;

typedef struct _callback_args{
  Cpa32U completedOperations; /* number of offloads completed */
  Cpa32U numOperations; /* number of offloads to complete*/
  struct COMPLETION_STRUCT *completion; /* Use this to communicate last offload completion */
  packet_stats **stats;
} callback_args;

typedef struct _submit_td_args{
  CpaInstanceHandle *dcInstHandle;
  CpaDcSessionHandle *sessionHandle;
  Cpa32U numOperations;
  Cpa32U bufferSize;
} submitter_args;

void *dc_polling(void *args);
CpaStatus createThread(pthread_t *thread, void *func, void *arg);
CpaStatus createThreadJoinable(pthread_t *thread, void *func, void *arg);

void dcLatencyCallback(void *pCallbackTag, CpaStatus status);
void dcPerfCallback(void *pCallbackTag, CpaStatus status);


#endif