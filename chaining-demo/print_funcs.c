#include "print_funcs.h"

void printStats(packet_stats **stats, Cpa32U numOperations, Cpa32U bufferSize){
/* Collect Latencies */
  Cpa32U freqKHz = 2080;
  Cpa64U avgLatency = 0;
  Cpa64U minLatency = UINT64_MAX;
  Cpa64U maxLatency = 0;
  Cpa64U firstSubmitTime = stats[0]->submitTime;
  Cpa64U lastReceiveTime = stats[numOperations-1]->receiveTime;
  Cpa64U exeCycles = lastReceiveTime - firstSubmitTime;
  Cpa64U exeTime = exeCycles/freqKHz;
  double offloadsPerSec = numOperations / (double)exeTime;
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
  printf("Execution Time(us): %lu\n", exeTime);
  printf("OffloadsPerSec: %f\n", offloadsPerSec);
  printf("Throughput(GB/s): %f\n", offloadsPerSec * bufferSize / 1024 / 1024 / 1024);

}

void printMultiThreadStats(packet_stats ***arrayOfPacketStatsArrayPointers, Cpa32U numFlows, Cpa32U numOperations, Cpa32U bufferSize){
  printf("--------------------------------------\n");
  printf("BufferSize: %d\n", bufferSize);
  printf("NumOperations: %d\n", numOperations);
  printf("--------------------------------------\n");
  for(int i=0; i<numFlows; i++){
    printf("--------------------------------------\n");
    printf("Flow: %d\n", i);
    printStats((arrayOfPacketStatsArrayPointers[i]), numOperations, bufferSize);
  }
  printf("--------------------------------------\n");
}