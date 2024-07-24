APU-Gym: A framework for on-chip accelerator design space exploration
* APUGym is used for the accelerator and gpcore profiling stage of bottleneck analysis for latency-critical / user-facing services.


### Profiling a GPCore/APU-Accelerated Request
The goal of this workflow is to get a breakdown of the end-to-end execution of a request executing on a general-purpose core and compare it with the same request with part of the request offloaded to an accelerator.
We will need:
(1) A time-stamped "GPCore Request"
(2) A input payload "Allocator Function"
(3) A input payload "Free function"
* Given all of these are created, these functions can be passed directly to a "Test Harness" which executes the request in the context of an "Executor"
* There are "Latency Breakdown" and "Closed System Throughput" Executors
* "Latency Breakdown" executor shows where the time goes during the execution of a request
* "Closed System Throughput" executor implements a FCFS scheduling policy and determines the offered load when executing the passed in request on a single GPCore with optional offloads to on-chip accelerators