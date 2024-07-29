static bool is_signal_set(preempt_signal * signal){
  return signal->status == SIGNAL_SET;
}
void probe_point(preempt_signal *sig, fcontext_t ctx){
  if(is_signal_set(sig)){
    fcontext_transfer_t parent_resume =
      fcontext_swap( ctx, NULL);
    p_sig = (preempt_signal *)parent_resume.data;
  }
}