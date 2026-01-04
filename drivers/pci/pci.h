#ifndef JOS_INC_PCI_H
#define JOS_INC_PCI_H

#include <stdbool.h>

#include <inc/acpi_base.h>

typedef struct {
    uint64_t base;
    uint16_t group;
    uint8_t start_num;
    uint8_t end_num;
    uint32_t reserved;
} ECAM;

typedef struct {
    ACPISDTHeader header;
    uint64_t reserved;
    ECAM ecam_info;
} MCFG;

typedef struct {
    uint8_t capability_id;
    uint8_t next_capability_ptr;
} PciCapabilityHdr;

#define PCI_CAPABILITY_ID_VENDOR 0x09

typedef void *PciDevice;

bool pci_enumerate_devices();

uint8_t pci_get_capability_pointer(PciDevice *dev);
uint16_t pci_get_vid(PciDevice *dev);
uint16_t pci_get_did(PciDevice *dev);

uint8_t pci_config_read8(PciDevice *dev, uint8_t offset);
uint16_t pci_config_read16(PciDevice *dev, uint8_t offset);
uint32_t pci_config_read32(PciDevice *dev, uint8_t offset);
uint64_t pci_config_read64(PciDevice *dev, uint8_t offset);

uint8_t pci_access_read8(PciDevice *dev, uint8_t bar_index, uint8_t offset);
uint16_t pci_access_read16(PciDevice *dev, uint8_t bar_index, uint8_t offset);
uint32_t pci_access_read32(PciDevice *dev, uint8_t bar_index, uint8_t offset);
uint64_t pci_access_read64(PciDevice *dev, uint8_t bar_index, uint8_t offset);

void pci_config_write8(PciDevice *dev, uint8_t value);
void pci_config_write16(PciDevice *dev, uint16_t value);
void pci_config_write32(PciDevice *dev, uint32_t value);
void pci_config_write64(PciDevice *dev, uint64_t value);

void pci_access_write8(PciDevice *dev, uint8_t bar_index, uint8_t value);
void pci_access_write16(PciDevice *dev, uint8_t bar_index, uint16_t value);
void pci_access_write32(PciDevice *dev, uint8_t bar_index, uint32_t value);
void pci_access_write64(PciDevice *dev, uint8_t bar_index, uint64_t value);

#endif // JOS_INC_PCI_H
