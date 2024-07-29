IAA_INCLUDES = -I/home/n869p538/spr-accel-profiling/interrupt_qat_dc/idxd-config/test \
	-I/home/n869p538/spr-accel-profiling/interrupt_qat_dc/idxd-config/test/algorithms
IDXD_INCLUDES = -I/home/n869p538/spr-accel-profiling/interrupt_qat_dc/idxd-config
ACCFG_INCLUDES = -I/home/n869p538/spr-accel-profiling/interrupt_qat_dc/idxd-config/accfg
IAA_LIBS = -L/home/n869p538/spr-accel-profiling/interrupt_qat_dc/idxd-config/test -liaa -lz -lcrypto
ACCFG_LIBS = -L/home/n869p538/spr-accel-profiling/interrupt_qat_dc/idxd-config/accfg/lib -laccel-config

INCLUDES = -I./inc $(IAA_INCLUDES) $(ACCFG_INCLUDES) $(IDXD_INCLUDES)
LIBS = -ldml $(IAA_LIBS) $(ACCFG_LIBS)

CXXFLAGS = -O2

all: switch_to_fill \
	walker \
	mlp \
	decomp_and_hash \
	decomp_and_hash_multi_threaded \
	memfill_and_gather scan_and_gather \
	decomp_and_scatter \
	tests

obj/jump_x86_64_sysv_elf_gas.o: src/jump_x86_64_sysv_elf_gas.S
	$(CXX) $(CXXFLAGS) -c -o $@ $^  -I./inc

obj/make_x86_64_sysv_elf_gas.o: src/make_x86_64_sysv_elf_gas.S
	$(CXX) $(CXXFLAGS) -c -o $@ $^  -I./inc

obj/ontop_x86_64_sysv_elf_gas.o: src/ontop_x86_64_sysv_elf_gas.S
	$(CXX) $(CXXFLAGS) -c -o $@ $^  -I./inc

obj/context_fast.o: src/context_fast.S
	$(CXX) $(CXXFLAGS) -c -o $@ $^  -I./inc

router.pb.o: src/router.pb.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $^  -I./inc `pkg-config --cflags --libs protobuf`

obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^  $(INCLUDES) `pkg-config --cflags --libs protobuf` -fpermissive

switch_to_fill.o: switch_to_fill.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^  $(INCLUDES)
SWITCH_TO_FILL_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o router.pb.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o obj/router_requests.o \
	obj/ch3_hash.o obj/request_executors.o obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o
switch_to_fill: $(SWITCH_TO_FILL_OBJS) switch_to_fill.o router.pb.o
	$(CXX) $(CXXFLAGS) $(FCONTEXT_OBJ) -o $@ $^ `pkg-config --cflags --libs protobuf` \
	-ldml


walker.o: walker.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^  -I./inc $(INCLUDES)
WALKER_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/walker_requests.o obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o obj/posting_list.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/request_executors.o obj/runners.o
walker: $(WALKER_OBJS) walker.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -ldml



MLP_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/ch3_hash.o \
	obj/request_executors.o obj/runners.o obj/iaa_offloads.o \
	obj/dsa_offloads.o
mlp.o: mlp.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^ $(INCLUDES) -fpermissive
mlp: $(MLP_OBJS) mlp.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -ldml $(LIBS)
mlp_clean:
	rm -f mlp mlp.o

MEMFILL_AND_GATHER_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/ch3_hash.o \
	obj/request_executors.o obj/runners.o obj/iaa_offloads.o \
	obj/dsa_offloads.o \
	obj/gather_scatter.o
memfill_and_gather.o: memfill_and_gather.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^ $(INCLUDES) -fpermissive
memfill_and_gather: $(MEMFILL_AND_GATHER_OBJS) memfill_and_gather.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -ldml $(LIBS)
memfill_and_gather_clean:
	rm -f memfill_and_gather memfill_and_gather.o

DECOMP_AND_SCATTER_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/ch3_hash.o \
	obj/request_executors.o obj/runners.o obj/iaa_offloads.o \
	obj/dsa_offloads.o \
	obj/decompress_and_hash_request.o \
	obj/gather_scatter.o \
	obj/payload_gen.o \
	obj/decompress_and_scatter_request.o
decomp_and_scatter.o: decomp_and_scatter.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^ $(INCLUDES) -fpermissive
decomp_and_scatter: $(DECOMP_AND_SCATTER_OBJS) decomp_and_scatter.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -ldml $(LIBS)
decomp_and_scatter_clean:
	rm -f decomp_and_scatter decomp_and_scatter.o

scan_and_gather_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/ch3_hash.o \
	obj/request_executors.o obj/runners.o obj/iaa_offloads.o \
	obj/dsa_offloads.o \
	obj/decompress_and_hash_request.o
scan_and_gather.o: scan_and_gather.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^ $(INCLUDES) -fpermissive
scan_and_gather: $(MLP_OBJS) scan_and_gather.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -ldml $(LIBS)
scan_and_gather_clean:
	rm -f scan_and_gather scan_and_gather.o

DECOMP_AND_HASH_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/ch3_hash.o \
	obj/request_executors.o obj/runners.o obj/iaa_offloads.o \
	obj/payload_gen.o \
	obj/decompress_and_hash_request.o
decomp_and_hash.o: decomp_and_hash.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^ $(INCLUDES) -fpermissive
decomp_and_hash: $(DECOMP_AND_HASH_OBJS) decomp_and_hash.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -ldml $(LIBS)
decomp_and_hash_clean:
	rm -f decomp_and_hash decomp_and_hash.o obj/decompress_and_hash_request.o

DECOMP_AND_HASH_MT_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/ch3_hash.o \
	obj/request_executors.o obj/runners.o obj/iaa_offloads.o \
	obj/payload_gen.o \
	obj/decompress_and_hash_request.o
decomp_and_hash_multi_threaded.o: decomp_and_hash_multi_threaded.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^ $(INCLUDES) -fpermissive
decomp_and_hash_multi_threaded: $(DECOMP_AND_HASH_MT_OBJS) decomp_and_hash_multi_threaded.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -ldml $(LIBS)
decomp_and_hash_multi_threaded_clean:
	rm -f decomp_and_hash_multi_threaded decomp_and_hash_multi_threaded.o


TEST_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/ch3_hash.o \
	obj/request_executors.o obj/runners.o obj/iaa_offloads.o
tests: tests/iaa_compress_and_decompress_verify
tests/iaa_compress_and_decompress_verify.o: tests/iaa_compress_and_decompress_verify.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^ $(INCLUDES) -fpermissive
tests/iaa_compress_and_decompress_verify: tests/iaa_compress_and_decompress_verify.o $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f obj/*.o switch_to_fill switch_to_fill.o \
		walker walker.o \
		mlp mlp.o tests/*.o \
		decomp_and_hash decomp_and_hash.o \
		decomp_and_hash_multi_threaded \
		decomp_and_hash_multi_threaded.o \
		memfill_and_gather memfill_and_gather.o \
		decomp_and_scatter decomp_and_scatter.o \
		scan_and_gather scan_and_gather.o