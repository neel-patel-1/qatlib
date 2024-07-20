#include "dsa_alloc.h"

#include <cstdlib>



typedef struct _node{
  uint32_t docID;
  struct _node *next;
} node;

bool gDebugParam = true;
int main(){

  node *src_node_1 = (node *)malloc(sizeof(node));
  node *src_node_2 = (node *)malloc(sizeof(node));
  node *dst_node_1 = (node *)malloc(sizeof(node));
  node *dst_node_2 = (node *)malloc(sizeof(node));

  node *head = src_node_1;

  src_node_1->next = dst_node_2;
  src_node_1->docID = 1;

  src_node_2->next = NULL;
  src_node_2->docID = 2;

  /*traverse the llc linked_list*/

  dsa_llc_realloc(dst_node_1, src_node_1, sizeof(node));
  dsa_llc_realloc(dst_node_2, src_node_2, sizeof(node));

  return 0;
}