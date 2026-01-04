#ifndef JOS_INC_MMIO_H
#define JOS_INC_MMIO_H

#include <inc/types.h>

void *mmio_map_region(physaddr_t addr, size_t size);
void *mmio_remap_last_region(physaddr_t addr, void *oldva, size_t oldsz, size_t size);

#endif // JOS_INC_MMIO_H
