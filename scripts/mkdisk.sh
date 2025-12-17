#!/bin/bash
set -e

IMG=disks/disk0.img
SIZE=1024

mkdir -p bin/userland

gcc -m64 -nostdlib -static -fno-pie -no-pie -ffreestanding \
    -Wl,-Ttext=0x400000 -Wl,--build-id=none \
    -o bin/userland/sh userland/shell/shell.c

gcc -m64 -nostdlib -static -fno-pie -no-pie -ffreestanding \
    -Wl,-Ttext=0x400000 -Wl,--build-id=none \
    -o bin/userland/hello userland/hello/hello.c

truncate -s ${SIZE}M $IMG 

parted -s $IMG mklabel gpt
parted -s $IMG mkpart bios_boot 1MiB 2MiB
parted -s $IMG set 1 bios_grub on
parted -s $IMG mkpart ESP fat32 2MiB 100MiB
parted -s $IMG set 2 esp on  
parted -s $IMG mkpart root fat32 100MiB 100%

LOOP=$(sudo losetup --show -fP $IMG)

sudo mkfs.fat -F32 ${LOOP}p2
sudo mkfs.fat -F32 ${LOOP}p3

sudo mkdir -p /mnt/esp /mnt/root
sudo mount ${LOOP}p2 /mnt/esp
sudo mount ${LOOP}p3 /mnt/root

sudo mkdir -p /mnt/esp/limine /mnt/esp/EFI/BOOT
sudo cp bin/stasis /mnt/esp/kern
sudo cp limine.conf /mnt/esp/limine/
sudo cp limine/BOOTX64.EFI /mnt/esp/EFI/BOOT/

sudo mkdir -p /mnt/root/system/cmd
sudo cp bin/userland/sh /mnt/root/system/cmd/sh
sudo cp bin/userland/hello /mnt/root/system/cmd/hello


sudo umount /mnt/esp /mnt/root
sudo losetup -d $LOOP

./limine/limine bios-install $IMG

echo "Done!"