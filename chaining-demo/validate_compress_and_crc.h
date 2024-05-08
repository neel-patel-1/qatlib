#ifndef VALIDATE_COMPRESS_AND_CRC
#define VALIDATE_COMPRESS_AND_CRC
#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"
#include <zlib.h>

#include "buffer_prepare_funcs.h"

#define CRC64_ECMA182_POLY 0x42F0E1EBA9EA3693ULL
extern uint64_t crc64table[256];

void generate_crc64_table(uint64_t table[256], uint64_t poly);
uint64_t crc64_be(uint64_t crc, const void *p, size_t len);
CpaStatus validateCompressAndCrc64(
  CpaBufferList *srcBufferList,
  CpaBufferList *dstBufferList,
  Cpa32U bufferSize,
  CpaDcRqResults *dcResults,
  CpaInstanceHandle dcInstHandle,
  CpaCrcData *crcData);

CpaStatus validateCompress(
  CpaBufferList *srcBufferList,
  CpaBufferList *dstBufferList,
  CpaDcRqResults *dcResults,
  Cpa32U bufferSize);

#endif