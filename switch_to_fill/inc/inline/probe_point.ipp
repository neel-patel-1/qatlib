static bool is_signal_set(preempt_signal * signal){
  return signal->status == SIGNAL_SET;
}
static inline void probe_point(preempt_signal *sig, fcontext_t ctx){
  if(is_signal_set(sig)){
    fcontext_transfer_t parent_resume =
      fcontext_swap( ctx, NULL);
    p_sig = (preempt_signal *)parent_resume.data;
  }
}

static inline void probe_point(){
  if(is_signal_set(p_sig)){ /*check signal */
    fcontext_transfer_t parent_resume =
      fcontext_swap( sched_ctx.prev_context, NULL); /* switch to sig */
    p_sig = (preempt_signal *)parent_resume.data; /*set the new signal returned from the scheduler */
  }
  return NULL;
}

static inline void init_probe(fcontext_transfer_t arg){
  p_sig = (preempt_signal *)arg.data;
  sched_ctx = arg;
}