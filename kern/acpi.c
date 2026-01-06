#include <inc/acpi.h>
#include <inc/acpi_base.h>
#include <inc/assert.h>
#include <inc/mmio.h>
#include <inc/string.h>
#include <inc/uefi.h>
#include <inc/stdio.h>

static size_t bytes_check_sum(const void *const var_ptr, const size_t var_len) {
    assert(var_ptr);

    size_t sum = 0;
    const uint8_t *var_cptr = (const uint8_t*)var_ptr;
    for (size_t i = 0; i < var_len; ++i) {
        sum += var_cptr[i];
    }
    return sum;
}

static bool validate_acpi1(const RSDP *const rsdp_ptr) {
    assert(rsdp_ptr);

    uint8_t check_sum = 0;
    check_sum += bytes_check_sum(rsdp_ptr->Signature, sizeof(rsdp_ptr->Signature));
    check_sum += bytes_check_sum(&rsdp_ptr->Checksum, sizeof(rsdp_ptr->Checksum));
    check_sum += bytes_check_sum(rsdp_ptr->OEMID, sizeof(rsdp_ptr->OEMID));
    check_sum += bytes_check_sum(&rsdp_ptr->Revision, sizeof(rsdp_ptr->Revision));
    check_sum += bytes_check_sum(&rsdp_ptr->RsdtAddress, sizeof(rsdp_ptr->RsdtAddress));

    return check_sum == 0;
}

static bool validate_acpi2(const RSDP *const rsdp_ptr) {
    assert(rsdp_ptr);

    if (!validate_acpi1(rsdp_ptr)) {
        cprintf("validate_acpi2: failed to validate_acpi1 part!\n");
        return false;
    }

    uint8_t check_sum = 0;
    check_sum += bytes_check_sum(&rsdp_ptr->Length, sizeof(rsdp_ptr->Length));
    check_sum += bytes_check_sum(&rsdp_ptr->XsdtAddress, sizeof(rsdp_ptr->XsdtAddress));
    check_sum += bytes_check_sum(&rsdp_ptr->ExtendedChecksum, sizeof(rsdp_ptr->ExtendedChecksum));
    check_sum += bytes_check_sum(rsdp_ptr->reserved, sizeof(rsdp_ptr->reserved));

    return check_sum == 0;
}

static bool validate_acpi_sdt_hdr(const ACPISDTHeader *const hdr_ptr) {
    if (!hdr_ptr) {
        return false;
    }

    uint8_t check_sum = 0;
    const uint8_t *hdr_cptr = (const uint8_t*)hdr_ptr;
    for (uint32_t i = 0; i < hdr_ptr->Length; ++i) {
        check_sum += hdr_cptr[i];
    }

    return check_sum == 0;
}

static void *search_in_acpi_rsdt(const char *sign, const RSDT *const rsdp_ptr) {
    assert(sign);
    assert(rsdp_ptr);

    if (!validate_acpi_sdt_hdr(&rsdp_ptr->h)) {
        panic("search_in_acpi_rsdt: Failed to validate ACPI SDT header!\n");
    }

    const size_t entry_cnt = (rsdp_ptr->h.Length - sizeof(rsdp_ptr->h)) / 4;

    ACPISDTHeader *prev_hdr = NULL;
    for (size_t i = 0; i < entry_cnt; ++i) {

        ACPISDTHeader *curr_hdr = NULL;
        if (prev_hdr) {
            curr_hdr = (ACPISDTHeader*)mmio_remap_last_region(rsdp_ptr->PointerToOtherSDT[i], prev_hdr, 4, 4);
        } else {
            curr_hdr = (ACPISDTHeader*)mmio_map_region(rsdp_ptr->PointerToOtherSDT[i], 4);
        }
        prev_hdr = curr_hdr;

        if (!validate_acpi_sdt_hdr(curr_hdr)) {
            continue;
        }

        if (!strncmp(curr_hdr->Signature, sign, 4)) {
            return curr_hdr;
        }
    }

    return NULL;
}

static void *search_in_acpi_xsdt(const char *sign, const RSDT *const xsdp_ptr) {
    assert(sign);
    assert(xsdp_ptr);

    if (!validate_acpi_sdt_hdr(&xsdp_ptr->h)) {
        panic("search_in_acpi_xsdt: Failed to validate ACPI SDT header!\n");
    }

    const size_t entry_cnt = (xsdp_ptr->h.Length - sizeof(xsdp_ptr->h)) / 8;

    ACPISDTHeader *prev_hdr = NULL;
    for (size_t i = 0; i < entry_cnt; ++i) {

        ACPISDTHeader *curr_hdr = NULL;
        if (prev_hdr) {
            curr_hdr = (ACPISDTHeader*)mmio_remap_last_region(xsdp_ptr->PointerToOtherSDT[i], prev_hdr, 8, 8);
        } else {
            curr_hdr = (ACPISDTHeader*)mmio_map_region(xsdp_ptr->PointerToOtherSDT[i], 8);
        }
        prev_hdr = curr_hdr;

        if (!validate_acpi_sdt_hdr(curr_hdr)) {
            continue;
        }

        if (!strncmp(curr_hdr->Signature, sign, 4)) {
            return curr_hdr;
        }
    }

    return NULL;
}

void *
acpi_find_table(const char *sign) {
    /*
     * This function performs lookup of ACPI table by its signature
     * and returns valid pointer to the table mapped somewhere.
     *
     * It is a good idea to checksum tables before using them.
     *
     * HINT: Use mmio_map_region/mmio_remap_last_region
     * before accessing table addresses
     * (Why mmio_remap_last_region is requrired?)
     * HINT: RSDP address is stored in uefi_lp->ACPIRoot
     * HINT: You may want to distunguish RSDT/XSDT
     */

    assert(sign);

    RSDP *const rsdp_ptr = (RSDP*)mmio_map_region(uefi_lp->ACPIRoot, sizeof(RSDP));
    void *found_table = NULL;

    if (rsdp_ptr->Revision == 0) {
        if (!validate_acpi1(rsdp_ptr)) {
            cprintf("Failed to validate_acpi1!\n");
            return NULL;
        }

        RSDT *const rsdt_ptr = mmio_remap_last_region(rsdp_ptr->RsdtAddress, rsdp_ptr, sizeof(RSDP), sizeof(RSDT));
        found_table = search_in_acpi_rsdt(sign, rsdt_ptr);

    } else if (rsdp_ptr->Revision == 2) {
        if (!validate_acpi2(rsdp_ptr)) {
            cprintf("Failed to validate_acpi2!\n");
            return NULL;
        }

        RSDT *const xsdt_ptr = mmio_remap_last_region(rsdp_ptr->XsdtAddress, rsdp_ptr, sizeof(RSDP), sizeof(RSDT));
        found_table = search_in_acpi_xsdt(sign, xsdt_ptr);

    } else {
        cprintf("acpi_find_table: unsupported revision of RSDP = %d\n", rsdp_ptr->Revision);
        return NULL;
    }

    return found_table;
}
