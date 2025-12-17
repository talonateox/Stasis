#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "xhci.h"

typedef struct {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
} __attribute__((packed)) usb_kbd_report_t;

#define USB_KBD_MOD_LCTRL (1 << 0)
#define USB_KBD_MOD_LSHIFT (1 << 1)
#define USB_KBD_MOD_LALT (1 << 2)
#define USB_KBD_MOD_LGUI (1 << 3)
#define USB_KBD_MOD_RCTRL (1 << 4)
#define USB_KBD_MOD_RSHIFT (1 << 5)
#define USB_KBD_MOD_RALT (1 << 6)
#define USB_KBD_MOD_RGUI (1 << 7)

typedef struct usb_keyboard {
    xhci_controller_t *xhci;
    xhci_device_t *dev;

    uint8_t interface;
    uint8_t endpoint;
    uint16_t max_packet_size;
    uint8_t interval;

    xhci_ring_t *int_ring;
    void *report_buffer;
    uint64_t report_buffer_phys;

    usb_kbd_report_t last_report;

    struct usb_keyboard *next;
} usb_keyboard_t;

int usb_keyboard_probe(xhci_controller_t *xhci, xhci_device_t *dev);

void usb_keyboard_poll(void);

void usb_keyboard_task();