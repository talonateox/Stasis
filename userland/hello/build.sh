gcc -m64 -nostdlib -static -fno-pie -no-pie -ffreestanding -Wl,-Ttext=0x500000 -o ../../src/programs/hello.elf hello.c
