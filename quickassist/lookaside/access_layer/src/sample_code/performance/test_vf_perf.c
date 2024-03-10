
#include "cpa_dc.h"
#include "icp_sal_user.h"
#include <stdlib.h>
#include "cpa_sample_utils.h"

#define FAIL_ON(cond, args...) if (cond) { PRINT(args); return CPA_STATUS_FAIL; }

int main(){
    CpaStatus stat;
    stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to start user process SSL")
    stat = qaeMemInit();
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to open usdm driver")

    Cpa16U numInst = 0;
    stat = cpaDcGetNumInstances(&numInst);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to get number of instances")
    printf("Number of available dc instances: %d\n", numInst);
    CpaInstanceHandle *dcInstHandles 
        = (CpaInstanceHandle *)qaeMemAlloc(sizeof(CpaInstanceHandle) * numInst);
    FAIL_ON(dcInstHandles == NULL, "Failed to allocate memory for instance handles")
    stat = cpaDcGetInstances(numInst, dcInstHandles);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to get instances: %d\n", stat);

    numInst = 16;

    Cpa32U buffMetaSize = 0;
    CpaBufferList **bufferInterArray = NULL;
    Cpa16U numInterBuffLists = 0;
    for (Cpa16U i = 0; i < numInst; i++) {
        CpaInstanceHandle *dcInstHandle = &dcInstHandles[i];
        sampleDcGetInstance(dcInstHandle);
        CpaDcInstanceCapabilities cap = {0};
        stat = cpaDcQueryCapabilities(dcInstHandle, &cap);
        FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to query capabilities");
        stat = cpaDcBufferListGetMetaSize(dcInstHandle, 1, &buffMetaSize);
        stat = cpaDcGetNumIntermediateBuffers(dcInstHandle,
                                                    &numInterBuffLists);
        FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to get number of intermediate buffer lists");
        stat = PHYS_CONTIG_ALLOC(
                &bufferInterArray, numInterBuffLists * sizeof(CpaBufferList *));
        FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to allocate intermediate buffer list");
    }
    
    icp_sal_userStop();
    qaeMemDestroy();
}