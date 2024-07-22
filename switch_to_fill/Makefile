all: switch_to_fill
obj/jump_x86_64_sysv_elf_gas.o: src/jump_x86_64_sysv_elf_gas.S
	$(CXX) -g -c -o $@ $^  -I./inc

obj/make_x86_64_sysv_elf_gas.o: src/make_x86_64_sysv_elf_gas.S
	$(CXX) -g -c -o $@ $^  -I./inc

obj/ontop_x86_64_sysv_elf_gas.o: src/ontop_x86_64_sysv_elf_gas.S
	$(CXX) -g -c -o $@ $^  -I./inc

obj/context_fast.o: src/context_fast.S
	$(CXX) -g -c -o $@ $^  -I./inc

switch_to_fill.o: switch_to_fill.cpp
	$(CXX) -g -c -o $@ $^  -I./inc

router.pb.o: src/router.pb.cc
	$(CXX) -g -c -o $@ $^  -I./inc `pkg-config --cflags --libs protobuf`

obj/%.o: src/%.cpp
	$(CXX) -g -c -o $@ $^  -I./inc `pkg-config --cflags --libs protobuf`

OBJS = obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o \
	obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o router.pb.o \
	obj/emul_ax.o obj/thread_utils.o  obj/offload.o obj/router_requests.o \
	obj/ch3_hash.o obj/request_executors.o obj/context_management.o \
	obj/rps.o \
	switch_to_fill.o

switch_to_fill: $(OBJS)
	$(CXX) -g $(FCONTEXT_OBJ) -o $@ $^ `pkg-config --cflags --libs protobuf`

clean:
	rm -f obj/*.o switch_to_fill switch_to_fill.o