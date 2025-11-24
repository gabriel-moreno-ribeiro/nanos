/* Kernel threads with preemptive round-robin scheduling. */
#include "kernel.h"

#define MAX_TASKS 8
#define TASK_STACK 8192

static struct task tasks[MAX_TASKS];
static struct task *current;
static uint32_t next_id = 1;
static int lock_depth;
static uint32_t lock_flags;

extern void context_switch(uint32_t *old_esp, uint32_t new_esp);
extern void task_trampoline(void);

void task_lock(void) {
    uint32_t f = save_flags();
    cli();
    if (lock_depth++ == 0) lock_flags = f;
}

void task_unlock(void) {
    if (--lock_depth == 0) restore_flags(lock_flags);
}

void task_init(void) {
    memset(tasks, 0, sizeof(tasks));
    current = &tasks[0];
    current->id = next_id++;
    current->state = 1;
    memcpy(current->name, "kernel", 7);
}

struct task *task_current(void) { return current; }

/* Runs on the new task's stack the first time it is switched to. */
void task_start(void) {
    struct task *t = current;
    t->entry(t->arg);
    task_exit();
}

struct task *task_create(const char *name, void (*entry)(void *), void *arg) {
    task_lock();
    struct task *t = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == 0) {
            t = &tasks[i];
            break;
        }
    }
    if (!t) {
        task_unlock();
        return 0;
    }
    uint8_t *stack = kmalloc(TASK_STACK);
    if (!stack) {
        task_unlock();
        return 0;
    }
    memset(t, 0, sizeof(*t));
    t->id = next_id++;
    t->entry = entry;
    t->arg = arg;
    t->stack = stack;
    size_t n = strlen(name);
    if (n >= TASK_NAME_LEN) n = TASK_NAME_LEN - 1;
    memcpy(t->name, name, n);
    /* build the frame context_switch expects: edi, esi, ebx, ebp, then the return address */
    uint32_t *sp = (uint32_t *)(stack + TASK_STACK);
    *--sp = (uint32_t)task_trampoline;
    *--sp = 0; /* ebp */
    *--sp = 0; /* ebx */
    *--sp = 0; /* esi */
    *--sp = 0; /* edi */
    t->esp = (uint32_t)sp;
    t->state = 1;
    task_unlock();
    return t;
}

/* Picks the next runnable task after the current one and switches to it. */
void schedule(void) {
    if (lock_depth) return;
    uint32_t flags = save_flags();
    cli();
    int start = (int)(current - tasks);
    struct task *next = 0;
    for (int k = 1; k <= MAX_TASKS; k++) {
        struct task *t = &tasks[(start + k) % MAX_TASKS];
        if (t->state == 1) {
            next = t;
            break;
        }
    }
    if (next && next != current) {
        struct task *prev = current;
        current = next;
        next->switches++;
        context_switch(&prev->esp, next->esp);
    }
    restore_flags(flags);
}

void yield(void) { schedule(); }

void task_exit(void) {
    cli();
    current->state = 2;
    /* the stack is released when the slot is reused; switch away for good */
    for (;;) {
        schedule();
        sti();
        hlt();
    }
}

int task_list(struct task **out, int max) {
    int n = 0;
    for (int i = 0; i < MAX_TASKS && n < max; i++) {
        if (tasks[i].state) out[n++] = &tasks[i];
    }
    return n;
}
