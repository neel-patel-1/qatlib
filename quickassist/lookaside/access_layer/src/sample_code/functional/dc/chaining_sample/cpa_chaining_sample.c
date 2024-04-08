#include "cpa.h"
#include "cpa_cy_im.h"
#include "cpa_cy_sym_dp.h"
#include "icp_sal_poll.h"
#include "cpa_sample_utils.h"
#include "Osal.h"

/*
Tests:
(1) SingleCPU-RR-Submit and poll
- params: batch size
(2) Submit and dedicated poll
- params: number of dedicated pollers, per-cb-thread spinup on/off

Which configurations are truly comparable?:
(1) a dedicated polling thread per accelerator could spin up new threads
for each callback function - we will test this
(2) a single-host request-poller could spin up new threads as well
- does the removal of cb's from the critical path
1 - which configuration can achieve the lowest latency?

*/
void runSptComparison(){

    startFullPayloadBlockBetweenEachAcceleratorSingleHost(
    /*ChainLength=*/3,
    /*numBuffers*/32,
    /*batchSize*/1, /*Minimum fwding granularity shown optimal*/
    /*bufferSize=*/ 128*1024, /*Best size for aes and hash*/
    /*useSpt*/ CPA_FALSE,
    /* intensity = */0,
    /*cbs are dependent*/ CPA_TRUE);

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
    runSptComparison();
}

