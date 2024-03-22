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

Cpa16U numDcResps_g = 0;

Cpa16U nResps_g = 0;
Cpa16U numBufs_g = 0;
Cpa32U bufSize_g = 0;

CpaBufferList **pSrcBufferList_g = NULL;
CpaBufferList **pDstBufferList_g = NULL;

Cpa32U fragmentSize_g = 0;

struct timespec startTime_g = {0};

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

/*
 * This function polls a crypto instance.
 *
 */

static void dcCallback(void *pCallbackTag, CpaStatus status)
{
    if ((Cpa16U) (numDcResps_g + 1) < (Cpa16U)numDcResps_g){
        if(startTime_g.tv_sec > 0){
            struct timespec curTime;
            clock_gettime(CLOCK_MONOTONIC, &curTime);
            uint64_t ns = curTime.tv_sec * 1000000000 + curTime.tv_nsec -
                (startTime_g.tv_sec * 1000000000 + startTime_g.tv_nsec);
            printf("BW(MB/s): %ld\n", (numDcResps_g * bufSize_g) / (ns/1000));
        }
        clock_gettime(CLOCK_MONOTONIC, &startTime_g);

    }
    numDcResps_g++;
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
    printf("Buffer Meta Size: %d\n", buffMetaSize);
    status = cpaDcGetNumIntermediateBuffers(dcInstHandle,
                                                    &numInterBuffLists);
    printf("Num Intermediate Buffers: %d\n", numInterBuffLists);

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
    int jRq = 0;
    CpaDcRqResults dcResults;
    CpaDcOpData opData = {};
    INIT_OPDATA(&opData, CPA_DC_FLUSH_FINAL);
    while (gPollingCy)
    {
        if (icp_sal_CyPollInstance(cyInstHandle, 0) == CPA_STATUS_SUCCESS){
            // printf("Got Cy Response\n");
            for(int i=0; i<nResps_g; i++){
                status = cpaDcCompressData2(
                    dcInstHandle,
                    sessionHdl,
                    pSrcBufferList_g[(jRq+i)%numBufs_g],     /* source buffer list */
                    pDstBufferList_g[(jRq+i)%numBufs_g],     /* destination buffer list */
                    &opData,            /* Operational data */
                    &dcResults,         /* results structure */
                    NULL);
            }
            jRq = (jRq + nResps_g) % numBufs_g;
            fflush(NULL);
            // printf("Forwarded %d requests\n", nResps_g);
        }
        nResps_g = 0;
    }

    sampleThreadExit();
}
#endif
/*
 * This function checks the instance info. If the instance is
 * required to be polled then it starts a polling thread.
 */

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
            // printf("Num Dc Resps: %d\n", numDcResps_g);
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
        sampleThreadCreate(&gPollingThreadDc, sal_dc_polling, dcInstHandle);
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