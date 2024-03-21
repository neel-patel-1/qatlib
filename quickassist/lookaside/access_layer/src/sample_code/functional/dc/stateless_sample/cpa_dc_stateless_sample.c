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
 * This is sample code that demonstrates usage of the data compression API,
 * and specifically using this API to statelessly compress an input buffer. It
 * will compress the data using deflate with dynamic huffman trees.
 */

#include "cpa.h"
#include "cpa_dc.h"

#include "cpa_sample_utils.h"

#include <stdbool.h>

extern int gDebugParam;

#define INIT_OPDATA(x, flag)                                                   \
    do                                                                         \
    {                                                                          \
        (x)->flushFlag = (flag);                                               \
        SET_CNV(x, getCnVFlag());                                              \
        SET_CNV_RECOVERY(x, getCnVnRFlag());                                   \
        (x)->inputSkipData.skipMode = CPA_DC_SKIP_DISABLED;                    \
        (x)->outputSkipData.skipMode = CPA_DC_SKIP_DISABLED;                   \
    } while (0)

static void dcCallback(void *pCallbackTag, CpaStatus status)
{
    PRINT_DBG("Ax0 Callback\n", status);
    // int index = *(int *)pCallbackTag;
    // g_ts[index] = sampleCoderdtsc();

    if (NULL != pCallbackTag)
    {
        /* indicate that the function has been called */
        COMPLETE((struct COMPLETION_STRUCT *)pCallbackTag);
    }
}

static void ax1Callback(void *pCallbackTag, CpaStatus status)
{
    PRINT_DBG("Ax0 Callback\n", status);
    // int index = *(int *)pCallbackTag;
    // g_ts[index] = sampleCoderdtsc();

    if (NULL != pCallbackTag)
    {
        /* indicate that the function has been called */
        COMPLETE((struct COMPLETION_STRUCT *)pCallbackTag);
    }
}

//</snippet>



/*
@param: sessionHdl - Session handle to populate
@param: dcInstHandle - Instance handle to populate
@param: sd - Session setup data
@param: bufferSize - size of the buffers the session will be using
@param: do_sync - whether to use synchronous or asynchronous operations
*/
CpaStatus startupDcSession(CpaDcSessionHandle sessionHdl,
                           CpaInstanceHandle dcInstHandle,
                           CpaDcSessionSetupData sd,
                           Cpa32U bufferSize,
                           bool do_sync)
{
    CpaDcInstanceCapabilities cap = {0};
    CpaBufferList **bufferInterArray = NULL;
    Cpa32U buffMetaSize = 0;
    Cpa16U numInterBuffLists = 0;
    Cpa16U bufferNum = 0;
    Cpa32U sess_size = 0;
    Cpa32U ctx_size = 0;

    CpaStatus status = CPA_STATUS_SUCCESS;

    sampleDcGetInstance(&dcInstHandle);

    cpaDcQueryCapabilities(dcInstHandle, &cap);
    status = cpaDcSetAddressTranslation(dcInstHandle, sampleVirtToPhys);

    status = cpaDcBufferListGetMetaSize(dcInstHandle, 1, &buffMetaSize);
    status = cpaDcGetNumIntermediateBuffers(dcInstHandle,
                                                    &numInterBuffLists);
    printf("NoBuffer\n");

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
                        2 * bufferSize);
            bufferInterArray[bufferNum]->numBuffers = 1;
                    bufferInterArray[bufferNum]->pBuffers->dataLenInBytes =
                        2 * bufferSize;
        }
    }
    status = cpaDcStartInstance(
        dcInstHandle, numInterBuffLists, bufferInterArray);

    sampleDcStartPolling(dcInstHandle);

    status = cpaDcGetSessionSize(dcInstHandle, &sd, &sess_size, &ctx_size);
    status = PHYS_CONTIG_ALLOC(&sessionHdl, sess_size);
    if(do_sync){
        status = cpaDcInitSession(
            dcInstHandle,
            sessionHdl, /* session memory */
            &sd,        /* session setup data */
            NULL, /* pContexBuffer not required for stateless operations */
            NULL); /* callback function */
    } else {
        status = cpaDcInitSession(
            dcInstHandle,
            sessionHdl, /* session memory */
            &sd,        /* session setup data */
            NULL, /* pContexBuffer not required for stateless operations */
            dcCallback); /* callback function */
    }

    if(status != CPA_STATUS_SUCCESS){
        PRINT_ERR("Failed to initialize session\n");
        return status;
    } else{
        PRINT_DBG("Session initialized\n");
    }
    return CPA_STATUS_SUCCESS;
}

CpaStatus startTest(void){
    CpaStatus status;
    CpaDcSessionHandle dcSessionHandle = NULL;
    CpaInstanceHandle dcInstanceHandle = NULL;
    CpaDcSessionSetupData sd = {0};

    Cpa32U bufSize = 4096;

    sd.compLevel = CPA_DC_L1;
    sd.compType = CPA_DC_DEFLATE;
    sd.huffType = CPA_DC_HT_STATIC;
    sd.autoSelectBestHuffmanTree = CPA_DC_ASB_DISABLED;
    sd.sessDirection = CPA_DC_DIR_COMPRESS;
    sd.sessState = CPA_DC_STATELESS;
    sd.checksum = CPA_DC_CRC32;

    status = startupDcSession(dcSessionHandle,
        dcSessionHandle,
        sd,
        bufSize,
        false);


    CpaDcOpData opData = {};
    INIT_OPDATA(&opData, CPA_DC_FLUSH_FINAL);

    CpaDcRqResults dcResults = {};

    Cpa32U numBuffers = 1;
    Cpa32U bufferSize = bufSize;
    Cpa32U bufferListMemSize =
            sizeof(CpaBufferList) + (numBuffers * sizeof(CpaFlatBuffer));

    CpaBufferList *pBufferListSrc = NULL;
    status = OS_MALLOC(&pBufferListSrc, bufferListMemSize);
    Cpa8U *pSrcBuffer = NULL;
    status = PHYS_CONTIG_ALLOC(&pSrcBuffer, bufferSize);
    pBufferListSrc->pBuffers = pSrcBuffer;
    pBufferListSrc->numBuffers = 1;
    Cpa8U *pBufferMetaSrc = NULL;
    Cpa32U bufferMetaSize = 0;
    status =
        cpaDcBufferListGetMetaSize(dcInstanceHandle, numBuffers, &bufferMetaSize);
    status = PHYS_CONTIG_ALLOC(&pBufferMetaSrc, bufferMetaSize);
    pBufferListSrc->pPrivateMetaData = pBufferMetaSrc;

    CpaBufferList *pBufferListDst = NULL;
    status = OS_MALLOC(&pBufferListDst, bufferListMemSize);
    Cpa8U *pDstBuffer = NULL;
    status = PHYS_CONTIG_ALLOC(&pDstBuffer, bufferSize);
    pBufferListDst->pBuffers = pDstBuffer;
    pBufferListDst->numBuffers = 1;
    Cpa8U *pBufferMetaDst = NULL;
    status =
        cpaDcBufferListGetMetaSize(dcInstanceHandle, numBuffers, &bufferMetaSize);
    status = PHYS_CONTIG_ALLOC(&pBufferMetaDst, bufferMetaSize);
    pBufferListDst->pPrivateMetaData = pBufferMetaDst;


    status = OS_MALLOC(&pBufferListDst, bufferListMemSize);
    struct COMPLETION_STRUCT complete;
    COMPLETION_INIT(&complete);
    void *pCallbackTagPh2 = (void *)&complete;
    /* Send Requests to Instance */
    // while(1){
        PRINT_DBG("Sending request\n");
        status = cpaDcCompressData2(
            dcInstanceHandle,
            dcSessionHandle,
            pBufferListSrc,     /* source buffer list */
            pBufferListDst,     /* destination buffer list */
            &opData,            /* Operational data */
            &dcResults,         /* results structure */
            &pCallbackTagPh2);
    // }
    COMPLETION_WAIT(&complete, 500);
}