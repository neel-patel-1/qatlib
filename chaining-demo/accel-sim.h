#ifndef ACCEL_SIM_H
#define ACCEL_SIM_H

#include "idxd.h"
#include "dsa.h"

#include "dsa_funcs.h"


extern struct acctest_context *dsa;

/* Use this to produce buffers in the LLC for Access Overhead*/
void dsa_memcpy(
  void *input,
  int input_size,
  void *output,
  int output_size
  )
{

    struct task *tsk =
      acctest_alloc_task(dsa);
    /* use the task id to map to the correct comp */
    prepare_memcpy_task_flags(tsk,
      dsa,
      (uint8_t *)input,
      input_size,
      (uint8_t *)output,
      IDXD_OP_FLAG_BOF | IDXD_OP_FLAG_CC);
    if(enqcmd(dsa->wq_reg, tsk->desc)){
      PRINT_ERR("Failed to enqueue task\n");
      exit(-1);
    }
    while(tsk->comp->status == 0){
      _mm_pause();
    }

    if(tsk->comp->status != DSA_COMP_SUCCESS){
      PRINT_ERR("Task failed: 0x%x\n", tsk->comp->status);
    }

}



#endif