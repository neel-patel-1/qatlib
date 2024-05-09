#ifndef DSA_FUNCS
#define DSA_FUNCS
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

#include "idxd.h"
#include "dsa.h"


CpaStatus functionalDSACrcGen(Cpa32U buf_size){
  /*setup device*/
  struct acctest_context *dsa = NULL;
  int tflags = TEST_FLAGS_BOF;
  int rc;
  int wq_type = ACCFG_WQ_SHARED;
  int dev_id = 0;
  int wq_id = 0;
  int opcode = 16;

  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;

  if (!dsa)
		return -ENOMEM;

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
	if (rc < 0)
		return -ENOMEM;

  /* Alloc and init tasks */
  struct task_node *tsk_node;
  rc = acctest_alloc_multiple_tasks(dsa, 1);
  if(rc != ACCTEST_STATUS_OK){
    return rc;
  }
  tsk_node = dsa->multi_task_node;
  while (tsk_node) {
    tsk_node->tsk->xfer_size = buf_size;

    rc = init_task(tsk_node->tsk, tflags, opcode, buf_size);
    if (rc != ACCTEST_STATUS_OK)
      return rc;

    tsk_node = tsk_node->next;
  }

  rc = dsa_crcgen_multi_task_nodes(dsa);
  if(rc != ACCTEST_STATUS_OK){
    return rc;
  }
  rc = task_result_verify_task_nodes(dsa, 0);
  if(rc != ACCTEST_STATUS_OK){
    return rc;
  }
  acctest_free(dsa);
  return rc;
}



#endif