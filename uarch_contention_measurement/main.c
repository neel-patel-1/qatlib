#include "idxd.h"
#include "thread_utils.h"
#include "print_utils.h"
#include "timer_utils.h"
#include "fcontext.h"
#include "linked_list.h"

#include <stdbool.h>
#include <x86intrin.h>

bool gDebugParam = 0;

typedef struct completion_record ax_comp;
typedef struct _desc{
  int32_t id;
  ax_comp *comp;
  uint8_t rsvd[52];
} desc;
typedef struct _ax_setup_args{
  uint64_t offload_time;
  bool *running;
  desc **pp_desc; /*pointer to the single desc submission allowed - switch to NULL once ax accepted */
} ax_setup_args;
typedef struct _offload_request_args{
  desc **pp_desc;
  ax_comp *comp;
  int32_t id;
  linked_list *ll;
} offload_request_args;
typedef struct _filler_request_args{
  ax_comp *comp;
} filler_request_args;
_Atomic bool offload_pending_acceptance = false;

void blocking_emul_ax(void *arg){
  ax_setup_args *args = (ax_setup_args *)arg;
  bool *running = args->running;
  bool offload_in_flight = false, offload_pending = false;
  uint64_t start_time = 0;
  uint64_t offload_time = args->offload_time;
  desc **pp_desc = args->pp_desc;

  desc *off_desc = NULL;
  ax_comp *comp = NULL;
  int32_t offloader_id = -1;

  while(*running){
    if(offload_in_flight){
      uint64_t wait_until = start_time + offload_time;
      uint64_t cur;

      cur = sampleCoderdtsc();
      while( cur < wait_until){ cur = sampleCoderdtsc(); }
      PRINT_DBG("Request id: %d completed in %ld\n",
        offloader_id, cur - start_time);
      comp->status = 1;
      offload_in_flight = false;
    }
    if(offload_pending_acceptance){
      start_time = sampleCoderdtsc();

      off_desc = *pp_desc;
      offloader_id = off_desc->id;
      comp = off_desc->comp;

      PRINT_DBG("Ax accepted id: %d desc 0x%p at portal 0x%p\n",
        offloader_id, (void *)*pp_desc, (void **)pp_desc);

      offload_in_flight = true;
      offload_pending_acceptance = false;
    }
  }
  PRINT_DBG("AX thread exiting\n");
}

void nothing_kernel(ax_comp *comp, fcontext_t parent){
  while(1){
    if(comp->status == 1){
      PRINT_DBG("Filler preempted\n");
      fcontext_swap(parent, NULL);
    }
  }
}

void pollution_kernel(ax_comp *comp, fcontext_t parent){
  volatile int glb = 0;
  int buf_size = 48 * 1024 / sizeof(int32_t);
  int32_t buffer[buf_size];
  while(1){
    for(int i=0; i<buf_size / sizeof(int32_t); i++){
      if(i > 0)
        buffer[i] = buffer[i-1] + 1;
      /* check signal */
      if(comp->status == 1){
        PRINT_DBG("Filler preempted\n");
        fcontext_swap(parent, NULL);
      }
    }
    glb = buffer[buf_size - 1];
  }
}

void same_kernel(ax_comp *comp, fcontext_t parent, linked_list *ll){
  while(1){
    if(comp->status == 1){
      PRINT_DBG("Filler preempted\n");
      fcontext_swap(parent, NULL);
    }
    node *cur_node = ll->head;
    while(cur_node != NULL){
      cur_node = cur_node->next;
      if(comp->status == 1){
        PRINT_DBG("Filler preempted\n");
        fcontext_swap(parent, NULL);
      }
    }
  }
}


void blocking_offload_request(fcontext_transfer_t arg){
  offload_request_args *args = (offload_request_args *)arg.data;
  desc **pp_desc = args->pp_desc;
  fcontext_t parent = arg.prev_context;

  ax_comp *comp = args->comp;
  desc off_desc = {.comp = comp, .id = args->id};

  linked_list *ll = args->ll;

  comp->status = 0;
  *pp_desc = &off_desc;
  offload_pending_acceptance = true;
  PRINT_DBG("Request id: %d submitting desc 0x%p to portal 0x%p\n", off_desc.id, (void *)*pp_desc,(void **) pp_desc);
  while(offload_pending_acceptance){ _mm_pause(); }
  while(comp->status == 0){ _mm_pause(); }

  /* Execute post offload kernel */
  /* post-process a linked list */
  ll_print(ll);
  node *cur_node = ll->head;
  while(cur_node != NULL){
    cur_node = cur_node->next;
  }

  PRINT_DBG("Request id: %d completed\n", off_desc.id);
  fcontext_swap(parent, NULL);
}


void offload_request(fcontext_transfer_t arg){
  offload_request_args *args = (offload_request_args *)arg.data;
  desc **pp_desc = args->pp_desc;
  fcontext_t parent = arg.prev_context;
  fcontext_transfer_t parent_xfer;

  ax_comp *comp = args->comp;
  desc off_desc = {.comp = comp, .id = args->id};

  linked_list *ll = args->ll;

  comp->status = 0;
  *pp_desc = &off_desc;
  offload_pending_acceptance = true;
  PRINT_DBG("Request id: %d submitting desc 0x%p to portal 0x%p\n", off_desc.id, (void *)*pp_desc,(void **) pp_desc);
  while(offload_pending_acceptance){ _mm_pause(); }

  parent_xfer = fcontext_swap(parent, NULL);

  /* Execute post offload kernel */
  /* post-process a linked list */
  ll_print(ll);
  node *cur_node = ll->head;
  while(cur_node != NULL){
    cur_node = cur_node->next;
  }

  PRINT_DBG("Request id: %d completed\n", off_desc.id);
  fcontext_swap(parent_xfer.prev_context, NULL);
}

void pollution_filler_request(fcontext_transfer_t arg){
  filler_request_args *args = (filler_request_args *)arg.data;
  ax_comp *comp = args->comp;

  /* execute probed kernel */
  pollution_kernel(comp, arg.prev_context);
}

void nothing_filler_request(fcontext_transfer_t arg){
  filler_request_args *args = (filler_request_args *)arg.data;
  ax_comp *comp = args->comp;

  /* execute probed kernel */
  nothing_kernel(comp, arg.prev_context);
}


void polluted_execution(int num_requests){
  desc *sub_desc = NULL; /* this is implicitly the portal, any assignments notify the accelerator*/
  ax_comp on_comp;

  fcontext_state_t *self = fcontext_create_proxy();
  fcontext_transfer_t offload_req_xfer;
  fcontext_state_t *off_req_state;
  fcontext_transfer_t filler_req_xfer;
  fcontext_state_t *filler_req_state;

  offload_request_args off_args;
  filler_request_args filler_args;

  int linked_list_size = 10;

  linked_list *ll = ll_init();

  bool running_signal = true;

  pthread_t ax_td;
  ax_setup_args ax_args;

  ax_args.offload_time = 2100;
  ax_args.running = &running_signal;
  ax_args.pp_desc = &sub_desc;

  create_thread_pinned(&ax_td, blocking_emul_ax, &ax_args, 20);

  populate_linked_list_ascending_values(ll, linked_list_size);

  for(int i=0; i<num_requests; i++){

    off_args.pp_desc = &sub_desc;
    off_args.comp = &on_comp;
    off_args.id = i;
    off_args.ll = ll;

    filler_args.comp = &on_comp;

    off_req_state = fcontext_create(offload_request); // start offload req
    offload_req_xfer = fcontext_swap(off_req_state->context, &off_args);

    filler_req_state = fcontext_create(nothing_filler_request); // start filler req
    filler_req_xfer = fcontext_swap(filler_req_state->context, &filler_args);

    offload_req_xfer = fcontext_swap(offload_req_xfer.prev_context, NULL); // resume offload req

    on_comp.status = 0;

    fcontext_destroy(off_req_state);
    fcontext_destroy(filler_req_state);

  }
  /* turn off ax */
  running_signal = false;
  pthread_join(ax_td, NULL);

  fcontext_destroy_proxy(self);
}

void blocking_execution(int num_requests){
  desc *sub_desc = NULL; /* this is implicitly the portal, any assignments notify the accelerator*/
  ax_comp on_comp;

  fcontext_state_t *self = fcontext_create_proxy();
  fcontext_transfer_t offload_req_xfer;
  fcontext_state_t *off_req_state;
  fcontext_transfer_t filler_req_xfer;
  fcontext_state_t *filler_req_state;

  offload_request_args off_args;
  filler_request_args filler_args;

  int linked_list_size = 10;

  linked_list *ll = ll_init();

  bool running_signal = true;

  pthread_t ax_td;
  ax_setup_args ax_args;

  ax_args.offload_time = 2100;
  ax_args.running = &running_signal;
  ax_args.pp_desc = &sub_desc;

  create_thread_pinned(&ax_td, blocking_emul_ax, &ax_args, 20);

  populate_linked_list_ascending_values(ll, linked_list_size);

  for(int i=0; i<num_requests; i++){

    off_args.pp_desc = &sub_desc;
    off_args.comp = &on_comp;
    off_args.id = i;
    off_args.ll = ll;

    filler_args.comp = &on_comp;

    off_req_state = fcontext_create(blocking_offload_request); // start offload req
    offload_req_xfer = fcontext_swap(off_req_state->context, &off_args);

    on_comp.status = 0;

    fcontext_destroy(off_req_state);
    fcontext_destroy(filler_req_state);

  }
  /* turn off ax */
  running_signal = false;
  pthread_join(ax_td, NULL);

  fcontext_destroy_proxy(self);
}

void nothing_execution(int num_requests){
  desc *sub_desc = NULL; /* this is implicitly the portal, any assignments notify the accelerator*/
  ax_comp on_comp;

  fcontext_state_t *self = fcontext_create_proxy();
  fcontext_transfer_t offload_req_xfer;
  fcontext_state_t *off_req_state;
  fcontext_transfer_t filler_req_xfer;
  fcontext_state_t *filler_req_state;

  offload_request_args off_args;
  filler_request_args filler_args;

  int linked_list_size = 10;

  linked_list *ll = ll_init();

  bool running_signal = true;

  pthread_t ax_td;
  ax_setup_args ax_args;

  ax_args.offload_time = 2100;
  ax_args.running = &running_signal;
  ax_args.pp_desc = &sub_desc;

  create_thread_pinned(&ax_td, blocking_emul_ax, &ax_args, 20);

  populate_linked_list_ascending_values(ll, linked_list_size);

  for(int i=0; i<num_requests; i++){

    off_args.pp_desc = &sub_desc;
    off_args.comp = &on_comp;
    off_args.id = i;
    off_args.ll = ll;

    filler_args.comp = &on_comp;

    off_req_state = fcontext_create(offload_request); // start offload req
    offload_req_xfer = fcontext_swap(off_req_state->context, &off_args);

    filler_req_state = fcontext_create(pollution_filler_request); // start filler req
    filler_req_xfer = fcontext_swap(filler_req_state->context, &filler_args);

    offload_req_xfer = fcontext_swap(offload_req_xfer.prev_context, NULL); // resume offload req

    on_comp.status = 0;

    fcontext_destroy(off_req_state);
    fcontext_destroy(filler_req_state);

  }
  /* turn off ax */
  running_signal = false;
  pthread_join(ax_td, NULL);

  fcontext_destroy_proxy(self);
}

static inline void create_contexts_and_transfers(
  fcontext_transfer_t * offload_req_xfer,
  fcontext_state_t ** off_req_state,
  fcontext_fn_t offload_fn,
  fcontext_transfer_t * filler_req_xfer,
  fcontext_state_t ** filler_req_state,
  fcontext_fn_t filler_fn,
  int num_requests,
  bool do_filler){
    for(int i=0; i<num_requests; i++){
      off_req_state[i] = fcontext_create(offload_fn);
      if(do_filler)
        filler_req_state[i] = fcontext_create(filler_fn);
    }
  }


static inline void execute_requests(
  fcontext_transfer_t * offload_req_xfer,
  fcontext_state_t ** off_req_state,
  offload_request_args * off_args,
  fcontext_transfer_t * filler_req_xfer,
  fcontext_state_t ** filler_req_state,
  filler_request_args * filler_args,
  int num_requests, ax_comp * on_comp,
  bool do_filler )
{
  for(int i=0; i<num_requests; i++){

    offload_req_xfer[i] = fcontext_swap(off_req_state[i]->context, &(off_args[i]));

    if(do_filler){

      filler_req_xfer[i] = fcontext_swap(filler_req_state[i]->context, &(filler_args[i]));

      offload_req_xfer[i] = fcontext_swap(offload_req_xfer[i].prev_context, NULL); // resume offload req

    }
    on_comp->status = 0;
  }
}

void execution(int num_requests, int iterations,
  fcontext_fn_t offload_fn, fcontext_fn_t filler_fn){
  desc *sub_desc = NULL; /* this is implicitly the portal, any assignments notify the accelerator*/
  ax_comp on_comp;

  fcontext_state_t *self = fcontext_create_proxy();
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;
  fcontext_transfer_t *filler_req_xfer;
  fcontext_state_t **filler_req_state;

  offload_request_args off_args[num_requests];
  filler_request_args filler_args[num_requests];

  int linked_list_size = 10;

  linked_list *ll = ll_init();

  bool running_signal = true;

  pthread_t ax_td;
  ax_setup_args ax_args;

  ax_args.offload_time = 2100;
  ax_args.running = &running_signal;
  ax_args.pp_desc = &sub_desc;

  create_thread_pinned(&ax_td, blocking_emul_ax, &ax_args, 20);

  populate_linked_list_ascending_values(ll, linked_list_size);

  /* preallocate args */
  for(int i=0; i<num_requests; i++){
    off_args[i].pp_desc = &sub_desc;
    off_args[i].comp = &on_comp;
    off_args[i].id = i;
    off_args[i].ll = ll;

    filler_args[i].comp = &on_comp;
  }

  /* Pre-create the contexts */
  offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * num_requests);
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * num_requests);
  filler_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * num_requests);
  filler_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * num_requests);

  for(int i=0; i<num_requests; i++){
    off_req_state[i] = fcontext_create(offload_fn);
    if(filler_fn != NULL)
      filler_req_state[i] = fcontext_create(filler_fn);
  }


  time_code_region(
    /*setup = */ create_contexts_and_transfers(offload_req_xfer,
      off_req_state, offload_fn, filler_req_xfer,
      filler_req_state, filler_fn, num_requests, filler_fn != NULL),
    /*main = */execute_requests(offload_req_xfer,
      off_req_state, off_args, filler_req_xfer,
      filler_req_state, filler_args, num_requests,
      &on_comp, filler_fn != NULL),
    NULL, iterations);


  for(int i=0; i<num_requests; i++){

    fcontext_destroy(off_req_state[i]);
    fcontext_destroy(filler_req_state[i]);

  }
  /* turn off ax */
  running_signal = false;
  pthread_join(ax_td, NULL);

  fcontext_destroy_proxy(self);

}


int main(int argc, char **argv){

  PRINT("Blocking\n");
  execution(1000, 100, blocking_offload_request, NULL);

  PRINT("Nothing\n");
  execution(1000, 100, offload_request, nothing_filler_request);

  PRINT("Pollution\n");
  execution(1000, 100, offload_request, pollution_filler_request);




  return 0;
}