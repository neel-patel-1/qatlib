obj/jump_x86_64_sysv_elf_gas.o: src/jump_x86_64_sysv_elf_gas.S
	$(CXX) -g -c -o $@ $^ -I ../include -I./inc

obj/make_x86_64_sysv_elf_gas.o: src/make_x86_64_sysv_elf_gas.S
	$(CXX) -g -c -o $@ $^ -I ../include -I./inc

obj/ontop_x86_64_sysv_elf_gas.o: src/ontop_x86_64_sysv_elf_gas.S
	$(CXX) -g -c -o $@ $^ -I ../include -I./inc

obj/context_fast.o: src/context_fast.S
	$(CXX) -g -c -o $@ $^ -I ../include -I./inc

switch_to_fill.o: switch_to_fill.cpp
	$(CXX) -g -c -o $@ $^ -I ../include -I./inc

switch_to_fill: switch_to_fill.o obj/jump_x86_64_sysv_elf_gas.o obj/make_x86_64_sysv_elf_gas.o obj/ontop_x86_64_sysv_elf_gas.o obj/context_fast.o
	$(CXX) -g $(FCONTEXT_OBJ) -o $@ $^

clean:
	rm -f switch_to_fill switch_to_fill.o obj/jump_x86_64_sysv_elf_gas.o