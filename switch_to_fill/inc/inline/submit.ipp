#include "submit.hpp"

static inline bool iaa_submit(struct acctest_context *iaa,
  struct hw_desc *desc){
  if (enqcmd(iaa->wq_reg, desc) ){
    return false;
  }
  return true;
}