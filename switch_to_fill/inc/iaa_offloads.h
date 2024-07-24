#ifndef IAA_OFFLOADS_H
#define IAA_OFFLOADS_H

#include "test_harness.h"
#include "print_utils.h"
#include "router_request_args.h"


extern "C" {
  #include "fcontext.h"
  #include "iaa.h"
  #include "accel_test.h"
  #include "iaa_compress.h"
}

extern struct acctest_context *iaa;

void initialize_iaa_wq(int dev_id,
  int wq_id, int wq_type);


#endif