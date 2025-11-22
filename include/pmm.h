#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

void   pmm_init(uint32_t memory_size);

void*  pmm_alloc(void);
void   pmm_free(void* frame);

size_t pmm_total_frames(void);
size_t pmm_used_frames(void);
size_t pmm_free_frames(void);

#endif
