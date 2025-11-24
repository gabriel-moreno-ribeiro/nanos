/* Physical page frames (bitmap allocator) and a first-fit kernel heap. */
#include "kernel.h"

#define FRAME_SIZE 4096
#define FRAME_BASE 0x00200000u /* frames start at 2 MiB, above the kernel and heap */
#define FRAME_END 0x01000000u  /* and end at 16 MiB */
#define FRAME_COUNT ((FRAME_END - FRAME_BASE) / FRAME_SIZE)

static uint32_t bitmap[FRAME_COUNT / 32];
static uint32_t free_frames;

extern uint8_t __kernel_end[];
#define HEAP_START 0x00100000u /* heap: 1 MiB .. 2 MiB */
#define HEAP_END 0x00200000u

struct block {
    uint32_t size; /* payload size, low bit set when free */
    struct block *next;
};
static struct block *heap_head;
static uint32_t heap_in_use;

void mem_init(void) {
    memset(bitmap, 0, sizeof(bitmap));
    free_frames = FRAME_COUNT;
    heap_head = (struct block *)HEAP_START;
    heap_head->size = (HEAP_END - HEAP_START - sizeof(struct block)) | 1;
    heap_head->next = 0;
    heap_in_use = 0;
    if ((uint32_t)__kernel_end > HEAP_START) panic("kernel overlaps the heap");
}

void *frame_alloc(void) {
    for (uint32_t i = 0; i < FRAME_COUNT / 32; i++) {
        if (bitmap[i] == 0xFFFFFFFFu) continue;
        for (int b = 0; b < 32; b++) {
            if (!(bitmap[i] & (1u << b))) {
                bitmap[i] |= 1u << b;
                free_frames--;
                return (void *)(FRAME_BASE + (i * 32 + b) * FRAME_SIZE);
            }
        }
    }
    return 0;
}

void frame_free(void *frame) {
    uint32_t addr = (uint32_t)frame;
    if (addr < FRAME_BASE || addr >= FRAME_END || addr % FRAME_SIZE) panic("bad frame free");
    uint32_t n = (addr - FRAME_BASE) / FRAME_SIZE;
    if (!(bitmap[n / 32] & (1u << (n % 32)))) panic("double frame free");
    bitmap[n / 32] &= ~(1u << (n % 32));
    free_frames++;
}

uint32_t frames_free(void) { return free_frames; }
uint32_t frames_total(void) { return FRAME_COUNT; }

static uint32_t block_size(const struct block *b) { return b->size & ~1u; }
static int block_free(const struct block *b) { return b->size & 1; }

void *kmalloc(size_t size) {
    size = (size + 7) & ~7u;
    if (size == 0) size = 8;
    uint32_t flags = save_flags();
    cli();
    for (struct block *b = heap_head; b; b = b->next) {
        if (!block_free(b) || block_size(b) < size) continue;
        if (block_size(b) >= size + sizeof(struct block) + 8) {
            /* split off the remainder */
            struct block *rest = (struct block *)((uint8_t *)(b + 1) + size);
            rest->size = (block_size(b) - size - sizeof(struct block)) | 1;
            rest->next = b->next;
            b->next = rest;
            b->size = size;
        } else {
            b->size = block_size(b);
        }
        heap_in_use += block_size(b);
        restore_flags(flags);
        return b + 1;
    }
    restore_flags(flags);
    return 0;
}

void kfree(void *ptr) {
    if (!ptr) return;
    uint32_t flags = save_flags();
    cli();
    struct block *b = (struct block *)ptr - 1;
    if (block_free(b)) panic("double free");
    heap_in_use -= block_size(b);
    b->size |= 1;
    /* merge with following free blocks */
    while (b->next && block_free(b->next)) {
        b->size = (block_size(b) + sizeof(struct block) + block_size(b->next)) | 1;
        b->next = b->next->next;
    }
    /* merge with the previous block if it is free */
    for (struct block *p = heap_head; p && p->next; p = p->next) {
        if (p->next == b && block_free(p)) {
            p->size = (block_size(p) + sizeof(struct block) + block_size(b)) | 1;
            p->next = b->next;
            break;
        }
    }
    restore_flags(flags);
}

uint32_t heap_used(void) { return heap_in_use; }
