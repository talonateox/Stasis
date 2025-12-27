#include "keyboard.h"
#include "../keyboard/keyboard.h"

#include "../../io/terminal.h"
#include "../../mem/alloc/heap.h"
#include "../../mem/alloc/page_frame_alloc.h"
#include "../../mem/paging/paging.h"
#include "../../std/string.h"
#include "../../task/scheduler.h"

static usb_keyboard_t *keyboards = NULL;

static const char hid_to_ascii_lower[128] = {
    0,    0,    0,    0,    'a', 'b', 'c', 'd', 'e',  'f',  'g', 'h', 'i',  'j', 'k', 'l', 'm', 'n', 'o', 'p',
    'q',  'r',  's',  't',  'u', 'v', 'w', 'x', 'y',  'z',  '1', '2', '3',  '4', '5', '6', '7', '8', '9', '0',
    '\n', 0x1B, '\b', '\t', ' ', '-', '=', '[', ']',  '\\', '#', ';', '\'', '`', ',', '.', '/', 0,   0,   0,
    0,    0,    0,    0,    0,   0,   0,   0,   0,    0,    0,   0,   0,    0,   0,   0,   0,   0,   0,   0,
    0,    0,    0,    0,    '/', '*', '-', '+', '\n', '1',  '2', '3', '4',  '5', '6', '7', '8', '9', '0', '.',
};

static const char hid_to_ascii_upper[128] = {
    0,    0,    0,    0,    'A', 'B', 'C', 'D', 'E',  'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q',  'R',  'S',  'T',  'U', 'V', 'W', 'X', 'Y',  'Z', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
    '\n', 0x1B, '\b', '\t', ' ', '_', '+', '{', '}',  '|', '~', ':', '"', '~', '<', '>', '?', 0,   0,   0,
    0,    0,    0,    0,    0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,    0,    0,    '/', '*', '-', '+', '\n', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '.',
};

static void usb_keyboard_process_report(usb_keyboard_t *kbd, usb_kbd_report_t *report) {
    bool shift = (report->modifiers & (USB_KBD_MOD_LSHIFT | USB_KBD_MOD_RSHIFT)) != 0;

    for (int i = 0; i < 6; i++) {
        uint8_t key = report->keys[i];
        if (key == 0)
            continue;

        bool was_pressed = false;
        for (int j = 0; j < 6; j++) {
            if (kbd->last_report.keys[j] == key) {
                was_pressed = true;
                break;
            }
        }

        if (!was_pressed && key < 128) {
            char c = shift ? hid_to_ascii_upper[key] : hid_to_ascii_lower[key];
            if (c != 0) {
                keyboard_buffer_put(c);
            }
        }
    }

    memcpy(&kbd->last_report, report, sizeof(usb_kbd_report_t));
}

static int usb_keyboard_set_protocol(usb_keyboard_t *kbd) {
    return xhci_control_transfer(kbd->xhci, kbd->dev, 0x21, 0x0B, 0, kbd->interface, NULL, 0);
}

static int usb_keyboard_set_idle(usb_keyboard_t *kbd) {
    return xhci_control_transfer(kbd->xhci, kbd->dev, 0x21, 0x0A, 0, kbd->interface, NULL, 0);
}

static int usb_keyboard_configure_endpoint(usb_keyboard_t *kbd) {
    xhci_device_t *dev = kbd->dev;
    xhci_controller_t *xhci = kbd->xhci;

    uint8_t ep_index = (kbd->endpoint & 0x0F) * 2 + 1;

    kbd->int_ring = (xhci_ring_t *)malloc(sizeof(xhci_ring_t));
    if (xhci_ring_init(kbd->int_ring, 64) < 0) {
        return -1;
    }

    kbd->report_buffer = pfallocator_request_page();
    if (!kbd->report_buffer) {
        return -1;
    }
    memset(kbd->report_buffer, 0, 4096);
    kbd->report_buffer_phys = virt_to_phys(kbd->report_buffer);

    memset(dev->input_ctx, 0, 4096);

    dev->input_ctx->ctrl.add_flags = (1 << 0) | (1 << ep_index);

    memcpy(&dev->input_ctx->slot, &dev->output_ctx->slot, sizeof(xhci_slot_ctx_t));

    uint32_t ctx_entries = ep_index;
    dev->input_ctx->slot.dw0 = (dev->input_ctx->slot.dw0 & 0x07FFFFFF) | (ctx_entries << 27);

    uint8_t ep_type = 7;
    uint8_t interval = kbd->interval;

    if (dev->speed < XHCI_SPEED_SUPER) {
        uint8_t xhci_interval = 0;
        while ((1 << xhci_interval) < interval && xhci_interval < 15) {
            xhci_interval++;
        }
        interval = xhci_interval + 3;
    }

    xhci_ep_ctx_t *ep_ctx = &dev->input_ctx->ep[ep_index - 1];
    ep_ctx->dw0 = (interval << 16);
    ep_ctx->dw1 = (kbd->max_packet_size << 16) | (ep_type << 3) | (3 << 1);
    ep_ctx->tr_dequeue = kbd->int_ring->phys | 1;
    ep_ctx->dw4 = 8;

    dev->ep_rings[ep_index - 1] = kbd->int_ring;

    if (xhci_configure_endpoint(xhci, dev->slot_id, dev->input_ctx_phys) < 0) {
        printkf_error("usb_keyboard_confugure_endpoint(): Failed\n");
        return -1;
    }

    return 0;
}

static void usb_keyboard_queue_transfer(usb_keyboard_t *kbd) {
    xhci_ring_t *ring = kbd->int_ring;
    
    xhci_trb_t *dest = &ring->trbs[ring->enqueue];
    
    dest->parameter = kbd->report_buffer_phys;
    dest->status = kbd->max_packet_size;
    dest->control = (TRB_TYPE_NORMAL << TRB_TYPE_SHIFT) | TRB_IOC;
    
    if (ring->cycle) {
        dest->control |= TRB_CYCLE;
    } else {
        dest->control &= ~TRB_CYCLE;
    }
    
    __asm__ volatile("mfence" ::: "memory");
    
    ring->enqueue++;
    
    if (ring->enqueue >= ring->size - 1) {
        xhci_trb_t *link = &ring->trbs[ring->size - 1];
        link->control = (TRB_TYPE_LINK << TRB_TYPE_SHIFT) | TRB_TC;
        if (ring->cycle) {
            link->control |= TRB_CYCLE;
        }
        
        __asm__ volatile("mfence" ::: "memory");
        
        ring->enqueue = 0;
        ring->cycle = !ring->cycle;
    }

    uint8_t ep_index = (kbd->endpoint & 0x0F) * 2 + 1;
    xhci_ring_doorbell(kbd->xhci, kbd->dev->slot_id, ep_index);
}

int usb_keyboard_probe(xhci_controller_t *xhci, xhci_device_t *dev) {
    uint8_t *config_buf = (uint8_t *)pfallocator_request_page();
    memset(config_buf, 0, 4096);

    if (xhci_control_transfer(xhci, dev, 0x80, USB_REQ_GET_DESCRIPTOR, (USB_DESC_CONFIGURATION << 8), 0, config_buf,
                              9) < 0) {
        pfallocator_free_page(config_buf);
        return -1;
    }

    usb_config_desc_t *config = (usb_config_desc_t *)config_buf;
    uint16_t total_length = config->total_length;

    if (xhci_control_transfer(xhci, dev, 0x80, USB_REQ_GET_DESCRIPTOR, (USB_DESC_CONFIGURATION << 8), 0, config_buf,
                              total_length) < 0) {
        pfallocator_free_page(config_buf);
        return -1;
    }

    uint8_t *ptr = config_buf + config->length;
    uint8_t *end = config_buf + total_length;

    usb_interface_desc_t *kbd_interface = NULL;
    usb_endpoint_desc_t *kbd_endpoint = NULL;

    while (ptr < end) {
        uint8_t len = ptr[0];
        uint8_t type = ptr[1];

        if (len == 0)
            break;

        if (type == USB_DESC_INTERFACE) {
            usb_interface_desc_t *iface = (usb_interface_desc_t *)ptr;

            if (iface->interface_class == USB_CLASS_HID && iface->interface_subclass == USB_HID_SUBCLASS_BOOT &&
                iface->interface_protocol == USB_HID_PROTOCOL_KEYBOARD) {
                kbd_interface = iface;
            }
        } else if (type == USB_DESC_ENDPOINT && kbd_interface) {
            usb_endpoint_desc_t *ep = (usb_endpoint_desc_t *)ptr;

            if ((ep->endpoint_address & 0x80) && (ep->attributes & 0x03) == 0x03) {
                kbd_endpoint = ep;
                break;
            }
        }

        ptr += len;
    }

    if (!kbd_interface || !kbd_endpoint) {
        pfallocator_free_page(config_buf);
        return -1;
    }

    if (xhci_control_transfer(xhci, dev, 0x00, USB_REQ_SET_CONFIGURATION, config->config_value, 0, NULL, 0) < 0) {
        printkf_error("usb_keyboard_probe(): SET_CONFIGURATION failed\n");
        pfallocator_free_page(config_buf);
        return -1;
    }

    usb_keyboard_t *kbd = (usb_keyboard_t *)malloc(sizeof(usb_keyboard_t));
    if (!kbd) {
        pfallocator_free_page(config_buf);
        return -1;
    }
    memset(kbd, 0, sizeof(usb_keyboard_t));

    kbd->xhci = xhci;
    kbd->dev = dev;
    kbd->interface = kbd_interface->interface_number;
    kbd->endpoint = kbd_endpoint->endpoint_address;
    kbd->max_packet_size = kbd_endpoint->max_packet_size;
    kbd->interval = kbd_endpoint->interval;

    pfallocator_free_page(config_buf);

    if (usb_keyboard_set_protocol(kbd) < 0) {
        printkf_error("usb_keyboard_probe(): SET_PROTOCOL failed\n");
    }

    usb_keyboard_set_idle(kbd);

    if (usb_keyboard_configure_endpoint(kbd) < 0) {
        free(kbd);
        return -1;
    }

    kbd->next = keyboards;
    keyboards = kbd;

    usb_keyboard_queue_transfer(kbd);

    return 0;
}

void usb_keyboard_poll() {
    for (usb_keyboard_t *kbd = keyboards; kbd; kbd = kbd->next) {
        xhci_controller_t *xhci = kbd->xhci;

        xhci_trb_t *event = &xhci->event_ring[xhci->event_dequeue];
        bool cycle = (event->control & TRB_CYCLE) != 0;

        if (cycle != xhci->event_cycle) {
            continue;
        }

        uint8_t type = (event->control >> TRB_TYPE_SHIFT) & 0x3F;

        if (type == TRB_TYPE_TRANSFER) {
            uint8_t cc = (event->status >> TRB_STATUS_COMP_CODE_SHIFT) & 0xFF;

            if (cc == CC_SUCCESS || cc == CC_SHORT_PACKET) {
                usb_keyboard_process_report(kbd, (usb_kbd_report_t *)kbd->report_buffer);
            }

            memset(kbd->report_buffer, 0, 8);
            usb_keyboard_queue_transfer(kbd);
        }

        xhci->event_dequeue++;
        if (xhci->event_dequeue >= xhci->event_ring_size) {
            xhci->event_dequeue = 0;
            xhci->event_cycle = !xhci->event_cycle;
        }

        volatile xhci_intr_regs_t *ir = &xhci->runtime->ir[0];
        ir->erdp = (xhci->event_ring_phys + (xhci->event_dequeue * sizeof(xhci_trb_t))) | (1 << 3);
    }
}

void usb_keyboard_task() {
    while (1) {
        usb_keyboard_poll();
        task_yield();
    }
}