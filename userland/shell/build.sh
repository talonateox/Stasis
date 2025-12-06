
gcc -m64 -nostdlib -static -fno-pie -no-pie -ffreestanding -Wl,-Ttext=0x400000 -Wl,--build-id=none -o shell shell.c

xxd -i shell > ../../src/programs/shell_elf.h

rm shell
