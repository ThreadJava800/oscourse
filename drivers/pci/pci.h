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

bool pci_enumerate_devices();

#endif // JOS_INC_PCI_H
