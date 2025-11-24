; Kernel entry, interrupt stubs and the context switch.
        bits 32
        section .text.entry
        global _start
        extern kmain
        extern __bss_start
        extern __bss_end

_start:
        mov esp, stack_top
        cld
        ; clear .bss (which contains this very stack, so it must happen before any call)
        mov edi, __bss_start
        mov ecx, __bss_end
        sub ecx, edi
        shr ecx, 2
        xor eax, eax
        rep stosd
        call kmain
.hang:  hlt
        jmp .hang

; --------------------------------------------------------------- interrupt stubs
; Every vector pushes a fake error code when the CPU does not, then its number,
; and jumps to a common stub that saves registers and calls isr_dispatch(frame).
        section .text
        extern isr_dispatch

%macro ISR_NOERR 1
        global isr%1
isr%1:
        push dword 0
        push dword %1
        jmp isr_common
%endmacro

%macro ISR_ERR 1
        global isr%1
isr%1:
        push dword %1
        jmp isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31
%assign i 32
%rep 16
ISR_NOERR i
%assign i i+1
%endrep

isr_common:
        pusha                       ; edi, esi, ebp, esp, ebx, edx, ecx, eax
        push ds
        push es
        mov ax, 0x10
        mov ds, ax
        mov es, ax
        push esp                    ; pointer to the frame
        call isr_dispatch
        add esp, 4
        pop es
        pop ds
        popa
        add esp, 8                  ; vector and error code
        iret

; table of stub addresses for idt_init
        section .rodata
        global isr_table
isr_table:
%assign i 0
%rep 48
        dd isr %+ i
%assign i i+1
%endrep

; ------------------------------------------------------------- context switch
; void context_switch(uint32_t *old_esp, uint32_t new_esp)
; Saves the callee-saved registers on the current stack, stores esp into
; *old_esp, loads the new stack and pops the other task's registers.
        section .text
        global context_switch
context_switch:
        mov eax, [esp + 4]          ; old_esp pointer
        mov edx, [esp + 8]          ; new esp
        push ebp
        push ebx
        push esi
        push edi
        mov [eax], esp
        mov esp, edx
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret

; Fresh tasks start here (their stack was built so that context_switch
; "returns" into this trampoline): enable interrupts and enter task_start.
        global task_trampoline
        extern task_start
task_trampoline:
        sti
        call task_start
.hang:  hlt
        jmp .hang

        section .bss
        align 16
stack_bottom: resb 16384
stack_top:
