static inline void ll_simple(node *head){
  node *cur = head;
  while(cur != NULL){
    LOG_PRINT(LOG_DEBUG, "Node: %d\n", cur->docID);
    cur = cur->next;
  }
}