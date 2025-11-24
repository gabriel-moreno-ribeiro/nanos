/* An interactive shell reading from the keyboard and the serial port. */
#include "kernel.h"

static volatile uint32_t counters[4];

static void counter_task(void *arg) {
    int slot = (int)(uintptr_t)arg;
    for (uint32_t i = 0; i < 200000; i++) {
        counters[slot] = i;
        if (i % 50000 == 0) yield();
    }
    counters[slot] = 200000;
    kprintf("[task %s done]\n", task_current()->name);
}

static int read_char(void) {
    for (;;) {
        int c = serial_getc();
        if (c < 0) c = keyboard_getc();
        if (c >= 0) return c;
        hlt();
    }
}

static void read_line(char *buf, int max) {
    int n = 0;
    for (;;) {
        int c = read_char();
        if (c == '\r' || c == '\n') {
            putc('\n');
            buf[n] = 0;
            return;
        }
        if (c == '\b' || c == 127) {
            if (n > 0) {
                n--;
                puts("\b \b");
            }
            continue;
        }
        if (c >= 32 && c < 127 && n < max - 1) {
            buf[n++] = (char)c;
            putc((char)c);
        }
    }
}

static void cmd_help(void) {
    puts("commands:\n"
         "  help            this text\n"
         "  echo <text>     print text\n"
         "  mem             frame and heap statistics\n"
         "  alloc <n>       allocate n frames, then free them\n"
         "  uptime          timer ticks since boot\n"
         "  spawn [name]    start a counting thread\n"
         "  ps              list tasks\n"
         "  int3            raise a breakpoint exception\n"
         "  div0            divide by zero\n"
         "  clear           clear the screen\n"
         "  exit            power off (QEMU)\n");
}

static void cmd_mem(void) {
    kprintf("frames: %u free of %u (%u KiB free)\n", frames_free(), frames_total(), frames_free() * 4);
    kprintf("heap: %u bytes in use\n", heap_used());
}

static void cmd_alloc(const char *arg) {
    int n = atoi(arg);
    if (n <= 0 || n > 1024) {
        puts("alloc: give a count between 1 and 1024\n");
        return;
    }
    void **frames = kmalloc(sizeof(void *) * (size_t)n);
    if (!frames) {
        puts("alloc: out of heap\n");
        return;
    }
    int got = 0;
    for (int i = 0; i < n; i++) {
        frames[i] = frame_alloc();
        if (!frames[i]) break;
        memset(frames[i], 0xA5, 4096); /* touch the memory */
        got++;
    }
    kprintf("allocated %d frames, first at %x, %u free now\n", got, got ? (uint32_t)frames[0] : 0, frames_free());
    for (int i = 0; i < got; i++) frame_free(frames[i]);
    kfree(frames);
    kprintf("freed them, %u free\n", frames_free());
}

static void cmd_ps(void) {
    struct task *list[8];
    int n = task_list(list, 8);
    puts("id  state     switches  name\n");
    for (int i = 0; i < n; i++) {
        kprintf("%-3u %-9s %-9u %s\n", list[i]->id, list[i]->state == 1 ? "runnable" : "finished", list[i]->switches, list[i]->name);
    }
    kprintf("counters: %u %u %u %u\n", counters[0], counters[1], counters[2], counters[3]);
}

static int spawned;

static void cmd_spawn(const char *arg) {
    char name[TASK_NAME_LEN];
    if (*arg) {
        size_t n = strlen(arg);
        if (n >= TASK_NAME_LEN) n = TASK_NAME_LEN - 1;
        memcpy(name, arg, n);
        name[n] = 0;
    } else {
        memcpy(name, "count", 6);
    }
    int slot = spawned % 4;
    struct task *t = task_create(name, counter_task, (void *)(uintptr_t)slot);
    if (!t) {
        puts("spawn: no free task slot\n");
        return;
    }
    spawned++;
    kprintf("spawned task %u (%s) on counter %d\n", t->id, t->name, slot);
}

/* A real `div` by zero (the compiler would happily optimise a C division away); the handler skips the 2-byte instruction. */
static void div0(void) {
    __asm__ volatile("xor %%ecx, %%ecx\n\tmov $1, %%eax\n\txor %%edx, %%edx\n\tdiv %%ecx" : : : "eax", "ecx", "edx");
}

void shell_run(void) {
    char line[128];
    for (;;) {
        puts("nanos> ");
        read_line(line, sizeof line);
        char *arg = line;
        while (*arg && *arg != ' ') arg++;
        if (*arg) *arg++ = 0;
        while (*arg == ' ') arg++;
        if (!line[0]) continue;
        if (!strcmp(line, "help")) cmd_help();
        else if (!strcmp(line, "echo")) kprintf("%s\n", arg);
        else if (!strcmp(line, "mem")) cmd_mem();
        else if (!strcmp(line, "alloc")) cmd_alloc(arg);
        else if (!strcmp(line, "uptime")) kprintf("%u ticks (%u.%02u s at %u Hz)\n", ticks, ticks / timer_hz(), (ticks % timer_hz()) * 100 / timer_hz(), timer_hz());
        else if (!strcmp(line, "spawn")) cmd_spawn(arg);
        else if (!strcmp(line, "ps")) cmd_ps();
        else if (!strcmp(line, "int3")) __asm__ volatile("int $3");
        else if (!strcmp(line, "div0")) div0();
        else if (!strcmp(line, "clear")) vga_clear();
        else if (!strcmp(line, "exit")) {
            puts("bye\n");
            qemu_exit(0);
        } else kprintf("unknown command: %s (try help)\n", line);
    }
}
