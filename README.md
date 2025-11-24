# nanos

A small operating system for 32-bit x86, written from scratch in assembly
and C. It boots from a raw disk image, drives the screen, keyboard and
serial port, handles interrupts and exceptions, manages memory, runs
preemptively scheduled kernel threads, and offers an interactive shell.
No libraries, no bootloader borrowed from elsewhere: everything from the
first byte the BIOS loads is in this repository.

```sh
make            # needs nasm, gcc, binutils (32-bit target) and qemu-system-i386
make run        # boot in QEMU with a window; the shell is also on the serial console
make headless   # serial console only, in the terminal
make test       # boots the image and drives the shell from a script
```

## Boot (`boot/boot.asm`)

The BIOS loads the 512-byte boot sector to `0x7C00` in real mode. It reads
the kernel from the disk with the extended BIOS read service (LBA), enables
the A20 line, installs a flat GDT, sets `CR0.PE` and far-jumps into 32-bit
protected mode at the kernel's entry point.

## Kernel (`kernel/`)

- `entry.asm` — the entry point, 48 interrupt stubs (32 CPU exceptions and
  16 IRQs) that save the registers and call C, the `context_switch`
  routine and the trampoline that starts a new thread.
- `console.c` — VGA text mode with scrolling and a hardware cursor, the
  16550 serial port, and a `kprintf` that writes to both. QEMU's
  isa-debug-exit port is used to power off.
- `idt.c` — the interrupt descriptor table, 8259 PIC remapping and
  masking, exception reporting (breakpoints and divide errors are
  survived; anything else panics with the faulting `eip`).
- `drivers.c` — the programmable interval timer at 100 Hz (which also
  drives the scheduler) and a PS/2 keyboard driver with shift handling and
  a ring buffer.
- `mem.c` — a bitmap allocator for 4 KiB physical frames (2..16 MiB) and a
  first-fit heap with splitting and coalescing for `kmalloc`/`kfree`.
- `task.c` — kernel threads: each gets its own stack prepared so that the
  first context switch "returns" into the trampoline; the timer interrupt
  calls the round-robin scheduler, so threads are preempted without
  cooperating; `yield` and `task_exit` are available too.
- `shell.c` — reads lines from the keyboard or the serial port and runs
  commands: `help`, `echo`, `mem`, `alloc`, `uptime`, `spawn`, `ps`,
  `int3`, `div0`, `clear`, `exit`.
- `kernel.c` — brings everything up in order, runs a memory self-test and
  starts the shell.

The linker script places the kernel at `0x10000` with the entry code
first; the image is the boot sector followed by the flat kernel binary.

## Tests

`make test` builds the image, boots it headless in QEMU with the serial
port on a pipe, types a script into the shell and checks the transcript:
the banner and memory self-test, `echo`, frame accounting before and after
`alloc 100`, timer ticks that advance between two `uptime` calls, two
spawned threads that run to completion while the shell keeps working, the
breakpoint and divide-by-zero exception reports, an unknown command, and a
clean power-off through the debug exit device (QEMU exit status 1).

## License

MIT
