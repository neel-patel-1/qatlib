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


int gDebugParam = 0;


typedef struct _crc_polling_args{
  struct acctest_context *dsa;
  struct _two_stage_packet_stats **stats;
  int id;
} crc_polling_args;

void *crc_polling(void *args){
  crc_polling_args *crcArgs = (crc_polling_args *)(args);
  int id = crcArgs->id;
  gPollingCrcs[id] = 1;
  struct acctest_context *dsa = crcArgs->dsa;
  struct task_node *tsk_node = crcArgs->dsa->multi_task_node;
  two_stage_packet_stats **stats = crcArgs->stats;
  int received = 0;
  while(tsk_node){
    dsa_wait_crcgen(dsa, tsk_node->tsk);

    PRINT_DBG("Received CR:%d\n", received);
    /* Assume received in order, this has been the case throughout testing*/
    stats[received]->receiveTime = sampleCoderdtsc();
    tsk_node = tsk_node->next;
    received++;
  }
  gPollingCrcs[id] = 0;
}

/* allocate the og and switch submitter / poller over to new tasks from this guy */

/* Enable multiple acctests to use the same work queue */
int acctest_duplicate_context(struct acctest_context *ctx, struct acctest_context *srcCtx){
  ctx->wq = srcCtx->wq;
  ctx->wq_reg = srcCtx->wq_reg;
  ctx->wq_size = srcCtx->wq_size;
  ctx->threshold = srcCtx->threshold;
  ctx->wq_idx = srcCtx->wq_idx;
  ctx->bof = srcCtx->bof;
  ctx->wq_max_batch_size = srcCtx->wq_max_batch_size;
  ctx->wq_max_xfer_size = srcCtx->wq_max_xfer_size;
  ctx->ats_disable = srcCtx->ats_disable;

  ctx->max_batch_size = srcCtx->max_batch_size;
  ctx->max_xfer_size = srcCtx->max_xfer_size;
  ctx->max_xfer_bits = srcCtx->max_xfer_bits;
  ctx->compl_size = srcCtx->compl_size;
  ctx->dedicated = srcCtx->dedicated;

	info("alloc wq %d %s size %d addr %p batch sz %#x xfer sz %#x\n",
	     ctx->wq_idx, (ctx->dedicated == ACCFG_WQ_SHARED) ? "shared" : "dedicated",
	     ctx->wq_size, ctx->wq_reg, ctx->max_batch_size, ctx->max_xfer_size);

	return 0;
}





typedef struct _dsaFwderCbArgs {
  Cpa32U packetId;
  struct acctest_context *dsa;
  struct task *toSubmit;
  struct _two_stage_packet_stats **stats;
} dsa_fwder_args;

void dcDsaCrcCallback(void *pCallbackTag, CpaStatus status){
  PRINT_DBG("DSA Fwding Callback Invoked\n");
  dsa_fwder_args *args = (dsa_fwder_args *)pCallbackTag;
  struct acctest_context *dsa;
  struct task *toSubmit;
  struct _two_stage_packet_stats **stats;
  Cpa32U idx;

  if(NULL != pCallbackTag){
    dsa = args->dsa;
    stats = args->stats;
    idx = args->packetId;
    stats[idx]->cbReceiveTime = sampleCoderdtsc();
    toSubmit = args->toSubmit;
    single_crc_submit_task(dsa, toSubmit);
  }
}

typedef struct _two_stage_packet_stats two_stage_packet_stats;

CpaStatus submitAndStampBeforeDSAFwdingCb(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle sessionHandle, CpaBufferList *pBufferListSrc,
  CpaBufferList *pBufferListDst, CpaDcOpData *opData, CpaDcRqResults *pDcResults, dsa_fwder_args *cb_args, int index){

  CpaStatus status = CPA_STATUS_SUCCESS;
  two_stage_packet_stats *stats = cb_args->stats[index];
  Cpa64U submitTime = sampleCoderdtsc();
  stats->submitTime = submitTime;
retry:
  status = cpaDcCompressData2(
    dcInstHandle,
    sessionHandle,
    pBufferListSrc,     /* source buffer list */
    pBufferListDst,     /* destination buffer list */
    opData,            /* Operational data */
    pDcResults,         /* results structure */
    (void *)cb_args); /* data sent as is to the callback function*/
  if(status == CPA_STATUS_RETRY){
    /* don't forget to retry on CPA_STATUS_RETRY for the QAT submission, we will overflow */
    goto retry;
  }
  return status;

}

CpaStatus prep_crc_test_cb_fwd_args(
  dsa_fwder_args *** pArrayCbArgsPtrs,
  struct acctest_context * dsa,
  two_stage_packet_stats **arrayPktStatsPtrs,
  int numOperations
){
  dsa_fwder_args **args;
  PHYS_CONTIG_ALLOC(&(args), sizeof(dsa_fwder_args*)*numOperations);
  struct task_node *waitTaskNode;
  int rc = CPA_STATUS_SUCCESS;

  waitTaskNode = dsa->multi_task_node;
  for(int argsIdx=0; argsIdx<numOperations; argsIdx++){
    if(waitTaskNode == NULL){
      rc = CPA_STATUS_FAIL;
      PRINT_ERR("Multi-Task Node has insufficient tasks\n");
      break;
    }
    /* args */
    PHYS_CONTIG_ALLOC(&(args[argsIdx]), sizeof(dsa_fwder_args));
    args[argsIdx]->dsa=dsa;
    args[argsIdx]->toSubmit = waitTaskNode->tsk;
    args[argsIdx]->stats = arrayPktStatsPtrs;
    args[argsIdx]->packetId = argsIdx; /* assume packets will be seen in order at dsa*/
    waitTaskNode = waitTaskNode->next;
  }
  *pArrayCbArgsPtrs = args;
  return rc;
}

CpaStatus alloc_crc_test_packet_stats(
  struct acctest_context * dsa,
  two_stage_packet_stats ***pArrayPktStatsPtrs,
  int numOperations)
{
  two_stage_packet_stats **stats2Phase = NULL;
  PHYS_CONTIG_ALLOC(&stats2Phase, sizeof(two_stage_packet_stats*) * numOperations);
  for(int argsIdx=0; argsIdx<numOperations; argsIdx++){
    PHYS_CONTIG_ALLOC(&(stats2Phase[argsIdx]), sizeof(two_stage_packet_stats));
    if(NULL == memset(stats2Phase[argsIdx], 0, sizeof(two_stage_packet_stats))){
      PRINT_ERR("Failed to MemSet pktstats\n");
      return CPA_STATUS_FAIL;
    }
  }
  *pArrayPktStatsPtrs = stats2Phase;
  return CPA_STATUS_SUCCESS;
}

void create_crc_polling_thread(struct acctest_context *dsa, int id, two_stage_packet_stats **stats, pthread_t *tid){
  crc_polling_args *crcArgs;
  OS_MALLOC(&crcArgs, sizeof(crc_polling_args));
  crcArgs->dsa=dsa;
  crcArgs->id = id;
  crcArgs->stats = stats;
  createThreadJoinable(tid,crc_polling, crcArgs);
}

void create_dc_polling_thread(struct acctest_context *dsa, int flowId,
  pthread_t *tid, CpaInstanceHandle dcInstHandle)
{
  dc_crc_polling_args *dcCrcArgs = NULL;
  OS_MALLOC(&dcCrcArgs, sizeof(dc_crc_polling_args));
  dcCrcArgs->dcInstance = dcInstHandle;
  dcCrcArgs->id = flowId;
  createThread(tid, dc_crc64_polling, dcCrcArgs);
}

CpaStatus submit_all_comp_crc_requests(
  int numOperations,
  CpaInstanceHandle dcInstHandle,
  CpaDcSessionHandle sessionHandle,
  CpaBufferList **srcBufferLists,
  CpaBufferList **dstBufferLists,
  CpaDcOpData **opData,
  CpaDcRqResults **dcResults,
  dsa_fwder_args **args
){
  CpaStatus status = CPA_STATUS_SUCCESS;
  int bufIdx = 0;
  while(bufIdx < numOperations){
    status |= submitAndStampBeforeDSAFwdingCb(dcInstHandle,
      sessionHandle,
      srcBufferLists[bufIdx],
      dstBufferLists[bufIdx],
      opData[bufIdx],
      dcResults[bufIdx],
      args[bufIdx],
      bufIdx);

    bufIdx++;
  }
  return status;
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
  prepareDcSession(dcInstHandle, &sessionHandle, dcDsaCrcCallback);


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
  pthread_t crcTid, dcToCrcTid;
  struct task *waitTask;
  Cpa8U *buf;
  struct task_node *waitTaskNode;

  dsa_fwder_args **args;
  int argsIdx = 0;
  int flowId = 0;

  /* Need a specialized two-stage packet stats structure */
  two_stage_packet_stats **stats2Phase = NULL;
  dc_crc_polling_args *dcCrcArgs = NULL;

  // struct acctest_context *crc32Ctx = NULL;
  // crc32Ctx = acctest_init(TEST_FLAGS_BOF);
  // rc = acctest_duplicate_context(crc32Ctx, dsa );
  // if(rc < 0){
  //   PRINT_ERR("Failed to allocate shared wq for crc32Ctx\n");
  //   goto exit;
  // }
  // return rc;

  /* Create args, stats packet stats, and dsa descs for the callback function to submit*/
  create_tsk_nodes_for_stage2_offload(srcBufferLists, numOperations, dsa);
  rc =alloc_crc_test_packet_stats(
    dsa,
    &stats2Phase,
    numOperations
  );
  if(rc != CPA_STATUS_SUCCESS){
    return CPA_STATUS_FAIL;
  }
  rc = prep_crc_test_cb_fwd_args(
    &args,
    dsa,
    stats2Phase,
    numOperations
  );
  if(rc != CPA_STATUS_SUCCESS){
    return CPA_STATUS_FAIL;
  }
  /* Create polling thread for DSA */
  create_crc_polling_thread(dsa, flowId, stats2Phase, &crcTid);

  /* Create intermediate polling thread for forwarding */
  create_dc_polling_thread(dsa, flowId, &dcToCrcTid, dcInstHandle);

  /* Submit to dcInst */
  // bufIdx = 0;
  // while(bufIdx < numOperations){
  //   rc = submitAndStampBeforeDSAFwdingCb(dcInstHandle,
  //     sessionHandle,
  //     srcBufferLists[bufIdx],
  //     dstBufferLists[bufIdx],
  //     opData[bufIdx],
  //     dcResults[bufIdx],
  //     args[bufIdx],
  //     bufIdx);

  //   bufIdx++;
  // }
    rc = submit_all_comp_crc_requests(
      numOperations,
      dcInstHandle,
      sessionHandle,
      srcBufferLists,
      dstBufferLists,
      opData,
      dcResults,
      args);

  pthread_join(crcTid, NULL);
  gPollingDcs[flowId] = 0;

  /* verify all crcs */
  rc = verifyCrcTaskNodes(dsa->multi_task_node, srcBufferLists, bufferSize);

  threadStats2P *thrStats = NULL;
  populate2PhaseThreadStats(stats2Phase, &thrStats, numOperations, bufferSize);
  printTwoPhaseSingleThreadStatsSummary(thrStats);



  if( CPA_STATUS_SUCCESS != rc ){
    PRINT_ERR("Invalid CRC\n");
  }

exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}