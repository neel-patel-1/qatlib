/***************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 *   redistributing this file, you may do so under either license.
 *
 *   GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2007-2022 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 *
 *   Contact Information:
 *   Intel Corporation
 *
 *   BSD LICENSE
 *
 *   Copyright(c) 2007-2022 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *
 ***************************************************************************/

/*
 * This is sample code that demonstrates usage of the dc chain API,
 * and specifically using this API to perform hash plus compression chain
 * operation.
 */

#include "cpa.h"
#include "cpa_cy_sym.h"
#include "cpa_dc.h"
#include "cpa_dc_chain.h"
#include "cpa_sample_utils.h"
#include "openssl/sha.h"
#include "zlib.h"
#include "cpa_chaining_sample_input.h"
#include "icp_sal_poll.h"
#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include <immintrin.h>

extern int gDebugParam;
volatile Cpa32U fragmentSize_g;
volatile Cpa16U numBufs_g = 0;
CpaBufferList **pSrcBufferList_g = NULL;
CpaBufferList **pDstBufferList_g = NULL;
volatile Cpa32U bufSize_g;
extern struct timespec hashStartTime_g;
extern volatile int test_complete;
volatile Cpa16U numSamples_g;

struct timespec *userDescStart;
struct timespec *userDescEnd;
struct timespec *userSubmitStart;
struct timespec *userSubmitEnd;
struct timespec *userSubmitHashStart;
struct timespec *userSubmitHashEnd;
struct timespec *userPollStart;
struct timespec *userPollEnd;
struct timespec sessionInitStart;
struct timespec sessionInitEnd;

int requestCtr = 0;

// reuse key and iv
static Cpa8U sampleCipherKey[] = {
    0xEE, 0xE2, 0x7B, 0x5B, 0x10, 0xFD, 0xD2, 0x58, 0x49, 0x77, 0xF1, 0x22,
    0xD7, 0x1B, 0xA4, 0xCA, 0xEC, 0xBD, 0x15, 0xE2, 0x52, 0x6A, 0x21, 0x0B,
    0x41, 0x4C, 0x41, 0x4E, 0xA1, 0xAA, 0x01, 0x3F};

/* Initialization vector */
static Cpa8U sampleCipherIv[] = {
    0x7E, 0x9B, 0x4C, 0x1D, 0x82, 0x4A, 0xC5, 0xDF, 0x99, 0x4C, 0xA1, 0x44,
    0xAA, 0x8D, 0x37, 0x27};

// Repeatedly encrypt for balanced chain
int numAxs_g;
CpaInstanceHandle *cyInst_g;
CpaCySymSessionCtx *sessionCtxs_g;

// For pollers
int *gPollingThreads;
int *gPollingCys;
struct pollerInfo {
    CpaInstanceHandle cyInstHandle;
    int idx;
    int rsvd;
};

// Round Robin Policy
int lastIdx = 0;
enum Policy{
    RR
};
enum Policy policy = RR;

// Synchronize between host and
volatile int complete;
struct cbArg{
    Cpa16U mIdx;
    Cpa16U bufIdx;
};
static void interCallback(void *pCallbackTag, CpaStatus status){
    struct cbArg *arg = (struct encChainArg *)pCallbackTag;
    Cpa16U mId = arg->mIdx;
    Cpa16U bufIdx = arg->bufIdx;
    if(arg->bufIdx == (numBufs_g-1)){
        printf("cb: %d complete\n", mId);
    }
    printf("cb: %d buf: %d\n", mId);
}
static void endCallback(void *pCallbackTag, CpaStatus status){
    struct cbArg *arg = (struct encChainArg *)pCallbackTag;
    Cpa16U mId = arg->mIdx;
    if(arg->bufIdx == (numBufs_g-1)){
        printf("cb: %d complete\n", mId);
        complete = 1;
    }
}

static void dcChainFreeBufferList(CpaBufferList **testBufferList)
{
    CpaBufferList *pBuffList = *testBufferList;
    CpaFlatBuffer *pFlatBuff = NULL;
    Cpa32U curBuff = 0;

    if (NULL == pBuffList)
    {
        PRINT_ERR("testBufferList is NULL\n");
        return;
    }

    pFlatBuff = pBuffList->pBuffers;
    while (curBuff < pBuffList->numBuffers)
    {
        if (NULL != pFlatBuff->pData)
        {
            PHYS_CONTIG_FREE(pFlatBuff->pData);
            pFlatBuff->pData = NULL;
        }
        pFlatBuff++;
        curBuff++;
    }

    if (NULL != pBuffList->pPrivateMetaData)
    {
        PHYS_CONTIG_FREE(pBuffList->pPrivateMetaData);
        pBuffList->pPrivateMetaData = NULL;
    }

    // OS_FREE(pBuffList);
    *testBufferList = NULL;
}

static CpaStatus dcChainBuildBufferList(CpaBufferList **testBufferList,
                                        Cpa32U numBuffers,
                                        Cpa32U bufferSize,
                                        Cpa32U bufferMetaSize)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    CpaBufferList *pBuffList = NULL;
    CpaFlatBuffer *pFlatBuff = NULL;
    Cpa32U curBuff = 0;
    Cpa8U *pMsg = NULL;
    /*
     * allocate memory for bufferlist and array of flat buffers in a contiguous
     * area and carve it up to reduce number of memory allocations required.
     */
    Cpa32U bufferListMemSize =
        sizeof(CpaBufferList) + (numBuffers * sizeof(CpaFlatBuffer));

    status = OS_MALLOC(&pBuffList, bufferListMemSize);
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Error in allocating pBuffList\n");
        return CPA_STATUS_FAIL;
    }

    pBuffList->numBuffers = numBuffers;

    if (bufferMetaSize)
    {
        status =
            PHYS_CONTIG_ALLOC(&pBuffList->pPrivateMetaData, bufferMetaSize);
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Error in allocating pBuffList->pPrivateMetaData\n");
            OS_FREE(pBuffList);
            return CPA_STATUS_FAIL;
        }
    }
    else
    {
        pBuffList->pPrivateMetaData = NULL;
    }

    pFlatBuff = (CpaFlatBuffer *)(pBuffList + 1);
    pBuffList->pBuffers = pFlatBuff;
    while (curBuff < numBuffers)
    {
        if (0 != bufferSize)
        {
            status = PHYS_CONTIG_ALLOC(&pMsg, bufferSize);
            if (CPA_STATUS_SUCCESS != status || NULL == pMsg)
            {
                PRINT_ERR("Error in allocating pMsg\n");
                dcChainFreeBufferList(&pBuffList);
                return CPA_STATUS_FAIL;
            }
            memset(pMsg, 0, bufferSize);
            pFlatBuff->pData = pMsg;
        }
        else
        {
            pFlatBuff->pData = NULL;
        }
        pFlatBuff->dataLenInBytes = bufferSize;
        pFlatBuff++;
        curBuff++;
    }

    *testBufferList = pBuffList;

    return CPA_STATUS_SUCCESS;
}

FILE * file = NULL;
#define CALGARY "/lib/firmware/calgary"
static CpaStatus populateBufferList(CpaBufferList **testBufferList,
                                        Cpa32U numBuffers,
                                        Cpa32U bufferSize,
                                        Cpa32U bufferMetaSize)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    CpaBufferList *pBuffList = *testBufferList;

    if(file == NULL){
        file = fopen(CALGARY, "r");
    }
    for(int i=0; i<numBuffers; i++){
        uint64_t offset = 0;
        offset = fread(pBuffList->pBuffers[i].pData, 1, bufferSize, file);
        if ( offset < bufferSize){
            rewind(file);
            if (fread((pBuffList->pBuffers[i].pData) + offset, 1, bufferSize - offset, file) < 1){
                PRINT_ERR("Error in reading file\n");
                return CPA_STATUS_FAIL;
            }
        }
    }
    return CPA_STATUS_SUCCESS;
}



static void spawnSingleAx(int numAxs){
    cyInst_g= qaeMemAlloc( sizeof(CpaInstanceHandle) * numAxs);
    OS_MALLOC(&sessionCtxs_g, sizeof(CpaCySymSessionCtx) * numAxs);

    /* Check available instances */
    Cpa16U numInstances = 0;
    CpaStatus status = CPA_STATUS_SUCCESS;
    CpaInstanceInfo2 info = {0};

    status = cpaCyGetNumInstances(&numInstances);
    if(numAxs > numInstances){
        printf("Not enough instances\n");
        exit(-1);
    }
    if ((status == CPA_STATUS_SUCCESS) && (numInstances > 0))
    {
        status = cpaCyGetInstances(numAxs, cyInst_g);
        if (status != CPA_STATUS_SUCCESS){
            printf("Failed to get Cy Instances\n");
            exit(-1);
        }
    }

    for(int i=0; i<numAxs; i++){
        status = cpaCyInstanceGetInfo2(cyInst_g[i], &info);
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("could not get instance info\n");
            return status;
        }
        CpaInstanceHandle singleCyInstHandle = cyInst_g[i];
        CpaCySymSessionCtx sessionCtx = sessionCtxs_g[i];
        Cpa32U sessionCtxSize = 0;
        CpaCySymSessionSetupData sessionSetupData = {0};
        CpaCySymStats64 symStats = {0};

        printf("Configuring cy inst at address: %p\n", singleCyInstHandle);
        if(singleCyInstHandle == NULL){
            printf("Failed to get Cy Instance\n");
            exit(-1);
        }

        status = cpaCyStartInstance(singleCyInstHandle);
        if (status != CPA_STATUS_SUCCESS)
        {
            printf("Failed to start Cy Instance\n");
            return CPA_STATUS_FAIL;
        }

        if (CPA_STATUS_SUCCESS == status)
        {
            /*
            * Set the address translation function for the instance
            */
            status = cpaCySetAddressTranslation(singleCyInstHandle, sampleVirtToPhys);
        }
        if (CPA_STATUS_SUCCESS == status)
        {
            /*
            * If the instance is polled start the polling thread. Note that
            * how the polling is done is implementation-dependent.
            */


            sessionSetupData.sessionPriority = CPA_CY_PRIORITY_NORMAL;
            sessionSetupData.symOperation = CPA_CY_SYM_OP_CIPHER;
            sessionSetupData.cipherSetupData.cipherAlgorithm =
                CPA_CY_SYM_CIPHER_AES_CBC;
            sessionSetupData.cipherSetupData.pCipherKey = sampleCipherKey;
            sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
                sizeof(sampleCipherKey);
            sessionSetupData.cipherSetupData.cipherDirection =
                CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;
            //</snippet>

            /* Determine size of session context to allocate */

            status = cpaCySymSessionCtxGetSize(
                singleCyInstHandle, &sessionSetupData, &sessionCtxSize);


        }
        if (CPA_STATUS_SUCCESS == status)
        {
            /* Allocate session context */
            status = PHYS_CONTIG_ALLOC(&sessionCtx, sessionCtxSize);
            printf("Address of sessionctx %p\n", sessionCtx);
            printf("Address of sessionCtxs_g %p\n", sessionCtxs_g[i]);
        }

        if (CPA_STATUS_SUCCESS == status)
        {
            /* Initialize the session */
            if( i >= numAxs - 1){
                status = cpaCySymInitSession(
                    singleCyInstHandle, endCallback, &sessionSetupData, sessionCtx);
            } else {
                status = cpaCySymInitSession(
                    singleCyInstHandle, interCallback, &sessionSetupData, sessionCtx);
            }
        }
        if (CPA_STATUS_SUCCESS != status){
            printf("Failed to initialize Cy Session\n");
            exit(-1);
        }

        if(pSrcBufferList_g == NULL){
            Cpa32U bufferMetaSize = 0;
            Cpa8U *pBufferMeta = NULL;
            PHYS_CONTIG_ALLOC(&pSrcBufferList_g, numBufs_g * sizeof(CpaBufferList *));
            PHYS_CONTIG_ALLOC(&pDstBufferList_g, numBufs_g * sizeof(CpaBufferList *));
            status =
                cpaCyBufferListGetMetaSize(singleCyInstHandle, numBufs_g, &bufferMetaSize);
            if(status != CPA_STATUS_SUCCESS){
                printf("Failed to get buffer meta size: %d\n", status);
                exit(-1);
            }
                status = PHYS_CONTIG_ALLOC(&pBufferMeta, bufferMetaSize);
            printf("Meta Size: %d\n", bufferMetaSize);
            for(int i=0; i<numBufs_g;i++){
                dcChainBuildBufferList(&pSrcBufferList_g[i], 1, fragmentSize_g, bufferMetaSize);
                dcChainBuildBufferList(&pDstBufferList_g[i], 1, fragmentSize_g, bufferMetaSize);
            }
            for(int i=0; i<numBufs_g;i++){
                populateBufferList(&pSrcBufferList_g[i], 1, fragmentSize_g, bufferMetaSize);
            }

        }
        printf("polling instance at address: %p\n", singleCyInstHandle);
        status = icp_sal_CyPollInstance(singleCyInstHandle, 0);
        if(status != CPA_STATUS_SUCCESS && status != CPA_STATUS_RETRY){
            printf("Failed to poll instance: %d\n", i);
            exit(-1);
        }
        printf("polling instance at address: %p\n", cyInst_g[i]);
        status = icp_sal_CyPollInstance(cyInst_g[i], 0);
        if(status != CPA_STATUS_SUCCESS && status != CPA_STATUS_RETRY){
            printf("Failed to poll instance: %d\n", i);
            exit(-1);
        }
        // Cpa8U *pIvBuffer = NULL;
        // status = PHYS_CONTIG_ALLOC(&pIvBuffer, sizeof(sampleCipherIv));
        // CpaCySymOpData *pOpData = NULL;
        // status = OS_MALLOC(&pOpData, sizeof(CpaCySymOpData));
        // pOpData->packetType = CPA_CY_SYM_PACKET_TYPE_FULL;
        // pOpData->pIv = pIvBuffer;
        // pOpData->ivLenInBytes = sizeof(sampleCipherIv);
        // pOpData->cryptoStartSrcOffsetInBytes = 0;
        // pOpData->sessionCtx = sessionCtx;
        // pOpData->messageLenToCipherInBytes = pSrcBufferList_g[0]->pBuffers->dataLenInBytes;
        // printf("%d\n", pOpData->messageLenToCipherInBytes);
        // status = cpaCySymPerformOp(
        //     cyInst_g[i],
        //     (void *)(),
        //     pOpData,
        //     pSrcBufferList_g[0],     /* source buffer list */
        //     pDstBufferList_g[0],     /* destination buffer list */
        //     NULL);
        // if(status != CPA_STATUS_SUCCESS){
        //     printf("Failed to submit request: %d\n", status);
        //     exit(-1);
        // }
        // while(icp_sal_CyPollInstance(cyInst_g[i], 0) != CPA_STATUS_SUCCESS){}
        // printf("Request to ax %d submitted and rcvd\n", i);
        sessionCtxs_g[i] = sessionCtx;
    }
    numAxs_g = numAxs;
    printf("Chain Configured\n");
    // exit(0);
}

/*
 At chained offload startup
 how long should a host core go on making requests
 before it starts checking for responses?

 Which axs should be polled?
 We know which ones still have in flight requests
 Config (1):
    Shared variable synchronization
    Is this realistic?
    We are somewhere between the library layer and application layer
    We can keep track of whatever book-keeping is necessary



The question we answer in this section is how the runtime or application
can integrate the SPT into the processing loop to prevent
By spawning a polling instance and scheduling it on the SPT thread,
a
*/

/*

*/

static inline CpaInstanceHandle getNextRequestHandleRR(){
    int nextIdx = (lastIdx + 1) % numAxs_g;
    CpaInstanceHandle instHandle = cyInst_g[nextIdx];
    return instHandle;
}

static inline CpaInstanceHandle getNextRequestHandle(){
    switch(policy){
        case RR:
            return getNextRequestHandleRR();
        default:
            return getNextRequestHandleRR();
    }
}

static void pollingThread(void * info)
{
    struct pollerInfo *pollerInfo = (struct pollerInfo *)info;
    CpaInstanceHandle cyInstHandle = cyInst_g[pollerInfo->idx];
    int idx = pollerInfo->idx;
    gPollingCys[idx] = 1;
    printf("Poller %d started\n", idx);
    while (gPollingCys[idx])
    {
        // printf("Polling cy inst %d at address: %p\n", idx, cyInstHandle);
        CpaStatus status = icp_sal_CyPollInstance(cyInstHandle, 0);
        if(status == CPA_STATUS_SUCCESS){
            gPollingCys[idx] = 0;
        }

        if(  status != CPA_STATUS_SUCCESS && status != CPA_STATUS_RETRY){
            printf("Failed to poll instance: %d\n", status);
            exit(-1);
        }
    }
    printf("Poller %d exited\n", idx);

    sampleThreadExit();
}

void startPollingAllAx()
{
    CpaInstanceInfo2 info2 = {0};
    CpaStatus status = CPA_STATUS_SUCCESS;
    gPollingThreads = malloc(sizeof(sampleThread) * numAxs_g);
    gPollingCys = malloc(sizeof(int) * numAxs_g);

    for(int i=0; i< numAxs_g; i++){
        gPollingCys[i] = 0;
        status = cpaCyInstanceGetInfo2(cyInst_g[i], &info2);
        if ((status == CPA_STATUS_SUCCESS) && (info2.isPolled == CPA_TRUE))
        {
            struct pollerInfo *pollerInfo = malloc(sizeof(struct pollerInfo));
            pollerInfo->cyInstHandle = cyInst_g[i];
            pollerInfo->idx = i;
            /* Start thread to poll instance */
            sampleThreadCreate(&gPollingThreads[i], pollingThread, (void *)pollerInfo);
        } else{
            printf("Failed to get instance info\n");
            exit(-1);
        }

    }

}

/*
Receive requests following some distribution. Dequeue the requests and submit them to the first accelerator

*/

static void singleCoreRequestTransformPoller(){
    CpaStatus status;
    Cpa8U *pIvBuffer = NULL;
    status = PHYS_CONTIG_ALLOC(&pIvBuffer, sizeof(sampleCipherIv));
    CpaCySymOpData *pOpData = NULL;
    status = OS_MALLOC(&pOpData, sizeof(CpaCySymOpData));
    pOpData->packetType = CPA_CY_SYM_PACKET_TYPE_FULL;
    pOpData->pIv = pIvBuffer;
    pOpData->ivLenInBytes = sizeof(sampleCipherIv);
    pOpData->cryptoStartSrcOffsetInBytes = 0;
    pOpData->messageLenToCipherInBytes = pSrcBufferList_g[0]->pBuffers->dataLenInBytes;
    struct cbArg *arg=NULL;

    int bufIdx = 0;
    for(int i=0; i<numBufs_g; i++){ /* Submit Everything */
        status = OS_MALLOC(&arg, sizeof(struct cbArg));
        int axIdx = 0; /* getNextRequestHandle Updated */
        arg->mIdx = axIdx;
        arg->bufIdx = bufIdx;
        pOpData->sessionCtx = sessionCtxs_g[axIdx];
        printf("Submitting request %d to cy inst %d\n", bufIdx, axIdx);
        status = cpaCySymPerformOp(
            cyInst_g[axIdx],
            (void *)arg,
            pOpData,
            pSrcBufferList_g[bufIdx],     /* source buffer list */
            pDstBufferList_g[bufIdx],     /* destination buffer list */
            NULL);
        if(status != CPA_STATUS_SUCCESS){
            printf("Failed to submit request: %d\n", status);
            exit(-1);
        }
        bufIdx = (bufIdx + 1) % numBufs_g;
        while(icp_sal_CyPollInstance(cyInst_g[axIdx], 0) != CPA_STATUS_SUCCESS){}
    }
    printf("Requests Submitted\n");
    while(!complete){

    }
}

/*
callback best location to fwd?
- direct access to payload before/after restructuring
- requires deciding whether to forward at each stage of the pipeline
- host should decide when to batch / forward and must know how many fragments from a given
ax have been accumulated
- if 4
*/

static void startExp(){
    struct timespec start, end;
    CpaStatus status;
    complete = 0;
    spawnSingleAx(1);
    // startPollingAllAx();

    clock_gettime(CLOCK_MONOTONIC, &start);
    singleCoreRequestTransformPoller();
    clock_gettime(CLOCK_MONOTONIC, &end);


    printf("Single Core Request Transform Time: %ld\n",
        (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec));
    for(int i=0; i<numAxs_g; i++){
        gPollingCys[i] = 0;
        symSessionWaitForInflightReq(sessionCtxs_g[i]);
        status = cpaCySymRemoveSession(cyInst_g[i], sessionCtxs_g[i]);
        if(status != CPA_STATUS_SUCCESS){
            printf("Failed to remove session: %d\n", status);
            exit(-1);
        }
    }
    printf("Test Complete\n");
    exit(0);
}


#define FAIL_ON_CPA_FAIL(x)                                                             \
    if (x != CPA_STATUS_SUCCESS)                                                                     \
    {                                                                          \
        PRINT_ERR("Error: %s\n", #x);                                           \
        return CPA_STATUS_FAIL;                                                \
    }
#define FAIL_ON(x, msg)                                                             \
    if (x)                                                                     \
    {                                                                          \
        PRINT_ERR("Error: %s\n", msg);                                          \
    }


#define NUM_SESSIONS_TWO (2)

/* Used by ZLIB */
#define DEFLATE_DEF_WINBITS (15)

/* Return digest length of hash algorithm */
#define GET_HASH_DIGEST_LENGTH(hashAlg)                                        \
    ({                                                                         \
        int length;                                                            \
        if (hashAlg == CPA_CY_SYM_HASH_SHA1)                                   \
        {                                                                      \
            length = 20;                                                       \
        }                                                                      \
        else if (hashAlg == CPA_CY_SYM_HASH_SHA256)                            \
        {                                                                      \
            length = 32;                                                       \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            length = 0;                                                        \
        }                                                                      \
        length;                                                                \
    })


/* Calculate software digest */
static inline CpaStatus calSWDigest(Cpa8U *msg,
                                    Cpa32U slen,
                                    Cpa8U *digest,
                                    Cpa32U dlen,
                                    CpaCySymHashAlgorithm hashAlg)
{
    switch (hashAlg)
    {
        case CPA_CY_SYM_HASH_SHA1:
            return (SHA1(msg, slen, digest) == NULL) ? CPA_STATUS_FAIL
                                                     : CPA_STATUS_SUCCESS;
        case CPA_CY_SYM_HASH_SHA256:
            return (SHA256(msg, slen, digest) == NULL) ? CPA_STATUS_FAIL
                                                       : CPA_STATUS_SUCCESS;
        default:
            PRINT_ERR("Unsupported hash algorithm %d\n", hashAlg);
            return CPA_STATUS_UNSUPPORTED;
    }
}

/* Initilise a zlib stream */
static CpaStatus inflate_init(z_stream *stream)
{
    int ret = 0;
    stream->zalloc = (alloc_func)0;
    stream->zfree = (free_func)0;
    stream->opaque = (voidpf)0;
    stream->next_in = Z_NULL;
    stream->next_out = Z_NULL;
    stream->avail_in = stream->avail_out = stream->total_out = 0;
    stream->adler = 0;

    ret = inflateInit2(stream, -DEFLATE_DEF_WINBITS);
    if (Z_OK != ret)
    {
        PRINT_ERR("Error in inflateInit2, ret = %d\n", ret);
        return CPA_STATUS_FAIL;
    }
    return CPA_STATUS_SUCCESS;
}

/* Decompress data on a zlib stream */
static CpaStatus inflate_decompress(z_stream *stream,
                                    const Cpa8U *src,
                                    Cpa32U slen,
                                    Cpa8U *dst,
                                    Cpa32U dlen)
{
    int ret = 0;
    int flushFlag = Z_NO_FLUSH;

    stream->next_in = (Cpa8U *)src;
    stream->avail_in = slen;
    stream->next_out = (Cpa8U *)dst;
    stream->avail_out = dlen;

    ret = inflate(stream, flushFlag);
    if (ret < Z_OK)
    {
        PRINT_ERR("Error in inflate, ret = %d\n", ret);
        PRINT_ERR("stream->msg = %s\n", stream->msg);
        PRINT_ERR("stream->adler = %u\n", (unsigned int)stream->adler);
        return CPA_STATUS_FAIL;
    }
    return CPA_STATUS_SUCCESS;
}

/* Close zlib stream */
static void inflate_destroy(struct z_stream_s *stream)
{
    inflateEnd(stream);
}

/* Copy multiple buffers data in buffer lists to flat buffer */
static void copyMultiFlatBufferToBuffer(CpaBufferList *pBufferListSrc,
                                        Cpa8U *pBufferDst)
{
    int i = 0;
    int offset = 0;
    CpaFlatBuffer *pBuffers = pBufferListSrc->pBuffers;

    for (; i < pBufferListSrc->numBuffers; i++)
    {
        memcpy(pBufferDst + offset, pBuffers->pData, pBuffers->dataLenInBytes);
        offset += pBuffers->dataLenInBytes;
        pBuffers++;
    }
}

/*
 * Callback function
 *
 * This function is "called back" (invoked by the implementation of
 * the API) when the asynchronous operation has completed.  The
 * context in which it is invoked depends on the implementation, but
 * as described in the API it should not sleep (since it may be called
 * in a context which does not permit sleeping, e.g. a Linux bottom
 * half).
 *
 * This function can perform whatever processing is appropriate to the
 * application.  For example, it may free memory, continue processing
 * of a packet, etc.  In this example, the function only sets the
 * complete variable to indicate it has been called.
 */
//<snippet name="dcCallback">

//</snippet>

/* Build dc chain buffer lists */


/* Free dc chain buffer lists */


uint64_t *ts;

/* Generate Bufferlist length 1 with data*/
static void createTestBufferList(CpaBufferList **ptrToSrcBufList,
                                 Cpa8U *pSrcBuffer,
                                 int size)
{
    CpaBufferList *pBufferListSrc = *ptrToSrcBufList;
    CpaFlatBuffer *pFlatBuffer = (CpaFlatBuffer *)(pBufferListSrc + 1);
    pBufferListSrc->pBuffers = pFlatBuffer;
    pBufferListSrc->numBuffers = 1;
    pFlatBuffer->dataLenInBytes = size;
    pFlatBuffer->pData = pSrcBuffer;
}

/*
Create numFragments bufferlists each of size fragmentSize
containing data from the file CALGARY

*/

static void createTestBufferLists(CpaBufferList ***testBufferLists,
                                  int fragmentSize,
                                  int numFragments)
{
    FILE *file = fopen(CALGARY, "r");
    fseek(file, 0, SEEK_END);
    uint64_t fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    CpaBufferList **srcBufferLists = NULL;
    OS_MALLOC(&srcBufferLists, numFragments * sizeof(CpaBufferList *));
    Cpa32U bufferListMemSize =
            sizeof(CpaBufferList) + (numFragments * sizeof(CpaFlatBuffer));
    Cpa8U *pSrcBuffer = NULL;
    CpaStatus status = PHYS_CONTIG_ALLOC(&pSrcBuffer, fragmentSize);
    for(int i=0; i<numFragments; i++){
        CpaBufferList *pBufferListSrc = srcBufferLists[i];
        status = OS_MALLOC(&pBufferListSrc, bufferListMemSize);
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Error in allocating pBufferListSrc\n");
            return;
        }
        uint64_t offset = 0;
        offset = fread(pSrcBuffer, 1, fragmentSize, file);
        if ( offset < fragmentSize){
            rewind(file);
            fread(pSrcBuffer + offset, 1, fragmentSize - offset, file);
        }
        createTestBufferList(&pBufferListSrc, pSrcBuffer, fragmentSize);
        if( memcmp(pBufferListSrc->pBuffers->pData, pSrcBuffer, fragmentSize) != 0){
            printf("Buffer List Creation Failed\n");
            exit(-1);
        }
        // printf("%s\0\n", pBufferListSrc->pBuffers->pData);
    }
    *testBufferLists = srcBufferLists;
}

void symCallback(void *pCallbackTag,
                        CpaStatus status,
                        const CpaCySymOp operationType,
                        void *pOpData,
                        CpaBufferList *pDstBuffer,
                        CpaBoolean verifyResult)
{


}

CpaStatus decompressAndVerify(Cpa8U* orig, Cpa8U* hwCompBuf,
    Cpa8U* hwDigest, Cpa32U size){
    struct z_stream_s stream = {0};
    Cpa8U *pDecompBuffer = NULL;
    Cpa8U *pHWCompBuffer = NULL;
    Cpa8U *swDigestBuffer = NULL;


    CpaStatus status;
    status = PHYS_CONTIG_ALLOC(&swDigestBuffer, SHA256_DIGEST_LENGTH);
    calSWDigest(orig, size,swDigestBuffer, SHA256_DIGEST_LENGTH, CPA_CY_SYM_HASH_SHA256);
    if (memcmp(swDigestBuffer, hwDigest, SHA256_DIGEST_LENGTH)!=0){
        PRINT_ERR("Decompressed data does not match original\n");
        return CPA_STATUS_FAIL;

    }

    status = inflate_init(&stream);
    status = PHYS_CONTIG_ALLOC(&pDecompBuffer, size);
    inflate_decompress(&stream, hwCompBuf, size, pDecompBuffer, size);
    if (memcmp(orig, pDecompBuffer, size)!=0){
        PRINT_ERR("Decompressed data does not match original\n");
        return CPA_STATUS_FAIL;
    }
    return CPA_STATUS_SUCCESS;

}

CpaStatus requestGen(int fragmentSize, int numFragments, int testIter){
    Cpa32U sessionCtxSize = 0;
    CpaInstanceHandle cyInstHandle = NULL;
    CpaCySymSessionCtx sessionCtx = NULL;
    CpaCySymSessionSetupData sessionSetupData = {0};
    CpaCySymStats64 symStats = {0};
    CpaStatus status = CPA_STATUS_SUCCESS;

    Cpa8U *pBufferMeta = NULL;
    Cpa32U bufferMetaSize = 0;
    CpaBufferList *pBufferList = NULL;
    CpaFlatBuffer *pFlatBuffer = NULL;
    CpaCySymOpData *pOpData = NULL;
    Cpa32U bufferSize = fragmentSize + SHA256_DIGEST_LENGTH;
    Cpa32U numBuffers = 1;
    Cpa32U bufferListMemSize =
        sizeof(CpaBufferList) + (numBuffers * sizeof(CpaFlatBuffer));
    Cpa8U *pSrcBuffer = NULL;
    Cpa8U **pDigestBuffers = NULL;
    struct COMPLETION_STRUCT complete;

    CpaInstanceHandle dcInstHandle = NULL;
    CpaDcSessionHandle sessionHdl = NULL;
    CpaDcSessionSetupData sd = {0};
    CpaDcStats dcStats = {0};
    CpaDcInstanceCapabilities cap = {0};
    CpaBufferList **bufferInterArray = NULL;
    Cpa32U buffMetaSize = 0;
    Cpa16U numInterBuffLists = 0;
    Cpa16U bufferNum = 0;
    Cpa32U sess_size = 0;
    Cpa32U ctx_size = 0;

    Cpa32U dstBufferSize = bufferSize - SHA256_DIGEST_LENGTH;
    CpaBufferList *pBufferListSrc = NULL;
    Cpa8U *pBufferMetaSrc = NULL;
    Cpa8U *pBufferMetaDst = NULL;
    CpaBufferList *pBufferListDst = NULL;
    Cpa8U *pDstBuffer = NULL;
    CpaDcOpData opData = {};
    CpaDcRqResults dcResults;
    INIT_OPDATA(&opData, CPA_DC_FLUSH_FINAL);

    numSamples_g = testIter;

    struct timespec *hashUserspacePerformOpStart, *hashUserspacePerformOpEnd,
        *dcUserspacePerformOpEnd, *dcUserspacePerformOpStart;
    struct timespec *userHashPollStart, *userHashPollEnd,
        *userDCPollStart, *userDCPollEnd;

    // sampleCyGetInstance(&cyInstHandle);


    // status = cpaCyStartInstance(cyInstHandle);
    // status = cpaCySetAddressTranslation(cyInstHandle, sampleVirtToPhys);
    // status = cpaDcSetAddressTranslation(dcInstHandle, sampleVirtToPhys);


    // sessionSetupData.sessionPriority = CPA_CY_PRIORITY_NORMAL;
    // sessionSetupData.symOperation = CPA_CY_SYM_OP_HASH;
    // sessionSetupData.hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_SHA256;
    // sessionSetupData.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_PLAIN;
    // sessionSetupData.hashSetupData.digestResultLenInBytes = SHA256_DIGEST_LENGTH;
    // sessionSetupData.digestIsAppended = CPA_FALSE;
    // sessionSetupData.verifyDigest = CPA_FALSE;

    fragmentSize_g = fragmentSize;
    bufSize_g = fragmentSize;

    // status = cpaCySymSessionCtxGetSize(
    //     cyInstHandle, &sessionSetupData, &sessionCtxSize);
    // status = PHYS_CONTIG_ALLOC(&sessionCtx, sessionCtxSize);
    // status = cpaCySymInitSession(
    //     cyInstHandle, symCallback, &sessionSetupData, sessionCtx);

    // status =
    //     cpaCyBufferListGetMetaSize(cyInstHandle, numBuffers, &bufferMetaSize);
    // status = PHYS_CONTIG_ALLOC(&pBufferMeta, bufferMetaSize);

    // status = cpaDcBufferListGetMetaSize(dcInstHandle, 1, &buffMetaSize);
    // // printf("Buffer Meta Size: %d\n", buffMetaSize);
    // status = cpaDcGetNumIntermediateBuffers(dcInstHandle,
                                                    // &numInterBuffLists);
    // printf("Num Intermediate Buffers: %d\n", numInterBuffLists);

    // if (numInterBuffLists > 0){
    //     status = PHYS_CONTIG_ALLOC(
    //             &bufferInterArray, numInterBuffLists * sizeof(CpaBufferList *));
    //     for (bufferNum = 0; bufferNum < numInterBuffLists; bufferNum++)
    //     {
    //         status = PHYS_CONTIG_ALLOC(&bufferInterArray[bufferNum],
    //                                         sizeof(CpaBufferList));
    //         status = PHYS_CONTIG_ALLOC(
    //                     &bufferInterArray[bufferNum]->pPrivateMetaData,
    //                     buffMetaSize);
    //         status =
    //                     PHYS_CONTIG_ALLOC(&bufferInterArray[bufferNum]->pBuffers,
    //                                     sizeof(CpaFlatBuffer));
    //         status = PHYS_CONTIG_ALLOC(
    //                     &bufferInterArray[bufferNum]->pBuffers->pData,
    //                     2 * fragmentSize);
    //         bufferInterArray[bufferNum]->numBuffers = 1;
    //                 bufferInterArray[bufferNum]->pBuffers->dataLenInBytes =
    //                     2 * fragmentSize;
    //     }
    // }
    // status = cpaDcStartInstance(
    //     dcInstHandle, numInterBuffLists, bufferInterArray);
    // sd.compLevel = CPA_DC_L1;
    // sd.compType = CPA_DC_DEFLATE;
    // sd.huffType = CPA_DC_HT_STATIC;
    // sd.autoSelectBestHuffmanTree = CPA_DC_ASB_DISABLED;
    // sd.sessDirection = CPA_DC_DIR_COMPRESS;
    // sd.sessState = CPA_DC_STATELESS;
    // sd.checksum = CPA_DC_CRC32;
    // // printf("Buffer Size: %d, Fragment Size: %d\n", bufferSize, fragmentSize);
    // status = cpaDcGetSessionSize(dcInstHandle, &sd, &sess_size, &ctx_size);
    // // printf("Session Size: %d\n", sess_size);
    // FAIL_ON_CPA_FAIL(status);
    // status = PHYS_CONTIG_ALLOC(&sessionHdl, sess_size);
    // bool do_sync = false;


    // status =
    //     cpaCyBufferListGetMetaSize(cyInstHandle, numBuffers, &bufferMetaSize);
    // status = PHYS_CONTIG_ALLOC(&pBufferMeta, bufferMetaSize);
    // status = OS_MALLOC(&pBufferList, bufferListMemSize);
    // status = PHYS_CONTIG_ALLOC(&pSrcBuffer, bufferSize);

    // pFlatBuffer = (CpaFlatBuffer *)(pBufferList + 1);

    // pBufferList->pBuffers = pFlatBuffer;
    // pBufferList->numBuffers = 1;
    // pBufferList->pPrivateMetaData = pBufferMeta;

    // pFlatBuffer->dataLenInBytes = bufferSize;
    // pFlatBuffer->pData = pSrcBuffer;

    // /* Hash Op Data */
    // status = OS_MALLOC(&pOpData, sizeof(CpaCySymOpData));
    // pOpData->sessionCtx = sessionCtx;
    // pOpData->packetType = CPA_CY_SYM_PACKET_TYPE_FULL;
    // pOpData->hashStartSrcOffsetInBytes = 0;
    // pOpData->messageLenToHashInBytes = fragmentSize;

    // COMPLETION_INIT((&complete));




    // /* DC Op */
    // status =
    //     cpaDcBufferListGetMetaSize(dcInstHandle, numBuffers, &bufferMetaSize);
    // status = cpaDcDeflateCompressBound(
    //     dcInstHandle, sd.huffType, bufferSize, &dstBufferSize);
    // status = cpaDcDeflateCompressBound(
    //     dcInstHandle, sd.huffType, bufferSize, &dstBufferSize);

    // status = PHYS_CONTIG_ALLOC(&pBufferMetaSrc, bufferMetaSize);
    // FAIL_ON_CPA_FAIL(status);

    // status = OS_MALLOC(&pBufferListSrc, bufferListMemSize);
    // FAIL_ON_CPA_FAIL(status);

    // status = PHYS_CONTIG_ALLOC(&pSrcBuffer, bufferSize);
    // FAIL_ON_CPA_FAIL(status);

    // status = PHYS_CONTIG_ALLOC(&pBufferMetaDst, bufferMetaSize);
    // FAIL_ON_CPA_FAIL(status);
    // status = OS_MALLOC(&pBufferListDst, bufferListMemSize);
    // FAIL_ON_CPA_FAIL(status);
    // status = PHYS_CONTIG_ALLOC(&pDstBuffer, dstBufferSize);
    // FAIL_ON_CPA_FAIL(status);


        /* */

    numBufs_g = numFragments;

    // sampleCyStartPolling(cyInstHandle);
    startExp();
    printf("Test complete\n");
    return 0;

    /* Run Tests */
    uint64_t exeTimes[testIter];
    PHYS_CONTIG_ALLOC(&ts, sizeof(uint64_t) * numFragments);
    int buf_idx = 0;

    /* Affinitize Requestor */
    pthread_t self = pthread_self();
    status = utilCodeThreadBind(self, 4);
    if(status != CPA_STATUS_SUCCESS){
        PRINT_ERR("Error in binding thread\n");
        return CPA_STATUS_FAIL;
    }
    test_complete = 0;
    while(!test_complete){
        // Cpa8U *pDigestBuffer = (srcBufferLists[buf_idx]->pBuffers->pData) + fragmentSize;
        // pOpData->pDigestResult = pDigestBuffer;
        // status = cpaCySymPerformOp(
        //     singleCyInstHandle,
        //     NULL, /* data sent as is to the callback function*/
        //     pOpData,           /* operational data struct */
        //     srcBufferLists[buf_idx],       /* source buffer list */
        //     dstBufferLists[buf_idx],       /* same src & dst for an in-place operation*/
        //     NULL); /*Don't verify*/


        // buf_idx = (buf_idx + 1) % numFragments;

    }
    test_complete = 0;
    printf("Test complete\n");



    PHYS_CONTIG_FREE(pSrcBuffer);
    OS_FREE(pBufferList);
    PHYS_CONTIG_FREE(pBufferMeta);
    OS_FREE(pOpData);


}