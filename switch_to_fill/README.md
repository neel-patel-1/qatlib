# APU-Gym: A framework for on-chip accelerator design space exploration
* APUGym is used for the accelerator and gpcore profiling stage of bottleneck analysis for latency-critical / user-facing services.


### Latency/Offered Load of a GPCore/APU-Accelerated Request
The goal of this workflow is to get a breakdown of the end-to-end execution of a request executing on a general-purpose core and compare it with the same request with part of the request offloaded to an accelerator.
We will need:
  (1) A time-stamped "{GPCore,Blocking-Offload, YieldingRequest} Request"
  (2) "{GPCore Input Payload, Offload Requestor Argument} Allocate/Free Functions"
* These functions are passed to a "Test Harness" which executes the request in the context of an "Executor"
See examples in src/decompress_and_hash.cpp and mlp.cpp

Design:
* There are "Latency Breakdown" and "Closed System Throughput" Executors
* "Latency Breakdown" executor shows where the time goes during the execution of a request
* "Closed System Throughput" executor implements a FCFS scheduling policy and determines the offered load when executing the passed in request on a single GPCore with optional offloads to on-chip accelerators


### To begin
* Go to configs/devid.sh and configs/phys_core.sh and set the iax/dsa device and submitting core
* run `sudo python3 scripts/accel_conf.py --load=configs/*.conf` to configure a device
* run `make -j` to compile all tests
* execute `./scripts/run_*.sh` and `./scripts/parse_*.sh` to execute and parse results