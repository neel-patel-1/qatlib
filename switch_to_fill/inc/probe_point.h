#ifndef PROBE_POINT_H
#define PROBE_POINT_H
#include "emul_ax.h"
typedef ax_comp preempt_signal;
#define SIGNAL_SET COMP_STATUS_COMPLETED;

preempt_signal *p_sig;
fcontext_transfer_t sched_ctx;

static bool is_signal_set(preempt_signal * signal);
static void probe_point(preempt_signal *sig, fcontext_t ctx);
static void probe_point();
static void init_probe(fcontext_transfer_t arg);

#include "inline/probe_point.ipp"

#endif