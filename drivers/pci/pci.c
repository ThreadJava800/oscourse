#include <stdbool.h>

#include <inc/acpi.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/string.h>
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


static uint32_t
pci_config_read32_impl(const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t off) {
    outl(CONFIG_ADDRESS, PCI_CFG_ADDR(bus, slot, func, off));
    return inl(CONFIG_DATA);
}

static uint8_t
pci_config_read8_impl(const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t off) {
    const uint32_t val32 = pci_config_read32_impl(bus, slot, func, off);
    return (val32 >> ((off & 3) * 8)) & 0xFF;
}

static uint16_t
pci_config_read16_impl(const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t off) {
    const uint32_t val32 = pci_config_read32_impl(bus, slot, func, off);
    return (val32 >> ((off & 2) * 8)) & 0xFFFF;
}

static void
pci_config_write32_impl(const uint32_t val, const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t off) {
    outl(CONFIG_ADDRESS, PCI_CFG_ADDR(bus, slot, func, off));
    outl(CONFIG_DATA, val);
}

static void
pci_config_write8_impl(const uint8_t val, const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t off) {
    uint32_t orig_val = pci_config_read32_impl(bus, slot, func, off);

    const uint32_t shift = (off & 3) * 8;
    orig_val &= ~(0xFF << shift);
    orig_val |= ((uint32_t)val << shift);

    pci_config_write32_impl(orig_val, bus, slot, func, off);
}

static void
pci_config_write16_impl(const uint16_t val, const uint8_t bus, const uint8_t slot, const uint8_t func, const uint8_t off) {
    uint32_t orig_val = pci_config_read32_impl(bus, slot, func, off);

    const uint32_t shift = (off & 2) * 8;
    orig_val &= ~(0xFFFF << shift);
    orig_val |= ((uint32_t)val << shift);

    pci_config_write32_impl(orig_val, bus, slot, func, off);
}

static PciDevice*
pci_dev_alloc(PciBus *const pci_bus, const uint8_t dev_num, const uint8_t fun_num) {
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

    memset(new_dev->io_bars, 0, sizeof(new_dev->io_bars));

    return new_dev;
}

static PciBus*
pci_bus_alloc(const uint8_t bus_num) {
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

static bool pci_scan_dev_bars(PciBus *const bus, PciDevice *const dev) {
    assert(bus);
    assert(dev);

    uint64_t curr_bar_reg = PCI_BAR_REG_START;
    for (uint8_t i = 0; i < MAX_PCI_BAR_CNT; ++i, curr_bar_reg += 4) {
        const uint32_t orig_val = pci_config_read32(dev, curr_bar_reg);
        pci_config_write32(dev, curr_bar_reg, 0xFFFFFFFF);

        const uint32_t mask = pci_config_read32(dev, curr_bar_reg);
        pci_config_write32(dev, curr_bar_reg, orig_val);

        if (!(mask & 0x1)) {
            // for now, only port I/O is supported
            continue;
        }

        dev->io_bars[i].base = orig_val & ~0x3;
        dev->io_bars[i].size = ~(mask & ~0x3) + 1;

        PCI_TRACE(
            "\tBAR(%d): base = 0x%x, size = %x\n",
            i,
            dev->io_bars[i].base,
            dev->io_bars[i].size
        );
    }
    return true;
}

static bool pci_scan_bus(PciBus *const bus);

static bool
pci_add_dev_node(PciBus *const bus, PciDevice *const dev) {
    assert(bus);
    assert(dev);

    dev->vendor_id = pci_config_read16_impl(bus->num, dev->num, dev->fun, PCI_VENDOR_ID);
    dev->device_id = pci_config_read16_impl(bus->num, dev->num, dev->fun, PCI_DEVICE_ID);

    dev->base_class = pci_config_read8_impl(bus->num, dev->num, dev->fun, PCI_CLASS);
    dev->sub_class = pci_config_read8_impl(bus->num, dev->num, dev->fun, PCI_SUBCLASS);
    dev->prog_if = pci_config_read8_impl(bus->num, dev->num, dev->fun, PCI_PROG_IF);

    dev->cap_ptr = pci_config_read32_impl(bus->num, dev->num, dev->fun, PCI_CAPLISTPTR_REG);
    dev->irq_line = pci_config_read32_impl(bus->num, dev->num, dev->fun, PCI_INTERRUPT_REG);

    PCI_TRACE(
        "Enumerated pci device %02x:%02x.%1o of type %02x.%02x.%02x: vendor_id = %02x, device_id = %02x\n",
        bus->num, dev->num, dev->fun,
        dev->base_class, dev->sub_class, dev->prog_if,
        dev->vendor_id, dev->device_id
    );

    if (!pci_scan_dev_bars(bus, dev)) {
        cprintf(
            "Failed to enumerate bars of pci device %02x:%02x.%1o of type %02x.%02x.%02x\n",
            bus->num, dev->num, dev->fun,
            dev->base_class, dev->sub_class, dev->prog_if
        );
        return false;
    }

    if (dev->base_class == PCI_CLASS_BRIDGE && dev->sub_class == PCI_SUBCLASS_PCI) {
        PCI_TRACE("\tThis device is a PCI bridge.\n");

        const uint8_t secondary_bus_num = pci_config_read8_impl(bus->num, dev->num, dev->fun, PCI_SECONDARY_BUS);
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

static bool
pci_scan_dev_funcs(PciBus *const bus, const uint8_t dev_num) {
    assert(bus);

    const uint8_t header_type = pci_config_read8_impl(bus->num, dev_num, 0, PCI_HEADER_TYPE);
    const uint8_t fun_cnt = (header_type & 0x80) ? MAX_PCI_FUNCTION_CNT : 1;

    for (uint8_t fun = 0; fun < fun_cnt; ++fun) {
        const uint16_t vendor_id = pci_config_read16_impl(bus->num, dev_num, fun, PCI_VENDOR_ID);
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

static bool
pci_scan_bus(PciBus *const bus) {
    assert(bus);

    for (uint8_t device = 0; device < MAX_PCI_DEVICE_CNT; ++device) {
        pci_scan_dev_funcs(bus, device);
    }

    return true;
}

static bool
pci_enumerate_devices() {
    PCI_TRACE("PCI: enumerate devices\n");

    const uint8_t header_type = pci_config_read8_impl(0, 0, 0, PCI_HEADER_TYPE);
    if (header_type & 0x80) {
        PCI_TRACE("There are multiple host controller in the system.\n");

        for (uint8_t fun = 0; fun < MAX_PCI_FUNCTION_CNT; ++fun) {
            const uint8_t vendor_id = pci_config_read16_impl(0, 0, fun, PCI_VENDOR_ID);
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

static void
pci_dump_bus(const PciBus *const bus, const unsigned depth) {
    assert(bus);
    assert(is_init);

    for (unsigned i = 0; i < depth; ++i) {
        cprintf("\t");
    }

    cprintf("Bus %02x\n", bus->num);

    for (size_t i = 0; i < bus->device_cnt; ++i) {
        const PciDevice dev = bus->devices[i];
        cprintf(
            "%02x:%02x.%1o - %02x.%02x.%02x: vendor_id = %02x, device_id = %02x\n",
            bus->num, dev.num, dev.fun,
            dev.base_class, dev.sub_class, dev.prog_if,
            dev.vendor_id, dev.device_id
        );

        for (uint8_t bar_ind = 0; bar_ind < MAX_PCI_BAR_CNT; ++bar_ind) {
            if (dev.io_bars[bar_ind].base == 0 || dev.io_bars[bar_ind].size == 0) {
                continue;
            }
            cprintf(
                "\tBAR(%d): base = 0x%x, size = %x\n",
                bar_ind,
                dev.io_bars[bar_ind].base,
                dev.io_bars[bar_ind].size
            );
        }

        if (dev.child_bus) {
            pci_dump_bus(dev.child_bus, depth + 1);
        }
    }
}

static PciDevice*
find_pci_dev_in_bus(PciBus *const bus, const uint16_t vendor_id, const uint16_t device_id) {
    assert(bus);
    assert(is_init);

    for (size_t i = 0; i < bus->device_cnt; ++i) {
        PciDevice *dev = &bus->devices[i];
        if (dev->vendor_id == vendor_id && dev->device_id == device_id) {
            return dev;
        }

        if (dev->child_bus) {
            dev = find_pci_dev_in_bus(dev->child_bus, vendor_id, device_id);
            if (dev) {
                return dev;
            }
        }
    }

    return NULL;
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

PciDevice *find_pci_dev(const uint16_t vendor_id, const uint16_t device_id) {
    if (!is_init) {
        cprintf("Failed to find pci device: PCI module was not initialized!\n");
        return NULL;
    }

    for (size_t i = 0; i < pci_bus_cnt; ++i) {
        PciBus bus = pci_buses[i];
        if (!bus.parent) { // we only want to run through "root" buses.
            PciDevice *const dev = find_pci_dev_in_bus(&bus, vendor_id, device_id);
            if (dev) {
                return dev;
            }
        }
    }

    return NULL;
}

uint8_t pci_get_capability_pointer(PciDevice *dev) {
    assert(dev);
    return dev->cap_ptr;
}

uint16_t pci_get_vid(PciDevice *dev) {
    assert(dev);
    return dev->vendor_id;
}

uint16_t pci_get_did(PciDevice *dev) {
    assert(dev);
    return dev->device_id;
}

uint8_t pci_config_read8(PciDevice *dev, uint8_t offset) {
    assert(dev);
    assert(dev->parent_bus);
    return pci_config_read8_impl(dev->parent_bus->num, dev->num, dev->fun, offset);
}

uint16_t pci_config_read16(PciDevice *dev, uint8_t offset) {
    assert(dev);
    assert(dev->parent_bus);
    return pci_config_read16_impl(dev->parent_bus->num, dev->num, dev->fun, offset);
}

uint32_t pci_config_read32(PciDevice *dev, uint8_t offset) {
    assert(dev);
    assert(dev->parent_bus);
    return pci_config_read32_impl(dev->parent_bus->num, dev->num, dev->fun, offset);
}

uint64_t pci_config_read64(PciDevice *dev, uint8_t offset) {
    assert(dev);
    assert(dev->parent_bus);

    const uint64_t lo = (uint64_t)pci_config_read32(dev, offset & 0xFC);
    const uint64_t hi = (uint64_t)pci_config_read32(dev, (offset & 0xFC) + 4);
    return (hi << 32) | lo;
}

uint8_t pci_access_read8(PciDevice *dev, uint8_t bar_index, uint8_t offset) {
    assert(dev);
    assert(bar_index < MAX_PCI_BAR_CNT);

    PciBar bar = dev->io_bars[bar_index];
    assert(bar.base != 0);
    assert(offset + 1 <= bar.size);

    return inb(bar.base + offset);
}

uint16_t pci_access_read16(PciDevice *dev, uint8_t bar_index, uint8_t offset) {
    assert(dev);
    assert(bar_index < MAX_PCI_BAR_CNT);

    PciBar bar = dev->io_bars[bar_index];
    assert(bar.base != 0);
    assert(offset + 2 <= bar.size);

    return inw(bar.base + offset);
}

uint32_t pci_access_read32(PciDevice *dev, uint8_t bar_index, uint8_t offset) {
    assert(dev);
    assert(bar_index < MAX_PCI_BAR_CNT);

    PciBar bar = dev->io_bars[bar_index];
    assert(bar.base != 0);
    assert(offset + 4 <= bar.size);

    return inl(bar.base + offset);
}

uint64_t pci_access_read64(PciDevice *dev, uint8_t bar_index, uint8_t offset) {
    assert(dev);
    assert(bar_index < MAX_PCI_BAR_CNT);

    PciBar bar = dev->io_bars[bar_index];
    assert(bar.base != 0);
    assert(offset + 8 <= bar.size);

    const uint64_t lo = (uint64_t)inl(bar.base + offset);
    const uint64_t hi = (uint64_t)inl(bar.base + offset + 4);
    return (hi << 32) | lo;
}

void pci_config_write8(PciDevice *dev, uint8_t offset, uint8_t value) {
    assert(dev);
    assert(dev->parent_bus);
    pci_config_write8_impl(value, dev->parent_bus->num, dev->num, dev->fun, offset);
}

void pci_config_write16(PciDevice *dev, uint8_t offset, uint16_t value) {
    assert(dev);
    assert(dev->parent_bus);
    pci_config_write16_impl(value, dev->parent_bus->num, dev->num, dev->fun, offset);
}

void pci_config_write32(PciDevice *dev, uint8_t offset, uint32_t value) {
    assert(dev);
    assert(dev->parent_bus);
    pci_config_write32_impl(value, dev->parent_bus->num, dev->num, dev->fun, offset);
}

void pci_config_write64(PciDevice *dev, uint8_t offset, uint64_t value) {
    assert(dev);
    assert(dev->parent_bus);

    const uint32_t lo = (uint32_t)(value & 0xFFFFFFFF);
    const uint32_t hi = (uint32_t)(value >> 32);

    pci_config_write32_impl(lo, dev->parent_bus->num, dev->num, dev->fun, offset);
    pci_config_write32_impl(hi, dev->parent_bus->num, dev->num, dev->fun, offset + 4);
}

void pci_access_write8(PciDevice *dev, uint8_t bar_index, uint8_t offset, uint8_t value) {
    assert(dev);
    assert(bar_index < MAX_PCI_BAR_CNT);

    PciBar bar = dev->io_bars[bar_index];
    assert(bar.base != 0);
    assert(offset + 1 <= bar.size);

    outb(bar.base + offset, value);
}

void pci_access_write16(PciDevice *dev, uint8_t bar_index, uint8_t offset, uint16_t value) {
    assert(dev);
    assert(bar_index < MAX_PCI_BAR_CNT);

    PciBar bar = dev->io_bars[bar_index];
    assert(bar.base != 0);
    assert(offset + 2 <= bar.size);

    outw(bar.base + offset, value);
}

void pci_access_write32(PciDevice *dev, uint8_t bar_index, uint8_t offset, uint32_t value) {
    assert(dev);
    assert(bar_index < MAX_PCI_BAR_CNT);

    PciBar bar = dev->io_bars[bar_index];
    assert(bar.base != 0);
    assert(offset + 4 <= bar.size);

    outl(bar.base + offset, value);
}

void pci_access_write64(PciDevice *dev, uint8_t bar_index, uint8_t offset, uint64_t value) {
    assert(dev);
    assert(bar_index < MAX_PCI_BAR_CNT);

    PciBar bar = dev->io_bars[bar_index];
    assert(bar.base != 0);
    assert(offset + 8 <= bar.size);

    const uint32_t lo = (uint32_t)(value & 0xFFFFFFFF);
    const uint32_t hi = (uint32_t)(value >> 32);

    outl(bar.base + offset, lo);
    outl(bar.base + offset + 4, hi);
}
