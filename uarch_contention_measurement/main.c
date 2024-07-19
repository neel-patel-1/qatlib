#include "idxd.h"
#include "thread_utils.h"
#include "print_utils.h"
#include "timer_utils.h"
#include "fcontext.h"
#include "linked_list.h"

#include <stdbool.h>
#include <x86intrin.h>

bool gDebugParam = 1;

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
      PRINT_DBG("Request id: %d completed in %ld\n", offloader_id, cur - start_time);
      comp->status = 1;
      offload_in_flight = false;
    }
    offload_pending = (*pp_desc != NULL);
    if(offload_pending){
      PRINT_DBG("Request id: %d accepted\n", (*pp_desc)->id);
      start_time = sampleCoderdtsc();

      off_desc = *pp_desc;
      offloader_id = off_desc->id;
      comp = off_desc->comp;

      offload_in_flight = true;
      *pp_desc = NULL;
    }
  }
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


void blocking_offload_request(fcontext_transfer_t arg){
  offload_request_args *args = (offload_request_args *)arg.data;
  desc **pp_desc = args->pp_desc;
  fcontext_t parent = arg.prev_context;

  ax_comp *comp = args->comp;
  desc off_desc = {.comp = comp, .id = args->id};

  linked_list *ll = args->ll;

  comp->status = 0;
  *pp_desc = &off_desc;
  while(*pp_desc != NULL){ _mm_pause(); }
  PRINT_DBG("Request id: %d submitted\n", off_desc.id);
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

  ax_comp *comp = args->comp;
  desc off_desc = {.comp = comp, .id = args->id};

  linked_list *ll = args->ll;

  comp->status = 0;
  *pp_desc = &off_desc;
  while(*pp_desc != NULL){ _mm_pause(); }
  PRINT_DBG("Request id: %d submitted\n", off_desc.id);
  fcontext_swap(parent, NULL);

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

void pollution_filler_request(fcontext_transfer_t arg){
  filler_request_args *args = (filler_request_args *)arg.data;
  ax_comp *comp = args->comp;

  /* execute probed kernel */
  pollution_kernel(comp, arg.prev_context);
  PRINT_DBG("Filler completed\n");

}

void nothing_filler_request(fcontext_transfer_t arg){
  filler_request_args *args = (filler_request_args *)arg.data;
  ax_comp *comp = args->comp;

  /* execute probed kernel */
  nothing_kernel(comp, arg.prev_context);
  PRINT_DBG("Filler completed\n");

}


int main(int argc, char **argv){

  desc *sub_desc = NULL; /* this is implicitly the portal, any assignments notify the accelerator*/
  bool running_signal = true;

  pthread_t ax_td;
  ax_setup_args ax_args;

  ax_comp on_comp;

  int num_requests = 100;

  fcontext_state_t *self = fcontext_create_proxy();
  fcontext_transfer_t offload_req_xfer;
  fcontext_state_t *off_req_state;
  fcontext_transfer_t filler_req_xfer;
  fcontext_state_t *filler_req_state;

  offload_request_args off_args;
  filler_request_args filler_args;

  int linked_list_size = 10;

  linked_list *ll = ll_init();
  for(int i=0; i<linked_list_size; i++){
    void *data = (void *)malloc(sizeof(int));
    *(int *)data = i;
    ll_insert(ll, data);
  }

  ax_args.offload_time = 2100;
  ax_args.running = &running_signal;
  ax_args.pp_desc = &sub_desc;

  create_thread_pinned(&ax_td, blocking_emul_ax, &ax_args, 20);

  for(int i=0; i<num_requests; i++){

    off_args.pp_desc = &sub_desc;
    off_args.comp = &on_comp;
    off_args.id = 0;
    off_args.ll = ll;

    filler_args.comp = &on_comp;

    off_req_state = fcontext_create(offload_request); // start offload req
    offload_req_xfer = fcontext_swap(off_req_state->context, &off_args);

    filler_req_state = fcontext_create(pollution_filler_request); // start filler req
    filler_req_xfer = fcontext_swap(filler_req_state->context, &filler_args);

    offload_req_xfer = fcontext_swap(offload_req_xfer.prev_context, NULL); // resume offload req

    fcontext_destroy(off_req_state);
    fcontext_destroy(filler_req_state);

  }
  /* turn off ax */
  running_signal = false;
  pthread_join(ax_td, NULL);

  return 0;
}