# nanos

> 🇺🇸 [English version below](#english)

Um sistema operacional pequeno pra x86 de 32 bits, em assembly e C. Dá boot de uma imagem de disco crua, controla tela, teclado e porta serial, trata interrupções e exceções, gerencia memória, roda threads de kernel com escalonamento preemptivo e tem um shell. Nenhuma biblioteca, nenhum bootloader emprestado: do primeiro byte que a BIOS carrega até o prompt, está tudo aqui.

Esse é o último da série e o que eu mais tinha medo. A primeira versão travava antes de imprimir uma letra, e o motivo era ridículo: o `kmain` zerava a seção `.bss`... onde ficava a própria pilha, apagando o endereço de retorno. Achei com o QEMU logando a triple fault. Depois disso foi ladeira abaixo.

```sh
make            # precisa de nasm, gcc, binutils (alvo 32 bits) e qemu-system-i386
make run        # boot no QEMU com janela; o shell também sai pela serial
make headless   # só console serial, no terminal
make test       # dá boot na imagem e dirige o shell com um script
```

## Boot (`boot/boot.asm`)

A BIOS carrega o setor de 512 bytes em `0x7C00` em modo real. Ele lê o kernel do disco com o serviço estendido da BIOS (LBA), liga a linha A20, instala uma GDT plana, seta `CR0.PE` e faz um far jump pro modo protegido de 32 bits no entry point do kernel.

## Kernel (`kernel/`)

- `entry.asm`: o entry point (zera a `.bss` antes de qualquer `call`), 48 stubs de interrupção (32 exceções e 16 IRQs) que salvam registradores e chamam C, o `context_switch` e o trampolim que inicia uma thread nova.
- `console.c`: modo texto VGA com scroll e cursor, a serial 16550, e um `kprintf` que escreve nos dois. A porta isa-debug-exit do QEMU desliga a máquina.
- `idt.c`: a IDT, remapeamento e máscara dos 8259, relatório de exceções (breakpoint e divisão por zero são sobrevividos; o resto dá panic com o `eip`).
- `drivers.c`: o PIT a 100 Hz (que também dispara o escalonador) e o teclado PS/2 com shift e ring buffer.
- `mem.c`: alocador de frames de 4 KiB por bitmap (2 a 16 MiB) e um heap first-fit com divisão e coalescência pra `kmalloc`/`kfree`.
- `task.c`: threads de kernel, cada uma com a própria pilha preparada pra o primeiro context switch "retornar" no trampolim; a interrupção do timer chama o round-robin, então threads são preemptadas sem cooperar.
- `shell.c`: lê linhas do teclado ou da serial e roda `help`, `echo`, `mem`, `alloc`, `uptime`, `spawn`, `ps`, `int3`, `div0`, `clear`, `exit`.
- `kernel.c`: sobe tudo em ordem, roda um self-test de memória e inicia o shell.

O linker script coloca o kernel em `0x10000` com o entry primeiro; a imagem é o setor de boot seguido do binário chato do kernel.

Testes: `make test` monta a imagem, dá boot headless no QEMU com a serial num pipe, digita um script no shell e confere o transcrito: banner e self-test, `echo`, contagem de frames antes e depois de `alloc 100`, ticks do timer avançando entre dois `uptime`, duas threads que rodam até o fim enquanto o shell continua respondendo, as exceções de breakpoint e divisão por zero, comando desconhecido e o desligamento limpo (status 1 do QEMU).

---

## English

A small operating system for 32-bit x86, in assembly and C. It boots from a raw disk image, drives the screen, keyboard and serial port, handles interrupts and exceptions, manages memory, runs kernel threads with preemptive scheduling and has a shell. No library, no borrowed bootloader: from the first byte the BIOS loads up to the prompt, it's all here.

This is the last one of the series and the one I was most afraid of. The first version hung before printing a single letter, and the reason was ridiculous: `kmain` zeroed the `.bss` section... where the stack itself lived, wiping the return address. I found it with QEMU logging the triple fault. After that it was downhill.

```sh
make            # needs nasm, gcc, binutils (32-bit target) and qemu-system-i386
make run        # boots in QEMU with a window; the shell also comes out over serial
make headless   # serial console only, in the terminal
make test       # boots the image and drives the shell with a script
```

## Boot (`boot/boot.asm`)

The BIOS loads the 512-byte sector at `0x7C00` in real mode. It reads the kernel from disk with the BIOS extended service (LBA), turns on the A20 line, installs a flat GDT, sets `CR0.PE` and does a far jump into 32-bit protected mode at the kernel's entry point.

## Kernel (`kernel/`)

- `entry.asm`: the entry point (zeroes `.bss` before any `call`), 48 interrupt stubs (32 exceptions and 16 IRQs) that save registers and call C, the `context_switch` and the trampoline that starts a new thread.
- `console.c`: VGA text mode with scrolling and cursor, the 16550 serial port, and a `kprintf` that writes to both. QEMU's isa-debug-exit port powers the machine off.
- `idt.c`: the IDT, remapping and masking of the 8259s, exception reporting (breakpoint and divide by zero are survived; the rest panics with the `eip`).
- `drivers.c`: the PIT at 100 Hz (which also fires the scheduler) and the PS/2 keyboard with shift and a ring buffer.
- `mem.c`: a bitmap allocator of 4 KiB frames (2 to 16 MiB) and a first-fit heap with splitting and coalescing for `kmalloc`/`kfree`.
- `task.c`: kernel threads, each with its own stack prepared so the first context switch "returns" into the trampoline; the timer interrupt calls the round-robin, so threads are preempted without cooperating.
- `shell.c`: reads lines from the keyboard or the serial port and runs `help`, `echo`, `mem`, `alloc`, `uptime`, `spawn`, `ps`, `int3`, `div0`, `clear`, `exit`.
- `kernel.c`: brings everything up in order, runs a memory self-test and starts the shell.

The linker script places the kernel at `0x10000` with the entry first; the image is the boot sector followed by the kernel's flat binary.

Tests: `make test` builds the image, boots it headless in QEMU with the serial port on a pipe, types a script into the shell and checks the transcript: banner and self-test, `echo`, frame count before and after `alloc 100`, timer ticks advancing between two `uptime`s, two threads running to completion while the shell keeps responding, the breakpoint and divide-by-zero exceptions, an unknown command and the clean shutdown (QEMU status 1).

MIT.
