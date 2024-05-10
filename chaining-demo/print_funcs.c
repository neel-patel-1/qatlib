#include "print_funcs.h"

void statsThreadPopulate(packet_stats **packetStatsPtrsArray, Cpa32U numOperations, Cpa32U bufferSize,
  threadStats *thrStats, Cpa32U id){
  Cpa32U freqKHz = 2080;
  Cpa64U avgLatency = 0;
  Cpa64U minLatency = UINT64_MAX;
  Cpa64U maxLatency = 0;
  Cpa64U firstSubmitTime = packetStatsPtrsArray[0]->submitTime;
  Cpa64U lastReceiveTime = packetStatsPtrsArray[numOperations-1]->receiveTime;
  Cpa64U exeCycles = lastReceiveTime - firstSubmitTime;
  Cpa64U exeTimeUs = exeCycles/freqKHz;
  double offloadsPerSec = numOperations / (double)exeTimeUs;
  offloadsPerSec = offloadsPerSec * 1000000;
  for(int i=0; i<numOperations; i++){
    Cpa64U latency = packetStatsPtrsArray[i]->receiveTime - packetStatsPtrsArray[i]->submitTime;
    uint64_t micros = latency / freqKHz;
    avgLatency += micros;
    if(micros < minLatency){
      minLatency = micros;
    }
    if(micros > maxLatency){
      maxLatency = micros;
    }
  }
  avgLatency = avgLatency / numOperations;
  thrStats->avgLatency = avgLatency;
  thrStats->minLatency = minLatency;
  thrStats->maxLatency = maxLatency;
  thrStats->exeTimeUs = exeTimeUs;
  thrStats->operations = numOperations;
  thrStats->operationSize = bufferSize;
  thrStats->id = id;

}

/* TODO: https://github.com/rxi/log.c*/

void printSingleThreadStatsSummary(threadStats *stats){
    Cpa64U exeTimeUs = stats->exeTimeUs;
    Cpa32U numOperations = stats->operations;
    Cpa32U bufferSize = stats->operationSize;
    double offloadsPerSec = numOperations / (double)exeTimeUs;
    offloadsPerSec = offloadsPerSec * 1000000;
    printf("Thread: %d\n", stats->id);
    printf("AvgLatency: %ld\n", stats->avgLatency);
    printf("MinLatency: %ld\n", stats->minLatency);
    printf("MaxLatency: %ld\n", stats->maxLatency);
    printf("OffloadsPerSec: %f\n", offloadsPerSec);
    printf("Throughput(GB/s): %f\n", offloadsPerSec * bufferSize / 1024 / 1024 / 1024);

}

void printMultiThreadStatsSummary(
  packet_stats ***arrayOfPacketStatsArrayPointers,
  Cpa32U numFlows, Cpa32U numOperations, Cpa32U bufferSize, CpaBoolean printThreadStats)
{
  threadStats thrStats[numFlows];
  Cpa32U maxOfAll=0;
  Cpa32U avgAcrossAll=0;
  Cpa32U minOfAll=UINT32_MAX;
  Cpa64U minLatency = UINT64_MAX;
  Cpa64U maxLatency = 0;
  Cpa64U avgLatency = 0;
  double totalThroughput = 0;
  Cpa64U totalOperations = 0;
  double offloadsPerSec = 0;

  for(int i=0; i<numFlows; i++){
    statsThreadPopulate(arrayOfPacketStatsArrayPointers[i], numOperations, bufferSize, &thrStats[i], i);
    if(printThreadStats){
      printSingleThreadStatsSummary(&thrStats[i]);
    }
    /* Exe times */
    if(thrStats[i].exeTimeUs > maxOfAll){
      maxOfAll = thrStats[i].exeTimeUs;
    }
    avgAcrossAll+=thrStats[i].exeTimeUs;
    if(thrStats[i].exeTimeUs < minOfAll){
      minOfAll = thrStats[i].exeTimeUs;
    }

    /* Latencies */
    if(thrStats[i].maxLatency > maxLatency){
      maxLatency = thrStats[i].maxLatency;
    }
    avgLatency+=thrStats[i].avgLatency;
    if(thrStats[i].minLatency < minLatency){
      minLatency = thrStats[i].minLatency;
    }
    totalOperations += thrStats[i].operations;

    /* Offloads Per Sec Should use the avg offloads per sec from each flow */    double perFlowOffloadsPerSec = thrStats[i].operations / (double)thrStats[i].exeTimeUs;
    perFlowOffloadsPerSec *= 1000000;
    offloadsPerSec += perFlowOffloadsPerSec;
  }
  /* If the execution time on one flow was much larger than the others, this metric weighs in the deterioration in performance
  from the long running flows*/
  offloadsPerSec = offloadsPerSec / numFlows;

  /* Throughput should use the average offloads per second scaled by the number of flows*/
  totalThroughput = offloadsPerSec * bufferSize / 1024 / 1024 / 1024;
  printf("\n");
  printf("NumFlows: %d\n", numFlows);
  printf("BufferSize: %d\n", bufferSize);
  printf("NumBuffers: %d\n", numOperations);
  printf("MaxExecutionTime(across-all-streams): %d\n", maxOfAll);
  printf("AvgExecutionTime(across-all-streams): %d\n", avgAcrossAll/numFlows);
  printf("MinExecutionTime(across-all-streams): %d\n", minOfAll);
  printf("MaxLatency(across-all-streams): %ld\n", maxLatency);
  printf("AvgLatency(across-all-streams): %ld\n", avgLatency/numFlows);
  printf("MinLatency(across-all-streams): %ld\n", minLatency);
  printf("TotalOperations(across-all-streams): %ld\n", totalOperations);
  printf("OffloadsPerSec(avg'd-across-threads): %f\n", offloadsPerSec);
  printf("AvgPerThreadThroughput(GB/s): %f\n", totalThroughput);
  printf("CumulativeOffloadsPerSec: %f\n", offloadsPerSec*numFlows);
  printf("CumulativeThroughput(GB/s): %f\n", totalThroughput * numFlows);
  printf("\n");
}

void populate2PhaseThreadStats(two_stage_packet_stats ** stats2Phase, threadStats2P **pThrStats, Cpa32U numOperations, Cpa32U bufferSize){
/* Print latencies for each phase */
  threadStats2P *thrStats;
  Cpa32U id = 0;
  Cpa32U freqKHz = 2080;
  Cpa64U avgLatency = 0;
  Cpa64U minLatency = UINT64_MAX;
  Cpa64U maxLatency = 0;
  Cpa64U firstSubmitTime = stats2Phase[0]->submitTime;
  Cpa64U lastReceiveTime = stats2Phase[numOperations-1]->receiveTime;
  Cpa64U exeCycles = lastReceiveTime - firstSubmitTime;
  Cpa64U exeTimeUs = exeCycles/freqKHz;
  Cpa64U avgLatencyP2 = 0;
  Cpa64U avgLatencyP1 = 0;
  double offloadsPerSec = numOperations / (double)exeTimeUs;
  offloadsPerSec = offloadsPerSec * 1000000;

  OS_MALLOC(&thrStats, sizeof(threadStats2P));
  for(int i=0; i<numOperations; i++){
    Cpa64U latencyP2 = stats2Phase[i]->receiveTime - stats2Phase[i]->cbReceiveTime;
    Cpa64U latencyP1 = stats2Phase[i]->cbReceiveTime - stats2Phase[i]->submitTime;
    Cpa64U latency = stats2Phase[i]->cbReceiveTime - stats2Phase[i]->submitTime;
    uint64_t e2eMicros = latency / freqKHz;
    uint64_t p1Micros = latencyP1 / freqKHz;
    uint64_t p2Micros = latencyP2 / freqKHz;

    avgLatencyP1 += p1Micros;
    avgLatencyP2 += p2Micros;
    avgLatency += (p1Micros + p2Micros);
    if(e2eMicros < minLatency){
      minLatency = e2eMicros;
    }
    if(e2eMicros > maxLatency){
      maxLatency = e2eMicros;
    }
  }
  avgLatency = avgLatency / numOperations;
  thrStats->avgLatencyS1 = avgLatencyP1 / numOperations;
  thrStats->avgLatencyS2 = avgLatencyP2 / numOperations;
  thrStats->avgLatency = avgLatency;
  thrStats->minLatency = minLatency;
  thrStats->maxLatency = maxLatency;
  thrStats->exeTimeUs = exeTimeUs;
  thrStats->operations = numOperations;
  thrStats->operationSize = bufferSize;
  thrStats->id = id;
  *pThrStats = thrStats;
}

void printTwoPhaseSingleThreadStatsSummary(threadStats2P *stats){
    Cpa64U exeTimeUs = stats->exeTimeUs;
    Cpa32U numOperations = stats->operations;
    Cpa32U bufferSize = stats->operationSize;
    double offloadsPerSec = numOperations / (double)exeTimeUs;
    offloadsPerSec = offloadsPerSec * 1000000;
    printf("Thread: %d\n", stats->id);
    printf("AvgLatency: %ld\n", stats->avgLatency);
    printf("MinLatency: %ld\n", stats->minLatency);
    printf("MaxLatency: %ld\n", stats->maxLatency);
    printf("AvgPhase1Latency: %ld\n", stats->avgLatencyS1);
    printf("AvgPhase2Latency: %ld\n", stats->avgLatencyS2);
    printf("OffloadsPerSec: %f\n", offloadsPerSec);
    printf("Throughput(GB/s): %f\n", offloadsPerSec * bufferSize / 1024 / 1024 / 1024);

}


void printStats(packet_stats **stats, Cpa32U numOperations, Cpa32U bufferSize){
/* Collect Latencies */
  Cpa32U freqKHz = 2080;
  Cpa64U avgLatency = 0;
  Cpa64U minLatency = UINT64_MAX;
  Cpa64U maxLatency = 0;
  Cpa64U firstSubmitTime = stats[0]->submitTime;
  Cpa64U lastReceiveTime = stats[numOperations-1]->receiveTime;
  Cpa64U exeCycles = lastReceiveTime - firstSubmitTime;
  Cpa64U exeTimeUs = exeCycles/freqKHz;
  double offloadsPerSec = numOperations / (double)exeTimeUs;
  offloadsPerSec = offloadsPerSec * 1000000;
  for(int i=0; i<numOperations; i++){
    Cpa64U latency = stats[i]->receiveTime - stats[i]->submitTime;
    uint64_t micros = latency / freqKHz;
    avgLatency += micros;
    if(micros < minLatency){
      minLatency = micros;
    }
    if(micros > maxLatency){
      maxLatency = micros;
    }
  }
  avgLatency = avgLatency / numOperations;
  printf("AveLatency(us): %lu\n", avgLatency);
  printf("MinLatency(us): %lu\n", minLatency);
  printf("MaxLatency(us): %lu\n", maxLatency);
  printf("Execution Time(us): %lu\n", exeTimeUs);
  printf("OffloadsPerSec: %f\n", offloadsPerSec);
  printf("Throughput(GB/s): %f\n", offloadsPerSec * bufferSize / 1024 / 1024 / 1024);

}

void printMultiThreadStats(packet_stats ***arrayOfPacketStatsArrayPointers, Cpa32U numFlows, Cpa32U numOperations, Cpa32U bufferSize){
  // printf("--------------------------------------\n");
  // printf("BufferSize: %d\n", bufferSize);
  // printf("NumOperations: %d\n", numOperations);
  // printf("--------------------------------------\n");
  // for(int i=0; i<numFlows; i++){
  //   printf("--------------------------------------\n");
  //   printf("Flow: %d\n", i);
  //   printStats((arrayOfPacketStatsArrayPointers[i]), numOperations, bufferSize);
  // }
  // printf("--------------------------------------\n");
  printMultiThreadStatsSummary(arrayOfPacketStatsArrayPointers, numFlows, numOperations, bufferSize, CPA_FALSE);
}
