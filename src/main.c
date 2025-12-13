#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/gdt/gdt.h"
#include "drivers/acpi/acpi.h"
#include "drivers/driver.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/nvme/nvme.h"
#include "drivers/pci/pci.h"
#include "drivers/pic/pic.h"
#include "drivers/timer/timer.h"
#include "elf/elf.h"
#include "fs/mount/mount.h"
#include "fs/tmpfs/tmpfs.h"
#include "fs/vfs/vfs.h"
#include "interrupts/interrupts.h"
#include "io/terminal.h"
#include "limine.h"
#include "mem/alloc/heap.h"
#include "mem/alloc/page_frame_alloc.h"
#include "mem/memmap.h"
#include "mem/paging/paging.h"
#include "std/string.h"
#include "syscall/syscall.h"
#include "task/scheduler.h"
#include "task/task.h"
#include "usermode/usermode.h"

extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

__attribute__((used, section(".limine_requests"))) static volatile uint64_t limine_base_revision[] =
    LIMINE_BASE_REVISION(4);

__attribute__((used,
               section(".limine_requests"))) static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 4};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 4};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID, .revision = 4};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID, .revision = 4};

__attribute__((used, section(".limine_requests_start"))) static volatile uint64_t limine_requests_start_marker[] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t limine_requests_end_marker[] =
    LIMINE_REQUESTS_END_MARKER;

static void hcf() {
    for (;;) {
        __asm__("hlt");
    }
}

void create_program(const char *path, const unsigned char *elf, unsigned int len) {
    int fd = vfs_open(path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        printkf_error("create_program(): Failed to create '%s'\n", path);
        return;
    }

    vfs_write(fd, elf, len);
    vfs_close(fd);
}

void create_programs() {
    uint8_t shell[] = {
#embed "programs/shell.elf"
    };

    uint8_t hello[] = {
#embed "programs/hello.elf"
    };

    create_program("/system/cmd/sh", shell, sizeof(shell));
    create_program("/hello", hello, sizeof(hello));
}

void setup_fs() {
    vfs_mkdir("/system");
    vfs_mkdir("/system/cmd");
}

void kmain() {
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    terminal_init(framebuffer);

    printkf("-------------------------------------\n");
    printkf("           STASIS KERNEL\n");
    printkf("-------------------------------------\n\n");

    if (memmap_request.response == NULL)
        panic("MEMMAP REQUEST INVALID!");
    if (hhdm_request.response == NULL)
        panic("HHDM REQUEST INVALID!");
    if (rsdp_request.response == NULL)
        panic("RSDP REQUEST INVALID");

    size_t offset = hhdm_request.response->offset;
    rsdp2_t *rsdp = (rsdp2_t *)rsdp_request.response->address;

    printkf_info("Loading GDT...\n");
    gdt_init();
    printkf_ok("Loaded GDT\n");

    memmap_init(memmap_request.response->entries, memmap_request.response->entry_count);

    pfallocator_init(offset);

    printkf_info("FREE RAM: %k%llu%k\n", 0xcccc66, pfallocator_get_free_ram(), 0xffffff);
    printkf_info("USED RAM: %k%llu%k\n", 0xcccc66, pfallocator_get_used_ram(), 0xffffff);

    page_table_init(offset, (uint64_t)_kernel_start, (uint64_t)_kernel_end);

    interrupts_init();
    pic_remap();
    sti();

    heap_init((void *)0xFFFF900000000000, 0x10, offset);

    vfs_init();
    mount_init();
    tmpfs_init();

    setup_fs();

    create_programs();

    driver_manager_init();
    acpi_init(rsdp, offset);

    pci_init(acpi_get_mcfg());
    nvme_driver_init();

    printkf_info("FREE RAM: %k%llu%k\n", 0xcccc66, pfallocator_get_free_ram(), 0xffffff);
    printkf_info("USED RAM: %k%llu%k\n", 0xcccc66, pfallocator_get_used_ram(), 0xffffff);

    timer_init(100);
    task_init();
    scheduler_init();
    timer_set_callback(scheduler_tick);

    keyboard_init();
    keyboard_pic_start();

    syscall_init();

    printkf_info("Starting /system/cmd/sh\n");
    task_t *init = task_create_elf("/system/cmd/sh", 16384);
    if (init == NULL) {
        printkf_error("init(): failed to create init shell\n");
        hcf();
    }

    int result = mount("/dev/nvme0p1", "/mnt", "fat32");
    if (result < 0) {
        printkf_error("main(): Failed to mount FAT32\n");
    }
    scheduler_add_task(init);
    scheduler_enable();

    hcf();
}
