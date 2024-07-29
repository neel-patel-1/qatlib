#ifndef PROBE_POINT_H
#define PROBE_POINT_H
#include "emul_ax.h"
typedef ax_comp preempt_signal;
#define SIGNAL_SET COMP_STATUS_COMPLETED;

extern preempt_signal *p_sig;

static bool is_signal_set(preempt_signal * signal);
void probe_point(preempt_signal *sig, fcontext_t ctx);

#include "inline/probe_point.ipp"

#endif