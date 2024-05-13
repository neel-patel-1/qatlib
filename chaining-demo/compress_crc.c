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
#include "smt-thread-exps.h"

#include "tests.h"

int gDebugParam = 1;
extern _Atomic int gMultiPollers[MAX_INSTANCES + MAX_DSAS];


// poll an arbitrary number of response rings using their respective interfaces following a polling priority associated with each response ring
// maybe round robin is good enough
// can multiAcc Poller poll all the rings we give it?
 typedef struct _multiAccelPollerArgs {
  int numDcInstances;
  CpaInstanceHandle *dcInstances;
  int numCyInstances;
  CpaInstanceHandle *cyInstances;

  int numCtxs;
  struct acctest_context **ctxs;

  int id;

  int *completedOperations;
  int neededOps;
 } multiAccelPollerArgs;

void multiAccelPoller(void *arg){
  multiAccelPollerArgs *mArgs = (multiAccelPollerArgs *)arg;
  CpaInstanceHandle *dcInstances = mArgs->dcInstances;
  CpaInstanceHandle *cyInstances = mArgs->cyInstances;
  struct acctest_context **ctxs = mArgs->ctxs;

  int numDcInstances = mArgs->numDcInstances;
  int numCyInstances = mArgs->numCyInstances;
  int numCtxs = mArgs->numCtxs;

  struct task_node *taskNode = NULL;
  struct task *tsk = NULL;
  struct completion_record *comp = NULL;

  int id = mArgs->id;

  /* need the multi-task nodes from each ctx */
  struct task_node **taskNodes = malloc(numCtxs * sizeof(struct task_node*));
  int i;
  for(i=0; i<numCtxs; i++){
    taskNodes[i] = ctxs[i]->multi_task_node;
  }

  /* Poll all taskNode's Ctxs in RR order */
  /* How does the host know that all offloads have completed?*/
  /* the polling thread serves no purpose unless the host gets to know when the offloads are completed and perform some action in response */
  /* In reality, there should be some callback performed each time the response is received -- here it is just checking whether this is the last offload and updating the polling thread's monitoring set*/
  /* There is a total number of offloads expected to be completed on each accelerator in a stream, there is a total number of offloads expected to be completed per stream */
  int numFinished = 0;
  int streamsCompleted = 0;
  int neededOps = mArgs->neededOps;
  int *completedOperations = mArgs->completedOperations;
  /* what if we would like a single polling thread to poll multiple streams ? - At the end of the day, the host can just stop the polling thread once all the offloads for the streams are
  completed. Here we just count and compare against the total number of expected operations across all accelerators in all streams. */
  while(gMultiPollers[id]){
    /*accel-cfg type*/
    for(int i=0; i<numCtxs; i++){
      taskNode = taskNodes[i];
      if(taskNode){
        tsk = taskNode->tsk;
        if(comp->status != 0){
          /* Callback function */
          (*completedOperations) ++ ;
          if((*completedOperations) >= neededOps){
            gMultiPollers[id] = 0; /* We can stop ourself */
          }

          /* update next wait response */
          taskNode = taskNode->next;
        }
      }
    }
    /*cpa type*/
    for(int i=0 ; i<numDcInstances; i++){
      icp_sal_DcPollInstance(dcInstances[i], 0);
      if((*completedOperations) >= neededOps){
        gMultiPollers[id] = 0; /* We can stop ourself */
      }
    }
    for(int i=0; i<numCyInstances; i++){
      icp_sal_CyPollInstance(cyInstances[i], 0);
      if((*completedOperations) >= neededOps){
        gMultiPollers[id] = 0; /* We can stop ourself */
      }
    }
  }
  sampleThreadExit();

}
typedef struct dcCountCallbackArgs {
  int *count;
} dcCountCallbackArgs;

void dcCountCallback(void *pCallbackTag, CpaStatus status){
  dcCountCallbackArgs *args = (dcCountCallbackArgs *)pCallbackTag;
  int *count = args->count;
  (*count)++;
}

typedef struct cyCountCallbackArgs {
  int *count;
} cyCountCallbackArgs;

void cyCountCallback(void *pCallbackTag,
                        CpaStatus status,
                        const CpaCySymOp operationType,
                        void *pOpData,
                        CpaBufferList *pDstBuffer,
                        CpaBoolean verifyResult){
  cyCountCallbackArgs *args = (cyCountCallbackArgs *)pCallbackTag;
  (*args->count)++;
                        }

void multiAccelPollerDsaTest(){
  int numCyInsts = 0;
  int dsaInsts = 1;
  int numDcInsts = 0;
  int iaaInsts = 0;
  int totalAccels = numCyInsts + dsaInsts + numDcInsts + iaaInsts;
  int totalStreams = 1;
  int numOpsPerStream = 1;

  int totalOpsAcrossStreams = numOpsPerStream * totalAccels * totalStreams;

  CpaInstanceHandle dcInstHandles[numDcInsts];
  CpaDcSessionHandle dcSessionHandles[numDcInsts];

  struct acctest_context *ctxs[dsaInsts];
  struct task_node *taskNodes[dsaInsts];
  int tflags = TEST_FLAGS_BOF;
  int rc;
  int wq_type = 0;
  int dev_id = 0;
  int wq_id = 0;

  int taskNodesPerDsa = numOpsPerStream/totalAccels; /*each accel can be used once per stream*/

  CpaBufferList *srcBuffers[numOpsPerStream];
  Cpa8U *srcBuffer = NULL;
  Cpa8U *pBufferMetaSrc = NULL;


  for(int i=0; i<dsaInsts; i++){
    ctxs[i] = malloc(sizeof(struct acctest_context));
    ctxs[i] = acctest_init(tflags);
    rc = acctest_alloc(ctxs[i],wq_type,dev_id,wq_id);
    acctest_alloc_multiple_tasks(ctxs[i],taskNodesPerDsa);

    /*
    @ name: generateBufferList
    @ description: generate a buffer list with data
    @ input: CpaBufferList **ppBufferList, Cpa32U bufferSize, Cpa32U bufferListMetaSize, Cpa32U numBuffers
    */
    Cpa32U numBuffers = 1;
    Cpa32U bufferSize = 4096;
    Cpa32U bufferListMemSize = sizeof(CpaBufferList) + (numBuffers * sizeof(CpaFlatBuffer));
    Cpa32U bufferMetaSize = 0;
    generateBufferList(&srcBuffers[i], bufferSize, bufferListMemSize, numBuffers, prepareZeroSampleBuffer);

    taskNodes[i] = ctxs[i]->multi_task_node;
    while(taskNodes[i]){
      PRINT_DBG("taskNode 0x%p\n", taskNodes[i]->tsk);
      prepare_crc_task(
        taskNodes[i]->tsk,
        ctxs[i],
        srcBuffers[i]->pBuffers[0].pData,
        srcBuffers[i]->pBuffers[0].dataLenInBytes);
      taskNodes[i] = taskNodes[i]->next;
      for(int j=0; j<bufferSize; j++){
        PRINT_DBG("%d ", srcBuffers[i]->pBuffers[0].pData[j]);
      }
    }
  }

  // prepareMultipleCompressAndCrc64InstancesAndSessions(dcInstHandles, dcSessionHandles, numDcInsts, numDcInsts);

  /* Assume a single CpaDc + dsa stream - total ops is 2*/
  CpaInstanceHandle dcInstHandle = dcInstHandles[0];

  /* get a dc inst and generate a request for a compression operation */
  /* share the count variable with the call backs and poller thread -- don't allocate, the poller thread will allocate and terminate itself */
  CpaDcOpData *dcOpData = NULL;
  OS_MALLOC(&dcOpData, sizeof(CpaDcOpData));

  /* get a dsa ctx and generate a request that the dc callback can use to submit -- put it in the callback args*/
}


int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;
  Cpa16U numInstances = 0;
  CpaInstanceHandle dcInstHandle = NULL;
  CpaDcSessionHandle sessionHandle = NULL;
  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcSessionHandle sessionHandles[MAX_INSTANCES];

  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);

  allocateDcInstances(dcInstHandles, &numInstances);
  if (0 == numInstances)
  {
    fprintf(stderr, "No instances found\n");
    return CPA_STATUS_FAIL;
  }

  multiAccelPollerDsaTest();


exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}