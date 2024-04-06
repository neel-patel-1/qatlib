#include "cpa.h"
#include "cpa_cy_im.h"
#include "cpa_cy_sym_dp.h"
#include "icp_sal_poll.h"
#include "cpa_sample_utils.h"

/* The digest length must be less than or equal to SHA256 digest
   length (16) for this example */
#define DIGEST_LENGTH 32

extern int gDebugParam;
/* AES key, 256 bits long */

int numAxs_g;
volatile Cpa32U fragmentSize_g;
volatile Cpa16U numBufs_g = 0;
CpaBufferList **pSrcBufferList_g = NULL;
CpaBufferList **pDstBufferList_g = NULL;
CpaInstanceHandle *cyInst_g;
CpaCySymDpSessionCtx *sessionCtxs_g;


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

static void symDpCallback(CpaCySymDpOpData *pOpData,
                          CpaStatus status,
                          CpaBoolean verifyResult)
{
    PRINT_DBG("Callback called with status = %d.\n", status);
    pOpData->pCallbackTag = (void *)1;
}

static CpaStatus symDpPerformOp(CpaInstanceHandle cyInstHandle,
                                CpaCySymSessionCtx sessionCtx)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    CpaCySymDpOpData *pOpData = NULL;
    Cpa32U bufferSize = sizeof(sampleAlgChainingSrc) + DIGEST_LENGTH;
    Cpa8U *pSrcBuffer = NULL;
    Cpa8U *pIvBuffer = NULL;

    /* Allocate Src buffer */
    status = PHYS_CONTIG_ALLOC(&pSrcBuffer, bufferSize);

    if (CPA_STATUS_SUCCESS == status)
    {
        /* Allocate IV buffer */
        status = PHYS_CONTIG_ALLOC(&pIvBuffer, sizeof(sampleCipherIv));
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        /* copy source into buffer */
        memcpy(pSrcBuffer, sampleAlgChainingSrc, sizeof(sampleAlgChainingSrc));

        /* copy IV into buffer */
        memcpy(pIvBuffer, sampleCipherIv, sizeof(sampleCipherIv));

        /* Allocate memory for operational data. Note this needs to be
         * 8-byte aligned, contiguous, resident in DMA-accessible
         * memory.
         */
        status =
            PHYS_CONTIG_ALLOC_ALIGNED(&pOpData, sizeof(CpaCySymDpOpData), 8);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        CpaPhysicalAddr pPhySrcBuffer;
        /** Populate the structure containing the operational data that is
         * needed to run the algorithm
         */
        //<snippet name="opDataDp">
        pOpData->cryptoStartSrcOffsetInBytes = 0;
        pOpData->messageLenToCipherInBytes = sizeof(sampleAlgChainingSrc);
        pOpData->iv =
            virtAddrToDevAddr((SAMPLE_CODE_UINT *)(uintptr_t)pIvBuffer,
                              cyInstHandle,
                              CPA_ACC_SVC_TYPE_CRYPTO);
        pOpData->pIv = pIvBuffer;
        pOpData->hashStartSrcOffsetInBytes = 0;
        pOpData->messageLenToHashInBytes = sizeof(sampleAlgChainingSrc);
        /* Even though MAC follows immediately after the region to hash
           digestIsAppended is set to false in this case due to
           errata number IXA00378322 */
        pPhySrcBuffer =
            virtAddrToDevAddr((SAMPLE_CODE_UINT *)(uintptr_t)pSrcBuffer,
                              cyInstHandle,
                              CPA_ACC_SVC_TYPE_CRYPTO);
        pOpData->digestResult = pPhySrcBuffer + sizeof(sampleAlgChainingSrc);
        pOpData->instanceHandle = cyInstHandle;
        pOpData->sessionCtx = sessionCtx;
        pOpData->ivLenInBytes = sizeof(sampleCipherIv);
        pOpData->srcBuffer = pPhySrcBuffer;
        pOpData->srcBufferLen = bufferSize;
        pOpData->dstBuffer = pPhySrcBuffer;
        pOpData->dstBufferLen = bufferSize;
        pOpData->thisPhys =
            virtAddrToDevAddr((SAMPLE_CODE_UINT *)(uintptr_t)pOpData,
                              cyInstHandle,
                              CPA_ACC_SVC_TYPE_CRYPTO);
        pOpData->pCallbackTag = (void *)0;
        //</snippet>
    }

    if (CPA_STATUS_SUCCESS == status)
    {

        PRINT_DBG("cpaCySymDpEnqueueOp\n");
        /** Enqueue symmetric operation */
        //<snippet name="enqueue">
        status = cpaCySymDpEnqueueOp(pOpData, CPA_FALSE);
        //</snippet>
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("cpaCySymDpEnqueueOp failed. (status = %d)\n", status);
        }
        else
        {

            /* Can now enqueue other requests before submitting all requests to
             * the hardware. The cost of submitting the request to the hardware
             * is
             * then amortized across all enqueued requests.
             * In this simple example we have only 1 request to send
             */

            PRINT_DBG("cpaCySymDpPerformOpNow\n");

            /** Submit all enqueued symmetric operations to the hardware */
            //<snippet name="perform">
            status = cpaCySymDpPerformOpNow(cyInstHandle);
            //</snippet>
            if (CPA_STATUS_SUCCESS != status)
            {
                PRINT_ERR("cpaCySymDpPerformOpNow failed. (status = %d)\n",
                          status);
            }
        }
    }
    /* Can now enqueue more operations and/or do other work while
     * hardware processes the request.
     * In this simple example we have no other work to do
     * */

    if (CPA_STATUS_SUCCESS == status)
    {
        /* Poll for responses.
         * Polling functions are implementation specific */
        do
        {
            status = icp_sal_CyPollDpInstance(cyInstHandle, 1);
        } while (
            ((CPA_STATUS_SUCCESS == status) || (CPA_STATUS_RETRY == status)) &&
            (pOpData->pCallbackTag == (void *)0));
    }

    /* Check result */
    if (CPA_STATUS_SUCCESS == status)
    {
        if (0 == memcmp(pSrcBuffer, expectedOutput, bufferSize))
        {
            PRINT_DBG("Output matches expected output\n");
        }
        else
        {
            PRINT_ERR("Output does not match expected output\n");
            status = CPA_STATUS_FAIL;
        }
    }

    PHYS_CONTIG_FREE(pSrcBuffer);
    PHYS_CONTIG_FREE(pIvBuffer);
    PHYS_CONTIG_FREE(pOpData);

    return status;
}

CpaStatus setupInstances(int desiredInstances,
    CpaInstanceHandle *cyInstHandles,
    CpaCySymSessionCtx *sessionCtxs){

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
        CpaCySymSessionCtx sessionCtx = sessionCtxs[i];
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
            status = cpaCySymDpRegCbFunc(cyInstHandle, symDpCallback);
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
            PRINT_DBG("cpaCySymDpSessionCtxGetSize\n");
            status = cpaCySymDpSessionCtxGetSize(
                cyInstHandle, &sessionSetupData, &sessionCtxSize);
        }
        if (CPA_STATUS_SUCCESS == status)
        {
            /* Allocate session context */
            status = PHYS_CONTIG_ALLOC(&sessionCtx, sessionCtxSize);

        }
    }

}


void startTest(int chainLength){
    CpaInstanceHandle instanceHandles[chainLength];
    CpaCySymSessionCtx sessionCtxs[chainLength];
    setupInstances(chainLength, instanceHandles, sessionCtxs);
}