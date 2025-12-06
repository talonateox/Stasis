gcc -m64 -nostdlib -static -fno-pie -no-pie -ffreestanding -Wl,-Ttext=0x400000 -o hello hello.c
xxd -i hello > ../../src/programs/hello_elf.h
rm hello
