Sample App flow:
enable latency measurements
register a userspace process
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/latency_sample_main.c#L451
Service Access Layer for Userspace
We use the V2 Config file: USE_V2_CONFIG_FILE
Query the number of instances 
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/common/ctrl/sal_instances.c#L111
Set to use a single instance if we are measuring latency.
Store instance handles in a CpaInstanceHandle Array:
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/latency_sample_main.c#L507
Allocate memory for the instance core mapping: cyInstMap_g
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/latency_sample_main.c#L526
Enumerate devices with compression enabled:
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/common/ctrl/sal_compression.c#L1805
setting up the test:
populate the corpus
get the number of decompression instances
create a buffer list for each instance
register the performance testing thread that will be used when createPerformanceThreads is called

Use this https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/common/qat_perf_cycles.h#L229 to create a timestamp for buffer alloc phase
perf_cycles_t req_temp;

performance testing thread:
gets assigned a quickassist instance
launches worker function -
 - worker function
prints stats
resets performance stats 

timestamp of buffer allocation begin
memory allocated for source and destination bufferlists
allocate memory for the buffer list structures
- specify the number of buffers in each list, the size of the buffer in each list
allocate the buffers in each buffer list
internally uses mmap after making an ioctl call which requests kernel memory
populate the source buffers: https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/qat_compression_main.c#L1472
allocate a compression session and initialize it for a compression thread

setup can be configured to induce an overflow of the destination buffer CnV (?): https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/qat_compression_main.c#L1502

OR compress data: https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/qat_compression_main.c#L1512
then update produced buffer length
then update total bytes produced and consumed
then 

OR Decompress data: 
- generate test data , but disable latency and COO testing:
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/qat_compression_main.c#L1539
set the number of loops after generating test data using num_loops of 1

OR Compress data with 
optionally set reliability flag to perform a software decompression  and compare the output buffers with src buffers (take a look to see if we can modify qatSWDecompress to match qathwdecompress to perform latency measurements and populate the perf structure)

OR Decompress with software compression used [1] to compress the buffers, decompression on qat numLoops times with buffers compared afterward

qatCompressData:
allocates the CpaInstanceInfo2 giving an entry into the device stats -- are we offloaded -- is the instance polled?
initialize a semaphore (?)

sync threads to start submitting requests at the same time
- start a timestamp once the threads start performing submissions
compress the file n_loops times

each time, 
we (1) submit a request
- we start a latency measurement taking note of the start time for a submission
- we also take note of the cycle at which the submission began
- a before creating cookies(?):
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/common/compression/dc_datapath.c#L1812

 and create a request submit a request to the firmware(?):
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/common/compression/dc_datapath.c#L1282
atomically incrementing the number of pending callback functions for the session: https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/common/compression/dc_datapath.c#L1913
this call chain is initiated by a cpaDC function: https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/common/compression/dc_datapath.c#L2125
which precedes a completion of the cost-off-offload measurement since request submission has ended:
- takes a timestamp after sentind a request 




(2) poll for responses && update the number of total submissions in the performanceStats struct: https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/qat_compression_main.c#L2386
- in the baseline code's latency measurement enabled path, the code polls for completion of a single buffer to be processed by QAT

---
[1] https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/qat_compression_main.h#L621
qatSwCompress compress source data

[2] qatSwZlibCompress: https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/qat_compression_utils.c#L975


What overheads should be included in the software compression path?
A new stream is created for each (de)comp which is not realistic




Are dc_dp enqueue/poll APIs clearer / easier to profile? Do they provide the same performance?
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/cpa_sample_code_dc_dp.c#L649

perform dc_dp enqueue single request or batch op: https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/cpa_sample_code_dc_dp.c#L1068

enqueue request: 
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/qat_compression_main.c#L1827
- checks for inflight stateful data:
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/qat_compression_main.c#L1827

request_submit_start - request_submit_end

Config
Session-based Compression APIs


---
Latency Measurement:

response time is tracked in the callback function:
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/cpa_sample_code_dc_utils.c#L218

dcPerformCallback: https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/cpa_sample_code_dc_utils.c#L174
gets passed in a tag and a status
- tag is a pointer to the test parameters
status tells whether the request was processed correctly

we only calculate latency on responses whose 

can we assume responses are received in the same order that requests were submitted?
The code does not check any ids or associate responses with requests in the performance monitoring callback function (cb) 




initialization of latency measurement data
countIncrement is how many submissions are made between each latency measurement. total number of submissions divided by MAX_LATENCY_COUNT (100)

latencyCount records the number of latency measurements taken
start_time is an array to record time-stamp just before a submission is made

response_time array is a location to record time-stamp when a response is received

submit_time is stored in start_time at the same index
as response time in the array
difference is time of the request

response time is captured in the registered callback function:
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/cpa_sample_code_dc_utils.c#L174

https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/common/qat_perf_latency.c#L184

summarization of latency measurement data:
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/cpa_sample_code_dc_utils.c#L1585
- divide collected latency by observed cpu frequency - statsLatency must be in cycles initially

printing the latency stats: https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/cpa_sample_code_dc_utils.c#L1331

- reports how long it took on average to "process a buffer"
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/cpa_sample_code_dc_utils.c#L1440
---


qatLatency pollingForResponses Measurement Function:
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/common/qat_perf_latency.c#L262
- we are requested to process a single buffer at a time, which can give the best latencies . does enabling improve latency compared to current tests(?) 
- looks to be enabled by default: https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/common/qat_perf_latency.c#L73

starts a latency measurement for every submitted request: https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/qat_compression_main.c#L1799


are dataplane functions faster?


dc_dp api also uses callback functions, what is the difference between dp apis?
https://vscode.dev/github/neel-patel-1/qatlib/blob/dc_latency/quickassist/lookaside/access_layer/src/sample_code/performance/compression/cpa_sample_code_dc_dp.c#L163


How to determine which invocations of the callback function correspond to a request for which a timestamp corresponding to a latency measurement was created?