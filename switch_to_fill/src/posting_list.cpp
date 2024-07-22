#include "posting_list.h"

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