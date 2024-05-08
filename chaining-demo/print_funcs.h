#ifndef PRINT_FUNCS
#define PRINT_FUNCS

#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"

#include "thread_utils.h"

void printStats(packet_stats **stats, Cpa32U numOperations, Cpa32U bufferSize);
void printMultiThreadStats(packet_stats ***arrayOfPacketStatsArrayPointers, Cpa32U numFlows, Cpa32U numOperations, Cpa32U bufferSize);

#endif