#include "dsa_alloc.h"
#include "print_utils.h"

#include <cstdlib>
#include <cstring>



typedef struct _node{
  int docID;
  struct _node *next;
  uint8_t padding[512 - sizeof(int) - sizeof(struct _node *)];
} node;

bool gDebugParam = true;
int main(){

  node *cpy_src = (node *)malloc(sizeof(node));
  node *cpy_dst = (node *)malloc(sizeof(node));
  node *tgt = (node *)malloc(sizeof(node));
  node *head = cpy_dst;

  int length = 10;
  int i;
  for(i=0; i<length-1; i++){
    cpy_src->docID = i;
    cpy_src->next = tgt;

    dsa_llc_realloc(cpy_dst, cpy_src, sizeof(node));
    PRINT_DBG("docID: %d\n", cpy_dst->docID);
    cpy_dst = tgt;
    tgt = (node *)malloc(sizeof(node));
  }
  tgt->docID = length -1;
  tgt->next = NULL;

  node *cur = head;
  while(cur != NULL){
    PRINT("docID: %d\n", cur->docID);
    cur = cur->next;
  }
  return 0;
}