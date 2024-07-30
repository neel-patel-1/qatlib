#include "submit.hpp"

static inline bool iaa_submit(struct acctest_context *iaa,
  struct hw_desc *desc){
  if (enqcmd(iaa->wq_reg, desc) ){
    return false;
  }
  return true;
}
static inline void blocking_iaa_submit(struct acctest_context *iaa,
  struct hw_desc *desc){
  while(enqcmd(iaa->wq_reg, desc) ){
    /* retry submit */
  }
}

static inline bool dsa_submit(struct acctest_context *dsa,
  struct hw_desc *desc){
  if (enqcmd(dsa->wq_reg, desc) ){
    return false;
  }
  return true;
}

static inline void blocking_dsa_submit(struct acctest_context *dsa,
  struct hw_desc *desc){
  while(enqcmd(dsa->wq_reg, desc) ){
    /* retry submit */
  }
}