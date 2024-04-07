#include "cpa.h"
#include "cpa_cy_im.h"
#include "cpa_cy_sym_dp.h"
#include "icp_sal_poll.h"
#include "cpa_sample_utils.h"
#include "Osal.h"

/* The digest length must be less than or equal to SHA256 digest
   length (16) for this example */
#define DIGEST_LENGTH 32

extern int gDebugParam;

/* AES key, 256 bits long */
volatile Cpa32U fragmentSize_g;
volatile Cpa16U numBufs_g = 0;
int numAxs_g;
CpaBoolean cbIsDep_g = 0;
Cpa16U intensity_g = 0;

volatile CpaBoolean globalDone = 0;
volatile CpaBoolean doPoll[10];

CpaInstanceHandle instanceHandles[10];
CpaCySymDpSessionCtx dpSessionCtxs_g[10];

struct cbArg{
    Cpa16U mIdx;
    Cpa16U bufIdx;
    Cpa16U numBufs;
    CpaBoolean kickTail;
    Cpa16U intensity;
    CpaBoolean dependent;
    char *operandBuffer;
    Cpa32U operandBufferSize;
};

struct pollerArg{
    CpaInstanceHandle instanceHandle;
    Cpa16U mIdx;
    Cpa16U rBS;
};

static Cpa8U sampleCipherKey[] = {
    0xEE, 0xE2, 0x7B, 0x5B, 0x10, 0xFD, 0xD2, 0x58, 0x49, 0x77, 0xF1, 0x22,
    0xD7, 0x1B, 0xA4, 0xCA, 0xEC, 0xBD, 0x15, 0xE2, 0x52, 0x6A, 0x21, 0x0B,
    0x41, 0x4C, 0x41, 0x4E, 0xA1, 0xAA, 0x01, 0x3F};


/* Initialization vector */
static Cpa8U sampleCipherIv[] = {
    0x7E, 0x9B, 0x4C, 0x1D, 0x82, 0x4A, 0xC5, 0xDF, 0x99, 0x4C, 0xA1, 0x44,
    0xAA, 0x8D, 0x37, 0x27};

/* Source data to encrypt */
static Cpa8U sampleAlgChainingSrc[] = {
    0xD7, 0x1B, 0xA4, 0xCA, 0xEC, 0xBD, 0x15, 0xE2, 0x52, 0x6A, 0x21, 0x0B,
    0x81, 0x77, 0x0C, 0x90, 0x68, 0xF6, 0x86, 0x50, 0xC6, 0x2C, 0x6E, 0xED,
    0x2F, 0x68, 0x39, 0x71, 0x75, 0x1D, 0x94, 0xF9, 0x0B, 0x21, 0x39, 0x06,
    0xBE, 0x20, 0x94, 0xC3, 0x43, 0x4F, 0x92, 0xC9, 0x07, 0xAA, 0xFE, 0x7F,
    0xCF, 0x05, 0x28, 0x6B, 0x82, 0xC4, 0xD7, 0x5E, 0xF3, 0xC7, 0x74, 0x68,
    0xCF, 0x05, 0x28, 0x6B, 0x82, 0xC4, 0xD7, 0x5E, 0xF3, 0xC7, 0x74, 0x68,
    0x80, 0x8B, 0x28, 0x8D, 0xCD, 0xCA, 0x94, 0xB8, 0xF5, 0x66, 0x0C, 0x00,
    0x5C, 0x69, 0xFC, 0xE8, 0x7F, 0x0D, 0x81, 0x97, 0x48, 0xC3, 0x6D, 0x24};

/* Expected output of the encryption operation with the specified
 * cipher (CPA_CY_SYM_CIPHER_AES_CBC), key (sampleCipherKey) and
 * initialization vector (sampleCipherIv) */
static Cpa8U expectedOutput[] = {
    0xC1, 0x92, 0x33, 0x36, 0xF9, 0x50, 0x4F, 0x5B, 0xD9, 0x79, 0xE1, 0xF6,
    0xC7, 0x7A, 0x7D, 0x75, 0x47, 0xB7, 0xE2, 0xB9, 0xA1, 0x1B, 0xB9, 0xEE,
    0x16, 0xF9, 0x1A, 0x87, 0x59, 0xBC, 0xF2, 0x94, 0x7E, 0x71, 0x59, 0x52,
    0x3B, 0xB7, 0xF6, 0xB0, 0xB8, 0xE6, 0xC3, 0x9C, 0xA2, 0x4B, 0x5A, 0x8A,
    0x25, 0x61, 0xAB, 0x65, 0x4E, 0xB5, 0xD1, 0x3D, 0xB2, 0x7D, 0xA3, 0x9D,
    0x1E, 0x71, 0x45, 0x14, 0x5E, 0x9B, 0xB4, 0x75, 0xD3, 0xA8, 0xED, 0x40,
    0x01, 0x19, 0x2B, 0xEB, 0x04, 0x35, 0xAA, 0xA9, 0xA7, 0x95, 0x69, 0x77,
    0x40, 0xD9, 0x1D, 0xE4, 0xE7, 0x1A, 0xF9, 0x35, 0x06, 0x61, 0x3F, 0xAF,
    /* Digest */
    0xEE, 0x6F, 0x90, 0x7C, 0xB5, 0xF4, 0xDE, 0x75, 0xD3, 0xBC, 0x11, 0x63,
    0xE7, 0xF0, 0x5D, 0x15, 0x5E, 0x61, 0x16, 0x13, 0x83, 0x1A, 0xD6, 0x56,
    0x44, 0xA7, 0xF6, 0xA2, 0x6D, 0xAB, 0x1A, 0xF2};

struct sptArg{
    CpaCySymDpOpData *pOpData;
    CpaStatus status;
    CpaBoolean verifyResult;
};

static inline void symDpCallback(CpaCySymDpOpData *pOpData,
                          CpaStatus status,
                          CpaBoolean verifyResult)
{
    struct cbArg *cbArg = (struct cbArg *)pOpData->pCallbackTag;
    CpaInstanceHandle toSubInst = instanceHandles[cbArg->mIdx+1];
    CpaCySymSessionCtx toSubSessionCtx = dpSessionCtxs_g[cbArg->mIdx];
    pOpData->instanceHandle = toSubInst;
    pOpData->sessionCtx = toSubSessionCtx;
    Cpa16U intensity = cbArg->intensity;
    char *dBuffer = (char *)(cbArg->operandBuffer);
    while(intensity > 0){
        for(int i=0; i<cbArg->operandBufferSize; i++){
            dBuffer[i] += 1;
        }
        cbArg->intensity--;
    }

    if(cbArg->mIdx == (numAxs_g - 1)){
        if(cbArg->bufIdx == cbArg->numBufs - 1){
            globalDone = CPA_TRUE;
        }
    } else {
        struct cbArg *newCbArg = (struct cbArg *)(pOpData->pCallbackTag);
        PHYS_CONTIG_ALLOC(&newCbArg, sizeof(struct cbArg));
        newCbArg->mIdx = cbArg->mIdx + 1;
        newCbArg->bufIdx = cbArg->bufIdx;
        newCbArg->kickTail = cbArg->kickTail;
        newCbArg->numBufs = cbArg->numBufs;
        pOpData->pCallbackTag = newCbArg;
        cpaCySymDpEnqueueOp(pOpData, cbArg->kickTail);
    }
}


void *sptCallback(void *arg){
    struct sptArg *pArg = (struct sptArg *)arg;
    symDpCallback(pArg->pOpData, pArg->status, pArg->verifyResult);
}

/* You can have a dedicated polling thread on a separate physical core executing in r2c.
You can also have a spt on a separate physical core, spinning up and assigning threads to to the colocated hw thread
calling thread of the first callback is the SPT thread
pthread is the app thread.
*/
static inline void callback(CpaCySymDpOpData *pOpData,
                          CpaStatus status,
                          CpaBoolean verifyResult){
    pthread_t tid;
    struct sptArg *pArg = NULL;
    PHYS_CONTIG_ALLOC(&pArg, sizeof(struct sptArg));
    pArg->pOpData = pOpData;
    pArg->status = status;
    pArg->verifyResult = verifyResult;

    struct cArg * cArg = pOpData->pCallbackTag;
    int appCore = 19;
    int sptCore = 59;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(appCore, &cpuset);
    status = sampleThreadCreate(&tid, sptCallback, (void *)pArg);
    if(0 != pthread_setaffinity_np(tid, sizeof(cpuset), &cpuset)){
        PRINT_ERR("Error setting affinity\n");
        exit(-1);
    }
}

static inline void populateOpData(CpaCySymDpOpData *pOpData,
CpaInstanceHandle cyInstHandle, CpaCySymDpSessionCtx sessionCtx,
Cpa8U *pSrcBuffer,Cpa8U *pDstBuffer,
Cpa8U *pIvBuffer, Cpa32U bufferSize, void *pCallbackTag){
    CpaPhysicalAddr pPhySrcBuffer;
    CpaPhysicalAddr pPhyDstBuffer;
    pOpData->cryptoStartSrcOffsetInBytes = 0;
    pOpData->messageLenToCipherInBytes = bufferSize;
    pOpData->iv =
        virtAddrToDevAddr((SAMPLE_CODE_UINT *)(uintptr_t)pIvBuffer,
                          cyInstHandle,
                          CPA_ACC_SVC_TYPE_CRYPTO);
    pOpData->pIv = pIvBuffer;
    pOpData->hashStartSrcOffsetInBytes = 0;
    pOpData->messageLenToHashInBytes = bufferSize;
    /* Even though MAC follows immediately after the region to hash
       digestIsAppended is set to false in this case due to
       errata number IXA00378322 */
    pPhySrcBuffer =
        virtAddrToDevAddr((SAMPLE_CODE_UINT *)(uintptr_t)pSrcBuffer,
                          cyInstHandle,
                          CPA_ACC_SVC_TYPE_CRYPTO);
    pPhyDstBuffer =
        virtAddrToDevAddr((SAMPLE_CODE_UINT *)(uintptr_t)pDstBuffer,
                          cyInstHandle,
                          CPA_ACC_SVC_TYPE_CRYPTO);
    pOpData->digestResult = pPhyDstBuffer + sizeof(sampleAlgChainingSrc);
    pOpData->instanceHandle = cyInstHandle;
    pOpData->sessionCtx = sessionCtx;
    pOpData->ivLenInBytes = sizeof(sampleCipherIv);
    pOpData->srcBuffer = pPhySrcBuffer;
    pOpData->srcBufferLen = bufferSize;
    pOpData->dstBuffer = pPhyDstBuffer;
    pOpData->dstBufferLen = bufferSize;
    pOpData->thisPhys =
        virtAddrToDevAddr((SAMPLE_CODE_UINT *)(uintptr_t)pOpData,
                          cyInstHandle,
                          CPA_ACC_SVC_TYPE_CRYPTO);
    pOpData->pCallbackTag = (void *)pCallbackTag;
}

static CpaStatus symDpSubmitBatch(CpaInstanceHandle cyInstHandle,
                                CpaCySymDpSessionCtx sessionCtx,
                                CpaCySymDpOpData **pOpData, /*Can we use
                                the same pOpData for multiple submissions?*/
                                Cpa8U **pSrcBufferList,
                                Cpa8U **pDstBufferList,
                                Cpa32U bufferSize,
                                Cpa32U numOps,
                                Cpa32U bufIdx,
                                Cpa16U intensity,
                                CpaBoolean cbIsDep)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    int sub = 0;
    int numRetries = 10;
    for(sub=bufIdx; sub<(bufIdx+numOps)-1; sub++){
        numRetries = 10;
        struct cbArg *cbArg = NULL;
        PHYS_CONTIG_ALLOC(&cbArg,sizeof(struct cbArg));
        cbArg->mIdx = 0;
        cbArg->bufIdx = sub;
        cbArg->numBufs = numBufs_g;
        cbArg->kickTail = CPA_FALSE;
        cbArg->intensity = intensity;
        cbArg->dependent = cbIsDep;
        cbArg->operandBuffer = pDstBufferList[sub];
        cbArg->operandBufferSize = bufferSize;
        populateOpData(pOpData[sub], cyInstHandle, sessionCtx,
            pSrcBufferList[sub], pDstBufferList[sub],
            pDstBufferList[sub] + sizeof(sampleAlgChainingSrc),
            bufferSize, cbArg);
retry:
        status = cpaCySymDpEnqueueOp(pOpData[sub], CPA_FALSE);
        if(CPA_STATUS_RETRY == status && numRetries > 0){
            if(numRetries > 0){
                numRetries--;
            }
            goto retry;
        }
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("cpaCySymDpEnqueueOp failed. (status = %d)\n", status);
            PRINT_ERR("OpInfo: %d %d %d %d %d\n", sub, pOpData[sub]->cryptoStartSrcOffsetInBytes,
                pOpData[sub]->messageLenToCipherInBytes,
                pOpData[sub]->hashStartSrcOffsetInBytes,
                pOpData[sub]->messageLenToHashInBytes);
            PRINT_ERR("cbArg: %d %d %d %d\n", cbArg->mIdx, cbArg->bufIdx, cbArg->numBufs, cbArg->kickTail);
            exit(-1);
        }
    }
    numRetries = 10;
    struct cbArg *cbArg = NULL;
    PHYS_CONTIG_ALLOC(&cbArg,sizeof(struct cbArg));
    cbArg->mIdx = 0;
    cbArg->bufIdx = sub;
    cbArg->numBufs = numBufs_g;
    cbArg->kickTail = CPA_TRUE;
    cbArg->intensity = intensity;
    cbArg->dependent = cbIsDep;
    cbArg->operandBuffer = pDstBufferList[sub];
    cbArg->operandBufferSize = bufferSize;
    populateOpData(pOpData[sub], cyInstHandle, sessionCtx,
        pSrcBufferList[sub], pDstBufferList[sub],
        pDstBufferList[sub] + sizeof(sampleAlgChainingSrc),
        bufferSize, cbArg);
retry_kick_tail:
    status = cpaCySymDpEnqueueOp(pOpData[sub], CPA_TRUE);
    if(CPA_STATUS_RETRY == status && numRetries > 0){
        if(numRetries > 0){
            numRetries--;
        }
        goto retry_kick_tail;
    }
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("cpaCySymDpEnqueueOp failed. (status = %d)\n", status);
        PRINT_ERR("cpaCySymDpEnqueueOp failed. (status = %d)\n", status);
        PRINT_ERR("OpInfo: %d %d %d %d %d\n", sub, pOpData[sub]->cryptoStartSrcOffsetInBytes,
            pOpData[sub]->messageLenToCipherInBytes,
            pOpData[sub]->hashStartSrcOffsetInBytes,
            pOpData[sub]->messageLenToHashInBytes);
        PRINT_ERR("cbArg: %d %d %d %d\n", cbArg->mIdx, cbArg->bufIdx, cbArg->numBufs, cbArg->kickTail);
        exit(-1);
    }
}

CpaStatus setupInstances(int desiredInstances,
    CpaInstanceHandle *cyInstHandles,
    CpaCySymDpSessionCtx *sessionCtxs,
    CpaBoolean sptEnabled){

    CpaStatus status = CPA_STATUS_FAIL;
    Cpa32U sessionCtxSize = 0;
    CpaInstanceHandle cyInstHandle = NULL;
    CpaCySymSessionSetupData sessionSetupData = {0};
    CpaInstanceInfo2 *info2 = NULL;
    Cpa16U numInstances = 0;
    status = OS_MALLOC(&info2, sizeof(CpaInstanceInfo2));
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Failed to allocate memory for info2");
        return CPA_STATUS_FAIL;
    }
    status = cpaCyGetNumInstances(&numInstances);
    if (numInstances >= desiredInstances)
    {
        numInstances = desiredInstances;
    }
    if ((status == CPA_STATUS_SUCCESS) && (numInstances > 0))
    {
        status = cpaCyGetInstances(numInstances, cyInstHandles);
    }
    if (0 == numInstances)
    {
        PRINT_ERR("No instances found for 'SSL'\n");
        PRINT_ERR("Please check your section names");
        PRINT_ERR(" in the config file.\n");
        PRINT_ERR("Also make sure to use config file version 2.\n");
    }

    for(int i=0; i<numInstances; i++){
        CpaInstanceHandle cyInstHandle = cyInstHandles[i];
        CpaCySymDpSessionCtx sessionCtx = sessionCtxs[i];
        status = cpaCyStartInstance(cyInstHandle);
        if (CPA_STATUS_SUCCESS == status)
        {
            status = cpaCyInstanceGetInfo2(cyInstHandle, info2);
        }
        if (CPA_STATUS_SUCCESS == status)
        {
            if (info2->isPolled == CPA_FALSE)
            {
                status = CPA_STATUS_FAIL;
                PRINT_ERR("This sample code works only with instances "
                        "configured in polling mode\n");
            }
        }
        if (CPA_STATUS_SUCCESS == status)
        {

            /*
            * Set the address translation function for the instance
            */
            status = cpaCySetAddressTranslation(cyInstHandle, sampleVirtToPhys);
        }
        if (CPA_STATUS_SUCCESS == status)
        {
            /* Register callback function for the instance */
            //<snippet name="regCb">
            if(sptEnabled){
                status = cpaCySymDpRegCbFunc(cyInstHandle, callback);
            } else {
                status = cpaCySymDpRegCbFunc(cyInstHandle, symDpCallback);
            }
            //</snippet>
        }
        if (CPA_STATUS_SUCCESS == status)
        {
            /* populate symmetric session data structure */
            //<snippet name="initSession">
            sessionSetupData.sessionPriority = CPA_CY_PRIORITY_HIGH;
            sessionSetupData.symOperation = CPA_CY_SYM_OP_ALGORITHM_CHAINING;
            sessionSetupData.algChainOrder =
                CPA_CY_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH;

            sessionSetupData.cipherSetupData.cipherAlgorithm =
                CPA_CY_SYM_CIPHER_AES_CBC;
            sessionSetupData.cipherSetupData.pCipherKey = sampleCipherKey;
            sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
                sizeof(sampleCipherKey);
            sessionSetupData.cipherSetupData.cipherDirection =
                CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;

            sessionSetupData.hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_SHA256;
            sessionSetupData.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_AUTH;
            sessionSetupData.hashSetupData.digestResultLenInBytes = DIGEST_LENGTH;
            sessionSetupData.hashSetupData.authModeSetupData.authKey =
                sampleCipherKey;
            sessionSetupData.hashSetupData.authModeSetupData.authKeyLenInBytes =
                sizeof(sampleCipherKey);

            /* Even though MAC follows immediately after the region to hash
            digestIsAppended is set to false in this case due to
            errata number IXA00378322 */
            sessionSetupData.digestIsAppended = CPA_FALSE;
            sessionSetupData.verifyDigest = CPA_FALSE;

            /* Determine size of session context to allocate */
            status = cpaCySymDpSessionCtxGetSize(
                cyInstHandle, &sessionSetupData, &sessionCtxSize);
        }
        if (CPA_STATUS_SUCCESS == status)
        {
            /* Allocate session context */
            status = PHYS_CONTIG_ALLOC(&sessionCtx, sessionCtxSize);
            sessionCtxs[i] = sessionCtx;
        }
        if (CPA_STATUS_SUCCESS == status)
        {
            /* Initialize the session */
            status =
                cpaCySymDpInitSession(cyInstHandle, &sessionSetupData, sessionCtx);
        }
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_DBG("Sample code failed with status of %d\n", status);
        }
#ifdef LAC_HW_PRECOMPUTES
        if (CPA_STATUS_SUCCESS == status)
        {
            /* Poll for hw pre-compute responses. */
            do
            {
                status = icp_sal_CyPollDpInstance(cyInstHandle, 0);
            } while (CPA_STATUS_SUCCESS != status);
        }

#endif
    }
    OS_FREE(info2);

}

CpaStatus tearDownInstances(int desiredInstances,
    CpaInstanceHandle *cyInstHandles,
    CpaCySymDpSessionCtx *sessionCtxs){

    for(int i=0; i<desiredInstances; i++){
        CpaInstanceHandle cyInstHandle = cyInstHandles[i];
        CpaCySymDpSessionCtx sessionCtx = sessionCtxs[i];
        CpaStatus status = CPA_STATUS_SUCCESS;
        CpaStatus sessionStatus = CPA_STATUS_SUCCESS;

        /* Wait for inflight requests to complete */
        symSessionWaitForInflightReq(sessionCtx);

        //<snippet name="removeSession">
        sessionStatus = cpaCySymDpRemoveSession(cyInstHandle, sessionCtx);
        //</snippet>

        /* maintain status of remove session only when status of all operations
         * before it are successful. */
        if (CPA_STATUS_SUCCESS == status)
        {
            status = sessionStatus;
        }
        PHYS_CONTIG_FREE(sessionCtx);

        PRINT_DBG("cpaCyStopInstance\n");
        cpaCyStopInstance(cyInstHandle);

        if (CPA_STATUS_SUCCESS == status)
        {
            PRINT_DBG("Sample code ran successfully\n");
        }
        else
        {
            PRINT_DBG("Sample code failed with status of %d\n", status);
        }

        return status;

    }

}

static void bufArrayFactory(Cpa8U ***ppBuffers, Cpa32U numBuffers, Cpa32U bufferSize){
    Cpa8U **pBuffers = NULL;
    PHYS_CONTIG_ALLOC(&pBuffers, sizeof(Cpa8U*) * numBuffers);
    for(int i=0; i<numBuffers; i++){
        Cpa8U *pBuf = NULL;
        PHYS_CONTIG_ALLOC(&pBuf, bufferSize);
        memcpy(pBuf, sampleAlgChainingSrc, sizeof(sampleAlgChainingSrc));
        pBuffers[i] = pBuf;
    }
    *ppBuffers = pBuffers;
}
static void arrayOfBufArraysFactory(Cpa8U ****pppBuffers,
Cpa32U numAxs, Cpa32U numBuffers, Cpa32U bufferSize){
    Cpa8U ***ppBuffers = NULL;
    PHYS_CONTIG_ALLOC(&ppBuffers, sizeof(Cpa8U**) * numAxs);
    for(int i=0; i<numAxs; i++){
        bufArrayFactory(&ppBuffers[i], numBuffers, bufferSize);
    }
    *pppBuffers = ppBuffers;
}
static void validateBufferArray(Cpa8U **pBuffers, Cpa32U numBuffers, Cpa32U bufferSize){
    CpaBoolean copiedCorrectly = CPA_TRUE;
    for(int i=0; i<numBuffers; i++){
        if (0 != memcmp(pBuffers[i], sampleAlgChainingSrc, sizeof(sampleAlgChainingSrc)))
        {
            PRINT_ERR("Output does not match expected output\n");
            copiedCorrectly = CPA_FALSE;
        }
    }
    if(!copiedCorrectly){
        PRINT_ERR("Buffers copied incorrectly\n");
        exit(-1);
    }
}
static void validateArrayOfBufferArrays(Cpa8U ***ppBuffers, Cpa32U numAxs, Cpa32U numBuffers, Cpa32U bufferSize){
    CpaBoolean copiedCorrectly = CPA_TRUE;
    for(int j=0; j<numAxs; j++){
        for(int i=0; i<numBuffers;i++){
            if (0 != memcmp(ppBuffers[j][i], sampleAlgChainingSrc, sizeof(sampleAlgChainingSrc)))
            {
                PRINT_ERR("Output does not match expected output\n");
                copiedCorrectly = CPA_FALSE;
            }
        }
        if(!copiedCorrectly){
            PRINT_ERR("Buffers copied correctly\n");
            exit(-1);
        }
    }
}

static void dedicatedRRPoller(void *arg){
    CpaStatus status = CPA_STATUS_SUCCESS;
    struct pollerArg *pArg = (struct pollerArg *)arg;
    int mIdx = pArg->mIdx;
    int rBS = pArg->rBS;
    PRINT_DBG("Poller Thread %d started\n", mIdx);
    while(doPoll[mIdx] == CPA_TRUE){
        status = icp_sal_CyPollDpInstance(instanceHandles[mIdx], rBS);
    }
    PRINT_DBG("Poller Thread %d stopped\n", mIdx);
}

/* These can use _g's */
static inline void hostSubmitOnly(int chainLength,
Cpa8U ***ppBuffers,
int numBuffers,
int rBS, int bufferSize, CpaCySymDpOpData **pOpData){
    globalDone = 0;
    CpaStatus status = CPA_STATUS_SUCCESS;

    int startSubmitIdx = 0; int endSubmitIdx = rBS;

    do
    {
        if(startSubmitIdx < numBuffers){
            for(int bufIdx=startSubmitIdx; bufIdx<endSubmitIdx; bufIdx+=rBS){
                symDpSubmitBatch(instanceHandles[0], dpSessionCtxs_g[0],
                    pOpData, ppBuffers[0], ppBuffers[1],
                    bufferSize, rBS, bufIdx, intensity_g, cbIsDep_g);
            }
            startSubmitIdx = endSubmitIdx;
            endSubmitIdx = startSubmitIdx + rBS;
        }
    } while (
        ((CPA_STATUS_SUCCESS == status) || (CPA_STATUS_RETRY == status)) &&
        (globalDone == CPA_FALSE));
}

static inline void roundRobinSubmitAndPoll(int chainLength,
Cpa8U ***ppBuffers,
int numBuffers,
int rBS, int bufferSize, CpaCySymDpOpData **pOpData){
    globalDone = 0;
    CpaStatus status = CPA_STATUS_SUCCESS;

    int startSubmitIdx = 0; int endSubmitIdx = rBS;

    do
    {
        if(startSubmitIdx < numBuffers){
            for(int bufIdx=startSubmitIdx; bufIdx<endSubmitIdx; bufIdx+=rBS){
                symDpSubmitBatch(instanceHandles[0], dpSessionCtxs_g[0],
                    pOpData, ppBuffers[0], ppBuffers[1],
                    bufferSize, rBS, bufIdx, intensity_g, cbIsDep_g);
            }
            startSubmitIdx = endSubmitIdx;
            endSubmitIdx = startSubmitIdx + rBS;
        }

        for(int i=0; i<numAxs_g; i++){
            status = icp_sal_CyPollDpInstance(instanceHandles[i], rBS);
        }
    } while (
        ((CPA_STATUS_SUCCESS == status) || (CPA_STATUS_RETRY == status)) &&
        (globalDone == CPA_FALSE));
}


static void spawnPollingThreads(int chainLength, pthread_t **ptid, int batchSize){
    /* SPR SMT-2's*/
    pthread_t *tid;
    PHYS_CONTIG_ALLOC(&tid, sizeof(pthread_t) * chainLength);
    uint32_t coreMap[19] = {41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
                            51, 52, 53, 54, 55, 56, 57, 58, 59};
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for(int i=0; i<chainLength; i++){
        struct pollerArg *axIdx = NULL;
        PHYS_CONTIG_ALLOC(&axIdx, sizeof(struct pollerArg));
        axIdx->instanceHandle = instanceHandles[i];
        axIdx->mIdx = i;
        axIdx->rBS = batchSize;
        doPoll[i] = CPA_TRUE;
        sampleThreadCreate(&tid[i], dedicatedRRPoller, (void *)axIdx);
        CPU_SET(coreMap[i], &cpuset);
        int status = pthread_setaffinity_np(tid[i], sizeof(cpuset), &cpuset);
        if(status != 0){
            PRINT_ERR("Error setting affinity\n");
            exit(-1);
        }
        // osalThreadCreate(&tid[i], NULL, dedicatedRRPoller,
        // (void *)axIdx);
        // osalThreadBind(&tid[i], coreMap[i]);
    }
    *ptid = tid;

}
static void killPollingThreads(int chainLength, pthread_t *tid){
    for(int i=0; i<chainLength; i++){
        doPoll[i] = CPA_FALSE;
        osalThreadKill(&tid[i]);
    }
}
static void genOpDataArray(CpaCySymDpOpData ***ppOpDatas, int numBuffers){
    CpaCySymDpOpData ** pOpDatas = NULL;
    PHYS_CONTIG_ALLOC(&pOpDatas, sizeof(CpaCySymDpOpData*) * numBuffers);
    for(int i=0; i<numBuffers; i++){
        CpaCySymDpOpData *pOpData = NULL;
        PHYS_CONTIG_ALLOC(&pOpData, sizeof(CpaCySymDpOpData));
        pOpDatas[i] = pOpData;
    }
    *ppOpDatas = pOpDatas;
}
void printStats(int numBuffers, int rBS, int bufferSize, int chainLength,
uint64_t avgCycles, CpaBoolean withSpt, Cpa16U cbIntensity, CpaBoolean cbIsDep){
    PRINT("NumBuffers: %d ", numBuffers);
    PRINT("BufferSize: %d ", bufferSize);
    PRINT("BatchSize: %d ", rBS);
    PRINT("ChainLength: %d ", chainLength);
    PRINT("WithSpt: %d ", withSpt);
    PRINT("CallbackIntensity: %d ", cbIntensity);
    PRINT("CallbackIsDependent: %d ", cbIsDep);

    PRINT("AvgOffloadCycles: %lu ", avgCycles);
    PRINT("AvgOffloadMicroseconds: %lu\n", (avgCycles/2000));
}
void startTest(int chainLength, int numBuffers, int rBS, int bufferSize,
CpaBoolean useSpt, Cpa16U cbIntensity, CpaBoolean cbIsDep){
    numAxs_g = chainLength;
    OS_MALLOC(&instanceHandles, sizeof(CpaInstanceHandle) * chainLength);
    OS_MALLOC(&dpSessionCtxs_g, sizeof(CpaCySymDpSessionCtx) * chainLength);
    setupInstances(chainLength, instanceHandles, dpSessionCtxs_g, useSpt);
    Cpa8U *pIvBuffer = NULL;
    CpaStatus status = CPA_STATUS_SUCCESS;
    CpaCySymDpOpData *pOpData;

    numAxs_g = chainLength;
    numBufs_g = numBuffers;
    intensity_g = cbIntensity;
    cbIsDep_g = cbIsDep;
    Cpa32U bufferMetaSize = 0;
    Cpa8U **pSrcBuffers[numAxs_g];

    Cpa8U ***ppBuffers = NULL;
    arrayOfBufArraysFactory(&ppBuffers, numAxs_g, numBuffers, bufferSize);
    validateArrayOfBufferArrays(ppBuffers, numAxs_g, numBuffers, bufferSize);

    CpaCySymDpOpData **pOpDatas = NULL;
    genOpDataArray(&pOpDatas, numBuffers);

    int numIterations = 1000;
    uint64_t start = sampleCoderdtsc();
    for(int i=0; i<numIterations; i++){
        roundRobinSubmitAndPoll(chainLength, ppBuffers,
            numBuffers, rBS, bufferSize, pOpDatas);
    }
    uint64_t end = sampleCoderdtsc();
    uint64_t cycles = end-start;
    uint64_t avgCycles = cycles/numIterations;
    // uint64_t freqKHz = sampleCodeFreqKHz();
    uint64_t cpuFreqMHz = 2000;

    printStats(numBuffers, rBS, bufferSize, chainLength, avgCycles, useSpt, cbIntensity, cbIsDep);
    tearDownInstances(chainLength, instanceHandles, dpSessionCtxs_g);
}
void startDedicatedPollerTest(int chainLength, int numBuffers, int rBS, int bufferSize){
    numAxs_g = chainLength;
    OS_MALLOC(&instanceHandles, sizeof(CpaInstanceHandle) * chainLength);
    OS_MALLOC(&dpSessionCtxs_g, sizeof(CpaCySymDpSessionCtx) * chainLength);
    setupInstances(chainLength, instanceHandles, dpSessionCtxs_g, CPA_FALSE);
    pthread_t *tid = NULL;
    spawnPollingThreads(chainLength, &tid, rBS);
    Cpa8U *pIvBuffer = NULL;
    CpaStatus status = CPA_STATUS_SUCCESS;
    CpaCySymDpOpData *pOpData;

    numAxs_g = chainLength;
    numBufs_g = numBuffers;
    Cpa32U bufferMetaSize = 0;
    Cpa8U **pSrcBuffers[numAxs_g];

    Cpa8U ***ppBuffers = NULL;
    arrayOfBufArraysFactory(&ppBuffers, numAxs_g, numBuffers, bufferSize);
    validateArrayOfBufferArrays(ppBuffers, numAxs_g, numBuffers, bufferSize);

    CpaCySymDpOpData **pOpDatas = NULL;
    genOpDataArray(&pOpDatas, numBuffers);

    int numIterations = 1000;
    uint64_t start = sampleCoderdtsc();
    for(int i=0; i<numIterations; i++){
        hostSubmitOnly(chainLength, ppBuffers,
            numBuffers, rBS, bufferSize, pOpDatas);
    }
    uint64_t end = sampleCoderdtsc();
    uint64_t cycles = end-start;
    uint64_t avgCycles = cycles/numIterations;
    // uint64_t freqKHz = sampleCodeFreqKHz();
    uint64_t cpuFreqMHz = 2000;

    printStats(numBuffers, rBS, bufferSize, chainLength, avgCycles, CPA_FALSE, 0, CPA_FALSE);
    killPollingThreads(chainLength, tid);
    tearDownInstances(chainLength, instanceHandles, dpSessionCtxs_g);
}


