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
  CpaInstanceHandle dcInstHandle, Cpa32U bufferSize){

  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;


  Cpa16U numBufferLists = 1;
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
  CpaBufferList *pBufferListDst, CpaDcOpData *opData, CpaDcRqResults *pDcResults, callback_args *cb_args){

  CpaStatus status = CPA_STATUS_SUCCESS;
  packet_stats *stats = cb_args->stats;
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

  /* single instance for latency test */
  dcInstHandle = dcInstHandles[0];
  prepareDcInst(&dcInstHandle);

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

  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;

  prepareTestBufferLists(&srcBufferLists, &dstBufferLists, dcInstHandles[0], 1024);

  CpaBufferList *pBufferListSrc = srcBufferLists[0];
  CpaBufferList *pBufferListDst = dstBufferLists[0];
  CpaDcOpData opData = {};
  CpaDcRqResults dcResults;
  CpaDcRqResults *pDcResults = &dcResults;

  struct COMPLETION_STRUCT complete;
  callback_args *cb_args = NULL;
  packet_stats *stats = NULL;
  Cpa64U submitTime;

  CpaCrcData crcData = {0};

  cpaDcQueryCapabilities(dcInstHandle, &cap);
  if(cap.integrityCrcs64b == CPA_TRUE ){
    PRINT_DBG("Integrity CRC is enabled\n");
    opData.integrityCrcCheck = CPA_TRUE;
    opData.pCrcData = &crcData;
  }

  COMPLETION_INIT(&complete);
  INIT_OPDATA(&opData, CPA_DC_FLUSH_FINAL);
  PHYS_CONTIG_ALLOC(&cb_args, sizeof(struct _callback_args));
  memset(cb_args, 0, sizeof(struct _callback_args));

  PHYS_CONTIG_ALLOC(&stats, sizeof(packet_stats));
  memset(stats, 0, sizeof(packet_stats));
  cb_args->completion = &complete;
  cb_args->stats = stats;

  submitAndStamp(dcInstHandle, sessionHandle, pBufferListSrc, pBufferListDst, &opData, pDcResults, cb_args);

  // stats->submitTime = sampleCoderdtsc();
  // submitTime = sampleCoderdtsc();
  // status = cpaDcCompressData2(
  //   dcInstHandle,
  //   sessionHandle,
  //   pBufferListSrc,     /* source buffer list */
  //   pBufferListDst,     /* destination buffer list */
  //   &opData,            /* Operational data */
  //   &dcResults,         /* results structure */
  //   (void *)cb_args); /* data sent as is to the callback function*/

  if(status != CPA_STATUS_SUCCESS){
    fprintf(stderr, "Error in compress data\n");
    goto exit;
  }
  if (!COMPLETION_WAIT(&complete, 5000))
  {
      PRINT_ERR("timeout or interruption in cpaDcCompressData2\n");
      status = CPA_STATUS_FAIL;
  }

  if (CPA_STATUS_SUCCESS != validateCompressAndCrc64(pBufferListSrc, pBufferListDst, 1024, &dcResults, dcInstHandle, &crcData)){
    PRINT_ERR("Buffer not compressed/decompressed correctly\n");
  }

  /* Collect Latencies */
  Cpa64U latency = stats->receiveTime - stats->submitTime;
  Cpa32U freqKHz = 2080;
  uint64_t micros = latency / freqKHz;
  printf("Latency(cycles): %lu\n", latency);
  printf("Latency(us): %lu\n", micros);

  if (CPA_STATUS_SUCCESS == status)
  {
    if (dcResults.status != CPA_DC_OK)
    {
        PRINT_ERR("Results status not as expected (status = %d)\n",
                  dcResults.status);
        status = CPA_STATUS_FAIL;
    }
  }
  COMPLETION_DESTROY(&complete);


  // functionalCompressAndCrc64(dcInstHandle, sessionHandle);


exit:


  gPollingDcs[0] = 0;


  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}