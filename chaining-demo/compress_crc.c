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

#include <zlib.h>


#include "idxd.h"
#include "dsa.h"

#include "dsa_funcs.h"

#include "validate_compress_and_crc.h"

#include "accel_test.h"

#include "sw_chain_comp_crc_funcs.h"
#include "smt-thread-exps.h"

#include "tests.h"

#include <xmmintrin.h>


int gDebugParam = 1;


#include <ucontext.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>



#define NUMCONTEXTS 10              /* how many contexts to make */
#define STACKSIZE 4096

extern int swapcontext_very_fast(ucontext_t *ouctx, ucontext_t *uctx);
ucontext_t *cur_context;            /* a pointer to the current_context */
int curcontext = 0;
ucontext_t contexts[NUMCONTEXTS];   /* store our context info */

void
mkcontext(ucontext_t *uc,  void *function)
{
    void * stack;

    getcontext(uc);

    stack = malloc(STACKSIZE);
    if (stack == NULL) {
        perror("malloc");
        exit(1);
    }
    /* we need to initialize the ucontext structure, give it a stack,
        flags, and a sigmask */
    uc->uc_stack.ss_sp = stack;
    uc->uc_stack.ss_size = STACKSIZE;
    uc->uc_stack.ss_flags = 0;
    if (sigemptyset(&uc->uc_sigmask) < 0){
      perror("sigemptyset");
      exit(1);
    }

    /* setup the function we're going to, and n-1 arguments. */
    makecontext(uc, function, 1);

    printf("context is %p\n", uc);
}

void
scheduler()
{
    printf("scheduling out thread %d\n", curcontext);

    curcontext = (curcontext + 1) % NUMCONTEXTS; /* round robin */
    cur_context = &contexts[curcontext];

    printf("scheduling in thread %d\n", curcontext);

    setcontext(cur_context); /* go */
}

void thread(){
  printf("Thread created\n");
}


int main(){


  mkcontext(&contexts[0], thread);

exit:

  return 0;
}