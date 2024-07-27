#ifndef DSA_OFFLOADS_H
#define DSA_OFFLOADS_H

#include "test_harness.h"
#include "print_utils.h"
#include "router_request_args.h"


extern "C" {
  #include "fcontext.h"
  #include "dsa.h"
  #include "accel_test.h"
}

extern struct acctest_context *dsa;

void initialize_dsa_wq(int dev_id, int wq_id, int wq_type);
void free_dsa_wq();
void prepare_dsa_memcpy_desc_with_preallocated_comp(
  struct hw_desc *hw, uint64_t src,
  uint64_t dst, uint64_t comp, uint64_t xfer_size);
void prepare_dsa_memfill_desc_with_preallocated_comp(
  struct hw_desc *hw, uint64_t src,
  uint64_t dst, uint64_t comp, uint64_t xfer_size);


#endif