IAA_INCLUDES = -I/home/n869p538/spr-accel-profiling/interrupt_qat_dc/idxd-config/test \
	-I/home/n869p538/spr-accel-profiling/interrupt_qat_dc/idxd-config/test/algorithms
IDXD_INCLUDES = -I/home/n869p538/spr-accel-profiling/interrupt_qat_dc/idxd-config
ACCFG_INCLUDES = -I/home/n869p538/spr-accel-profiling/interrupt_qat_dc/idxd-config/accfg
IAA_LIBS = -L/home/n869p538/spr-accel-profiling/interrupt_qat_dc/idxd-config/test -liaa -lz -lcrypto
ACCFG_LIBS = -L/home/n869p538/spr-accel-profiling/interrupt_qat_dc/idxd-config/accfg/lib -laccel-config

INCLUDES = -I./inc $(IAA_INCLUDES) $(ACCFG_INCLUDES) $(IDXD_INCLUDES)
LIBS = -ldml $(IAA_LIBS) $(ACCFG_LIBS)

all: switch_to_fill walker mlp decomp_and_hash
obj/jump_x86_64_sysv_elf_gas.o: src/jump_x86_64_sysv_elf_gas.S
	$(CXX) -g -c -o $@ $^  -I./inc

obj/make_x86_64_sysv_elf_gas.o: src/make_x86_64_sysv_elf_gas.S
	$(CXX) -g -c -o $@ $^  -I./inc

obj/ontop_x86_64_sysv_elf_gas.o: src/ontop_x86_64_sysv_elf_gas.S
	$(CXX) -g -c -o $@ $^  -I./inc

obj/context_fast.o: src/context_fast.S
	$(CXX) -g -c -o $@ $^  -I./inc

router.pb.o: src/router.pb.cc
	$(CXX) -g -c -o $@ $^  -I./inc `pkg-config --cflags --libs protobuf`

obj/%.o: src/%.cpp
	$(CXX) -g -c -o $@ $^  $(INCLUDES) `pkg-config --cflags --libs protobuf` -fpermissive

switch_to_fill.o: switch_to_fill.cpp
	$(CXX) -g -c -o $@ $^  $(INCLUDES)
SWITCH_TO_FILL_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o router.pb.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o obj/router_requests.o \
	obj/ch3_hash.o obj/request_executors.o obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o
switch_to_fill: $(SWITCH_TO_FILL_OBJS) switch_to_fill.o router.pb.o
	$(CXX) -g $(FCONTEXT_OBJ) -o $@ $^ `pkg-config --cflags --libs protobuf` \
	-ldml


walker.o: walker.cpp
	$(CXX) -g -c -o $@ $^  -I./inc $(INCLUDES)
WALKER_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/walker_requests.o obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o obj/posting_list.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/request_executors.o obj/runners.o
walker: $(WALKER_OBJS) walker.o
	$(CXX) -g -o $@ $^ -ldml



MLP_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/ch3_hash.o \
	obj/request_executors.o obj/runners.o obj/iaa_offloads.o \
	obj/decompress_and_hash_request.o
REQUEST_OBJS = obj/walker.o
mlp.o: mlp.cpp
	$(CXX) -g -c -o $@ $^ $(INCLUDES) -fpermissive
mlp: $(MLP_OBJS) mlp.o
	$(CXX) -g -o $@ $^ -ldml $(LIBS)
mlp_clean:
	rm -f mlp mlp.o

DECOMP_AND_HASH_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/ch3_hash.o \
	obj/request_executors.o obj/runners.o obj/iaa_offloads.o \
	obj/decompress_and_hash_request.o
REQUEST_OBJS = obj/walker.o
decomp_and_hash.o: decomp_and_hash.cpp
	$(CXX) -g -c -o $@ $^ $(INCLUDES) -fpermissive
decomp_and_hash: $(DECOMP_AND_HASH_OBJS) decomp_and_hash.o
	$(CXX) -g -o $@ $^ -ldml $(LIBS)
decomp_and_hash_clean:
	rm -f decomp_and_hash decomp_and_hash.o

TEST_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/ch3_hash.o \
	obj/request_executors.o obj/runners.o obj/iaa_offloads.o \
	obj/decompress_and_hash_request.o
tests: tests/iaa_compress_and_decompress_verify
tests/iaa_compress_and_decompress_verify.o: tests/iaa_compress_and_decompress_verify.cpp
	$(CXX) -g -c -o $@ $^ $(INCLUDES) -fpermissive
tests/iaa_compress_and_decompress_verify: tests/iaa_compress_and_decompress_verify.o $(TEST_OBJS)
	$(CXX) -g -o $@ $^ $(LIBS)

clean:
	rm -f obj/*.o switch_to_fill switch_to_fill.o walker walker.o mlp.o tests/*.o decomp_and_hash