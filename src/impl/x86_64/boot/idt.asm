global idt_load
global isr_stub_table

extern keyboard_handler

section .text

idt_load:
    lidt [rdi]
    ret

; Keyboard interrupt handler (IRQ1 = interrupt 33)
isr_stub_33:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    call keyboard_handler

    ; Send EOI (End of Interrupt) to PIC
    mov al, 0x20
    out 0x20, al

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

section .rodata
isr_stub_table:
    times 33 dq 0
    dq isr_stub_33
    times 222 dq 0
