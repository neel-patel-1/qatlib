
#include "cpa_dc.h"
#include "icp_sal_user.h"
#include <stdlib.h>
#include "cpa_sample_utils.h"
#include "cpa_sample_code_dc_utils.h"
#include "test_vf_perf_utils.h"

#define CALGARY "/lib/firmware/calgary"
#define FAIL_ON(cond, args...) if (cond) { PRINT(args); return CPA_STATUS_FAIL; }

int main(){
    /* Start process and name it for QAT Identification */
    CpaStatus stat;
    stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to start user process SSL")
    stat = qaeMemInit();
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to open usdm driver")

    Cpa16U numDcInstances_g = 0;
    stat = cpaDcGetNumInstances(&numDcInstances_g);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to get number of instances")
    printf("Number of available dc instances: %d\n", numDcInstances_g);
    CpaInstanceHandle *dcInstances_g 
        = (CpaInstanceHandle *)qaeMemAlloc(sizeof(CpaInstanceHandle) * numDcInstances_g);
    FAIL_ON(dcInstances_g == NULL, "Failed to allocate memory for instance handles")
    stat = cpaDcGetInstances(numDcInstances_g, dcInstances_g);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to get instances: %d\n", stat);

    Cpa32U *instMap = qaeMemAlloc(sizeof(Cpa32U) * numDcInstances_g);
    FAIL_ON(instMap == NULL, "Failed to allocate memory for instance map");

    CpaInstanceInfo2 *infoList = NULL;
    infoList = qaeMemAlloc(sizeof(CpaInstanceInfo2) * numDcInstances_g);
    FAIL_ON(infoList == NULL, "Failed to allocate memory for instance info");
    memset(infoList, 0, sizeof(CpaInstanceInfo2));

    for(int i = 0; i < numDcInstances_g; i++){
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
        (CpaBufferList ***)qaeMemAlloc(sizeof(CpaBufferList **) * numDcInstances_g);
    
    memset(
        pInterBuffList_g, 0, numDcInstances_g * sizeof(CpaBufferList **));
    CpaStatus status;
    Cpa32U expansionFactor_g = 1;
    /* Start the Loop to create Buffer List for each instance*/
    for (int i = 0; i < numDcInstances_g; i++)
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
        CpaBufferList ** tempBufferList = pInterBuffList_g[i];
        Cpa16U nodeId = infoList[i].nodeAffinity;
        Cpa32U buffSize = testBufferSize;
        for (int k = 0; k < numBuffers; k++)
        {
            tempBufferList[k] = (CpaBufferList *)qaeMemAllocNUMA(
                sizeof(CpaBufferList), nodeId, BYTE_ALIGNMENT_64);
            if (NULL == tempBufferList[k])
            {
                PRINT(" %s:: Unable to allocate memory for "
                        "tempBufferList\n",
                        __FUNCTION__);
                qaeMemFree((void **)&dcInstances_g);
                freeDcBufferList(tempBufferList, k + 1);
                qaeMemFree((void **)&pInterBuffList_g);
                return CPA_STATUS_FAIL;
            }
            tempBufferList[k]->pPrivateMetaData =
                qaeMemAllocNUMA(size, nodeId, BYTE_ALIGNMENT_64);
            
            if (NULL == tempBufferList[k]->pPrivateMetaData)
            {
                PRINT(" %s:: Unable to allocate memory for "
                        "pPrivateMetaData\n",
                        __FUNCTION__);
                qaeMemFree((void **)&dcInstances_g);
                freeDcBufferList(tempBufferList, k + 1);
                qaeMemFree((void **)&pInterBuffList_g);
                return CPA_STATUS_FAIL;
            }
            tempBufferList[k]->numBuffers = ONE_BUFFER_DC;
            /* allocate flat buffers */
            tempBufferList[k]->pBuffers = qaeMemAllocNUMA(
                (sizeof(CpaFlatBuffer)), nodeId, BYTE_ALIGNMENT_64);
            if (NULL == tempBufferList[k]->pBuffers)
            {
                PRINT_ERR("Unable to allocate memory for pBuffers\n");
                qaeMemFree((void **)&dcInstances_g);
                freeDcBufferList(tempBufferList, k + 1);
                qaeMemFree((void **)&pInterBuffList_g);
                return CPA_STATUS_FAIL;
            }

            tempBufferList[k]->pBuffers[0].pData = qaeMemAllocNUMA(
                (size_t)expansionFactor_g * EXTRA_BUFFER * buffSize,
                nodeId,
                BYTE_ALIGNMENT_64);
            if (NULL == tempBufferList[k]->pBuffers[0].pData)
            {
                PRINT_ERR("Unable to allocate Memory for pBuffers\n");
                qaeMemFree((void **)&dcInstances_g);
                freeDcBufferList(tempBufferList, k + 1);
                qaeMemFree((void **)&pInterBuffList_g);
                return CPA_STATUS_FAIL;
            }
            tempBufferList[k]->pBuffers[0].dataLenInBytes =
                expansionFactor_g * EXTRA_BUFFER * buffSize;
        }
        /* When starting the DC Instance, the API expects that the
            * private meta data should be greater than the dataLength
            */
        /* Configure memory Configuration Function */
        status = cpaDcSetAddressTranslation(
            dcInstances_g[i], (CpaVirtualToPhysical)qaeVirtToPhysNUMA);
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Error setting memory config for instance\n");
            qaeMemFree((void **)&dcInstances_g);
            freeDcBufferList(pInterBuffList_g[i], numBuffers);
            qaeMemFreeNUMA((void **)&pInterBuffList_g[i]);
            qaeMemFree((void **)&pInterBuffList_g);
            return CPA_STATUS_FAIL;
        }
        /* Start DC Instance */
        status = cpaDcStartInstance(
            dcInstances_g[i], numBuffers, pInterBuffList_g[i]);
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to start DC Instance\n");
            qaeMemFree((void **)&dcInstances_g);
            freeDcBufferList(pInterBuffList_g[i], numBuffers);
            qaeMemFreeNUMA((void **)&pInterBuffList_g[i]);
            qaeMemFree((void **)&pInterBuffList_g);
            return CPA_STATUS_FAIL;
        }
    }


    icp_sal_userStop();
    qaeMemDestroy();
}