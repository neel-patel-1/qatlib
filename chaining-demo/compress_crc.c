#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"

extern CpaDcHuffType huffmanType_g;
extern CpaStatus qaeMemInit(void);
extern void qaeMemDestroy(void);

#define MAX_INSTANCES 1

int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;
  Cpa16U numInstances = 0;
  CpaInstanceHandle dcInstHandle = NULL;
  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];

  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);

  status = cpaDcGetNumInstances(&numInstances);
  if (numInstances >= MAX_INSTANCES)
  {
      numInstances = MAX_INSTANCES;
  }
  if ((status == CPA_STATUS_SUCCESS) && (numInstances > 0))
  {
      status = cpaDcGetInstances(numInstances, dcInstHandles);
      if (status == CPA_STATUS_SUCCESS)
          dcInstHandle = dcInstHandles[0];
  }
  if (0 == numInstances)
  {
    fprintf(stderr, "No instances found\n");
  }



  return 0;
}