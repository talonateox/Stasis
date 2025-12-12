#include "nvme.h"

#include "../../io/terminal.h"
#include "../../mem/alloc/heap.h"
#include "../pci/pci.h"

static const pci_device_id_t nvme_ids[] = {
    {PCI_DEVICE(0x1b36, 0x10)},
    // {.vendor = PCI_ANY_ID, .device = PCI_ANY_ID, .class_mask = 0x010802},
    {0},
};

static int nvme_probe(pci_device_t* pdev) {
    printkf_ok("NVMe driver probing device %s\n", pdev->device);

    nvme_ctrl_t* ctrl = (nvme_ctrl_t*)malloc(sizeof(nvme_ctrl_t));
    if (!ctrl) {
        printkf_error("Failed to allocate NVMe controller\n");
        return -1;
    }

    device_set_driver_data(&pdev->device, ctrl);

    pci_enable_mmio(pdev);
    pci_enable_bus_mastering(pdev);

    ctrl->mmio_base = pci_map_bar(pdev, 0);
    if (!ctrl->mmio_base) {
        printkf_error("Failed to map NVMe BAR0\n");
        free(ctrl);
        return -1;
    }

    printkf_ok("NVMe controller at %p\n", ctrl->mmio_base);

    return 0;
}

static void nvme_remove(pci_device_t* pdev) {
    printkf_info("NVMe driver removing device %s\n", pdev->device.name);

    nvme_ctrl_t* ctrl = device_get_driver_data(&pdev->device);
    if (ctrl) {
        free(ctrl);
    }
}

static pci_driver_t nvme_driver = {
    .driver = {.name = "nvme"},
    .id_table = nvme_ids,
    .probe = nvme_probe,
    .remove = nvme_remove,
};

void nvme_driver_init() {
    printkf_info("Registering NVMe driver\n");
    pci_driver_register(&nvme_driver);
}