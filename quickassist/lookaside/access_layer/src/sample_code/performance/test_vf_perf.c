
#include "cpa_dc.h"
#include "icp_sal_user.h"
#include <stdlib.h>
#include "cpa_sample_utils.h"

#define FAIL_ON(cond, MSG) if (cond) { PRINT_ERR(#MSG "\n"); return CPA_STATUS_FAIL; }

int main(){
    CpaStatus stat;
    stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to start user process SSL")
    stat = qaeMemInit();
    FAIL_ON(stat != CPA_STATUS_SUCCESS, "Failed to open usdm driver")

    Cpa16U numInst = 0;
    CpaInstanceHandle *dcInstHandles 
        = (CpaInstanceHandle *)malloc(sizeof(CpaInstanceHandle));
    
    numInst = 16;


    for (Cpa16U i = 0; i < numInst; i++) {
        CpaInstanceHandle *dcInstHandle = &dcInstHandles[i];
        sampleDcGetInstance(dcInstHandle);
    }
    
    icp_sal_userStop();
    qaeMemDestroy();
}