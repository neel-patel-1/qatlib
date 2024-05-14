#ifndef PRINT_FUNCS
#define PRINT_FUNCS

#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"

#include "log.h"

#include "thread_utils.h"

typedef struct _threadStats2P {
  Cpa64U avgLatencyS1;
  Cpa64U avgLatencyS2;
  Cpa64U avgLatency;
  Cpa64U minLatency;
  Cpa64U maxLatency;
  Cpa64U exeTimeUs;
  Cpa32U operations;
  Cpa32U operationSize;
  Cpa32U id;
} threadStats2P;

typedef struct _threadStats {
  Cpa64U avgLatency;
  Cpa64U minLatency;
  Cpa64U maxLatency;
  Cpa64U exeTimeUs;
  Cpa32U operations;
  Cpa32U operationSize;
  Cpa32U id;
} threadStats;

void printStats(packet_stats **stats, Cpa32U numOperations, Cpa32U bufferSize);
void printMultiThreadStats(packet_stats ***arrayOfPacketStatsArrayPointers, Cpa32U numFlows, Cpa32U numOperations, Cpa32U bufferSize);
void printThroughputStats(uint64_t endTime, uint64_t startTime, int numOperations, int bufferSize);

void populate2PhaseThreadStats(two_stage_packet_stats ** stats2Phase, threadStats2P **pThrStats, Cpa32U numOperations, Cpa32U bufferSize, Cpa32U flowId);
void printTwoPhaseSingleThreadStatsSummary(threadStats2P *stats);

void printTwoPhaseMultiThreadStatsSummary(
  two_stage_packet_stats ***arrayOfPacketStatsArrayPointers,
  Cpa32U numFlows, Cpa32U numOperations, Cpa32U bufferSize, CpaBoolean printThreadStats);

void logLatencies(packet_stats **packetStatsPtrsArray, Cpa32U numOperations, char *configName);
void logLatencies2Phase(two_stage_packet_stats **packetStatsPtrsArray, Cpa32U numOperations,char *configName);
#endif