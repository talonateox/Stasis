#include "memmap.h"

#include "../io/terminal.h"

memmap_t _g_memmap = {0};

static const char *entry_names[LIMINE_MEMMAP_ACPI_TABLES + 1] = {
    "USABLE",
    "RESERVED",
    "ACPI_RECLAIMABLE",
    "ACPI_NVS",
    "BAD_MEMORY",
    "BOOTLOADER_RECLAIMABLE",
    "EXECUTABLE_AND_MODULES",
    "FRAMEBUFFER",
    "ACPI_TABLES",
};

void memmap_init(struct limine_memmap_entry **entries, size_t entry_count) {
    printkf_info("Initializing memmap...\n");
    _g_memmap.entries = entries;
    _g_memmap.entry_count = entry_count;
    printkf_ok("Initialized memmap with %k%d%r entries\n", 0xcccc66, entry_count);
}

size_t memmap_get_entry_count() {
    return _g_memmap.entry_count;
}

struct limine_memmap_entry *memmap_get_entry(int i) {
    return _g_memmap.entries[i];
}

size_t memmap_get_total() {
    static size_t memmap_total_bytes = 0;
    if (memmap_total_bytes > 0)
        return memmap_total_bytes;

    for (size_t i = 0; i < memmap_get_entry_count(); i++) {
        memmap_total_bytes += memmap_get_entry(i)->length;
    }

    return memmap_total_bytes;
}

void memmap_print() {
    for (size_t i = 0; i < memmap_get_entry_count(); i++) {
        struct limine_memmap_entry *entry = memmap_get_entry(i);
        printkf("%s: %k%p %k%llu%r\n", entry_names[entry->type], 0x55aaff, entry->base, 0xcccc66, entry->length);
    }

    printkf("TOTAL: %k%d%r\n", 0xcccc66, memmap_get_total());
}
