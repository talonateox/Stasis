#!/bin/bash
set -e

IMG=disks/disk0.img
SIZE=1024

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

sudo mkdir -p /mnt/esp/boot/limine /mnt/esp/EFI/BOOT
sudo cp bin/stasis /mnt/esp/boot/kImg
sudo cp limine.conf /mnt/esp/boot/limine/
sudo cp limine/BOOTX64.EFI /mnt/esp/EFI/BOOT/

echo "Hello from NVMe!" | sudo tee /mnt/root/TEST.TXT
sudo mkdir -p /mnt/root/TESTDIR

sudo umount /mnt/esp /mnt/root
sudo losetup -d $LOOP

./limine/limine bios-install $IMG

echo "Done!"