#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "drivers/acpi/acpi.h"
#include "limine.h"

#include "io/terminal.h"

#include "mem/memmap.h"
#include "mem/alloc/heap.h"
#include "mem/paging/paging.h"
#include "mem/alloc/page_frame_alloc.h"

#include "arch/x86_64/gdt/gdt.h"

#include "interrupts/interrupts.h"

#include "task/task.h"
#include "task/scheduler.h"

#include "drivers/pic/pic.h"
#include "drivers/timer/timer.h"
#include "drivers/keyboard/keyboard.h"

#include "programs/user_shell.h"
#include "usermode/usermode.h"

#include "syscall/syscall.h"
#include "usermode/usermode.h"

extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 4
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 4
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 4
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 4
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void hcf() {
    for(;;) {
        asm ("hlt");
    }
}

void kmain() {
    if(LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    if(framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    terminal_init(framebuffer);

    printkf("-------------------------------------\n");
    printkf("           STASIS KERNEL\n");
    printkf("-------------------------------------\n\n");

    if(memmap_request.response == NULL) panic("MEMMAP REQUEST INVALID!");
    if(hhdm_request.response == NULL) panic("HHDM REQUEST INVALID!");
    if(rsdp_request.response == NULL) panic("RSDP REQUEST INVALID");

    size_t offset = hhdm_request.response->offset;
    rsdp2_t* rsdp = (rsdp2_t*)rsdp_request.response->address;

    printkf_info("Loading GDT...\n");
    gdt_init();
    printkf_ok("Loaded GDT\n");

    memmap_init(memmap_request.response->entries, memmap_request.response->entry_count);

    // memmap_print();

    pfallocator_init(offset);

    printkf_info("FREE RAM: %k%llu%k\n", 0xcccc66, pfallocator_get_free_ram(), 0xffffff);
    printkf_info("USED RAM: %k%llu%k\n", 0xcccc66, pfallocator_get_used_ram(), 0xffffff);

    page_table_init(offset, (uint64_t)_kernel_start, (uint64_t)_kernel_end);

    interrupts_init();
    pic_remap();
    sti();

    heap_init((void*)0x0000100000000000, 0x10, offset);

    printkf_info("FREE RAM: %k%llu%k\n", 0xcccc66, pfallocator_get_free_ram(), 0xffffff);
    printkf_info("USED RAM: %k%llu%k\n", 0xcccc66, pfallocator_get_used_ram(), 0xffffff);

    acpi_init(rsdp, offset);

    timer_init(100);
    task_init();
    scheduler_init();
    timer_set_callback(scheduler_tick);

    keyboard_init();
    keyboard_pic_start();

    syscall_init();

    task_t* shell = task_create_user(user_shell, 16384);
    scheduler_add_task(shell);
    scheduler_enable();

    hcf();
}
