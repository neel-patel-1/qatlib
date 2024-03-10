
#include "cpa_dc.h"
#include "icp_sal_user.h"
#include <stdlib.h>
#include "cpa_sample_utils.h"
#include "cpa_sample_code_dc_utils.h"

#define CALGARY "/lib/firmware/calgary"
#define FAIL_ON(cond, args...) if (cond) { PRINT(args); return CPA_STATUS_FAIL; }

int main(){
    /* Start process and name it for QAT Identification */
    CpaStatus stat;
    stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to start user process SSL")
    stat = qaeMemInit();
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to open usdm driver")

    Cpa16U numInst = 0;
    stat = cpaDcGetNumInstances(&numInst);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to get number of instances")
    printf("Number of available dc instances: %d\n", numInst);
    CpaInstanceHandle *dcInstances_g 
        = (CpaInstanceHandle *)qaeMemAlloc(sizeof(CpaInstanceHandle) * numInst);
    FAIL_ON(dcInstances_g == NULL, "Failed to allocate memory for instance handles")
    stat = cpaDcGetInstances(numInst, dcInstances_g);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to get instances: %d\n", stat);

    Cpa32U *instMap = qaeMemAlloc(sizeof(Cpa32U) * numInst);
    FAIL_ON(instMap == NULL, "Failed to allocate memory for instance map");

    CpaInstanceInfo2 *infoList = NULL;
    infoList = qaeMemAlloc(sizeof(CpaInstanceInfo2) * numInst);
    FAIL_ON(infoList == NULL, "Failed to allocate memory for instance info");
    memset(infoList, 0, sizeof(CpaInstanceInfo2));

    for(int i = 0; i < numInst; i++){
        CpaInstanceInfo2 *info = &infoList[i];
        stat = cpaDcInstanceGetInfo2(dcInstances_g[i], info);
        FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to get instance info: %d\n", stat);
        Cpa32U coreAffinity=0;
        for (int j = 0; j < CPA_MAX_CORES; j++)
        {
            if (CPA_BITMAP_BIT_TEST(info->coreAffinity, j))
            {
                coreAffinity = j;
            }

        }
        PRINT("Inst %u, Affin: %u, Dev: %u, NumaNode: %u, Accel %u, "
            "EE %u, BDF %02X:%02X:%02X\n",
            i,
            coreAffinity,
            info->physInstId.packageId,
            info->nodeAffinity,
            info->physInstId.acceleratorId,
            info->physInstId.executionEngineId,
            (Cpa8U)((info->physInstId.busAddress) >> 8),
            (Cpa8U)((info->physInstId.busAddress) & 0xFF) >> 3,
            (Cpa8U)((info->physInstId.busAddress) & 7));
    }
    CpaDcInstanceCapabilities dcCap = {0};
    stat = cpaDcQueryCapabilities(dcInstances_g[0], &dcCap);
    
    /* Decompression Params */
    CpaDcHuffType huffType = CPA_DC_HT_STATIC;
    CpaDcSessionDir sessionDir = CPA_DC_DIR_COMPRESS;
    CpaDcCompLvl compLevel = 1;
    CpaDcSessionState state = CPA_DC_STATELESS;
    Cpa32U windowSize = DEFAULT_COMPRESSION_WINDOW_SIZE;
    Cpa32U testBufferSize = 65536;
    
    /* Getting test file prepared */
    FILE *file = fopen(CALGARY, "rb");
    FAIL_ON(file == NULL, "Failed to open corpus file: %s\n", CALGARY);
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    FAIL_ON(size < 0, "Failed to get file size\n");
    fseek(file, 0, SEEK_SET);
    int num_bufs = size / testBufferSize;
    printf("Testing on %d buffers\n", num_bufs);
    int num_iters = num_iters > num_bufs ? num_iters : num_bufs;
    printf("Testing on %d iterations\n", num_iters);

    CpaFlatBuffer *fileBuf = (CpaFlatBuffer *)qaeMemAlloc(sizeof(CpaFlatBuffer));
    FAIL_ON(fileBuf == NULL, "Failed to allocate memory for src buffers\n");
    fileBuf->pData = qaeMemAlloc(testBufferSize);
    FAIL_ON(fileBuf->pData == NULL, "Failed to allocate memory for src buffer 0\n");
    int rdLen = fread(fileBuf->pData, 1, testBufferSize, file);


    /* Dynamic Compression Buffer Lists */
    CpaBufferList ***pInterBuffList_g = 
        (CpaBufferList ***)qaeMemAlloc(sizeof(CpaBufferList **) * numInst);
    
    memset(
        pInterBuffList_g, 0, numInst * sizeof(CpaBufferList **));
    CpaStatus status;
    for (int i = 0; i < numInst; i++)
    {
        Cpa16U numBuffers;
        status =
            cpaDcGetNumIntermediateBuffers(dcInstances_g[i], &numBuffers);
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to allocate Memory for Dynamic Buffer\n");
            qaeMemFree((void **)&dcInstances_g);
            qaeMemFree((void **)&pInterBuffList_g);
            return CPA_STATUS_FAIL;
        }
        if (numBuffers > 0)
        {
            /* allocate the buffer list memory for the dynamic Buffers
                * only applicable for CPM prior to gen4 as it is done in HW */
            pInterBuffList_g[i] =
                qaeMemAllocNUMA(sizeof(CpaBufferList *) * numBuffers,
                                infoList[i].nodeAffinity,
                                BYTE_ALIGNMENT_64);
            if (NULL == pInterBuffList_g[i])
            {
                PRINT_ERR("Unable to allocate Memory for Dynamic Buffer\n");
                qaeMemFree((void **)&dcInstances_g);
                qaeMemFree((void **)&pInterBuffList_g);
                return CPA_STATUS_FAIL;
            }

            /* get the size of the Private meta data
                * needed to create Buffer List
                */
            Cpa32U size;
            status = cpaDcBufferListGetMetaSize(
                dcInstances_g[i], numBuffers, &size);
            if (CPA_STATUS_SUCCESS != status)
            {
                PRINT_ERR("Get Meta Size Data Failed\n");
                qaeMemFree((void **)&dcInstances_g);
                qaeMemFree((void **)&pInterBuffList_g);
                return CPA_STATUS_FAIL;
            }
        }
    }


    icp_sal_userStop();
    qaeMemDestroy();
}