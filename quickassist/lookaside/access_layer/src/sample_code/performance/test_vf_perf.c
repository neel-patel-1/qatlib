
#include "cpa_dc.h"
#include "icp_sal_user.h"
#include <stdlib.h>
#include "cpa_sample_utils.h"
#include "cpa_sample_code_dc_utils.h"
#include "test_vf_perf_utils.h"

#define CALGARY "/lib/firmware/calgary"

CpaInstanceHandle *dcInstances_g = NULL;
CpaInstanceInfo2 *instanceInfo2 = NULL;
Cpa16U numDcInstances_g = 0;
pthread_t *dcPollingThread_g = NULL;
volatile CpaBoolean dc_service_started_g = CPA_FALSE;
CpaBufferList *** pInterBuffList_g = NULL;
Cpa32U expansionFactor_g = 1;
Cpa32U coreLimit_g = 0;



int main(){
    /* Start process and name it for QAT Identification */
    CpaStatus stat;
    stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to start user process SSL")
    stat = qaeMemInit();
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to open usdm driver")

    stat = cpaDcGetNumInstances(&numDcInstances_g);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to get number of instances")
    printf("Number of available dc instances: %d\n", numDcInstances_g);
    dcInstances_g 
        = (CpaInstanceHandle *)qaeMemAlloc(sizeof(CpaInstanceHandle) * numDcInstances_g);
    FAIL_ON(dcInstances_g == NULL, "Failed to allocate memory for instance handles")
    stat = cpaDcGetInstances(numDcInstances_g, dcInstances_g);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to get instances: %d\n", stat);

    Cpa32U *instMap = qaeMemAlloc(sizeof(Cpa32U) * numDcInstances_g);
    FAIL_ON(instMap == NULL, "Failed to allocate memory for instance map");

    instanceInfo2 = qaeMemAlloc(sizeof(CpaInstanceInfo2) * numDcInstances_g);
    FAIL_ON(instanceInfo2 == NULL, "Failed to allocate memory for instance info");
    memset(instanceInfo2, 0, sizeof(CpaInstanceInfo2));

    numDcInstances_g = 2;
    
    /* Generate SPR Core Affinities for Socket 1 QAT Device */
    Cpa32U coreAffinities[numDcInstances_g];
    genCoreAffinities(numDcInstances_g, coreAffinities);
    for(int i = 0; i < numDcInstances_g; i++){
        CpaInstanceInfo2 *info = &instanceInfo2[i];
        stat = cpaDcInstanceGetInfo2(dcInstances_g[i], info);
        FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to get instance info: %d\n", stat);
        CPA_BITMAP_BIT_SET(info->coreAffinity,coreAffinities[i]);
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


    startDcServices(testBufferSize, TEMP_NUM_BUFFS);

    /* Test whether epoll works or we need separate config */
    int fd = -1;
    CpaStatus status = icp_sal_DcGetFileDescriptorForce(dcInstances_g[0], &fd, CPA_TRUE);
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to get file descriptor: %d\n", status);
    }

    createBusyPollThreads();


    icp_sal_userStop();
    qaeMemDestroy();
}