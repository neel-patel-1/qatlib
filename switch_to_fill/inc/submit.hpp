#ifndef SUBMIT_HPP
#define SUBMIT_HPP

#include "iaa_offloads.h"

static bool iaa_submit(struct acctest_context *iaa,
  struct hw_desc *desc);

#include "inline/submit.ipp"

#endif