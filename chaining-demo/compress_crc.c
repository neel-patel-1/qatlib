#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"

#include "dc_inst_utils.h"
#include "thread_utils.h"

#include "buffer_prepare_funcs.h"
#include "hw_comp_crc_funcs.h"
#include "sw_comp_crc_funcs.h"

#include "print_funcs.h"

int gDebugParam = 1;

#include <zlib.h>


#include "idxd.h"
#include "dsa.h"


int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;
  Cpa16U numInstances = 0;
  CpaInstanceHandle dcInstHandle = NULL;
  CpaDcSessionHandle sessionHandle = NULL;
  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcSessionHandle sessionHandles[MAX_INSTANCES];
  CpaDcInstanceCapabilities cap = {0};

  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);

  allocateDcInstances(dcInstHandles, &numInstances);
  if (0 == numInstances)
  {
    fprintf(stderr, "No instances found\n");
    return CPA_STATUS_FAIL;
  }
  dcInstHandle = dcInstHandles[0];
  prepareDcInst(&dcInstHandles[0]);


  Cpa32U numOperations = 1000;
  Cpa32U bufferSize = 1024;
  CpaBufferList **srcBufferLists = NULL;
  CpaBufferList **dstBufferLists = NULL;
  CpaDcRqResults **dcResults = NULL;
  CpaCrcData *crcData = NULL;
  callback_args **cb_args = NULL;
  packet_stats **stats = NULL;
  CpaDcOpData **opData = NULL;
  struct COMPLETION_STRUCT complete;

  multiBufferTestAllocations(
    &cb_args,
    &stats,
    &opData,
    &dcResults,
    &crcData,
    numOperations,
    bufferSize,
    cap,
    &srcBufferLists,
    &dstBufferLists,
    dcInstHandle,
    &complete
  );

  /*offload*/
  struct acctest_context *dsa = NULL;
  int tflags = TEST_FLAGS_BOF;
  int rc;
  int wq_type = ACCFG_WQ_SHARED;
  int dev_id = 0;
  int wq_id = 0;

  /* sudo ..//setup_dsa.sh -d dsa0 -w1 -ms -e4 */
  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;

  if (!dsa)
		return -ENOMEM;

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
	if (rc < 0)
		return -ENOMEM;

  struct task *tsk;

  tsk = acctest_alloc_task(dsa);
  tsk->xfer_size = 1024;
  tsk->pattern = 0x0123456789abcdef;
  tsk->crc_seed = 0x12345678;
  tsk->src1 = srcBufferLists[0]->pBuffers[0].pData;
  tsk->dst1 = (void *)&(crcData[0].crc32);
  tsk->opcode = 0x10;
  tsk->dflags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
  acctest_prep_desc_common(tsk->desc, tsk->opcode, (uint64_t)(tsk->dst1),
				 (uint64_t)(tsk->src1), tsk->xfer_size, tsk->dflags);
  tsk->desc->completion_addr = (uint64_t)(tsk->comp);
	tsk->comp->status = 0;
	tsk->desc->crc_seed = tsk->crc_seed;
	tsk->desc->seed_addr = (uint64_t)tsk->crc_seed_addr;

  acctest_desc_submit(dsa, tsk->desc);
  rc = dsa_wait_crcgen(dsa, tsk);

  acctest_free(dsa);
exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}