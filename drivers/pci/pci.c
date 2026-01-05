#include <stdbool.h>

#include <inc/acpi.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/x86.h>

#include "pci.h"

#ifndef pci_need_trace
#define pci_need_trace 0
#endif

#ifdef pci_need_trace
#define PCI_TRACE(fmt, ...)                                 \
    if (pci_need_trace) {                                   \
        cprintf("[TRACE]:[PCI]: " fmt, ##__VA_ARGS__);      \
    }
#else
#define PCI_TRACE(fmt, ...) do {} while(0)
#endif

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC

#define PCI_CFG_ADDR(bus, slot, func, off) \
    (uint32_t)(((bus) << 16) | ((slot) << 11) | ((func) << 8) | (off & 0xFC) | ((uint32_t)0x80000000))

static bool is_init = false;
static PciBus pci_buses[MAX_PCI_FUNCTION_CNT];
static size_t pci_bus_cnt = 0;

static uint8_t pci_read8(const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t off);
static uint16_t pci_read16(const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t off);
static uint32_t pci_read32(const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t off);

static PciDevice *pci_dev_alloc(PciBus *const pci_bus, const uint8_t dev_num, const uint8_t fun_num);
static PciBus *pci_bus_alloc(const uint8_t bus_num);

static bool pci_add_dev_node(PciBus *const bus, PciDevice *const dev);
static bool pci_scan_dev_funcs(PciBus *const bus, const uint8_t dev_num);
static bool pci_scan_bus(PciBus *const bus);
static bool pci_enumerate_devices();

static void pci_dump_bus(const PciBus *const bus, const unsigned depth);

uint8_t pci_read8(const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t off) {
    const uint32_t val32 = pci_read32(bus, slot, func, off);
    return (val32 >> ((off & 3) * 8)) & 0xFF;
}

uint16_t pci_read16(const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t off) {
    const uint32_t val32 = pci_read32(bus, slot, func, off);
    return (val32 >> ((off & 2) * 8)) & 0xFFFF;
}

uint32_t pci_read32(const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t off) {
    outl(CONFIG_ADDRESS, PCI_CFG_ADDR(bus, slot, func, off));
    return inl(CONFIG_DATA);
}

PciDevice *pci_dev_alloc(PciBus *const pci_bus, const uint8_t dev_num, const uint8_t fun_num) {
    assert(pci_bus);

    if (pci_bus->device_cnt >= MAX_PCI_DEVICE_CNT) {
        cprintf("Maximum device cnt exeeded for bus %d!\n", pci_bus->num);
        return NULL;
    }

    PciDevice *new_dev = &pci_bus->devices[pci_bus->device_cnt++];

    new_dev->num = dev_num;
    new_dev->fun = fun_num;
    new_dev->parent_bus = pci_bus;
    new_dev->child_bus = NULL;

    return new_dev;
}

PciBus *pci_bus_alloc(const uint8_t bus_num) {
    if (pci_bus_cnt >= MAX_PCI_BUS_CNT) {
        cprintf("Maximum bus cnt exeeded!\n");
        return NULL;
    }

    PciBus *const pci_bus = &pci_buses[pci_bus_cnt++];

    pci_bus->num = bus_num;
    pci_bus->parent = NULL;
    pci_bus->device_cnt = 0;

    return pci_bus;
}

bool pci_add_dev_node(PciBus *const bus, PciDevice *const dev) {
    assert(bus);
    assert(dev);

    dev->vendor_id = pci_read16(bus->num, dev->num, dev->fun, PCI_VENDOR_ID);
    dev->device_id = pci_read16(bus->num, dev->num, dev->fun, PCI_DEVICE_ID);

    dev->base_class = pci_read8(bus->num, dev->num, dev->fun, PCI_CLASS);
    dev->sub_class = pci_read8(bus->num, dev->num, dev->fun, PCI_SUBCLASS);
    dev->prog_if = pci_read8(bus->num, dev->num, dev->fun, PCI_PROG_IF);

    PCI_TRACE(
        "Enumerated pci device %02x:%02x.%1o of type %02x.%02x.%02x\n",
        bus->num, dev->num, dev->fun,
        dev->base_class, dev->sub_class, dev->prog_if
    );

    if (dev->base_class == PCI_CLASS_BRIDGE && dev->sub_class == PCI_SUBCLASS_PCI) {
        PCI_TRACE("\tThis device is a PCI bridge.\n");

        const uint8_t secondary_bus_num = pci_read8(bus->num, dev->num, dev->fun, PCI_SECONDARY_BUS);
        PciBus *const child_bus = pci_bus_alloc(secondary_bus_num);
        if (!child_bus) {
            cprintf("Failed to allocate child pci bus = %02x!\n", secondary_bus_num);
            return false;
        }

        child_bus->parent = bus;
        if (!pci_scan_bus(child_bus)) {
            cprintf("PCI: failed to scan bus = %02x!", secondary_bus_num);
            return false;
        }

        dev->child_bus = child_bus;
    }
    return true;
}

bool pci_scan_dev_funcs(PciBus *const bus, const uint8_t dev_num) {
    assert(bus);

    const uint8_t header_type = pci_read8(bus->num, dev_num, 0, PCI_HEADER_TYPE);
    const uint8_t fun_cnt = (header_type & 0x80) ? MAX_PCI_FUNCTION_CNT : 1;

    for (uint8_t fun = 0; fun < fun_cnt; ++fun) {
        const uint16_t vendor_id = pci_read16(bus->num, dev_num, fun, PCI_VENDOR_ID);
        if (vendor_id == 0xFFFF) {
            PCI_TRACE(
                "Device %02x:%02x.%1o is not present. Continue enumerating functions!\n",
                bus->num, dev_num, fun
            );
            continue;
        }

        PciDevice *const dev = pci_dev_alloc(bus, dev_num, fun);
        if (!dev) {
            cprintf("Failed to allocate device for bus = %02x!\n", bus->num);
            return false;
        }
        if (!pci_add_dev_node(bus, dev)) {
            cprintf("Failed to add device %02x:%02x.%1o!\n", bus->num, dev->num, dev->fun);
            return false;
        }
    }
    return true;
}

bool pci_scan_bus(PciBus *const bus) {
    assert(bus);

    for (uint8_t device = 0; device < MAX_PCI_DEVICE_CNT; ++device) {
        pci_scan_dev_funcs(bus, device);
    }

    return true;
}

bool pci_enumerate_devices() {
    PCI_TRACE("PCI: enumerate devices\n");

    const uint8_t header_type = pci_read8(0, 0, 0, PCI_HEADER_TYPE);
    if (header_type & 0x80) {
        PCI_TRACE("There are multiple host controller in the system.\n");

        for (uint8_t fun = 0; fun < MAX_PCI_FUNCTION_CNT; ++fun) {
            const uint8_t vendor_id = pci_read16(0, 0, fun, PCI_VENDOR_ID);
            if (vendor_id == 0xFFFF) {
                PCI_TRACE("Device at bus = %02x does not exist! Continue searching\n", fun);
                continue;
            }

            PciBus *const pci_bus = pci_bus_alloc(fun);
            if (!pci_bus) {
                cprintf("Failed to allocate pci bus = %02x!\n", fun);
                return false;
            }
            if (!pci_scan_bus(pci_bus)) {
                cprintf("PCI: failed to scan bus = %02x!", pci_bus->num);
                return false;
            }
        }
    } else {
        PCI_TRACE("There is only one host controller in the system.\n");

        PciBus *const pci_bus = pci_bus_alloc(0);
        if (!pci_bus) {
            cprintf("Failed to allocate pci bus = %02x!\n", 0);
            return false;
        }
        if (!pci_scan_bus(pci_bus)) {
            cprintf("PCI: failed to scan bus = %02x!", pci_bus->num);
            return false;
        }
    }
    return true;
}

void pci_dump_bus(const PciBus *const bus, const unsigned depth) {
    assert(bus);
    assert(is_init);

    for (unsigned i = 0; i < depth; ++i) {
        cprintf("\t");
    }

    cprintf("Bus %02x\n", bus->num);

    for (size_t i = 0; i < bus->device_cnt; ++i) {
        PciDevice dev = bus->devices[i];
        cprintf(
            "%02x:%02x.%1o - %02x.%02x.%02x: vendor_id = %02x, device_id = %02x\n",
            bus->num, dev.num, dev.fun,
            dev.base_class, dev.sub_class, dev.prog_if,
            dev.vendor_id, dev.device_id
        );

        if (dev.child_bus) {
            pci_dump_bus(dev.child_bus, depth + 1);
        }
    }
}

int pci_init() {
    if (is_init) {
        return 0;
    }

    if (!pci_enumerate_devices()) {
        cprintf("Failed to enumerate pci devices!\n");
        return -E_UNSPECIFIED;
    }

    is_init = true;
    return 0;
}

void pci_dump_tree() {
    if (!is_init) {
        cprintf("Failed to enumerate PCI devices: PCI module was not initialized!\n");
        return;
    }

    for (size_t i = 0; i < pci_bus_cnt; ++i) {
        const PciBus bus = pci_buses[i];
        if (!bus.parent) { // we only want to run through "root" buses.
            pci_dump_bus(&bus, 0);
            cprintf("\n");
        }
    }
}

uint8_t pci_get_capability_pointer(PciDevice *dev) {
    panic("pci_get_capability_pointer: Not yet implemented!");
    return 0;
}

uint16_t pci_get_vid(PciDevice *dev) {
    panic("pci_get_vid: Not yet implemented!");
    return 0;
}

uint16_t pci_get_did(PciDevice *dev) {
    panic("pci_get_did: Not yet implemented!");
    return 0;
}

uint8_t pci_config_read8(PciDevice *dev, uint8_t offset) {
    panic("pci_config_read8: Not yet implemented!");
    return 0;
}

uint16_t pci_config_read16(PciDevice *dev, uint8_t offset) {
    panic("pci_config_read16: Not yet implemented!");
    return 0;
}

uint32_t pci_config_read32(PciDevice *dev, uint8_t offset) {
    panic("pci_config_read32: Not yet implemented!");
    return 0;
}

uint64_t pci_config_read64(PciDevice *dev, uint8_t offset) {
    panic("pci_config_read64: Not yet implemented!");
    return 0;
}

uint8_t pci_access_read8(PciDevice *dev, uint8_t bar_index, uint8_t offset) {
    panic("pci_access_read8: Not yet implemented!");
    return 0;
}

uint16_t pci_access_read16(PciDevice *dev, uint8_t bar_index, uint8_t offset) {
    panic("pci_access_read16: Not yet implemented!");
    return 0;
}

uint32_t pci_access_read32(PciDevice *dev, uint8_t bar_index, uint8_t offset) {
    panic("pci_access_read32: Not yet implemented!");
    return 0;
}

uint64_t pci_access_read64(PciDevice *dev, uint8_t bar_index, uint8_t offset) {
    panic("pci_access_read64: Not yet implemented!");
    return 0;
}

void pci_config_write8(PciDevice *dev, uint8_t value) {
    panic("pci_config_write8: Not yet implemented!");
}

void pci_config_write16(PciDevice *dev, uint16_t value) {
    panic("pci_config_write16: Not yet implemented!");
}

void pci_config_write32(PciDevice *dev, uint32_t value) {
    panic("pci_config_write32: Not yet implemented!");
}

void pci_config_write64(PciDevice *dev, uint64_t value) {
    panic("pci_config_write64: Not yet implemented!");
}

void pci_access_write8(PciDevice *dev, uint8_t bar_index, uint8_t value) {
    panic("pci_access_write8: Not yet implemented!");
}

void pci_access_write16(PciDevice *dev, uint8_t bar_index, uint16_t value) {
    panic("pci_access_write16: Not yet implemented!");
}

void pci_access_write32(PciDevice *dev, uint8_t bar_index, uint32_t value) {
    panic("pci_access_write32: Not yet implemented!");
}

void pci_access_write64(PciDevice *dev, uint8_t bar_index, uint64_t value) {
    panic("pci_access_write64: Not yet implemented!");
}
