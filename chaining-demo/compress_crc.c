#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"

#include "dc_inst_utils.h"
#include "thread_utils.h"
#include "qat_compression_zlib.h"

int gDebugParam = 1;

#include "zlib.h"

#define CRC64_ECMA182_POLY 0x42F0E1EBA9EA3693ULL
static uint64_t crc64table[256] = {0};
void generate_crc64_table(uint64_t table[256], uint64_t poly)
{
	uint64_t i, j, c, crc;

	for (i = 0; i < 256; i++) {
		crc = 0;
		c = i << 56;

		for (j = 0; j < 8; j++) {
			if ((crc ^ c) & 0x8000000000000000ULL)
				crc = (crc << 1) ^ poly;
			else
				crc <<= 1;
			c <<= 1;
		}

		table[i] = crc;
	}
}

uint64_t crc64_be(uint64_t crc, const void *p, size_t len)
{
	size_t i, t;

	const unsigned char *_p = p;

	for (i = 0; i < len; i++) {
		t = ((crc >> 56) ^ (*_p++)) & 0xFF;
		crc = crc64table[t] ^ (crc << 8);
	}

	return crc;
}

CpaStatus validateCompressAndCrc64(
  CpaBufferList *srcBufferList,
  CpaBufferList *dstBufferList,
  Cpa32U bufferSize,
  CpaDcRqResults *dcResults,
  CpaInstanceHandle dcInstHandle,
  CpaCrcData *crcData)
{
  CpaStatus status = CPA_STATUS_SUCCESS;
  CpaBufferList *pBufferListDst = NULL;
  status = createDstBufferList(&pBufferListDst,bufferSize,  dcInstHandle, CPA_DC_HT_FULL_DYNAMIC);
  /*
    * We now check the results
    */
  if (CPA_STATUS_SUCCESS == status)
  {
    if (dcResults->status != CPA_DC_OK)
    {
        PRINT_ERR("Results status not as expected (status = %d)\n",
                  dcResults->status);
        status = CPA_STATUS_FAIL;
    }
  }

  /* Check the Buffer matches*/
  struct z_stream_s *stream = NULL;
  OS_MALLOC(&stream, sizeof(struct z_stream_s));
  int ret = 0;
  ret = inflateInit2(stream, -MAX_WBITS);
  if (ret != Z_OK)
  {
      PRINT_ERR("Error in zlib_inflateInit2, ret = %d\n", ret);
      return CPA_STATUS_FAIL;
  }
  stream->next_in = (Bytef *)dstBufferList->pBuffers[0].pData;
  stream->avail_in = dcResults->produced;
  stream->next_out = (Bytef *)pBufferListDst->pBuffers[0].pData;
  stream->avail_out = bufferSize;
  ret = inflate(stream, Z_FULL_FLUSH);

  ret = memcmp(srcBufferList->pBuffers[0].pData, pBufferListDst->pBuffers[0].pData, bufferSize);
  if(ret != 0){
    PRINT_ERR("Buffer data does not match\n");
    status = CPA_STATUS_FAIL;
  }

  generate_crc64_table(crc64table, CRC64_ECMA182_POLY);
  if(status == CPA_STATUS_SUCCESS){
    /* CHeck the crc64b matches*/
    uint64_t crc64 = crc64_be(0L, Z_NULL, 0);
    crc64 = crc64_be(crc64, pBufferListDst->pBuffers[0].pData, bufferSize);

    uint64_t crc64_orig = crcData->integrityCrc64b.iCrc;
    ret = memcmp(&crc64, &crc64_orig, sizeof(uint64_t));
    if(ret != 0){
      PRINT_ERR("Src CRC64 does not match\n");
      status = CPA_STATUS_FAIL;
    }

    crc64 = crc64_be(0L, Z_NULL, 0);
    crc64 = crc64_be(crc64, dstBufferList->pBuffers[0].pData, dcResults->produced);
    crc64_orig = crcData->integrityCrc64b.oCrc;
    ret = memcmp(&crc64, &crc64_orig, sizeof(uint64_t));
    if(ret != 0){
      PRINT_ERR("Dst CRC64 does not match\n");
      status = CPA_STATUS_FAIL;
    }
  }

  return status;


}

CpaStatus prepareTestBufferLists(CpaBufferList ***pSrcBufferLists, CpaBufferList ***pDstBufferLists,
  CpaInstanceHandle dcInstHandle, Cpa32U bufferSize, Cpa32U numBufferLists){

  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;

  Cpa8U *pBuffers = NULL;
  CpaDcHuffType huffType = CPA_DC_HT_FULL_DYNAMIC;

  prepareSampleBuffer(&pBuffers, bufferSize);

  OS_MALLOC(&srcBufferLists, numBufferLists * sizeof(CpaBufferList *));
  OS_MALLOC(&dstBufferLists, numBufferLists * sizeof(CpaBufferList *));
  for(int i=0; i<numBufferLists; i++){
    createSourceBufferList(&srcBufferLists[i], pBuffers, bufferSize, dcInstHandle, huffType);
    createDstBufferList(&dstBufferLists[i], bufferSize, dcInstHandle, huffType);
  }
  *pSrcBufferLists = srcBufferLists;
  *pDstBufferLists = dstBufferLists;
}

CpaStatus submitAndStamp(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle sessionHandle, CpaBufferList *pBufferListSrc,
  CpaBufferList *pBufferListDst, CpaDcOpData *opData, CpaDcRqResults *pDcResults, callback_args *cb_args, int index){

  CpaStatus status = CPA_STATUS_SUCCESS;
  packet_stats *stats = cb_args->stats[index];
  Cpa64U submitTime = sampleCoderdtsc();
  stats->submitTime = submitTime;
  status = cpaDcCompressData2(
    dcInstHandle,
    sessionHandle,
    pBufferListSrc,     /* source buffer list */
    pBufferListDst,     /* destination buffer list */
    opData,            /* Operational data */
    pDcResults,         /* results structure */
    (void *)cb_args); /* data sent as is to the callback function*/
  return status;

}

CpaStatus multiBufferTestAllocations(callback_args ***pcb_args, packet_stats ***pstats,
  CpaDcOpData ***popData, CpaDcRqResults ***pdcResults, CpaCrcData **pCrcData,
  Cpa32U numOperations, Cpa32U bufferSize, CpaDcInstanceCapabilities cap,
  CpaBufferList ***pSrcBufferLists, CpaBufferList ***pDstBufferLists,
  CpaInstanceHandle dcInstHandle, struct COMPLETION_STRUCT *complete){

  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaDcOpData **opData = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;


  PHYS_CONTIG_ALLOC(&cb_args, sizeof(callback_args*) * numOperations);
  PHYS_CONTIG_ALLOC(&stats, sizeof(packet_stats*) * numOperations);
  PHYS_CONTIG_ALLOC(&opData, sizeof(CpaDcOpData*) * numOperations);
  PHYS_CONTIG_ALLOC(&dcResults, sizeof(CpaDcRqResults*) * numOperations);
  PHYS_CONTIG_ALLOC(&crcData, sizeof(CpaCrcData) * numOperations);

  for(int i=0; i<numOperations; i++){
    /* Per packet callback arguments */
    PHYS_CONTIG_ALLOC(&(cb_args[i]), sizeof(callback_args));
    memset(cb_args[i], 0, sizeof(struct _callback_args));
    cb_args[i]->completion = complete;
    cb_args[i]->stats = stats;
    /* here we assume that the i'th callback invocation will be processing the i'th packet*/
    cb_args[i]->completedOperations = i;
    cb_args[i]->numOperations = numOperations;

    /* Per Packet packet stats */
    PHYS_CONTIG_ALLOC(&(stats[i]), sizeof(packet_stats));
    memset(stats[i], 0, sizeof(packet_stats));

    /* Per packet CrC Datas */
    memset(&crcData[i], 0, sizeof(CpaDcOpData));

    /* Per packet opDatas*/
    PHYS_CONTIG_ALLOC(&(opData[i]), sizeof(CpaDcOpData));
    memset(opData[i], 0, sizeof(CpaDcOpData));
    INIT_OPDATA(opData[i], CPA_DC_FLUSH_FINAL);
    if(cap.integrityCrcs64b == CPA_TRUE){
      opData[i]->integrityCrcCheck = CPA_TRUE;
      opData[i]->pCrcData = &(crcData[i]);
    }

    /* Per packet results */
    PHYS_CONTIG_ALLOC(&(dcResults[i]), sizeof(CpaDcRqResults));
    memset(dcResults[i], 0, sizeof(CpaDcRqResults));
  }
  prepareTestBufferLists(&srcBufferLists, &dstBufferLists, dcInstHandle, bufferSize, numOperations);
  *pcb_args = cb_args;
  *pstats = stats;
  *popData = opData;
  *pdcResults = dcResults;
  *pCrcData = crcData;
  *pSrcBufferLists = srcBufferLists;
  *pDstBufferLists = dstBufferLists;


  return CPA_STATUS_SUCCESS;

}

void printStats(packet_stats **stats, Cpa32U numOperations, Cpa32U bufferSize){
/* Collect Latencies */
  Cpa32U freqKHz = 2080;
  Cpa64U avgLatency = 0;
  Cpa64U minLatency = UINT64_MAX;
  Cpa64U maxLatency = 0;
  Cpa64U firstSubmitTime = stats[0]->submitTime;
  Cpa64U lastReceiveTime = stats[numOperations-1]->receiveTime;
  Cpa64U exeCycles = lastReceiveTime - firstSubmitTime;
  Cpa64U exeTime = exeCycles/freqKHz;
  double offloadsPerSec = numOperations / (double)exeTime;
  offloadsPerSec = offloadsPerSec * 1000000;
  for(int i=0; i<numOperations; i++){
    Cpa64U latency = stats[i]->receiveTime - stats[i]->submitTime;
    uint64_t micros = latency / freqKHz;
    avgLatency += micros;
    if(micros < minLatency){
      minLatency = micros;
    }
    if(micros > maxLatency){
      maxLatency = micros;
    }
  }
  avgLatency = avgLatency / numOperations;
  printf("AveLatency(us): %lu\n", avgLatency);
  printf("MinLatency(us): %lu\n", minLatency);
  printf("MaxLatency(us): %lu\n", maxLatency);
  printf("Execution Time(us): %lu\n", exeTime);
  printf("OffloadsPerSec: %f\n", offloadsPerSec);
  printf("Throughput(GB/s): %f\n", offloadsPerSec * bufferSize / 1024 / 1024 / 1024);

}

CpaStatus doSubmissionsCompressAndCrc64AndWaitForFinal(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle sessionHandle,
  CpaBufferList **srcBufferLists, CpaBufferList **dstBufferLists, CpaDcOpData **opData, CpaDcRqResults **dcResults,
  callback_args **cb_args, Cpa32U numOperations, struct COMPLETION_STRUCT *complete){

  CpaStatus status = CPA_STATUS_SUCCESS;
  for(int i=0; i<numOperations; i++){
retry:
    status = submitAndStamp(
      dcInstHandle,
      sessionHandle,
      srcBufferLists[i],
      dstBufferLists[i],
      (opData[i]),
      dcResults[i],
      cb_args[i],
      i
    );
    if(status != CPA_STATUS_SUCCESS){
      fprintf(stderr, "Error in compress data on %d'th packet\n", i);
      goto retry;
    }
  }
  if(!COMPLETION_WAIT(complete, 5000)){
    PRINT_ERR("timeout or interruption in cpaDcCompressData2\n");
    status = CPA_STATUS_FAIL;
  }
  return status;
}

CpaStatus singleStreamOfCompressAndCrc64(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle sessionHandle,
  Cpa32U numOperations, Cpa32U bufferSize
  ){
    CpaDcInstanceCapabilities cap = {0};
    CpaDcOpData **opData = NULL;
    CpaDcRqResults **dcResults = NULL;
    CpaCrcData *crcData = NULL;
    struct COMPLETION_STRUCT complete;
    callback_args **cb_args = NULL;
    packet_stats **stats = NULL;
    CpaBufferList **srcBufferLists = NULL;
    CpaBufferList **dstBufferLists = NULL;
    cpaDcQueryCapabilities(dcInstHandle, &cap);
    COMPLETION_INIT(&complete);

    multiBufferTestAllocations(&cb_args,
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
      &complete);
    CpaStatus status = doSubmissionsCompressAndCrc64AndWaitForFinal(
      dcInstHandle,
      sessionHandle,
      srcBufferLists,
      dstBufferLists,
      opData,
      dcResults,
      cb_args,
      numOperations,
      &complete
    );
    /* validate all results */
    for(int i=0; i<numOperations; i++){
      if (CPA_STATUS_SUCCESS != validateCompressAndCrc64(srcBufferLists[i], dstBufferLists[i], 1024, dcResults[i], dcInstHandle, &(crcData[i]))){
        PRINT_ERR("Buffer not compressed/decompressed correctly\n");
      }
    }
    printStats(stats, numOperations, bufferSize);
    COMPLETION_DESTROY(&complete);
}


void *streamFn(void *arg){
  submitter_args *args = (submitter_args *)arg;
  singleStreamOfCompressAndCrc64(
    args->dcInstHandle,
    args->sessionHandle,
    args->numOperations,
    args->bufferSize
  );
}

CpaStatus prepareMultipleCompressAndCrc64InstancesAndSessions(CpaInstanceHandle *dcInstHandles, CpaDcSessionHandle *sessionHandles,
  Cpa16U numInstances, Cpa16U numSessions){
  CpaStatus status = CPA_STATUS_SUCCESS;
  for(int i=0; i<numInstances; i++){
    dcInstHandles[i] = dcInstHandles[i];
    prepareDcInst(&dcInstHandles[i]);
    sessionHandles[i] = sessionHandles[i];
    prepareDcSession(dcInstHandles[i], &sessionHandles[i], dcPerfCallback);
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
    goto exit;
  }

  Cpa32U numFlows = 2;

  /* multiple instance for latency test */
  prepareMultipleCompressAndCrc64InstancesAndSessions(dcInstHandles, sessionHandles, numInstances, numFlows);


  CpaInstanceInfo2 info2 = {0};

  status = cpaDcInstanceGetInfo2(dcInstHandle, &info2);

  if ((status == CPA_STATUS_SUCCESS) && (info2.isPolled == CPA_TRUE))
  {
      /* Start thread to poll instance */
      pthread_t thread[MAX_INSTANCES];
      thread_args *args = NULL;
      args = (thread_args *)malloc(sizeof(thread_args));
      args->dcInstHandle = dcInstHandle;
      args->id = 0;
      createThread(&thread[0], dc_polling, (void *)args);
  }

  sessionHandle = sessionHandles[0];
  prepareDcSession(dcInstHandle, &sessionHandle, dcPerfCallback);

  Cpa32U numOperations = 10000;
  Cpa32U bufferSize = 1024;

  pthread_t subThread;
  submitter_args *submitterArgs = NULL;
  OS_MALLOC(&submitterArgs, sizeof(submitter_args));
  submitterArgs->dcInstHandle = dcInstHandle;
  submitterArgs->sessionHandle = sessionHandle;
  submitterArgs->numOperations = numOperations;
  submitterArgs->bufferSize = bufferSize;

  createThreadJoinable(&subThread, streamFn, (void *)submitterArgs);

  pthread_join(subThread, NULL);

exit:
  gPollingDcs[0] = 0;



  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}