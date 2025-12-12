#include "pci.h"

#include "../../io/terminal.h"
#include "../../mem/alloc/heap.h"
#include "../../mem/paging/paging.h"
#include "../../std/string.h"
#include "names.h"

static int pci_bus_match(device_t* dev, driver_t* drv);
static int pci_bus_probe(device_t* dev);

bus_type_t pci_bus_type = {
    .name = "pci",
    .match = pci_bus_match,
    .probe = pci_bus_probe,
};

static int pci_bus_match(device_t* dev, driver_t* drv) {
    pci_device_t* pdev = (pci_device_t*)dev;
    pci_driver_t* pdrv = (pci_driver_t*)drv;

    if (!pdrv->id_table) return -1;

    const pci_device_id_t* id = pdrv->id_table;
    while (id->vendor || id->device || id->class) {
        uint32_t dev_class = (pdev->header->_class << 16) |
                             (pdev->header->subclass << 8) |
                             pdev->header->prog_if;

        if (id->class) {
            uint32_t mask = id->class_mask ? id->class_mask : 0xFFFFFF;
            if ((dev_class & mask) == (id->class & mask)) {
                return 0;
            }
        } else if ((id->vendor == PCI_ANY_ID ||
                    id->vendor == pdev->header->vendor) &&
                   (id->device == PCI_ANY_ID ||
                    id->device == pdev->header->device)) {
            return 0;
        }

        id++;
    }

    return -1;
}

static int pci_bus_probe(device_t* dev) {
    pci_device_t* pdev = (pci_device_t*)dev;
    pci_driver_t* pdrv = (pci_driver_t*)dev->driver;

    if (pdrv && pdrv->probe) {
        return pdrv->probe(pdev);
    }

    return -1;
}

static pci_device_t* pci_create_device(uint64_t func_address, uint8_t bus,
                                       uint8_t slot, uint8_t func) {
    pci_device_header_t* header = (pci_device_header_t*)func_address;

    if (header->device == 0 || header->device == 0xffff) {
        return NULL;
    }

    pci_device_t* pdev = (pci_device_t*)malloc(sizeof(pci_device_t));
    if (!pdev) return NULL;

    memset(pdev, 0, sizeof(pci_device_t));

    snprintf(pdev->device.name, sizeof(pdev->device.name), "pci:%02x:%02x.%x",
             bus, slot, func);
    pdev->device.type = DEVICE_TYPE_PCI;
    pdev->device.bus = &pci_bus_type;

    pdev->header = header;
    pdev->bus = bus;
    pdev->slot = slot;
    pdev->func = func;
    pdev->base_address = func_address;

    return pdev;
}

static void func_enumerate(uint64_t dev_address, uint64_t bus, uint64_t dev,
                           uint64_t func) {
    uint64_t offset = func << 12;
    uint64_t func_address = dev_address + offset;
    page_map_memory((void*)func_address, (void*)func_address);

    pci_device_t* pdev = pci_create_device(func_address, bus, dev, func);
    if (!pdev) return;

    const char* vendor_name = pci_get_vendor_name(pdev->header->vendor);
    const char* class_name = pci_get_class_name(pdev->header->_class);

    printkf_info("%x/%x:%x - %s %s (%x:%x) [%02x:%02x:%02x]\n", bus, dev, func,
                 vendor_name, class_name, pdev->header->vendor,
                 pdev->header->device, pdev->header->_class,
                 pdev->header->subclass, pdev->header->prog_if);

    device_register(&pdev->device);
}

static void dev_enumerate(uint64_t bus_address, uint64_t bus, uint64_t dev) {
    uint64_t offset = dev << 15;
    uint64_t dev_address = bus_address + offset;
    page_map_memory((void*)dev_address, (void*)dev_address);

    pci_device_header_t* header = (pci_device_header_t*)dev_address;
    if (header->device == 0 || header->device == 0xffff) return;

    bool is_multifunction = (header->header_type & 0x80) != 0;

    func_enumerate(dev_address, bus, dev, 0);

    if (is_multifunction) {
        for (uint64_t func = 1; func < 8; func++) {
            func_enumerate(dev_address, bus, dev, func);
        }
    }
}

static void bus_enumerate(uint64_t base_address, uint64_t bus) {
    uint64_t offset = bus << 20;
    uint64_t bus_address = base_address + offset;
    page_map_memory((void*)bus_address, (void*)bus_address);

    for (uint64_t dev = 0; dev < 32; dev++) {
        dev_enumerate(bus_address, bus, dev);
    }
}

void pci_init(mcfg_header_t* mcfg) {
    bus_register(&pci_bus_type);

    printkf_info("Enumerating PCI devices...\n");

    int entries = (mcfg->header.length - sizeof(mcfg_header_t)) /
                  sizeof(pci_device_config_t);

    for (int d = 0; d < entries; d++) {
        pci_device_config_t* dev =
            (pci_device_config_t*)((uint64_t)mcfg + sizeof(mcfg_header_t) +
                                   (sizeof(pci_device_config_t) * d));
        for (uint64_t bus = dev->start_bus; bus < dev->end_bus; bus++) {
            bus_enumerate(dev->base_address, bus);
        }
    }
}

int pci_driver_register(pci_driver_t* drv) {
    if (!drv) return -1;

    drv->driver.bus = &pci_bus_type;
    drv->driver.probe = &pci_bus_probe;

    return driver_register(&drv->driver);
}

int pci_driver_unregister(pci_driver_t* drv) {
    if (!drv) return -1;

    return driver_unregister(&drv->driver);
}

uint32_t pci_read_config_dword(pci_device_t* dev, uint8_t offset) {
    return *(volatile uint32_t*)(dev->base_address + offset);
}

uint16_t pci_read_config_word(pci_device_t* dev, uint8_t offset) {
    return *(volatile uint16_t*)(dev->base_address + offset);
}

uint8_t pci_read_config_byte(pci_device_t* dev, uint8_t offset) {
    return *(volatile uint8_t*)(dev->base_address + offset);
}

void pci_write_config_dword(pci_device_t* dev, uint8_t offset, uint32_t val) {
    *(volatile uint32_t*)(dev->base_address + offset) = val;
}

void pci_write_config_word(pci_device_t* dev, uint8_t offset, uint16_t val) {
    *(volatile uint16_t*)(dev->base_address + offset) = val;
}

void pci_write_config_byte(pci_device_t* dev, uint8_t offset, uint8_t val) {
    *(volatile uint8_t*)(dev->base_address + offset) = val;
}

void* pci_map_bar(pci_device_t* dev, uint8_t bar_num) {
    if (bar_num >= 6) return NULL;

    uint32_t bar = pci_read_config_dword(dev, 0x10 + (bar_num * 4));

    if (bar & 1) {
        return NULL;
    }

    uint64_t addr = bar & ~0xF;

    if ((bar & 0x6) == 0x4) {
        uint32_t bar_high =
            pci_read_config_dword(dev, 0x10 + ((bar_num + 1) * 4));
        addr |= ((uint64_t)bar_high << 32);
    }

    for (int i = 0; i < 8; i++) {
        page_map_memory((void*)(addr + i * 0x1000), (void*)(addr + i * 0x1000));
    }

    return (void*)addr;
}

void pci_enable_bus_mastering(pci_device_t* dev) {
    uint16_t command = pci_read_config_word(dev, 0x04);
    command |= (1 << 2);
    pci_write_config_word(dev, 0x04, command);
}

void pci_enable_mmio(pci_device_t* dev) {
    uint16_t command = pci_read_config_word(dev, 0x04);
    command |= (1 << 1);
    pci_write_config_word(dev, 0x04, command);
}