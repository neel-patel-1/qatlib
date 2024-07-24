#include "test_harness.h"
#include "print_utils.h"

extern "C" {
  #include "fcontext.h"
  #include "iaa.h"
  #include "accel_test.h"
}

int gLogLevel = LOG_MONITOR;
bool gDebugParam = false;
int main(){
  struct task_node *tsk_node;
  struct acctest_context *iaa;
  int tflags = TEST_FLAGS_BOF;
  int opcode = IAX_OPCODE_NOOP;
  int wq_id = 0;
  int dev_id = 3;
  int wq_type = SHARED;

  iaa = acctest_init(tflags);
}