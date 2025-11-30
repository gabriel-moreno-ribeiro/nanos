# nanos

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

**EN:** a small 32-bit x86 operating system in assembly and C: a real-mode boot sector that loads the kernel and enters protected mode, VGA and serial console, IDT/PIC with exception handling, PIT and PS/2 keyboard drivers, a frame allocator and a coalescing heap, preemptively scheduled kernel threads, and a shell. `make test` boots it in QEMU and drives the shell over the serial port. MIT.
