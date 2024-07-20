#include "dsa_alloc.h"

#include <cstdlib>

extern "C"{
  #include "linked_list.h"
}


bool gDebugParam = true;
int main(){

  node *src_node_1 = (node *)malloc(sizeof(node));
  node *dst_node_2 = (node *)malloc(sizeof(node));

  dsa_llc_realloc(NULL, NULL, 0);
  return 0;
}