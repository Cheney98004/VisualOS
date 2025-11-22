#include <stdint.h>
#include <stddef.h>

#define FRAME_SIZE 4096

extern uint8_t _kernel_end;   // from linker

static uint8_t* pmm_bitmap = NULL;
static uint32_t total_frames = 0;
static uint32_t used_frames  = 0;

static inline void bitmap_set(uint32_t frame)
{
    pmm_bitmap[frame / 8] |= (1 << (frame % 8));
}

static inline void bitmap_clear(uint32_t frame)
{
    pmm_bitmap[frame / 8] &= ~(1 << (frame % 8));
}

static inline int bitmap_test(uint32_t frame)
{
    return pmm_bitmap[frame / 8] & (1 << (frame % 8));
}

static inline uintptr_t pmm_base_address(void)
{
    return ((uintptr_t)&_kernel_end + 0xFFF) & ~0xFFF;
}

void pmm_init(uint32_t mem_size)
{
    total_frames = mem_size / FRAME_SIZE;

    uintptr_t bmp_addr = pmm_base_address();
    pmm_bitmap = (uint8_t*)bmp_addr;

    size_t bmp_bytes = total_frames / 8;
    for (size_t i = 0; i < bmp_bytes; i++)
        pmm_bitmap[i] = 0;

    used_frames = 0;
}

void* pmm_alloc(void)
{
    uintptr_t base = pmm_base_address();

    for (uint32_t f = 0; f < total_frames; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames++;
            return (void*)(base + f * FRAME_SIZE);
        }
    }
    return NULL;
}

void pmm_free(void* addr)
{
    uintptr_t base = pmm_base_address();

    if ((uintptr_t)addr < base)
        return;

    uint32_t f = ((uintptr_t)addr - base) / FRAME_SIZE;

    if (f >= total_frames)
        return;

    bitmap_clear(f);
    used_frames--;
}

uint32_t pmm_total_frames(){ return total_frames; }
uint32_t pmm_used_frames(){ return used_frames; }
uint32_t pmm_free_frames(){ return total_frames - used_frames; }
