#include <inc/types.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/stdio.h>
#include <inc/x86.h>
#include <inc/uefi.h>
#include <kern/timer.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/trap.h>

#define kilo      (1000ULL)
#define Mega      (kilo * kilo)
#define Giga      (kilo * Mega)
#define Tera      (kilo * Giga)
#define Peta      (kilo * Tera)
#define ULONG_MAX ~0UL

#if LAB <= 6
/* Early variant of memory mapping that does 1:1 aligned area mapping
 * in 2MB pages. You will need to reimplement this code with proper
 * virtual memory mapping in the future. */
void *
mmio_map_region(physaddr_t pa, size_t size) {
    void map_addr_early_boot(uintptr_t addr, uintptr_t addr_phys, size_t sz);
    const physaddr_t base_2mb = 0x200000;
    uintptr_t org = pa;
    size += pa & (base_2mb - 1);
    size += (base_2mb - 1);
    pa &= ~(base_2mb - 1);
    size &= ~(base_2mb - 1);
    map_addr_early_boot(pa, pa, size);
    return (void *)org;
}
void *
mmio_remap_last_region(physaddr_t pa, void *addr, size_t oldsz, size_t newsz) {
    return mmio_map_region(pa, newsz);
}
#endif

struct Timer timertab[MAX_TIMERS];
struct Timer *timer_for_schedule;

struct Timer timer_hpet0 = {
        .timer_name = "hpet0",
        .timer_init = hpet_init,
        .get_cpu_freq = hpet_cpu_frequency,
        .enable_interrupts = hpet_enable_interrupts_tim0,
        .handle_interrupts = hpet_handle_interrupts_tim0,
};

struct Timer timer_hpet1 = {
        .timer_name = "hpet1",
        .timer_init = hpet_init,
        .get_cpu_freq = hpet_cpu_frequency,
        .enable_interrupts = hpet_enable_interrupts_tim1,
        .handle_interrupts = hpet_handle_interrupts_tim1,
};

struct Timer timer_acpipm = {
        .timer_name = "pm",
        .timer_init = acpi_enable,
        .get_cpu_freq = pmtimer_cpu_frequency,
};

void
acpi_enable(void) {
    FADT *fadt = get_fadt();
    outb(fadt->SMI_CommandPort, fadt->AcpiEnable);
    while ((inw(fadt->PM1aControlBlock) & 1) == 0) /* nothing */
        ;
}

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

static void *
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

/* Obtain and map FADT ACPI table address. */
FADT *
get_fadt(void) {
    static FADT *fadt_ptr = NULL;
    fadt_ptr = acpi_find_table("FACP");

    return fadt_ptr;
}

/* Obtain and map RSDP ACPI table address. */
HPET *
get_hpet(void) {
    static HPET* hpet_ptr = NULL;
    hpet_ptr = acpi_find_table("HPET");

    return hpet_ptr;
}

/* Getting physical HPET timer address from its table. */
HPETRegister *
hpet_register(void) {
    HPET *hpet_timer = get_hpet();
    if (!hpet_timer->address.address) panic("hpet is unavailable\n");

    uintptr_t paddr = hpet_timer->address.address;
    return mmio_map_region(paddr, sizeof(HPETRegister));
}

/* Debug HPET timer state. */
void
hpet_print_struct(void) {
    HPET *hpet = get_hpet();
    assert(hpet != NULL);
    cprintf("signature = %s\n", (hpet->h).Signature);
    cprintf("length = %08x\n", (hpet->h).Length);
    cprintf("revision = %08x\n", (hpet->h).Revision);
    cprintf("checksum = %08x\n", (hpet->h).Checksum);

    cprintf("oem_revision = %08x\n", (hpet->h).OEMRevision);
    cprintf("creator_id = %08x\n", (hpet->h).CreatorID);
    cprintf("creator_revision = %08x\n", (hpet->h).CreatorRevision);

    cprintf("hardware_rev_id = %08x\n", hpet->hardware_rev_id);
    cprintf("comparator_count = %08x\n", hpet->comparator_count);
    cprintf("counter_size = %08x\n", hpet->counter_size);
    cprintf("reserved = %08x\n", hpet->reserved);
    cprintf("legacy_replacement = %08x\n", hpet->legacy_replacement);
    cprintf("pci_vendor_id = %08x\n", hpet->pci_vendor_id);
    cprintf("hpet_number = %08x\n", hpet->hpet_number);
    cprintf("minimum_tick = %08x\n", hpet->minimum_tick);

    cprintf("address_structure:\n");
    cprintf("address_space_id = %08x\n", (hpet->address).address_space_id);
    cprintf("register_bit_width = %08x\n", (hpet->address).register_bit_width);
    cprintf("register_bit_offset = %08x\n", (hpet->address).register_bit_offset);
    cprintf("address = %08lx\n", (unsigned long)(hpet->address).address);
}

static volatile HPETRegister *hpetReg;
/* HPET timer period (in femtoseconds) */
static uint64_t hpetFemto = 0;
/* HPET timer frequency */
static uint64_t hpetFreq = 0;

/* HPET timer initialisation */
void
hpet_init() {
    if (hpetReg == NULL) {
        nmi_disable();
        hpetReg = hpet_register();
        uint64_t cap = hpetReg->GCAP_ID;
        hpetFemto = (uintptr_t)(cap >> 32);
        if (!(cap & HPET_LEG_RT_CAP)) panic("HPET has no LegacyReplacement mode");

        // cprintf("hpetFemto = %llu\n", hpetFemto);
        hpetFreq = (1 * Peta) / hpetFemto;
        // cprintf("HPET: Frequency = %d.%03dMHz\n", (uintptr_t)(hpetFreq / Mega), (uintptr_t)(hpetFreq % Mega));
        /* Enable ENABLE_CNF bit to enable timer */
        hpetReg->GEN_CONF |= HPET_ENABLE_CNF;
        nmi_enable();
    }
}

/* HPET register contents debugging. */
void
hpet_print_reg(void) {
    cprintf("GCAP_ID = %016lx\n", (unsigned long)hpetReg->GCAP_ID);
    cprintf("GEN_CONF = %016lx\n", (unsigned long)hpetReg->GEN_CONF);
    cprintf("GINTR_STA = %016lx\n", (unsigned long)hpetReg->GINTR_STA);
    cprintf("MAIN_CNT = %016lx\n", (unsigned long)hpetReg->MAIN_CNT);
    cprintf("TIM0_CONF = %016lx\n", (unsigned long)hpetReg->TIM0_CONF);
    cprintf("TIM0_COMP = %016lx\n", (unsigned long)hpetReg->TIM0_COMP);
    cprintf("TIM0_FSB = %016lx\n", (unsigned long)hpetReg->TIM0_FSB);
    cprintf("TIM1_CONF = %016lx\n", (unsigned long)hpetReg->TIM1_CONF);
    cprintf("TIM1_COMP = %016lx\n", (unsigned long)hpetReg->TIM1_COMP);
    cprintf("TIM1_FSB = %016lx\n", (unsigned long)hpetReg->TIM1_FSB);
    cprintf("TIM2_CONF = %016lx\n", (unsigned long)hpetReg->TIM2_CONF);
    cprintf("TIM2_COMP = %016lx\n", (unsigned long)hpetReg->TIM2_COMP);
    cprintf("TIM2_FSB = %016lx\n", (unsigned long)hpetReg->TIM2_FSB);
}

/* HPET main timer counter value. */
uint64_t
hpet_get_main_cnt(void) {
    return hpetReg->MAIN_CNT;
}

/* - Configure HPET timer 0 to trigger every 0.5 seconds on IRQ_TIMER line
 * - Configure HPET timer 1 to trigger every 1.5 seconds on IRQ_CLOCK line
 *
 * HINT To be able to use HPET as PIT replacement consult
 *      LegacyReplacement functionality in HPET spec.
 * HINT Don't forget to unmask interrupt in PIC */
void
hpet_enable_interrupts_tim0(void) {
    hpetReg->GEN_CONF |= HPET_LEG_RT_CNF;
    hpetReg->TIM0_CONF = HPET_TN_TYPE_CNF | HPET_TN_INT_ENB_CNF | (IRQ_TIMER << 9);
    hpetReg->TIM0_COMP = hpetFreq / 2;
    pic_irq_unmask(IRQ_TIMER);
}

void
hpet_enable_interrupts_tim1(void) {
    hpetReg->GEN_CONF |= HPET_LEG_RT_CNF;
    hpetReg->TIM1_CONF = HPET_TN_TYPE_CNF | HPET_TN_INT_ENB_CNF | (IRQ_CLOCK << 9);
    hpetReg->TIM1_COMP = hpetFreq / 2 * 3;
    pic_irq_unmask(IRQ_CLOCK);
}

void
hpet_handle_interrupts_tim0(void) {
    pic_send_eoi(IRQ_TIMER);
}

void
hpet_handle_interrupts_tim1(void) {
    pic_send_eoi(IRQ_CLOCK);
}

/* Calculate CPU frequency in Hz with the help with HPET timer.
 * HINT Use hpet_get_main_cnt function and do not forget about
 * about pause instruction. */
uint64_t
hpet_cpu_frequency(void) {
    static uint64_t cpu_freq = 0;
    if (cpu_freq != 0) {
        return cpu_freq;
    }

    const uint64_t start_hpet_cnt = hpet_get_main_cnt();
    const uint64_t start_clocks = read_tsc();

    uint64_t end_hpet_cnt = start_hpet_cnt;
    while (end_hpet_cnt - start_hpet_cnt < hpetFreq / 1000) {
        end_hpet_cnt = hpet_get_main_cnt();
        asm volatile("pause");
    }

    cpu_freq = (read_tsc() - start_clocks) * 1000;
    return cpu_freq;
}

uint32_t
pmtimer_get_timeval(void) {
    FADT *fadt = get_fadt();
    return inl(fadt->PMTimerBlock);
}

bool
pmtimer_is_32_bit(void) {
    FADT *fadt = get_fadt();
    return fadt->Flags & (1 << 8);
}

/* Calculate CPU frequency in Hz with the help with ACPI PowerManagement timer.
 * HINT Use pmtimer_get_timeval function and do not forget that ACPI PM timer
 *      can be 24-bit or 32-bit. */
uint64_t
pmtimer_cpu_frequency(void) {
    static uint64_t cpu_freq;
    if (cpu_freq != 0) {
        return cpu_freq;
    }

    uint64_t mask = 0;
    if (pmtimer_is_32_bit()) {
        mask = 0xFFFFFFFF;
    } else {
        mask = 0xFFFFFF;
    }

    const uint32_t start_pm_cnt = pmtimer_get_timeval();
    const uint64_t start_clocks = read_tsc();

    uint32_t delta = 0;
    while (delta < PM_FREQ / 1000) {
        delta = (pmtimer_get_timeval() - start_pm_cnt) & mask;
        asm volatile("pause");
    }

    cpu_freq = (read_tsc() - start_clocks) * 1000;

    return cpu_freq;
}
