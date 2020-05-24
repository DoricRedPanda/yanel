yanel.o: yanel.inc
	nasm -f elf64 $< -o $@ -dMODULE
test-c: test-c.o yanel.o
	gcc test-c.o yanel.o -o test-c -fno-pie -no-pie -m64
test-c.o: test-c.c yanel.h
	gcc test-c.c -c -m64
test-nasm: test-nasm.o
	ld --dynamic-linker /lib64/ld-linux-x86-64.so.2 test-nasm.o -m elf_x86_64 -o test-nasm
test-nasm.o: test-nasm.asm yanel.inc
	nasm -O0 -g -f elf64 -F dwarf $< -l $(patsubst %.o, %.lst, $@) -o $@
