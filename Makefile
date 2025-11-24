# nanos build: NASM for the boot sector and entry code, GCC (-m32) for the kernel.
CC      = gcc
LD      = ld
NASM    = nasm
CFLAGS  = -m32 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin -nostdlib -O2 -Wall -Wextra -std=gnu11 -Ikernel
LDFLAGS = -m elf_i386 -T kernel/linker.ld -nostdlib --no-warn-rwx-segments
QEMU    = qemu-system-i386

BUILD   = build
OBJS    = $(BUILD)/entry.o $(BUILD)/kernel.o $(BUILD)/console.o $(BUILD)/idt.o $(BUILD)/drivers.o \
          $(BUILD)/mem.o $(BUILD)/task.o $(BUILD)/shell.o $(BUILD)/string.o

all: $(BUILD)/os.img

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.bin: boot/boot.asm | $(BUILD)
	$(NASM) -f bin $< -o $@

$(BUILD)/entry.o: kernel/entry.asm | $(BUILD)
	$(NASM) -f elf32 $< -o $@

$(BUILD)/%.o: kernel/%.c kernel/kernel.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(OBJS) kernel/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	objcopy -O binary $< $@
	@size=$$(stat -c %s $@); if [ $$size -gt 65536 ]; then echo "kernel too big: $$size bytes"; exit 1; fi

# boot sector + kernel, padded to 128 sectors of kernel space
$(BUILD)/os.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $(BUILD)/boot.bin $(BUILD)/kernel.bin > $@
	truncate -s 66048 $@

run: $(BUILD)/os.img
	$(QEMU) -drive format=raw,file=$< -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04

# headless: the serial console on stdin/stdout, no window
headless: $(BUILD)/os.img
	$(QEMU) -drive format=raw,file=$< -display none -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04

test: $(BUILD)/os.img
	python3 tests/test_os.py

clean:
	rm -rf $(BUILD)

.PHONY: all run headless test clean
