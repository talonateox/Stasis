gcc -m64 -nostdlib -static -fno-pie -no-pie -ffreestanding -Wl,-Ttext=0x400000 -Wl,--build-id=none -o ../../src/programs/shell.elf shell.c
