; Boot sector: the BIOS loads these 512 bytes to 0x7C00 in 16-bit real mode.
; We load the kernel from the disk to 0x10000, enable the A20 line, install
; a flat GDT, switch to 32-bit protected mode and jump to the kernel.

KERNEL_LOAD_SEG  equ 0x1000       ; kernel goes to 0x1000:0000 = 0x10000
KERNEL_SECTORS   equ 128          ; 64 KiB, read in chunks of 32 sectors
CHUNK            equ 32

        bits 16
        org 0x7C00

start:
        cli
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0x7C00
        sti
        mov [boot_drive], dl

        mov si, msg_loading
        call print

        ; --- read the kernel with the BIOS extended read (LBA) service ---
        mov cx, KERNEL_SECTORS / CHUNK
        mov word [dap_lba], 1             ; kernel starts right after the boot sector
        mov word [dap_seg], KERNEL_LOAD_SEG
.read_chunk:
        push cx
        mov si, dap
        mov ah, 0x42
        mov dl, [boot_drive]
        int 0x13
        jc disk_error
        add word [dap_lba], CHUNK
        add word [dap_seg], CHUNK * 512 / 16
        pop cx
        loop .read_chunk

        ; --- enable A20 through the fast gate ---
        in al, 0x92
        or al, 2
        and al, 0xFE
        out 0x92, al

        ; --- protected mode ---
        cli
        lgdt [gdt_descriptor]
        mov eax, cr0
        or eax, 1
        mov cr0, eax
        jmp 0x08:protected_start

disk_error:
        mov si, msg_disk
        call print
        jmp $

; print a NUL-terminated string with the BIOS teletype service
print:
        mov ah, 0x0E
.next:  lodsb
        test al, al
        jz .done
        int 0x10
        jmp .next
.done:  ret

        bits 32
protected_start:
        mov ax, 0x10
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax
        mov esp, 0x90000
        jmp 0x08:0x10000                  ; kernel entry point

; --- data ---
boot_drive: db 0
msg_loading: db "nanos boot", 13, 10, 0
msg_disk:    db "disk read failed", 13, 10, 0

        align 4
dap:                                       ; disk address packet for int 13h / ah=42h
        db 0x10, 0
        dw CHUNK                           ; sectors to read
        dw 0                               ; destination offset
dap_seg: dw 0                              ; destination segment
dap_lba: dq 0                              ; starting LBA

gdt:
        dq 0                               ; null descriptor
        dq 0x00CF9A000000FFFF              ; code: base 0, limit 4G, 32-bit, ring 0
        dq 0x00CF92000000FFFF              ; data: base 0, limit 4G, writable
gdt_descriptor:
        dw gdt_descriptor - gdt - 1
        dd gdt

        times 510 - ($ - $$) db 0
        dw 0xAA55
