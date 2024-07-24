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

}