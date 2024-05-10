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

extern const unsigned long crc32_table[8][256];


uint32_t dsa_calculate_crc32(void *data, size_t length, uint32_t seed, uint32_t flags);

CpaStatus functionalDSACrcGen(Cpa32U buf_size);

CpaStatus dsaCrcGenCompareWithSw(Cpa8U *buf, Cpa32U buf_size);

CpaStatus compareDSACRCsWithSW();


void single_crc_submit_task(struct acctest_context *dsa, struct task *tsk);

struct task *alloc_crc_task(struct acctest_context *dsa, Cpa8U *srcAddr);

/* prepare an allocated tsk for crcgen on the srcAddr */
void prepare_crc_task(
    struct task *tsk,
    struct acctest_context *dsa, Cpa8U *srcAddr, Cpa64U bufferSize
    );

/* tsks are in a linked list, we are waiting on multiple tasks in sequence, is traversing the linked list to get the next task to wait on the bottlneck?*/
/* Array of Buffer Lists contain all the data for which dsa ops will be performed, but dsa is the second step in the offload sequence and must use the dst as src */
/* Task nodes are allocated on dsa and prepped for buffer lists*/
CpaStatus create_tsk_nodes_for_stage2_offload(CpaBufferList **srcBufferLists,
  int numOperations, struct acctest_context *dsa);

CpaStatus singleSubmitValidation(CpaBufferList **srcBufferLists);
CpaStatus validateCrc32DSA(struct task *tsk, Cpa8U *buf, Cpa64U bufLen);


#endif