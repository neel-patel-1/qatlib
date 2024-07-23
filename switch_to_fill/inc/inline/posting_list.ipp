static inline void ll_simple(node *head){
  node *cur = head;
  while(cur != NULL){
    LOG_PRINT(LOG_DEBUG, "Node: %d\n", cur->docID);
    cur = cur->next;
  }
}

static inline void intersect_posting_lists(node *intersected, node *head1, node *head2){
  node *cur1 = head1;
  node *cur2 = head2;
  node *cur3 = intersected;
  node *cur3_prev = NULL;
  while(cur1 != NULL && cur2 != NULL){
    if(cur1->docID == cur2->docID){
      LOG_PRINT(LOG_DEBUG, "IntersectVal: %d\n", cur1->docID);
      cur3->docID = cur1->docID;
      cur3 = cur3->next;
      cur1 = cur1->next;
      cur2 = cur2->next;
    }else if(cur1->docID < cur2->docID){
      cur1 = cur1->next;
    }else{
      cur2 = cur2->next;
    }
  }
}

static inline int dot_product(uint8_t *a, uint8_t *b, int size){
  int i;
  int sum = 0;
  for(i=0; i<size; i++){
    sum += a[i] * b[i];
  }
  return sum;
}

static inline void ll_dynamic_rank(node *head){
  node *cur = head;
  int sums[512];
  int idx= 0;
  while(cur != NULL){
    int sum = dot_product(cur->padding,
      cur->padding + sizeof(cur->padding)/2, sizeof(cur->padding)/2);
    cur = cur->next;

    sums[idx++] = sum;
  }

}