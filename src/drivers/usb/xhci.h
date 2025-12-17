#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../pci/pci.h"

#define XHCI_CMD_RUN (1 << 0)
#define XHCI_CMD_HCRST (1 << 1)
#define XHCI_CMD_INTE (1 << 2)
#define XHCI_CMD_HSEE (1 << 3)

#define XHCI_STS_HCH (1 << 0)
#define XHCI_STS_HSE (1 << 2)
#define XHCI_STS_EINT (1 << 3)
#define XHCI_STS_PCD (1 << 4)
#define XHCI_STS_CNR (1 << 11)

#define XHCI_PORTSC_CCS (1 << 0)
#define XHCI_PORTSC_PED (1 << 1)
#define XHCI_PORTSC_OCA (1 << 3)
#define XHCI_PORTSC_PR (1 << 4)
#define XHCI_PORTSC_PP (1 << 9)
#define XHCI_PORTSC_CSC (1 << 17)
#define XHCI_PORTSC_PEC (1 << 18)
#define XHCI_PORTSC_PRC (1 << 21)
#define XHCI_PORTSC_SPEED_MASK (0xF << 10)
#define XHCI_PORTSC_SPEED_SHIFT 10

#define XHCI_SPEED_FULL 1
#define XHCI_SPEED_LOW 2
#define XHCI_SPEED_HIGH 3
#define XHCI_SPEED_SUPER 4

#define TRB_TYPE_NORMAL 1
#define TRB_TYPE_SETUP 2
#define TRB_TYPE_DATA 3
#define TRB_TYPE_STATUS 4
#define TRB_TYPE_LINK 6
#define TRB_TYPE_EVENT_DATA 7
#define TRB_TYPE_NOOP 8
#define TRB_TYPE_ENABLE_SLOT 9
#define TRB_TYPE_DISABLE_SLOT 10
#define TRB_TYPE_ADDRESS_DEV 11
#define TRB_TYPE_CONFIG_EP 12
#define TRB_TYPE_EVAL_CTX 13
#define TRB_TYPE_RESET_EP 14
#define TRB_TYPE_STOP_EP 15
#define TRB_TYPE_SET_TR_DEQ 16
#define TRB_TYPE_RESET_DEV 17
#define TRB_TYPE_NOOP_CMD 23

#define TRB_TYPE_TRANSFER 32
#define TRB_TYPE_CMD_COMPLETE 33
#define TRB_TYPE_PORT_STATUS 34
#define TRB_TYPE_HOST_CTRL 37

#define TRB_CYCLE (1 << 0)
#define TRB_IOC (1 << 5)
#define TRB_IDT (1 << 6)
#define TRB_TYPE_SHIFT 10
#define TRB_TYPE_MASK (0x3F << 10)

#define TRB_STATUS_COMP_CODE_MASK 0xFF000000
#define TRB_STATUS_COMP_CODE_SHIFT 24

#define CC_SUCCESS 1
#define CC_DATA_BUFFER_ERROR 2
#define CC_BABBLE 3
#define CC_USB_TRANSACTION 4
#define CC_TRB_ERROR 5
#define CC_STALL 6
#define CC_SHORT_PACKET 13
#define CC_SLOT_NOT_ENABLED 11
#define CC_NO_SLOTS 9

#define USB_REQ_GET_STATUS 0x00
#define USB_REQ_CLEAR_FEATURE 0x01
#define USB_REQ_SET_FEATURE 0x03
#define USB_REQ_SET_ADDRESS 0x05
#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_REQ_SET_DESCRIPTOR 0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE 0x0A
#define USB_REQ_SET_INTERFACE 0x0B

#define USB_DESC_DEVICE 0x01
#define USB_DESC_CONFIGURATION 0x02
#define USB_DESC_STRING 0x03
#define USB_DESC_INTERFACE 0x04
#define USB_DESC_ENDPOINT 0x05
#define USB_DESC_HID 0x21
#define USB_DESC_HID_REPORT 0x22

#define USB_CLASS_HID 0x03
#define USB_HID_SUBCLASS_BOOT 0x01
#define USB_HID_PROTOCOL_KEYBOARD 0x01
#define USB_HID_PROTOCOL_MOUSE 0x02

typedef struct {
    uint8_t caplength;
    uint8_t reserved;
    uint16_t hciversion;
    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hcsparams3;
    uint32_t hccparams1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hccparams2;
} __attribute__((packed)) xhci_cap_regs_t;

typedef struct {
    uint32_t usbcmd;
    uint32_t usbsts;
    uint32_t pagesize;
    uint32_t reserved1[2];
    uint32_t dnctrl;
    uint64_t crcr;
    uint32_t reserved2[4];
    uint64_t dcbaap;
    uint32_t config;
} __attribute__((packed)) xhci_op_regs_t;

typedef struct {
    uint32_t portsc;
    uint32_t portpmsc;
    uint32_t portli;
    uint32_t porthlpmc;
} __attribute__((packed)) xhci_port_regs_t;

typedef struct {
    uint32_t iman;
    uint32_t imod;
    uint32_t erstsz;
    uint32_t reserved;
    uint64_t erstba;
    uint64_t erdp;
} __attribute__((packed)) xhci_intr_regs_t;

typedef struct {
    uint32_t mfindex;
    uint32_t reserved[7];
    xhci_intr_regs_t ir[1024];
} __attribute__((packed)) xhci_runtime_regs_t;

typedef struct {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} __attribute__((packed)) xhci_trb_t;

typedef struct {
    uint64_t ring_base;
    uint32_t ring_size;
    uint32_t reserved;
} __attribute__((packed)) xhci_erst_entry_t;

typedef struct {
    uint32_t dw0;
    uint32_t dw1;
    uint32_t dw2;
    uint32_t dw3;
    uint32_t reserved[4];
} __attribute__((packed)) xhci_slot_ctx_t;

typedef struct {
    uint32_t dw0;
    uint32_t dw1;
    uint64_t tr_dequeue;
    uint32_t dw4;
    uint32_t reserved[3];
} __attribute__((packed)) xhci_ep_ctx_t;

typedef struct {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[5];
    uint8_t config_value;
    uint8_t interface_num;
    uint8_t alternate_setting;
    uint8_t reserved2;
} __attribute__((packed)) xhci_input_ctrl_ctx_t;

typedef struct {
    xhci_slot_ctx_t slot;
    xhci_ep_ctx_t ep[31];
} __attribute__((packed)) xhci_device_ctx_t;

typedef struct {
    xhci_input_ctrl_ctx_t ctrl;
    xhci_slot_ctx_t slot;
    xhci_ep_ctx_t ep[31];
} __attribute__((packed)) xhci_input_ctx_t;

typedef struct {
    xhci_trb_t *trbs;
    uint64_t phys;
    uint32_t size;
    uint32_t enqueue;
    bool cycle;
} xhci_ring_t;

typedef struct xhci_device {
    uint8_t slot_id;
    uint8_t port;
    uint8_t speed;

    xhci_device_ctx_t *output_ctx;
    xhci_input_ctx_t *input_ctx;
    uint64_t output_ctx_phys;
    uint64_t input_ctx_phys;

    xhci_ring_t *ep_rings[31];

    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t _class;
    uint8_t subclass;
    uint8_t protocol;

    struct xhci_device *next;
} xhci_device_t;

typedef struct {
    pci_device_t *pci_dev;

    volatile xhci_cap_regs_t *cap;
    volatile xhci_op_regs_t *op;
    volatile xhci_runtime_regs_t *runtime;
    volatile uint32_t *doorbell;
    volatile xhci_port_regs_t *ports;

    uint8_t max_slots;
    uint8_t max_ports;
    uint16_t max_interrupters;
    uint32_t page_size;
    bool context_size_64;

    uint64_t *dcbaa;
    uint64_t dcbaa_phys;

    xhci_ring_t cmd_ring;

    xhci_trb_t *event_ring;
    uint64_t event_ring_phys;
    uint32_t event_ring_size;
    uint32_t event_dequeue;
    bool event_cycle;

    xhci_erst_entry_t *erst;
    uint64_t erst_phys;

    uint64_t *scratchpad_array;
    uint64_t scratchpad_array_phys;

    xhci_device_t *devices;
} xhci_controller_t;

typedef struct {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t usb_version;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint8_t max_packet_size;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_version;
    uint8_t manufacturer_index;
    uint8_t product_index;
    uint8_t serial_index;
    uint8_t num_configurations;
} __attribute__((packed)) usb_device_desc_t;

typedef struct {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t total_length;
    uint8_t num_interfaces;
    uint8_t config_value;
    uint8_t config_index;
    uint8_t attributes;
    uint8_t max_power;
} __attribute__((packed)) usb_config_desc_t;

typedef struct {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t num_endpoints;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t interface_index;
} __attribute__((packed)) usb_interface_desc_t;

typedef struct {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t endpoint_address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} __attribute__((packed)) usb_endpoint_desc_t;

typedef struct {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t hid_version;
    uint8_t country_code;
    uint8_t num_descriptors;
    uint8_t report_desc_type;
    uint16_t report_desc_length;
} __attribute__((packed)) usb_hid_desc_t;

void xhci_init();
int xhci_probe(pci_device_t *pdev);
void xhci_remove(pci_device_t *pdev);

int xhci_reset(xhci_controller_t *xhci);
int xhci_start(xhci_controller_t *xhci);
int xhci_stop(xhci_controller_t *xhci);

int xhci_port_reset(xhci_controller_t *xhci, uint8_t port);
int xhci_enumerate_ports(xhci_controller_t *xhci);

int xhci_ring_init(xhci_ring_t *ring, uint32_t size);
void xhci_ring_enqueue(xhci_ring_t *ring, xhci_trb_t *trb);
void xhci_ring_doorbell(xhci_controller_t *xhci, uint8_t slot, uint8_t target);

xhci_device_t *xhci_address_device(xhci_controller_t *xhci, uint8_t port, uint8_t speed);
int xhci_configure_device(xhci_controller_t *xhci, xhci_device_t *dev);

int xhci_control_transfer(xhci_controller_t *xhci,
                          xhci_device_t *dev,
                          uint8_t request_type,
                          uint8_t request,
                          uint16_t value,
                          uint16_t index,
                          void *data,
                          uint16_t length);

int xhci_send_command(xhci_controller_t *xhci, xhci_trb_t *trb);
int xhci_enable_slot(xhci_controller_t *xhci, uint8_t *slot_id);
int xhci_address_device_cmd(xhci_controller_t *xhci, uint8_t slot_id, uint64_t input_ctx_phys, bool bsr);
int xhci_configure_endpoint(xhci_controller_t *xhci, uint8_t slot_id, uint64_t input_ctx_phys);