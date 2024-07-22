#include "timer_utils.h"
#include <pthread.h>
#include "print_utils.h"
#include "status.h"
#include "emul_ax.h"
#include "thread_utils.h"
#include "offload.h"
extern "C"{
#include "fcontext.h"
}
#include <cstdlib>
#include <atomic>
#include <list>
#include <cstring>
#include <x86intrin.h>
#include <string>


#include "posting_list.h"




typedef struct _ranker_offload_args{
  node *list_head;
} ranker_offload_args;

/*
  blocking linked list merge request:
    (same as router)
    ll_simple
    ll_dynamic

*/

int gLogLevel = LOG_DEBUG;
bool gDebugParam = false;
int main(){
  node *head = build_llc_ll(10);
  ll_simple(head);

}