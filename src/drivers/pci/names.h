#pragma once

#include <stdint.h>

static const char* pci_get_vendor_name(uint16_t vendor_id) {
    switch(vendor_id) {
        case 0x8086: return "Intel";
        case 0x1969: return "Qualcomm Atheros";
        case 0x10B7: return "3Com";
        case 0x1106: return "VIA";
        case 0x1095: return "Silicon Image";
        case 0x11AB: return "Marvell";
        case 0x13F6: return "C-Media";
        case 0x1000: return "LSI Logic / Symbios Logic";
        case 0x15AD: return "VMware";
        case 0x1B36: return "Red Hat (QEMU)";
        case 0x5333: return "S3 Graphics";
        case 0x102B: return "Matrox";
        case 0x1013: return "Cirrus Logic";
        case 0x1274: return "Ensoniq";
        case 0x8139: return "Realtek";
        case 0x104C: return "Texas Instruments";
        case 0x1217: return "O2 Micro";
        case 0x197B: return "JMicron";
        case 0x1B21: return "ASMedia";
        case 0x1234: return "QEMU";
        default: return "Unknown";
    }
}

static const char* pci_get_class_name(uint8_t class_code) {
    switch(class_code) {
        case 0x00: return "Unclassified";
        case 0x01: return "Mass Storage Controller";
        case 0x02: return "Network Controller";
        case 0x03: return "Display Controller";
        case 0x04: return "Multimedia Controller";
        case 0x05: return "Memory Controller";
        case 0x06: return "Bridge Device";
        case 0x07: return "Communication Controller";
        case 0x08: return "Generic System Peripheral";
        case 0x09: return "Input Device Controller";
        case 0x0A: return "Docking Station";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial Bus Controller";
        case 0x0D: return "Wireless Controller";
        case 0x0E: return "Intelligent Controller";
        case 0x0F: return "Satellite Controller";
        case 0x10: return "Encryption Controller";
        case 0x11: return "Signal Processing Controller";
        default: return "Unknown";
    }
}
