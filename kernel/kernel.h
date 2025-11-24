#ifndef NANOS_KERNEL_H
#define NANOS_KERNEL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* ---- port I/O ------------------------------------------------------------ */
static inline void outb(uint16_t port, uint8_t value) { __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port)); return v; }
static inline void outl(uint16_t port, uint32_t value) { __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port)); }
static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }
static inline uint32_t save_flags(void) { uint32_t f; __asm__ volatile("pushf; pop %0" : "=r"(f)); return f; }
static inline void restore_flags(uint32_t f) { __asm__ volatile("push %0; popf" : : "r"(f) : "memory", "cc"); }

/* ---- string.c ------------------------------------------------------------ */
void *memset(void *dst, int value, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *utoa(uint32_t value, char *buf, unsigned base);
int atoi(const char *s);

/* ---- console (vga.c, serial.c, printf.c) --------------------------------- */
void vga_init(void);
void vga_putc(char c);
void vga_clear(void);
void serial_init(void);
void serial_putc(char c);
int serial_getc(void); /* -1 when nothing is waiting */
void putc(char c);
void puts(const char *s);
void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, va_list ap);

/* ---- interrupts (idt.c) -------------------------------------------------- */
struct frame {
    uint32_t es, ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t vector, error;
    uint32_t eip, cs, eflags;
};
typedef void (*irq_handler_t)(struct frame *);
void idt_init(void);
void irq_register(unsigned irq, irq_handler_t handler);
extern volatile uint32_t exception_count;

/* ---- timer.c / keyboard.c ------------------------------------------------ */
void timer_init(unsigned hz);
extern volatile uint32_t ticks;
unsigned timer_hz(void);
void keyboard_init(void);
int keyboard_getc(void); /* -1 when the buffer is empty */

/* ---- mem.c --------------------------------------------------------------- */
void mem_init(void);
void *frame_alloc(void);
void frame_free(void *frame);
uint32_t frames_free(void);
uint32_t frames_total(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
uint32_t heap_used(void);

/* ---- task.c -------------------------------------------------------------- */
#define TASK_NAME_LEN 16
struct task {
    uint32_t esp;
    uint32_t id;
    char name[TASK_NAME_LEN];
    int state; /* 0 free, 1 runnable, 2 finished */
    uint32_t switches;
    void (*entry)(void *);
    void *arg;
    uint8_t *stack;
};
void task_init(void);
struct task *task_create(const char *name, void (*entry)(void *), void *arg);
void schedule(void);
void yield(void);
void task_exit(void);
struct task *task_current(void);
int task_list(struct task **out, int max);
void task_lock(void);
void task_unlock(void);

/* ---- shell.c ------------------------------------------------------------- */
void shell_run(void);

/* ---- misc ---------------------------------------------------------------- */
void qemu_exit(uint32_t code);
void panic(const char *msg);

#endif
