#include "xhci.h"
#include "keyboard.h"

#include "../../io/terminal.h"
#include "../../mem/alloc/heap.h"
#include "../../mem/alloc/page_frame_alloc.h"
#include "../../mem/paging/paging.h"
#include "../../std/string.h"

static const pci_device_id_t xhci_ids[] = {
    {PCI_DEVICE_CLASS(0x0C0330, 0xFFFFFF)},
    {0},
};

static xhci_controller_t *g_xhci = NULL;

static bool xhci_wait_ready(xhci_controller_t *xhci, uint32_t timeout_ms) {
    while (timeout_ms--) {
        if (!(xhci->op->usbsts & XHCI_STS_CNR)) {
            return true;
        }
        for (volatile int i = 0; i < 1000; i++)
            ;
    }
    return false;
}

static bool xhci_wait_halted(xhci_controller_t *xhci, uint32_t timeout_ms) {
    while (timeout_ms--) {
        if (xhci->op->usbsts & XHCI_STS_HCH) {
            return true;
        }
        for (volatile int i = 0; i < 1000; i++)
            ;
    }
    return false;
}

int xhci_ring_init(xhci_ring_t *ring, uint32_t size) {
    ring->size = size;
    ring->enqueue = 0;
    ring->cycle = true;

    ring->trbs = (xhci_trb_t *)pfallocator_request_page();
    if (!ring->trbs) {
        return -1;
    }

    memset(ring->trbs, 0, 4096);
    ring->phys = virt_to_phys(ring->trbs);

    xhci_trb_t *link = &ring->trbs[size - 1];
    link->parameter = ring->phys;
    link->status = 0;
    link->control = (TRB_TYPE_LINK << TRB_TYPE_SHIFT) | TRB_CYCLE;

    return 0;
}

void xhci_ring_enqueue(xhci_ring_t *ring, xhci_trb_t *trb) {
    xhci_trb_t *dest = &ring->trbs[ring->enqueue];

    dest->parameter = trb->parameter;
    dest->status = trb->status;
    dest->control = trb->control;

    if (ring->cycle) {
        dest->control |= TRB_CYCLE;
    } else {
        dest->control &= ~TRB_CYCLE;
    }

    ring->enqueue++;

    if (ring->enqueue >= ring->size - 1) {
        xhci_trb_t *link = &ring->trbs[ring->size - 1];
        if (ring->cycle) {
            link->control |= TRB_CYCLE;
        } else {
            link->control &= ~TRB_CYCLE;
        }

        ring->enqueue = 0;
        ring->cycle = !ring->cycle;
    }
}

void xhci_ring_doorbell(xhci_controller_t *xhci, uint8_t slot, uint8_t target) {
    xhci->doorbell[slot] = target;
}

int xhci_reset(xhci_controller_t *xhci) {
    xhci->op->usbcmd &= ~XHCI_CMD_RUN;

    if (!xhci_wait_halted(xhci, 100)) {
        printkf_error("xhci_reset(): Controller failed to halt\n");
        return -1;
    }

    xhci->op->usbcmd |= XHCI_CMD_HCRST;

    uint32_t timeout = 1000;
    while ((xhci->op->usbcmd & XHCI_CMD_HCRST) && timeout--) {
        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    if (xhci->op->usbcmd & XHCI_CMD_HCRST) {
        printkf_error("xhci_reset(): Reset timeout\n");
        return -1;
    }

    if (!xhci_wait_ready(xhci, 100)) {
        printkf_error("xhci_reset(): Controller not ready after reset\n");
        return -1;
    }

    return 0;
}

static int xhci_setup_scratchpad(xhci_controller_t *xhci) {
    uint32_t hcs2 = xhci->cap->hcsparams2;
    uint32_t max_scratch_hi = (hcs2 >> 21) & 0x1F;
    uint32_t max_scratch_lo = (hcs2 >> 27) & 0x1F;
    uint32_t max_scratch = (max_scratch_hi << 5) | max_scratch_lo;

    if (max_scratch == 0) {
        return 0;
    }

    xhci->scratchpad_array = (uint64_t *)pfallocator_request_page();
    if (!xhci->scratchpad_array) {
        return -1;
    }
    memset(xhci->scratchpad_array, 0, 4096);
    xhci->scratchpad_array_phys = virt_to_phys(xhci->scratchpad_array);

    for (uint32_t i = 0; i < max_scratch; i++) {
        void *buf = pfallocator_request_page();
        if (!buf) {
            return -1;
        }
        memset(buf, 0, 4096);
        xhci->scratchpad_array[i] = virt_to_phys(buf);
    }

    xhci->dcbaa[0] = xhci->scratchpad_array_phys;

    return 0;
}

static int xhci_setup_contexts(xhci_controller_t *xhci) {
    xhci->dcbaa = (uint64_t *)pfallocator_request_page();
    if (!xhci->dcbaa) {
        printkf_error("xhci_setup_contexts(): Failed to allocate DCBAA\n");
        return -1;
    }
    memset(xhci->dcbaa, 0, 4096);
    xhci->dcbaa_phys = virt_to_phys(xhci->dcbaa);

    if (xhci_setup_scratchpad(xhci) < 0) {
        printkf_error("xhci_setup_contexts(): Failed to setup scratchpad\n");
        return -1;
    }

    xhci->op->dcbaap = xhci->dcbaa_phys;

    if (xhci_ring_init(&xhci->cmd_ring, 64) < 0) {
        printkf_error("xhci_setup_contexts(): Failed to init command ring\n");
        return -1;
    }

    xhci->op->crcr = xhci->cmd_ring.phys | 1;

    xhci->event_ring_size = 64;
    xhci->event_ring = (xhci_trb_t *)pfallocator_request_page();
    if (!xhci->event_ring) {
        printkf_error("xhci_setup_contexts(): Failed to allocate event ring\n");
        return -1;
    }
    memset(xhci->event_ring, 0, 4096);
    xhci->event_ring_phys = virt_to_phys(xhci->event_ring);
    xhci->event_dequeue = 0;
    xhci->event_cycle = true;

    xhci->erst = (xhci_erst_entry_t *)pfallocator_request_page();
    if (!xhci->erst) {
        printkf_error("xhci_setup_contexts(): Failed to allocate ERST\n");
        return -1;
    }
    memset(xhci->erst, 0, 4096);
    xhci->erst_phys = virt_to_phys(xhci->erst);

    xhci->erst[0].ring_base = xhci->event_ring_phys;
    xhci->erst[0].ring_size = xhci->event_ring_size;

    volatile xhci_intr_regs_t *ir = &xhci->runtime->ir[0];
    ir->erstsz = 1;
    ir->erdp = xhci->event_ring_phys;
    ir->erstba = xhci->erst_phys;
    ir->iman = 0x2;
    ir->imod = 0;

    return 0;
}

int xhci_start(xhci_controller_t *xhci) {
    xhci->op->config = xhci->max_slots;

    xhci->op->usbcmd |= XHCI_CMD_RUN | XHCI_CMD_INTE;

    uint32_t timeout = 100;
    while ((xhci->op->usbsts & XHCI_STS_HCH) && timeout--) {
        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    if (xhci->op->usbsts & XHCI_STS_HCH) {
        printkf_error("xhci_start(): Controller failed to start\n");
        return -1;
    }

    return 0;
}

static int xhci_wait_event(xhci_controller_t *xhci, xhci_trb_t *result, uint32_t timeout_ms) {
    while (timeout_ms--) {
        xhci_trb_t *event = &xhci->event_ring[xhci->event_dequeue];

        bool cycle = (event->control & TRB_CYCLE) != 0;
        if (cycle == xhci->event_cycle) {
            if (result) {
                memcpy(result, event, sizeof(xhci_trb_t));
            }

            xhci->event_dequeue++;
            if (xhci->event_dequeue >= xhci->event_ring_size) {
                xhci->event_dequeue = 0;
                xhci->event_cycle = !xhci->event_cycle;
            }

            volatile xhci_intr_regs_t *ir = &xhci->runtime->ir[0];
            ir->erdp = (xhci->event_ring_phys + (xhci->event_dequeue * sizeof(xhci_trb_t))) | (1 << 3);

            return 0;
        }

        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    return -1;
}

int xhci_send_command(xhci_controller_t *xhci, xhci_trb_t *trb) {
    xhci_ring_enqueue(&xhci->cmd_ring, trb);

    xhci_ring_doorbell(xhci, 0, 0);

    xhci_trb_t result;
    uint32_t timeout = 5000;

    while (timeout--) {
        if (xhci_wait_event(xhci, &result, 1) < 0) {
            continue;
        }

        uint8_t type = (result.control >> TRB_TYPE_SHIFT) & 0x3F;

        if (type == TRB_TYPE_PORT_STATUS) {
            uint8_t port = ((result.parameter >> 24) & 0xFF) - 1;
            if (port < xhci->max_ports) {
                uint32_t portsc = xhci->ports[port].portsc;
                portsc |= XHCI_PORTSC_CSC | XHCI_PORTSC_PEC | XHCI_PORTSC_PRC;
                portsc &= ~XHCI_PORTSC_PED;
                xhci->ports[port].portsc = portsc;
            }
            continue;
        }

        if (type != TRB_TYPE_CMD_COMPLETE) {
            printkf_error("xhci_send_command(): Unexpected event type %u\n", type);
            continue;
        }

        uint8_t cc = (result.status >> TRB_STATUS_COMP_CODE_SHIFT) & 0xFF;
        if (cc != CC_SUCCESS) {
            printkf_error("xhci_send_command(): Command failed with code %u\n", cc);
            return -cc;
        }

        memcpy(trb, &result, sizeof(xhci_trb_t));
        return 0;
    }

    printkf_error("xhci_send_command(): Timeout waiting for command completion\n");
    return -1;
}

int xhci_enable_slot(xhci_controller_t *xhci, uint8_t *slot_id) {
    xhci_trb_t trb = {0};
    trb.control = (TRB_TYPE_ENABLE_SLOT << TRB_TYPE_SHIFT);

    if (xhci_send_command(xhci, &trb) < 0) {
        return -1;
    }

    *slot_id = (trb.control >> 24) & 0xFF;

    return 0;
}

int xhci_address_device_cmd(xhci_controller_t *xhci, uint8_t slot_id, uint64_t input_ctx_phys, bool bsr) {
    xhci_trb_t trb = {0};
    trb.parameter = input_ctx_phys;
    trb.control = (TRB_TYPE_ADDRESS_DEV << TRB_TYPE_SHIFT) | (slot_id << 24);
    if (bsr) {
        trb.control |= (1 << 9);
    }

    return xhci_send_command(xhci, &trb);
}

int xhci_configure_endpoint(xhci_controller_t *xhci, uint8_t slot_id, uint64_t input_ctx_phys) {
    xhci_trb_t trb = {0};
    trb.parameter = input_ctx_phys;
    trb.control = (TRB_TYPE_CONFIG_EP << TRB_TYPE_SHIFT) | (slot_id << 24);

    return xhci_send_command(xhci, &trb);
}

int xhci_port_reset(xhci_controller_t *xhci, uint8_t port) {
    volatile xhci_port_regs_t *p = &xhci->ports[port];

    uint32_t portsc = p->portsc;
    portsc &= ~(XHCI_PORTSC_PED);
    portsc |= XHCI_PORTSC_PR;
    p->portsc = portsc;

    uint32_t timeout = 500;
    while ((p->portsc & XHCI_PORTSC_PR) && timeout--) {
        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    if (p->portsc & XHCI_PORTSC_PR) {
        printkf_error("xhci_port_reset(): Port %u reset timeout\n", port);
        return -1;
    }

    p->portsc = p->portsc | XHCI_PORTSC_PRC;

    if (!(p->portsc & XHCI_PORTSC_PED)) {
        printkf_error("xhci_port_reset(): Port %u not enabled after reset\n", port);
        return -1;
    }

    return 0;
}

static uint8_t xhci_get_port_speed(xhci_controller_t *xhci, uint8_t port) {
    uint32_t portsc = xhci->ports[port].portsc;
    return (portsc & XHCI_PORTSC_SPEED_MASK) >> XHCI_PORTSC_SPEED_SHIFT;
}

static const char *xhci_speed_string(uint8_t speed) {
    switch (speed) {
    case XHCI_SPEED_FULL:
        return "Full (12 Mbps)";
    case XHCI_SPEED_LOW:
        return "Low (1.5 Mbps)";
    case XHCI_SPEED_HIGH:
        return "High (480 Mbps)";
    case XHCI_SPEED_SUPER:
        return "Super (5 Gbps)";
    default:
        return "Unknown";
    }
}

xhci_device_t *xhci_address_device(xhci_controller_t *xhci, uint8_t port, uint8_t speed) {
    uint8_t slot_id;

    if (xhci_enable_slot(xhci, &slot_id) < 0) {
        printkf_error("xhci_address_device(): Failed to enable slot\n");
        return NULL;
    }

    xhci_device_t *dev = (xhci_device_t *)malloc(sizeof(xhci_device_t));
    if (!dev) {
        return NULL;
    }
    memset(dev, 0, sizeof(xhci_device_t));

    dev->slot_id = slot_id;
    dev->port = port;
    dev->speed = speed;

    dev->output_ctx = (xhci_device_ctx_t *)pfallocator_request_page();
    dev->input_ctx = (xhci_input_ctx_t *)pfallocator_request_page();
    if (!dev->output_ctx || !dev->input_ctx) {
        free(dev);
        return NULL;
    }

    memset(dev->output_ctx, 0, 4096);
    memset(dev->input_ctx, 0, 4096);

    dev->output_ctx_phys = virt_to_phys(dev->output_ctx);
    dev->input_ctx_phys = virt_to_phys(dev->input_ctx);

    xhci->dcbaa[slot_id] = dev->output_ctx_phys;

    dev->ep_rings[0] = (xhci_ring_t *)malloc(sizeof(xhci_ring_t));
    if (xhci_ring_init(dev->ep_rings[0], 64) < 0) {
        free(dev);
        return NULL;
    }

    dev->input_ctx->ctrl.add_flags = (1 << 0) | (1 << 1);

    uint32_t route_string = 0;
    uint32_t root_hub_port = port + 1;
    uint32_t context_entries = 1;

    dev->input_ctx->slot.dw0 = (context_entries << 27) | (speed << 20) | route_string;
    dev->input_ctx->slot.dw1 = (root_hub_port << 16);

    uint16_t max_packet_size;
    switch (speed) {
    case XHCI_SPEED_LOW:
    case XHCI_SPEED_FULL:
        max_packet_size = 8;
        break;
    case XHCI_SPEED_HIGH:
        max_packet_size = 64;
        break;
    case XHCI_SPEED_SUPER:
    default:
        max_packet_size = 512;
        break;
    }

    dev->input_ctx->ep[0].dw0 = 0;
    dev->input_ctx->ep[0].dw1 = (max_packet_size << 16) | (4 << 3) | (3 << 1);
    dev->input_ctx->ep[0].tr_dequeue = dev->ep_rings[0]->phys | 1;
    dev->input_ctx->ep[0].dw4 = 8;

    if (xhci_address_device_cmd(xhci, slot_id, dev->input_ctx_phys, false) < 0) {
        printkf_error("xhci_address_device(): Address Device command failed\n");
        free(dev);
        return NULL;
    }

    dev->next = xhci->devices;
    xhci->devices = dev;

    return dev;
}

int xhci_control_transfer(xhci_controller_t *xhci,
                          xhci_device_t *dev,
                          uint8_t request_type,
                          uint8_t request,
                          uint16_t value,
                          uint16_t index,
                          void *data,
                          uint16_t length) {
    xhci_ring_t *ring = dev->ep_rings[0];

    xhci_trb_t setup = {0};
    setup.parameter =
        request_type | (request << 8) | ((uint32_t)value << 16) | ((uint64_t)index << 32) | ((uint64_t)length << 48);
    setup.status = 8;

    uint8_t trt = 0;
    if (length > 0) {
        trt = (request_type & 0x80) ? 3 : 2;
    }
    setup.control = (TRB_TYPE_SETUP << TRB_TYPE_SHIFT) | TRB_IDT | (trt << 16);

    xhci_ring_enqueue(ring, &setup);

    uint64_t data_phys = 0;
    if (length > 0 && data) {
        data_phys = virt_to_phys(data);

        xhci_trb_t data_trb = {0};
        data_trb.parameter = data_phys;
        data_trb.status = length;
        data_trb.control = (TRB_TYPE_DATA << TRB_TYPE_SHIFT);
        if (request_type & 0x80) {
            data_trb.control |= (1 << 16);
        }

        xhci_ring_enqueue(ring, &data_trb);
    }

    xhci_trb_t status = {0};
    status.control = (TRB_TYPE_STATUS << TRB_TYPE_SHIFT) | TRB_IOC;
    if (length == 0 || !(request_type & 0x80)) {
        status.control |= (1 << 16);
    }

    xhci_ring_enqueue(ring, &status);

    xhci_ring_doorbell(xhci, dev->slot_id, 1);

    xhci_trb_t result;
    if (xhci_wait_event(xhci, &result, 5000) < 0) {
        printkf_error("xhci_control_transfer(): Timeout\n");
        return -1;
    }

    uint8_t cc = (result.status >> TRB_STATUS_COMP_CODE_SHIFT) & 0xFF;
    if (cc != CC_SUCCESS && cc != CC_SHORT_PACKET) {
        printkf_error("xhci_control_transfer(): Failed with code %u\n", cc);
        return -1;
    }

    return 0;
}

static void xhci_drain_events(xhci_controller_t *xhci) {
    xhci_trb_t result;
    int drained = 0;

    while (xhci_wait_event(xhci, &result, 10) == 0) {
        uint8_t type = (result.control >> TRB_TYPE_SHIFT) & 0x3F;

        if (type == TRB_TYPE_PORT_STATUS) {
            uint8_t port = ((result.parameter >> 24) & 0xFF) - 1;
            if (port < xhci->max_ports) {
                uint32_t portsc = xhci->ports[port].portsc;
                portsc |= XHCI_PORTSC_CSC | XHCI_PORTSC_PEC | XHCI_PORTSC_PRC;
                portsc &= ~XHCI_PORTSC_PED;
                xhci->ports[port].portsc = portsc;
            }
        }

        drained++;
        if (drained > 100)
            break;
    }
}

int xhci_enumerate_ports(xhci_controller_t *xhci) {
    xhci_drain_events(xhci);

    for (uint8_t i = 0; i < xhci->max_ports; i++) {
        uint32_t portsc = xhci->ports[i].portsc;

        if (!(portsc & XHCI_PORTSC_CCS)) {
            continue;
        }

        uint8_t speed = (portsc >> XHCI_PORTSC_SPEED_SHIFT) & 0xF;

        if (xhci_port_reset(xhci, i) < 0) {
            continue;
        }

        speed = xhci_get_port_speed(xhci, i);

        xhci_device_t *dev = xhci_address_device(xhci, i, speed);
        if (!dev) {
            printkf_error("xhci_enumerate_ports(): Failed to address device on port %u\n", i);
            continue;
        }

        usb_device_desc_t *desc = (usb_device_desc_t *)pfallocator_request_page();
        memset(desc, 0, 4096);

        if (xhci_control_transfer(xhci, dev, 0x80, USB_REQ_GET_DESCRIPTOR, (USB_DESC_DEVICE << 8), 0, desc, 18) < 0) {
            printkf_error("xhci_enumerate_ports(): Failed to get device descriptor\n");
            pfallocator_free_page(desc);
            continue;
        }

        dev->vendor_id = desc->vendor_id;
        dev->product_id = desc->product_id;
        dev->_class = desc->device_class;
        dev->subclass = desc->device_subclass;
        dev->protocol = desc->device_protocol;

        pfallocator_free_page(desc);

        if (usb_keyboard_probe(xhci, dev) == 0) {
            printkf_ok("xHCI: USB Keyboard detected and configured with speed %s\n", xhci_speed_string(speed));
        }
    }

    return 0;
}

int xhci_probe(pci_device_t *pdev) {
    xhci_controller_t *xhci = (xhci_controller_t *)malloc(sizeof(xhci_controller_t));
    if (!xhci) {
        return -1;
    }
    memset(xhci, 0, sizeof(xhci_controller_t));

    xhci->pci_dev = pdev;
    device_set_driver_data(&pdev->device, xhci);

    pci_enable_mmio(pdev);
    pci_enable_bus_mastering(pdev);

    void *mmio = pci_map_bar(pdev, 0);
    if (!mmio) {
        printkf_error("xhci_probe(): Failed to map BAR0\n");
        free(xhci);
        return -1;
    }

    xhci->cap = (volatile xhci_cap_regs_t *)mmio;
    xhci->op = (volatile xhci_op_regs_t *)((uint8_t *)mmio + xhci->cap->caplength);
    xhci->runtime = (volatile xhci_runtime_regs_t *)((uint8_t *)mmio + xhci->cap->rtsoff);
    xhci->doorbell = (volatile uint32_t *)((uint8_t *)mmio + xhci->cap->dboff);

    uint32_t hcs1 = xhci->cap->hcsparams1;
    xhci->max_slots = hcs1 & 0xFF;
    xhci->max_interrupters = (hcs1 >> 8) & 0x7FF;
    xhci->max_ports = (hcs1 >> 24) & 0xFF;

    uint32_t hcc1 = xhci->cap->hccparams1;
    xhci->context_size_64 = (hcc1 & 0x4) != 0;

    xhci->page_size = xhci->op->pagesize << 12;

    xhci->ports = (volatile xhci_port_regs_t *)((uint8_t *)xhci->op + 0x400);

    if (xhci_reset(xhci) < 0) {
        free(xhci);
        return -1;
    }

    if (xhci_setup_contexts(xhci) < 0) {
        free(xhci);
        return -1;
    }

    if (xhci_start(xhci) < 0) {
        free(xhci);
        return -1;
    }

    g_xhci = xhci;

    xhci_enumerate_ports(xhci);

    return 0;
}

void xhci_remove(pci_device_t *pdev) {
    xhci_controller_t *xhci = device_get_driver_data(&pdev->device);
    if (!xhci) {
        return;
    }

    xhci->op->usbcmd &= ~XHCI_CMD_RUN;

    // please free shit later i gotta do this i swear

    free(xhci);
}

static pci_driver_t xhci_driver = {
    .driver = {.name = "xhci"},
    .id_table = xhci_ids,
    .probe = xhci_probe,
    .remove = xhci_remove,
};

void xhci_init(void) {
    pci_driver_register(&xhci_driver);
}