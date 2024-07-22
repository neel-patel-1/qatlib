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

#include "dsa_alloc.h"

typedef struct _node{
  int docID;
  struct _node *next;
  uint8_t padding[64 - sizeof(int) - sizeof(struct _node *)];
} node;

node *build_llc_ll(int length){
  node *cpy_src = (node *)malloc(sizeof(node));
  node *cpy_dst = (node *)malloc(sizeof(node));
  node *tgt = (node *)malloc(sizeof(node));
  node *head = cpy_dst;

  int i;
  for(i=0; i<length-1; i++){
    cpy_src->docID = i;
    cpy_src->next = tgt;

    dsa_llc_realloc(cpy_dst, cpy_src, sizeof(node));
    cpy_dst = tgt;
    tgt = (node *)malloc(sizeof(node));
  }
  cpy_src->docID = length-1; /* left with a hanging tgt to cpy to*/
  cpy_src->next = NULL;
  dsa_llc_realloc(cpy_dst, cpy_src, sizeof(node));

  free(cpy_src);

  return head;
}

static inline void ll_simple(node *head){
  node *cur = head;
  while(cur != NULL){
    LOG_PRINT(LOG_DEBUG, "Node: %d\n", cur->docID);
    cur = cur->next;
  }
}

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