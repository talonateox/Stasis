#include "acpi.h"

#include <stdbool.h>
#include <stddef.h>

#include "../../io/terminal.h"
#include "../pci/pci.h"

static mcfg_header_t *_g_mcfg = NULL;

void acpi_init(rsdp2_t *rsdp, uint64_t offset) {
    printkf_info("Initializing ACPI...\n");

    mcfg_header_t *mcfg = (mcfg_header_t *)acpi_find_table(rsdp, offset, "MCFG");
    _g_mcfg = mcfg;

    printkf_ok("ACPI initialized\n");
}

sdt_header_t *acpi_find_table(rsdp2_t *rsdp, uint64_t offset, const char *signature) {
    if (rsdp->revision == 0) {
        uint32_t rsdt_phys = rsdp->rsdt_address;
        sdt_header_t *rsdt = (sdt_header_t *)((uint64_t)rsdt_phys + offset);

        const char *expected = "RSDT";
        for (int i = 0; i < 4; i++) {
            if (rsdt->signature[i] != expected[i]) {
                return NULL;
            }
        }

        size_t entry_count = (rsdt->length - sizeof(sdt_header_t)) / 4;

        for (size_t i = 0; i < entry_count; i++) {
            uint32_t phys_addr = *(uint32_t *)((uint64_t)rsdt + sizeof(sdt_header_t) + (i * 4));

            if (phys_addr == 0)
                continue;

            sdt_header_t *header = (sdt_header_t *)((uint64_t)phys_addr + offset);

            bool match = true;
            for (int c = 0; c < 4; c++) {
                if (header->signature[c] != signature[c]) {
                    match = false;
                    break;
                }
            }

            if (match)
                return header;
        }

    } else {
        sdt_header_t *xsdt = (sdt_header_t *)(rsdp->xsdt_address + offset);

        const char *expected = "XSDT";
        for (int i = 0; i < 4; i++) {
            if (xsdt->signature[i] != expected[i]) {
                return NULL;
            }
        }

        size_t entry_count = (xsdt->length - sizeof(sdt_header_t)) / 8;

        for (size_t i = 0; i < entry_count; i++) {
            uint64_t phys_addr = *(uint64_t *)((uint64_t)xsdt + sizeof(sdt_header_t) + (i * 8));

            if (phys_addr == 0)
                continue;

            sdt_header_t *header = (sdt_header_t *)(phys_addr + offset);

            bool match = true;
            for (int c = 0; c < 4; c++) {
                if (header->signature[c] != signature[c]) {
                    match = false;
                    break;
                }
            }

            if (match)
                return header;
        }
    }

    return NULL;
}

mcfg_header_t *acpi_get_mcfg() {
    return _g_mcfg;
}
