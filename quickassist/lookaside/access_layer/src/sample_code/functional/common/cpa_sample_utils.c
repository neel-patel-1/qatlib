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

/**
 *****************************************************************************
 * @file cpa_sample_utils.c
 *
 * @ingroup sampleCode
 *
 * @description
 * Defines functions to get an instance and poll an instance
 *
 ***************************************************************************/

#include "cpa_sample_utils.h"
#include "cpa_dc.h"
#include "icp_sal_poll.h"
#include <time.h>

/*
 * Maximum number of instances to query from the API
 */
#ifdef USER_SPACE
#define MAX_INSTANCES 1024
#else
#define MAX_INSTANCES 1
#endif

#define UPPER_HALF_OF_REGISTER 32

#ifdef DO_CRYPTO
static sampleThread gPollingThread;
static volatile int gPollingCy = 0;
#endif

static sampleThread gPollingThreadDc;
static volatile int gPollingDc = 0;

volatile int batch_complete = 0;

volatile int dcRequestGen_g = 0;
int testIter = 0;

volatile Cpa16U numDcResps_g = 0;
volatile Cpa16U numEncResps_g = 0;
volatile Cpa16U numHashResps_g = 0;
volatile Cpa16U lastHashResp_idx = 0;

volatile int test_complete = 0;

Cpa16U numBufs_g = 0;
Cpa32U bufSize_g = 0;

CpaBufferList **pSrcBufferList_g = NULL;
CpaBufferList **pDstBufferList_g = NULL;

Cpa32U fragmentSize_g = 0;

struct timespec dcStartTime_g = {0};
struct timespec hashStartTime_g = {0};

Cpa16U numSamples_g = 0;

#ifdef SC_ENABLE_DYNAMIC_COMPRESSION
CpaDcHuffType huffmanType_g = CPA_DC_HT_FULL_DYNAMIC;
#else
CpaDcHuffType huffmanType_g = CPA_DC_HT_STATIC;
#endif
/* *************************************************************
 *
 * Common instance functions
 *
 * *************************************************************
 */

/*
 * This function returns a handle to an instance of the cryptographic
 * API.  It does this by querying the API for all instances and
 * returning the first such instance.
 */
//<snippet name="getInstance">
#ifdef DO_CRYPTO
void sampleCyGetInstance(CpaInstanceHandle *pCyInstHandle)
{
    CpaInstanceHandle cyInstHandles[MAX_INSTANCES];
    Cpa16U numInstances = 0;
    CpaStatus status = CPA_STATUS_SUCCESS;

    *pCyInstHandle = NULL;
    status = cpaCyGetNumInstances(&numInstances);
    if (numInstances >= MAX_INSTANCES)
    {
        numInstances = MAX_INSTANCES;
    }
    if ((status == CPA_STATUS_SUCCESS) && (numInstances > 0))
    {
        status = cpaCyGetInstances(numInstances, cyInstHandles);
        if (status == CPA_STATUS_SUCCESS)
            *pCyInstHandle = cyInstHandles[0];
    }

    if (0 == numInstances)
    {
        PRINT_ERR("No instances found for 'SSL'\n");
        PRINT_ERR("Please check your section names");
        PRINT_ERR(" in the config file.\n");
        PRINT_ERR("Also make sure to use config file version 2.\n");
    }
}

void symSessionWaitForInflightReq(CpaCySymSessionCtx pSessionCtx)
{

/* Session reuse is available since Cryptographic API version 2.2 */
#if CY_API_VERSION_AT_LEAST(2, 2)
    CpaBoolean sessionInUse = CPA_FALSE;

    do
    {
        cpaCySymSessionInUse(pSessionCtx, &sessionInUse);
    } while (sessionInUse);
#endif

    return;
}
#endif
//</snippet>

void printeHashBWAndUpdateLastHashTimeStamp(void)
{
    if(hashStartTime_g.tv_nsec == 0){
        clock_gettime(CLOCK_MONOTONIC, &hashStartTime_g);
        return;
    } else {
        struct timespec curTime;
        clock_gettime(CLOCK_MONOTONIC, &curTime);
        uint64_t ns = curTime.tv_sec * 1000000000 + curTime.tv_nsec -
            (hashStartTime_g.tv_sec * 1000000000 + hashStartTime_g.tv_nsec);
        uint64_t us = ns/1000;
        if(us == 0){
            return;
        }
        printf("Hash-BW(MB/s): %ld\n", (numHashResps_g * bufSize_g) / (us));
        clock_gettime(CLOCK_MONOTONIC, &hashStartTime_g);
    }
}

void printEncBWAndUpdateLastEncTimeStamp(void)
{
    if(dcStartTime_g.tv_nsec == 0){
        clock_gettime(CLOCK_MONOTONIC, &dcStartTime_g);
        return;
    } else {
        struct timespec curTime;
        clock_gettime(CLOCK_MONOTONIC, &curTime);
        uint64_t ns = curTime.tv_sec * 1000000000 + curTime.tv_nsec -
            (dcStartTime_g.tv_sec * 1000000000 + dcStartTime_g.tv_nsec);
        uint64_t us = ns/1000;
        if(us == 0){
            return;
        }
        printf("DC-BW(MB/s): %ld\n", (numEncResps_g * bufSize_g) / (us));
        clock_gettime(CLOCK_MONOTONIC, &dcStartTime_g);
    }
}

void printeDCBWAndUpdateLastDCTimeStamp(void)
{
    if(dcStartTime_g.tv_nsec == 0){
        clock_gettime(CLOCK_MONOTONIC, &dcStartTime_g);
        return;
    } else {
        struct timespec curTime;
        clock_gettime(CLOCK_MONOTONIC, &curTime);
        uint64_t ns = curTime.tv_sec * 1000000000 + curTime.tv_nsec -
            (dcStartTime_g.tv_sec * 1000000000 + dcStartTime_g.tv_nsec);
        uint64_t us = ns/1000;
        if(us == 0){
            return;
        }
        printf("DC-BW(MB/s): %ld\n", (numDcResps_g * bufSize_g) / (us));
        clock_gettime(CLOCK_MONOTONIC, &dcStartTime_g);
    }
}

void symCallback(void *pCallbackTag,
                        CpaStatus status,
                        const CpaCySymOp operationType,
                        void *pOpData,
                        CpaBufferList *pDstBuffer,
                        CpaBoolean verifyResult)
{
    if ((Cpa16U) (numHashResps_g + 1) < numHashResps_g ){
        printeHashBWAndUpdateLastHashTimeStamp();
    }
    numHashResps_g++;


}

/*
 * This function polls a crypto instance.
 *
 */

static void dcCallback(void *pCallbackTag, CpaStatus status)
{
    if ((Cpa16U) (numDcResps_g + 1) < (Cpa16U)numDcResps_g){
        printeDCBWAndUpdateLastDCTimeStamp();

    }
    if(pCallbackTag != NULL){
        batch_complete = 1;
    }
    numDcResps_g++;

}


static Cpa8U sampleCipherKey[] = {
    0xEE, 0xE2, 0x7B, 0x5B, 0x10, 0xFD, 0xD2, 0x58, 0x49, 0x77, 0xF1, 0x22,
    0xD7, 0x1B, 0xA4, 0xCA, 0xEC, 0xBD, 0x15, 0xE2, 0x52, 0x6A, 0x21, 0x0B,
    0x41, 0x4C, 0x41, 0x4E, 0xA1, 0xAA, 0x01, 0x3F};

static void encCallback(void *pCallbackTag, CpaStatus status)
{
    if ((Cpa16U) (numEncResps_g + 1) < (Cpa16U)numEncResps_g){
        printEncBWAndUpdateLastEncTimeStamp();
        testIter++;

    }
    if(pCallbackTag != NULL){
        batch_complete = 1;
    }
    numEncResps_g++;
    if(testIter > numSamples_g){
        dcRequestGen_g = 0; /* Test complete stop initial requestor */
    }

}
static void comp_enc_fwder(CpaInstanceHandle dcInstHandle){
    CpaStatus status = CPA_STATUS_FAIL;
    CpaCySymSessionCtx sessionCtx = NULL;
    Cpa32U sessionCtxSize = 0;
    CpaInstanceHandle cyInstHandle = NULL;
    CpaCySymSessionSetupData sessionSetupData = {0};
    CpaCySymStats64 symStats = {0};

    sampleCyGetInstance(&cyInstHandle);
    if (cyInstHandle == NULL)
    {
        return CPA_STATUS_FAIL;
    }

    /* Start Cryptographic component */

    status = cpaCyStartInstance(cyInstHandle);

    if (CPA_STATUS_SUCCESS == status)
    {
        /*
         * Set the address translation function for the instance
         */
        status = cpaCySetAddressTranslation(cyInstHandle, sampleVirtToPhys);
    }
    if (CPA_STATUS_SUCCESS == status)
    {
        /*
         * If the instance is polled start the polling thread. Note that
         * how the polling is done is implementation-dependent.
         */
        sampleDcStartPolling(cyInstHandle);

        /* populate symmetric session data structure */
        sessionSetupData.sessionPriority = CPA_CY_PRIORITY_NORMAL;
        //<snippet name="initSession">
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

        sessionSetupData.hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_SHA512;
        sessionSetupData.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_AUTH;
        #define LAC_HASH_SHA512_DIGEST_SIZE 64
        sessionSetupData.hashSetupData.digestResultLenInBytes = LAC_HASH_SHA512_DIGEST_SIZE;
        sessionSetupData.hashSetupData.authModeSetupData.authKey =
            sampleCipherKey;
        sessionSetupData.hashSetupData.authModeSetupData.authKeyLenInBytes =
            sizeof(sampleCipherKey);

        /* The resulting MAC is to be placed immediately after the ciphertext */
        sessionSetupData.digestIsAppended = CPA_TRUE;
        sessionSetupData.verifyDigest = CPA_FALSE;
        //</snippet>

        /* Determine size of session context to allocate */

        status = cpaCySymSessionCtxGetSize(
            cyInstHandle, &sessionSetupData, &sessionCtxSize);


    }
    if (CPA_STATUS_SUCCESS == status)
    {
        /* Allocate session context */
        status = PHYS_CONTIG_ALLOC(&sessionCtx, sessionCtxSize);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        /* Initialize the session */

        status = cpaCySymInitSession(
            cyInstHandle, encCallback, &sessionSetupData, sessionCtx);
    }
    int cur=0;
    while(dcRequestGen_g){ /* While the requestor thread is running */
        if (icp_sal_DcPollInstance(dcInstHandle, 0) == CPA_STATUS_SUCCESS){
            printf("Got compressed, fwding to enc\n");
            int bufIdx = cur % numBufs_g;
            if(__builtin_expect(!!(cur > numDcResps_g),0)){
                while (cur < UINT16_MAX){
                    status = cpaCySymPerformOp(
                        cyInstHandle,
                        sessionCtx,
                        pSrcBufferList_g[bufIdx],     /* source buffer list */
                        pDstBufferList_g[bufIdx],     /* destination buffer list */
                        NULL,            /* Operational data */
                        NULL);         /* results structure */
                    cur=(cur + 1);
                }
            }
            while( cur < numDcResps_g ){
                status = cpaCySymPerformOp(
                    cyInstHandle,
                    sessionCtx,
                    pSrcBufferList_g[bufIdx],     /* source buffer list */
                    pDstBufferList_g[bufIdx],     /* destination buffer list */
                    NULL,            /* Operational data */
                    NULL);         /* results structure */
                cur= (cur + 1);
            }
            printEncBWAndUpdateLastEncTimeStamp();
        }
    }
}

#ifdef DO_CRYPTO
static void sal_polling(CpaInstanceHandle cyInstHandle)
{
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
    CpaStatus status;
    sampleDcGetInstance(&dcInstHandle);


    status = cpaDcQueryCapabilities(dcInstHandle, &cap);

    status = cpaDcBufferListGetMetaSize(dcInstHandle, 1, &buffMetaSize);
    // printf("Buffer Meta Size: %d\n", buffMetaSize);
    status = cpaDcGetNumIntermediateBuffers(dcInstHandle,
                                                    &numInterBuffLists);
    // printf("Num Intermediate Buffers: %d\n", numInterBuffLists);

    if (numInterBuffLists > 0){
        status = PHYS_CONTIG_ALLOC(
                &bufferInterArray, numInterBuffLists * sizeof(CpaBufferList *));
        for (bufferNum = 0; bufferNum < numInterBuffLists; bufferNum++)
        {
            status = PHYS_CONTIG_ALLOC(&bufferInterArray[bufferNum],
                                            sizeof(CpaBufferList));
            status = PHYS_CONTIG_ALLOC(
                        &bufferInterArray[bufferNum]->pPrivateMetaData,
                        buffMetaSize);
            status =
                        PHYS_CONTIG_ALLOC(&bufferInterArray[bufferNum]->pBuffers,
                                        sizeof(CpaFlatBuffer));
            status = PHYS_CONTIG_ALLOC(
                        &bufferInterArray[bufferNum]->pBuffers->pData,
                        2 * fragmentSize_g);
            bufferInterArray[bufferNum]->numBuffers = 1;
                    bufferInterArray[bufferNum]->pBuffers->dataLenInBytes =
                        2 * fragmentSize_g;
        }
    }
    status = cpaDcStartInstance(
        dcInstHandle, numInterBuffLists, bufferInterArray);
    sd.compLevel = CPA_DC_L1;
    sd.compType = CPA_DC_DEFLATE;
    sd.huffType = CPA_DC_HT_STATIC;
    sd.autoSelectBestHuffmanTree = CPA_DC_ASB_DISABLED;
    sd.sessDirection = CPA_DC_DIR_COMPRESS;
    sd.sessState = CPA_DC_STATELESS;
    sd.checksum = CPA_DC_CRC32;
    if(status != CPA_STATUS_SUCCESS){
        printf("Failed to start DC Instance\n");
    }

    status = cpaDcGetSessionSize(dcInstHandle, &sd, &sess_size, &ctx_size);
    status = PHYS_CONTIG_ALLOC(&sessionHdl, sess_size);

    status = cpaDcInitSession(
        dcInstHandle,
        sessionHdl, /* session memory */
        &sd,        /* session setup data */
        NULL, /* pContexBuffer not required for stateless operations */
        dcCallback); /* callback function */
    if(status != CPA_STATUS_SUCCESS){
        printf("Failed to start DC Session: %d\n"  , status);
    }
    sampleDcStartPolling(dcInstHandle);

    gPollingCy = 1;
    CpaDcRqResults dcResults;
    CpaDcOpData opData = {};
    INIT_OPDATA(&opData, CPA_DC_FLUSH_FINAL);
    clock_gettime(CLOCK_MONOTONIC, &hashStartTime_g);
    clock_gettime(CLOCK_MONOTONIC, &dcStartTime_g);
    // while (gPollingCy)
    // {
        int cur=0;
    //     if (icp_sal_CyPollInstance(cyInstHandle, 0) == CPA_STATUS_SUCCESS){
            /* what if all numBufs_g got callback'd? L

            while( cur < numHashResps_g)   but use cur % numBufs_g  for bufs approach
                - need to check if numHashResps_g overflow'd (cur > numHashReps_g):
                if(unlikely(cur > numHashResps_g)){
                    while (cur < MAX_CPA16U){
                        comp(cur%num_bufs)
                    }
                }
                while( cur < numHashResps_g)
                    comp(cur%num_bufs)
            */

// #define unlikely(x) __builtin_expect(!!(x), 0)
//            if(unlikely(cur > numHashResps_g)){

            //     while (cur < UINT16_MAX){
            //         status = cpaDcCompressData2(
            //             dcInstHandle,
            //             sessionHdl,
            //             pSrcBufferList_g[cur%numBufs_g],     /* source buffer list */
            //             pDstBufferList_g[cur%numBufs_g],     /* destination buffer list */
            //             &opData,            /* Operational data */
            //             &dcResults,         /* results structure */
            //             NULL);
            //         cur= (cur + 1);
            //     }
            // }
            dcRequestGen_g = 1;
            while(dcRequestGen_g){
                batch_complete = 0;
                while( cur < numBufs_g - 1){
                    status = cpaDcCompressData2(
                        dcInstHandle,
                        sessionHdl,
                        pSrcBufferList_g[cur],     /* source buffer list */
                        pDstBufferList_g[cur],     /* destination buffer list */
                        &opData,            /* Operational data */
                        &dcResults,         /* results structure */
                        NULL);
                    cur= (cur + 1);
                }
                status = cpaDcCompressData2(
                        dcInstHandle,
                        sessionHdl,
                        pSrcBufferList_g[cur],     /* source buffer list */
                        pDstBufferList_g[cur],     /* destination buffer list */
                        &opData,            /* Operational data */
                        &dcResults,         /* results structure */
                        (void *)1);
                while(!batch_complete){ }
                cur = 0;
            }
            test_complete = 1;
            testIter = 0;
            // printf("Caught up to responses\n");
        // }
    // }

    sampleThreadExit();
}
#endif
/*
 * This function checks the instance info. If the instance is
 * required to be polled then it starts a polling thread.
 */

CpaStatus utilCodeThreadBind(sampleThread *thread, Cpa32U logicalCore)
{
    int status = 1;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(logicalCore, &cpuset);

    status = pthread_setaffinity_np(*thread, sizeof(cpu_set_t), &cpuset);
    if (status != 0)
    {
        return CPA_STATUS_FAIL;
    }
    return CPA_STATUS_SUCCESS;
}

#ifdef DO_CRYPTO
void sampleCyStartPolling(CpaInstanceHandle cyInstHandle)
{
    CpaInstanceInfo2 info2 = {0};
    CpaStatus status = CPA_STATUS_SUCCESS;

    status = cpaCyInstanceGetInfo2(cyInstHandle, &info2);
    if ((status == CPA_STATUS_SUCCESS) && (info2.isPolled == CPA_TRUE))
    {
        /* Start thread to poll instance */
        sampleThreadCreate(&gPollingThread, sal_polling, cyInstHandle);
        printf("Affinitizing cy thread: %ld\n", gPollingThread);
        printf("to core: %d\n", 3);
        utilCodeThreadBind(&gPollingThread, 3);
    }
}
#endif
/*
 * This function stops the polling of a crypto instance.
 */
#ifdef DO_CRYPTO
void sampleCyStopPolling(void)
{
    gPollingCy = 0;
    OS_SLEEP(10);
}
#endif

/*
 * This function returns a handle to an instance of the data
 * compression API.  It does this by querying the API for all
 * instances and returning the first such instance.
 */
//<snippet name="getInstanceDc">
void sampleDcGetInstance(CpaInstanceHandle *pDcInstHandle)
{
    CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
    Cpa16U numInstances = 0;
    CpaStatus status = CPA_STATUS_SUCCESS;

    *pDcInstHandle = NULL;
    status = cpaDcGetNumInstances(&numInstances);
    if (numInstances >= MAX_INSTANCES)
    {
        numInstances = MAX_INSTANCES;
    }
    if ((status == CPA_STATUS_SUCCESS) && (numInstances > 0))
    {
        status = cpaDcGetInstances(numInstances, dcInstHandles);
        if (status == CPA_STATUS_SUCCESS)
            *pDcInstHandle = dcInstHandles[0];
    }

    if (0 == numInstances)
    {
        PRINT_ERR("No instances found for 'SSL'\n");
        PRINT_ERR("Please check your section names");
        PRINT_ERR(" in the config file.\n");
        PRINT_ERR("Also make sure to use config file version 2.\n");
    }
}
//</snippet>

/*
 * This function polls a compression instance.
 *
 */
static void sal_dc_polling(CpaInstanceHandle dcInstHandle)
{

    gPollingDc = 1;
    while (gPollingDc)
    {
        if(icp_sal_DcPollInstance(dcInstHandle, 0) == CPA_STATUS_SUCCESS){
        }
    }

    sampleThreadExit();
}


/*
 * This function checks the instance info. If the instance is
 * required to be polled then it starts a polling thread.
 */
void sampleDcStartPolling(CpaInstanceHandle dcInstHandle)
{
    CpaInstanceInfo2 info2 = {0};
    CpaStatus status = CPA_STATUS_SUCCESS;

    status = cpaDcInstanceGetInfo2(dcInstHandle, &info2);
    if ((status == CPA_STATUS_SUCCESS) && (info2.isPolled == CPA_TRUE))
    {
        /* Start thread to poll instance */
        printf("Starting DC Polling\n");
        sampleThreadCreate(&gPollingThreadDc, comp_enc_fwder, dcInstHandle);
        printf("Affinitizing dc thread: %ld\n", gPollingThreadDc);
        printf("to core: %d\n", 2);
        utilCodeThreadBind(&gPollingThreadDc, 2);
    }
}

/*
 * This function stops the thread polling the compression instance.
 */
void sampleDcStopPolling(void)
{
    gPollingDc = 0;
    OS_SLEEP(10);
}

/*
 * This function reads the value of Time Stamp Counter (TSC) and
 * returns a 64-bit value.
 */
Cpa64U sampleCoderdtsc(void)
{
    volatile unsigned long a, d;

    asm volatile("rdtsc" : "=a"(a), "=d"(d));
    return (((Cpa64U)a) | (((Cpa64U)d) << UPPER_HALF_OF_REGISTER));
}

/*
 * This function prints out a hexadecimal representation of bytes.
 */
void hexLog(Cpa8U *pData, Cpa32U numBytes, const char *caption)
{
    int i = 0;

    if (NULL == pData)
    {
        return;
    }

    if (caption != NULL)
    {
        PRINT("\n=== %s ===\n", caption);
    }

    for (i = 0; i < numBytes; i++)
    {
        PRINT("%02X ", pData[i]);

        if (!((i + 1) % 12))
            PRINT("\n");
    }
    PRINT("\n");
}


CpaPhysicalAddr virtAddrToDevAddr(void *pVirtAddr,
                                  CpaInstanceHandle instanceHandle,
                                  CpaAccelerationServiceType type)
{
    CpaStatus status;
    CpaInstanceInfo2 instanceInfo = { 0 };

    /* Get the address translation mode */
    switch (type)
    {
#ifdef DO_CRYPTO
        case CPA_ACC_SVC_TYPE_CRYPTO:
            status = cpaCyInstanceGetInfo2(instanceHandle, &instanceInfo);
            break;
#endif
        case CPA_ACC_SVC_TYPE_DATA_COMPRESSION:
            status = cpaDcInstanceGetInfo2(instanceHandle, &instanceInfo);
            break;
        default:
            status = CPA_STATUS_UNSUPPORTED;
    }

    if (CPA_STATUS_SUCCESS != status)
    {
        return (CpaPhysicalAddr)(uintptr_t)NULL;
    }

    if (instanceInfo.requiresPhysicallyContiguousMemory)
    {
        return sampleVirtToPhys(pVirtAddr);
    }
    else
    {
        return (CpaPhysicalAddr)(uintptr_t)pVirtAddr;
    }
}