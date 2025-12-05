#include "pci.h"

#include "../../mem/paging/paging.h"
#include "../../io/terminal.h"
#include "names.h"

void func_enumerate(uint64_t dev_address, uint64_t bus, uint64_t dev, uint64_t func) {
    uint64_t offset = func << 12;
    uint64_t func_address = dev_address + offset;
    page_map_memory((void*)func_address, (void*)func_address);

    pci_device_header_t* header = (pci_device_header_t*)func_address;
    if(header->device == 0) return;
    if(header->device == 0xffff) return;
    const char* vendor_name = pci_get_vendor_name(header->vendor);
    const char* class_name = pci_get_class_name(header->_class);

    printkf("%x/%x:%x - %s %s (%x:%x)\n", bus, dev, func, vendor_name, class_name, header->vendor, header->device);
}

void dev_enumerate(uint64_t bus_address, uint64_t bus, uint64_t dev) {
    uint64_t offset = dev << 15;
    uint64_t dev_address = bus_address + offset;
    page_map_memory((void*)dev_address, (void*)dev_address);

    pci_device_header_t* header = (pci_device_header_t*)dev_address;
    if(header->device == 0) return;
    if(header->device == 0xffff) return;

    for(uint64_t func = 0; func < 8; func++) {
        func_enumerate(dev_address, bus, dev, func);
    }
}

void bus_enumerate(uint64_t base_address, uint64_t bus) {
    uint64_t offset = bus << 20;
    uint64_t bus_address = base_address + offset;
    page_map_memory((void*)bus_address, (void*)bus_address);

    pci_device_header_t* header = (pci_device_header_t*)bus_address;
    if(header->device == 0) return;
    if(header->device == 0xffff) return;

    for(uint64_t dev = 0; dev < 32; dev++) {
        dev_enumerate(bus_address, bus, dev);
    }
}

void pci_enumerate(mcfg_header_t* mcfg) {
    int entries = (mcfg->header.length - sizeof(mcfg_header_t)) / sizeof(pci_device_config_t);
    for(int d = 0; d < entries; d++) {
        pci_device_config_t* dev = (pci_device_config_t*)((uint64_t)mcfg + sizeof(mcfg_header_t) + (sizeof(pci_device_config_t) * d));
        for(uint64_t bus = dev->start_bus; bus < dev->end_bus; bus++) {
            bus_enumerate(dev->base_address, bus);
        }
    }
}
