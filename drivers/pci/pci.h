#ifndef JOS_INC_PCI_H
#define JOS_INC_PCI_H

#include <inc/acpi_base.h>

#define MAX_PCI_BUS_CNT 256
#define MAX_PCI_FUNCTION_CNT 8
#define MAX_PCI_DEVICE_CNT 32
#define MAX_PCI_BAR_CNT 6

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

#pragma pack(push)
typedef struct {
    uint8_t capability_id;
    uint8_t next_capability_ptr;
} PciCapabilityHdr;

typedef struct PciBar PciBar;
typedef struct PciBus PciBus;
typedef struct PciDevice PciDevice;

typedef enum {
    PciBarNotPresent,
    PciBarPMIO,
    PciBarMMIO
} PciBarType;

struct PciBar {
    uint32_t base;
    uint32_t size;
    PciBarType type;
};

struct PciDevice {
    uint8_t num;
    uint8_t fun;

    PciBus *parent_bus;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t base_class;
    uint8_t sub_class;
    uint8_t prog_if;

    uint8_t cap_ptr;

    uint32_t irq_line;

    PciBus *child_bus;

    PciBar io_bars[MAX_PCI_BAR_CNT];
};

struct PciBus {
    uint8_t num;
    PciBus *parent;

    PciDevice devices[MAX_PCI_DEVICE_CNT];
    size_t device_cnt;
};
#pragma pack(pop)

#define PCI_VENDOR_ID        0x00
#define PCI_DEVICE_ID        0x02
#define PCI_PROG_IF          0x09
#define PCI_HEADER_TYPE      0x0E
#define PCI_CLASS            0x0B
#define PCI_SUBCLASS         0x0A

#define	PCI_INTERRUPT_REG    0x3C
#define PCI_CAPLISTPTR_REG   0x34
#define PCI_BAR_REG_START    0x10

#define PCI_CLASS_BRIDGE     0x06
#define PCI_SUBCLASS_PCI     0x04

#define PCI_SECONDARY_BUS    0x19

int pci_init();
void pci_dump_tree();
PciDevice *find_pci_dev(const uint16_t vendor_id, const uint16_t device_id);

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

void pci_config_write8(PciDevice *dev, uint8_t offset, uint8_t value);
void pci_config_write16(PciDevice *dev, uint8_t offset, uint16_t value);
void pci_config_write32(PciDevice *dev, uint8_t offset, uint32_t value);
void pci_config_write64(PciDevice *dev, uint8_t offset, uint64_t value);

void pci_access_write8(PciDevice *dev, uint8_t bar_index, uint8_t offset, uint8_t value);
void pci_access_write16(PciDevice *dev, uint8_t bar_index, uint8_t offset, uint16_t value);
void pci_access_write32(PciDevice *dev, uint8_t bar_index, uint8_t offset, uint32_t value);
void pci_access_write64(PciDevice *dev, uint8_t bar_index, uint8_t offset, uint64_t value);

#endif // JOS_INC_PCI_H
