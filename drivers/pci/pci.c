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
