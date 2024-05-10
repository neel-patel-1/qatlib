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

typedef struct _two_stage_packet_stats{
  Cpa32U packetId;
  Cpa64U submitTime;
  Cpa64U cbReceiveTime;
  Cpa64U receiveTime;
} two_stage_packet_stats;

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

typedef struct _threadStats2P {
  Cpa64U avgLatencyS1;
  Cpa64U avgLatencyS2;
  Cpa64U avgLatency;
  Cpa64U minLatency;
  Cpa64U maxLatency;
  Cpa64U exeTimeUs;
  Cpa32U operations;
  Cpa32U operationSize;
  Cpa32U id;
} threadStats2P;


void populate2PhaseThreadStats(two_stage_packet_stats ** stats2Phase, threadStats2P **pThrStats, Cpa32U numOperations, Cpa32U bufferSize){
/* Print latencies for each phase */
  threadStats2P *thrStats;
  Cpa32U id = 0;
  Cpa32U freqKHz = 2080;
  Cpa64U avgLatency = 0;
  Cpa64U minLatency = UINT64_MAX;
  Cpa64U maxLatency = 0;
  Cpa64U firstSubmitTime = stats2Phase[0]->submitTime;
  Cpa64U lastReceiveTime = stats2Phase[numOperations-1]->receiveTime;
  Cpa64U exeCycles = lastReceiveTime - firstSubmitTime;
  Cpa64U exeTimeUs = exeCycles/freqKHz;
  Cpa64U avgLatencyP2 = 0;
  Cpa64U avgLatencyP1 = 0;
  double offloadsPerSec = numOperations / (double)exeTimeUs;
  offloadsPerSec = offloadsPerSec * 1000000;

  OS_MALLOC(&thrStats, sizeof(threadStats2P));
  for(int i=0; i<numOperations; i++){
    Cpa64U latencyP2 = stats2Phase[i]->receiveTime - stats2Phase[i]->cbReceiveTime;
    Cpa64U latencyP1 = stats2Phase[i]->cbReceiveTime - stats2Phase[i]->submitTime;
    Cpa64U latency = stats2Phase[i]->cbReceiveTime - stats2Phase[i]->submitTime;
    uint64_t e2eMicros = latency / freqKHz;
    uint64_t p1Micros = latencyP1 / freqKHz;
    uint64_t p2Micros = latencyP2 / freqKHz;

    avgLatencyP1 += p1Micros;
    avgLatencyP2 += p2Micros;
    avgLatency += (p1Micros + p2Micros);
    if(e2eMicros < minLatency){
      minLatency = e2eMicros;
    }
    if(e2eMicros > maxLatency){
      maxLatency = e2eMicros;
    }
  }
  avgLatency = avgLatency / numOperations;
  thrStats->avgLatencyS1 = avgLatencyP1 / numOperations;
  thrStats->avgLatencyS2 = avgLatencyP2 / numOperations;
  thrStats->avgLatency = avgLatency;
  thrStats->minLatency = minLatency;
  thrStats->maxLatency = maxLatency;
  thrStats->exeTimeUs = exeTimeUs;
  thrStats->operations = numOperations;
  thrStats->operationSize = bufferSize;
  thrStats->id = id;
  *pThrStats = thrStats;
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

/* verify crcs */
CpaStatus verifyCrcTaskNodes(struct task_node *waitTaskNode,
  CpaBufferList **srcBufferLists, Cpa32U bufferSize
  )
{
  int bufIdx = 0;
  Cpa8U *buf;
  struct task *waitTask;
  int rc = CPA_STATUS_SUCCESS;

  while(waitTaskNode){
    waitTask = waitTaskNode->tsk;
    buf = srcBufferLists[bufIdx]->pBuffers[0].pData;

    rc = validateCrc32DSA(waitTask,buf, bufferSize);
    if(rc != CPA_STATUS_SUCCESS){
      PRINT_ERR("DSA CRC32 Incorrect\n");
      rc = CPA_STATUS_FAIL;
      break;
    }

    waitTaskNode = waitTaskNode->next;
    bufIdx++;
  }
}



void printTwoPhaseSingleThreadStatsSummary(threadStats2P *stats){
    Cpa64U exeTimeUs = stats->exeTimeUs;
    Cpa32U numOperations = stats->operations;
    Cpa32U bufferSize = stats->operationSize;
    double offloadsPerSec = numOperations / (double)exeTimeUs;
    offloadsPerSec = offloadsPerSec * 1000000;
    printf("Thread: %d\n", stats->id);
    printf("AvgLatency: %ld\n", stats->avgLatency);
    printf("MinLatency: %ld\n", stats->minLatency);
    printf("MaxLatency: %ld\n", stats->maxLatency);
    printf("AvgPhase1Latency: %ld\n", stats->avgLatencyS1);
    printf("AvgPhase2Latency: %ld\n", stats->avgLatencyS2);
    printf("OffloadsPerSec: %f\n", offloadsPerSec);
    printf("Throughput(GB/s): %f\n", offloadsPerSec * bufferSize / 1024 / 1024 / 1024);

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
  pthread_t pTid, dcToCrcTid;
  struct task *waitTask;
  Cpa8U *buf;
  struct task_node *waitTaskNode;

  dsa_fwder_args **args;
  dc_crc_polling_args *dcCrcArgs;
  int argsIdx = 0;
  int flowId = 0;

  /* Need a specialized two-stage packet stats structure */
  two_stage_packet_stats **stats2Phase = NULL;
  PHYS_CONTIG_ALLOC(&stats2Phase, sizeof(two_stage_packet_stats*) * numOperations);


  /* Create args and dsa descs for the callback function to submit*/
  PHYS_CONTIG_ALLOC(&(args), sizeof(dsa_fwder_args*)*numOperations);
  create_tsk_nodes_for_stage2_offload(srcBufferLists, numOperations, dsa);
  waitTaskNode = dsa->multi_task_node;
  while(waitTaskNode){
    /* args */
    PHYS_CONTIG_ALLOC(&(args[argsIdx]), sizeof(dsa_fwder_args));
    args[argsIdx]->dsa=dsa;
    args[argsIdx]->toSubmit = waitTaskNode->tsk;
    args[argsIdx]->stats = stats2Phase;
    args[argsIdx]->packetId = argsIdx; /* assume packets will be seen in order at dsa*/
    waitTaskNode = waitTaskNode->next;

    /* packet stats */
    PHYS_CONTIG_ALLOC(&(stats2Phase[argsIdx]), sizeof(two_stage_packet_stats));
    memset(stats2Phase[argsIdx], 0, sizeof(two_stage_packet_stats));
      argsIdx++;
  }

  /* Create polling thread for DSA */
  OS_MALLOC(&crcArgs, sizeof(crc_polling_args));
  crcArgs->dsa=dsa;
  crcArgs->id = tid;
  crcArgs->stats = stats2Phase;
  createThreadJoinable(&pTid,crc_polling, crcArgs);

  /* Create intermediate polling thread for forwarding */
  OS_MALLOC(&dcCrcArgs, sizeof(dc_crc_polling_args));
  dcCrcArgs->dcInstance = dcInstHandle;
  dcCrcArgs->id = flowId;
  createThread(&dcToCrcTid, dc_crc64_polling, dcCrcArgs);

  /* Submit to dcInst */
  bufIdx = 0;
  while(bufIdx < numOperations){
    rc = submitAndStampBeforeDSAFwdingCb(dcInstHandle,
      sessionHandle,
      srcBufferLists[bufIdx],
      dstBufferLists[bufIdx],
      opData[bufIdx],
      dcResults[bufIdx],
      args[bufIdx],
      bufIdx);

    bufIdx++;
  }

  pthread_join(pTid, NULL);
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