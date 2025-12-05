#pragma once

#include <stdint.h>

typedef struct {
    char signature[8];          // 0-7: "RSD PTR "
    uint8_t checksum;           // 8: Checksum
    char oem_id[6];             // 9-14: OEM ID
    uint8_t revision;           // 15: Revision
    uint32_t rsdt_address;      // 16-19: RSDT physical address
    // ACPI 2.0+ fields below
    uint32_t length;            // 20-23: Length
    uint64_t xsdt_address;      // 24-31: XSDT physical address
    uint8_t extended_checksum;  // 32: Extended checksum
    uint8_t reserved[3];        // 33-35: Reserved
} __attribute__((packed)) rsdp2_t;

typedef struct {
    unsigned char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t oem_table_id[8];
    uint32_t oem_revisiion;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) sdt_header_t;

typedef struct {
    sdt_header_t header;
    uint64_t reserved;
} __attribute__((packed)) mcfg_header_t;

void acpi_init(rsdp2_t* rsdp, uint64_t offset);
sdt_header_t* acpi_find_table(rsdp2_t* rsdp, uint64_t offset, const char* signature);
