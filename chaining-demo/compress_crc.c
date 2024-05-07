#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"

extern CpaDcHuffType huffmanType_g;
extern CpaStatus qaeMemInit(void);
extern void qaeMemDestroy(void);

#define MAX_INSTANCES 1
#define SAMPLE_MAX_BUFF 1024

extern int gDebugParam;

int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;
  Cpa16U numInstances = 0;
  CpaInstanceHandle dcInstHandle = NULL;
  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcInstanceCapabilities cap = {0};

  Cpa16U numInterBuffLists = 0;
  Cpa16U bufferNum = 0;
  Cpa32U buffMetaSize = 0;
  CpaBufferList **bufferInterArray = NULL;

  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);

  status = cpaDcGetNumInstances(&numInstances);
  if (numInstances >= MAX_INSTANCES)
  {
      numInstances = MAX_INSTANCES;
  }
  if ((status == CPA_STATUS_SUCCESS) && (numInstances > 0))
  {
      status = cpaDcGetInstances(numInstances, dcInstHandles);
      if (status == CPA_STATUS_SUCCESS)
          dcInstHandle = dcInstHandles[0];
  }
  if (0 == numInstances)
  {
    fprintf(stderr, "No instances found\n");
    goto exit;
  }
  status = cpaDcQueryCapabilities(dcInstHandle, &cap);
  {
        return status;
  }

  if (!cap.statelessDeflateCompression ||
      !cap.statelessDeflateDecompression || !cap.checksumAdler32 ||
      !cap.dynamicHuffman)
  {
      fprintf(stderr, "Capabilities not supported\n");
      goto exit;
  }
  if (cap.dynamicHuffmanBufferReq)
  {
      status = cpaDcBufferListGetMetaSize(dcInstHandle, 1, &buffMetaSize);

      if (CPA_STATUS_SUCCESS == status)
      {
          status = cpaDcGetNumIntermediateBuffers(dcInstHandle,
                                                  &numInterBuffLists);
      }
      if (CPA_STATUS_SUCCESS == status && 0 != numInterBuffLists)
      {
          status = PHYS_CONTIG_ALLOC(
              &bufferInterArray, numInterBuffLists * sizeof(CpaBufferList *));
      }
      for (bufferNum = 0; bufferNum < numInterBuffLists; bufferNum++)
      {
          if (CPA_STATUS_SUCCESS == status)
          {
              status = PHYS_CONTIG_ALLOC(&bufferInterArray[bufferNum],
                                          sizeof(CpaBufferList));
          }
          if (CPA_STATUS_SUCCESS == status)
          {
              status = PHYS_CONTIG_ALLOC(
                  &bufferInterArray[bufferNum]->pPrivateMetaData,
                  buffMetaSize);
          }
          if (CPA_STATUS_SUCCESS == status)
          {
              status =
                  PHYS_CONTIG_ALLOC(&bufferInterArray[bufferNum]->pBuffers,
                                    sizeof(CpaFlatBuffer));
          }
          if (CPA_STATUS_SUCCESS == status)
          {
              /* Implementation requires an intermediate buffer approximately
                          twice the size of the output buffer */
              status = PHYS_CONTIG_ALLOC(
                  &bufferInterArray[bufferNum]->pBuffers->pData,
                  2 * SAMPLE_MAX_BUFF);
              bufferInterArray[bufferNum]->numBuffers = 1;
              bufferInterArray[bufferNum]->pBuffers->dataLenInBytes =
                  2 * SAMPLE_MAX_BUFF;
          }

      } /* End numInterBuffLists */
  }
  if (CPA_STATUS_SUCCESS == status)
  {
      /*
        * Set the address translation function for the instance
        */
      status = cpaDcSetAddressTranslation(dcInstHandle, sampleVirtToPhys);
  }

  if (CPA_STATUS_SUCCESS == status)
  {
      /* Start DataCompression component */
      PRINT_DBG("cpaDcStartInstance\n");
      status = cpaDcStartInstance(
          dcInstHandle, numInterBuffLists, bufferInterArray);
  }


exit:
  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}