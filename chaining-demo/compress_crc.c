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

#include <ucontext.h>



int gDebugParam = 1;
#define NUMCONTEXTS 10              /* how many contexts to make */

ucontext_t *cur_context;            /* a pointer to the current_context */
int curcontext = 0;
ucontext_t contexts[NUMCONTEXTS];   /* store our context info */
void
scheduler()
{
    printf("scheduling out thread %d\n", curcontext);

    curcontext = (curcontext + 1) % NUMCONTEXTS; /* round robin */
    cur_context = &contexts[curcontext];

    printf("scheduling in thread %d\n", curcontext);

    setcontext(cur_context); /* go */
}


int main(){




exit:

  return 0;
}