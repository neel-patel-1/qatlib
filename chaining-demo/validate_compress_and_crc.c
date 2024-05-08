#include "validate_compress_and_crc.h"


uint64_t crc64table[256] = {0};
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

CpaStatus validateCompress(
  CpaBufferList *srcBufferList,
  CpaBufferList *dstBufferList,
  CpaDcRqResults *dcResults,
  Cpa32U bufferSize)
{
  CpaStatus status = CPA_STATUS_SUCCESS;
  /* Check the Buffer matches*/
  z_stream *stream;
  stream = malloc(sizeof(z_stream));
  stream->zalloc = Z_NULL;
  stream->zfree = Z_NULL;
  stream->opaque = Z_NULL;
  status = inflateInit(stream);
  if (status != Z_OK)
  {
      PRINT_ERR("Error in zlib_inflateInit2, ret = %d\n", status);
      return CPA_STATUS_FAIL;
  }

  Bytef *tmpBuf = NULL;
  OS_MALLOC(&tmpBuf, bufferSize);

  stream->next_in = (Bytef *)dstBufferList->pBuffers[0].pData;
  stream->avail_in = dcResults->produced;
  stream->next_out = (Bytef *)tmpBuf;
  stream->avail_out = bufferSize;
  int ret = inflate(stream, Z_FULL_FLUSH);

  inflateEnd(stream);

  ret = memcmp(srcBufferList->pBuffers[0].pData, tmpBuf, bufferSize);
  if(ret != 0){
    PRINT_ERR("Buffer data does not match\n");
    status = CPA_STATUS_FAIL;
  }
  return status;
}