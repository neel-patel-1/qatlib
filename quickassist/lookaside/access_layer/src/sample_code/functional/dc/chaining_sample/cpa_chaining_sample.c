#include "cpa.h"
#include "cpa_cy_im.h"
#include "cpa_cy_sym_dp.h"
#include "icp_sal_poll.h"
#include "cpa_sample_utils.h"
#include "Osal.h"



void indVsDepSpt(){
    int intensities[] = {256};
    // int intensities[] = {0, 1, 2, 4, 8, 16, 32, 64, 128, 256};
    int bufSizeShift = 7;
    for( int cI=0; cI<(sizeof(intensities)/sizeof(int)); cI++){
        startTest(
        /*ChainLength=*/3,
        /*numBuffers*/(1<<bufSizeShift),
        /*batchSize*/1, /*Minimum fwding granularity shown optimal*/
        /*bufferSize=*/ (1<<20 - bufSizeShift), /*Best size for aes and hash*/
        /*useSpt*/ CPA_FALSE,
        /* intensity = */intensities[cI],
        /*cbs are dependent*/ CPA_TRUE);
<<<<<<< Updated upstream
        startTest(
        /*ChainLength=*/3,
        /*numBuffers*/(1<<bufSizeShift),
        /*batchSize*/1, /*Minimum fwding granularity shown optimal*/
        /*bufferSize=*/ (1<<20 - bufSizeShift), /*Best size for aes and hash*/
        /*useSpt*/ CPA_FALSE,
        /* intensity = */intensities[cI],
        /*cbs are dependent*/ CPA_FALSE);
=======
        // startTest(
        // /*ChainLength=*/3,
        // /*numBuffers*/(1<<bufSizeShift),
        // /*batchSize*/1, /*Minimum fwding granularity shown optimal*/
        // /*bufferSize=*/ (1<<20 - bufSizeShift), /*Best size for aes and hash*/
        // /*useSpt*/ CPA_FALSE,
        // /* intensity = */intensities[cI],
        // /*cbs are dependent*/ CPA_FALSE);
>>>>>>> Stashed changes
    }

}
void singleCallbackProcessingThreadSPTComparison(){
    int intensities[] = {32, 64};
    // int intensities[] = {0, 1, 2, 4, 8, 16, 32, 64, 128, 256};
    for( int cI=0; cI<(sizeof(intensities)/sizeof(int)); cI++){
        startTest(
        /*ChainLength=*/3,
        /*numBuffers*/(1<<3),
        /*batchSize*/1, /*Minimum fwding granularity shown optimal*/
        /*bufferSize=*/ (1<<17), /*Best size for aes and hash*/
        /*useSpt*/ CPA_FALSE,
        /* intensity = */intensities[cI],
        /*cbs are dependent*/ CPA_TRUE);
        startTest(
        /*ChainLength=*/3,
        /*numBuffers*/(1<<3),
        /*batchSize*/1, /*Minimum fwding granularity shown optimal*/
        /*bufferSize=*/ (1<<17), /*Best size for aes and hash*/
        /*useSpt*/ CPA_TRUE,
        /* intensity = */intensities[cI],
        /*cbs are dependent*/ CPA_TRUE);
    }

}

void fwdingGranularityImpact(){
    int numBufferses[] = {1, 2, 4, 8, 16, 32};
    int bufferSizes[] = {1*1024*1024, 512* 1024, 256*1024, 128*1024, 64*1024, 32*1024};
    for(int numBuffersIdx = 0; numBuffersIdx<sizeof(numBufferses)/sizeof(int); numBuffersIdx++){
        int maxAllowedBatchSize = numBufferses[numBuffersIdx];


        for(int batchSize = 1; batchSize<=maxAllowedBatchSize; batchSize*=2){
            startTest(/*ChainLength=*/3, numBufferses[numBuffersIdx], batchSize,
                /*bufferSize=*/bufferSizes[numBuffersIdx], /*useSpt*/ CPA_FALSE,
                /* intensity = */0, /*cbs are dependent*/ CPA_TRUE);
        }
    }
}


void runExps(){
    indVsDepSpt();
}

