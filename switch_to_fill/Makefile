all: switch_to_fill walker
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
	$(CXX) -g -c -o $@ $^  -I./inc `pkg-config --cflags --libs protobuf`

switch_to_fill.o: switch_to_fill.cpp
	$(CXX) -g -c -o $@ $^  -I./inc
SWITCH_TO_FILL_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o router.pb.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o obj/router_requests.o \
	obj/ch3_hash.o obj/request_executors.o obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o
switch_to_fill: $(SWITCH_TO_FILL_OBJS) switch_to_fill.o router.pb.o
	$(CXX) -g $(FCONTEXT_OBJ) -o $@ $^ `pkg-config --cflags --libs protobuf` \
	-ldml


walker.o: walker.cpp
	$(CXX) -g -c -o $@ $^  -I./inc
WALKER_OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o \
	obj/walker_requests.o obj/emul_ax.o obj/thread_utils.o  obj/offload.o \
	obj/context_management.o obj/posting_list.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o obj/test_harness.o \
	obj/request_executors.o
walker: $(WALKER_OBJS) walker.o
	$(CXX) -g -o $@ $^ -ldml

clean:
	rm -f obj/*.o switch_to_fill switch_to_fill.o walker walker.o