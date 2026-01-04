#ifndef JOS_INC_ACPI_BASE_H
#define JOS_INC_ACPI_BASE_H

#include <inc/types.h>

#pragma pack(push, 1)

typedef struct {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t reserved[3];
} RSDP;

typedef struct {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} ACPISDTHeader;

typedef struct {
    ACPISDTHeader h;
    uint32_t PointerToOtherSDT[];
} RSDT;

#endif // JOS_INC_ACPI_BASE_H
