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

int gDebugParam = 0;

typedef struct _crc_polling_args{
  struct acctest_context *dsa;
  int id;
} crc_polling_args;

void *crc_polling(void *args){
  crc_polling_args *crcArgs = (crc_polling_args *)(args);
  int id = crcArgs->id;
  gPollingCrcs[id] = 1;
  struct acctest_context *dsa = crcArgs->dsa;
  struct task_node *tsk_node = crcArgs->dsa->multi_task_node;
  while(tsk_node){
    dsa_wait_crcgen(dsa, tsk_node->tsk);
    tsk_node = tsk_node->next;
  }
  gPollingCrcs[id] = 0;
}

typedef struct dc_crc_polling_args{
  CpaInstanceHandle dcInstance;
  Cpa32U id;
} dc_crc_polling_args;

void *dc_crc64_polling(void *args){
  dc_crc_polling_args *dcArgs = (dc_crc_polling_args *)args;
  CpaInstanceHandle dcInstance = dcArgs->dcInstance;
  Cpa32U id = dcArgs->id;
  gPollingDcs[id] = 1;
  while(gPollingDcs[id] == 1){
    icp_sal_DcPollInstance(dcInstance, 0); /* Poll here, forward in callback */
  }
}

/* Callback needs to know which task to submit to DSA */
/* Who populates - initial submitter
  How to populate with the right task to submit
  what does sub
*/
typedef struct _dsaFwderCbArgs {
  struct acctest_context *dsa;
  struct task *toSubmit;
  Cpa64U intermediateTimestamp; /* allows us to see dsa crc32 latency */
} dsa_fwder_args;

void dcDsaCrcCallback(void *pCallbackTag, CpaStatus status){
  PRINT_DBG("DSA Fwding Callback Invoked\n");
  dsa_fwder_args *args = (dsa_fwder_args *)pCallbackTag;
  struct acctest_context *dsa;
  struct task *toSubmit;

  if(NULL != pCallbackTag){
    dsa = args->dsa;
    args->intermediateTimestamp = sampleCoderdtsc();
    toSubmit = args->toSubmit;
    single_crc_submit_task(dsa, toSubmit);
  }
}


int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;
  Cpa16U numInstances = 0;
  CpaInstanceHandle dcInstHandle = NULL;
  CpaDcSessionHandle sessionHandle = NULL;
  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcSessionHandle sessionHandles[MAX_INSTANCES];
  CpaDcInstanceCapabilities cap = {0};

  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);

  allocateDcInstances(dcInstHandles, &numInstances);
  if (0 == numInstances)
  {
    fprintf(stderr, "No instances found\n");
    return CPA_STATUS_FAIL;
  }
  dcInstHandle = dcInstHandles[0];
  prepareDcInst(&dcInstHandles[0]);


  Cpa32U numOperations = 1000;
  Cpa32U bufferSize = 1024;
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaDcOpData **opData = NULL;
  struct COMPLETION_STRUCT complete;

  multiBufferTestAllocations(
    &cb_args,
    &stats,
    &opData,
    &dcResults,
    &crcData,
    numOperations,
    bufferSize,
    cap,
    &srcBufferLists,
    &dstBufferLists,
    dcInstHandle,
    &complete
  );

  /* sudo ..//setup_dsa.sh -d dsa0 -w1 -ms -e4 */
  struct acctest_context *dsa = NULL;
  int tflags = TEST_FLAGS_BOF;
  int rc;
  int wq_type = ACCFG_WQ_SHARED;
  int dev_id = 0;
  int wq_id = 0;
  int opcode = 16;
  struct task *tsk;

  /* Use seeded value */
  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;

  if (!dsa)
		return -ENOMEM;

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
	if (rc < 0)
		return -ENOMEM;

  int bufIdx = 0;
  crc_polling_args *crcArgs = NULL;
  int tid=0;
  pthread_t pTid;
  struct task *waitTask;
  Cpa8U *buf;
  struct task_node *waitTaskNode;

  dsa_fwder_args **args;
  int argsIdx = 0;


  /* Create args and dsa descs for the callback function to submit*/
  PHYS_CONTIG_ALLOC(&(args), sizeof(dsa_fwder_args *)*numOperations);
  create_tsk_nodes_for_stage2_offload(srcBufferLists, numOperations, dsa);
  waitTaskNode = dsa->multi_task_node;
  while(waitTaskNode){
    PHYS_CONTIG_ALLOC(&(args[argsIdx]), sizeof(dsa_fwder_args));
    args[argsIdx]->dsa=dsa;
    args[argsIdx]->toSubmit = waitTaskNode->tsk;
    argsIdx++;
    waitTaskNode = waitTaskNode->next;
  }

  /* Create polling thread for DSA */
  OS_MALLOC(&crcArgs, sizeof(crc_polling_args));
  crcArgs->dsa=dsa;
  crcArgs->id = tid;
  createThreadJoinable(&pTid,crc_polling, crcArgs);

  bufIdx = 0;
  while(bufIdx < numOperations){
    dcDsaCrcCallback(args[bufIdx], CPA_STATUS_SUCCESS);
    bufIdx++;
  }

  pthread_join(pTid, NULL);

  bufIdx = 0;
  waitTaskNode = dsa->multi_task_node;
  for(int i=0; i<numOperations; i++){
    waitTask = waitTaskNode->tsk;
    buf = srcBufferLists[bufIdx]->pBuffers[0].pData;

    rc = validateCrc32DSA(waitTask,buf, bufferSize);
    if(rc != CPA_STATUS_SUCCESS){
      PRINT_ERR("DSA CRC32 Incorrect\n");
      break;
    }

    waitTaskNode = waitTaskNode->next;
    bufIdx++;
  }


  if( CPA_STATUS_SUCCESS != rc ){
    goto exit;
  }
  // tsk=dstBufCrcTaskNodes->tsk;
  // single_crc_submit_task(dsa, tsk);

  // singleSubmitValidation(srcBufferLists);
exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}