#pragma once

#include "../acpi/acpi.h"
#include "../driver.h"

typedef struct {
    uint16_t vendor;
    uint16_t device;
    uint16_t subvendor;
    uint16_t subdevice;
    uint32_t class_mask;
    void* driver_data;
} pci_device_id_t;

#define PCI_ANY_ID 0xFFFF

#define PCI_DEVICE(vend, dev)                                   \
    .vendor = (vend), .device = (dev), .subvendor = PCI_ANY_ID, \
    .subdevice = PCI_ANY_ID

typedef struct {
    uint16_t vendor;
    uint16_t device;
    uint16_t command;
    uint16_t status;
    uint8_t revision;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t _class;
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
} pci_device_header_t;

typedef struct {
    uint64_t base_address;
    uint16_t pci_seg_group;
    uint8_t start_bus;
    uint8_t end_bus;
    uint32_t reserved;
} __attribute__((packed)) pci_device_config_t;

typedef struct {
    device_t device;
    pci_device_header_t* header;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint64_t base_address;
    uint32_t bar[6];
    uint8_t irq;
} pci_device_t;

typedef struct {
    driver_t driver;

    const pci_device_id_t* id_table;

    int (*probe)(pci_device_t* pdev);
    void (*remove)(pci_device_t* pdev);
} pci_driver_t;

void pci_init(mcfg_header_t* mcfg);
int pci_driver_register(pci_driver_t* drv);
int pci_driver_unregister(pci_driver_t* drv);

uint32_t pci_read_config_dword(pci_device_t* dev, uint8_t offset);
uint16_t pci_read_config_word(pci_device_t* dev, uint8_t offset);
uint8_t pci_read_config_byte(pci_device_t* dev, uint8_t offset);

void pci_write_config_dword(pci_device_t* dev, uint8_t offset, uint32_t val);
void pci_write_config_word(pci_device_t* dev, uint8_t offset, uint16_t val);
void pci_write_config_byte(pci_device_t* dev, uint8_t offset, uint8_t val);

void* pci_map_bar(pci_device_t* dev, uint8_t bar_num);
void pci_enable_bus_mastering(pci_device_t* dev);
void pci_enable_mmio(pci_device_t* dev);

extern bus_type_t pci_bus_type;
