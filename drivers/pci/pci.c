#include <inc/acpi.h>
#include <inc/assert.h>
#include <inc/types.h>
#include <inc/stdio.h>

#include "pci.h"

static uint64_t PCIE_ECAM_BASE = 0;

#define MAX_PCI_DEVICE_CNT 32

#define PCIE_CFG_ADDR(bus, dev, func, off) \
    PCIE_ECAM_BASE + ((dev) << 20) + ((dev) << 15) + ((func) << 12) + off

static MCFG *get_mcfg();
static uint16_t pcie_read16(const uint8_t base, const uint8_t dev, const uint8_t func, const uint8_t off);

MCFG *get_mcfg() {
    return acpi_find_table("MCFG");
}

uint16_t pcie_read16(const uint8_t base, const uint8_t dev, const uint8_t func, const uint8_t off) {
    assert(PCIE_ECAM_BASE);
    return *((uint16_t*)PCIE_CFG_ADDR(base, dev, func, off));
}

bool pci_enumerate_devices() {
    const MCFG *const mcfg = (const MCFG*)get_mcfg();
    if (!mcfg) {
        cprintf("Failed to get MCFG ACPI table!\n");
        return false;
    }

    PCIE_ECAM_BASE = mcfg->ecam_info.base;

    return true;
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
