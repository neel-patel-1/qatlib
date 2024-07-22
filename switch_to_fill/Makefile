all: switch_to_fill
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

emul_ax_router_request_breakdown.o: emul_ax_router_request_breakdown.cpp
	$(CXX) -g -c -o $@ $^  -I./inc

OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o router.pb.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o obj/router_requests.o \
	obj/ch3_hash.o obj/request_executors.o obj/context_management.o \
	obj/rps.o obj/dsa_alloc.o obj/stats.o


switch_to_fill: $(OBJS) switch_to_fill.o router.pb.o
	$(CXX) -g $(FCONTEXT_OBJ) -o $@ $^ `pkg-config --cflags --libs protobuf` \
	-ldml

emul_ax_router_request_breakdown: $(OBJS) emul_ax_router_request_breakdown.o router.pb.o
	$(CXX) -g -o $@ $^ `pkg-config --cflags --libs protobuf`

clean:
	rm -f obj/*.o switch_to_fill switch_to_fill.o