#include "idt.h"

struct IDT_Entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct IDT_Pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct IDT_Entry idt[256];
struct IDT_Pointer idtr;

extern void* isr_stub_table[];

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    struct IDT_Entry* descriptor = &idt[vector];

    descriptor->offset_low = (uint64_t)isr & 0xFFFF;
    descriptor->selector = 0x08; // GDT code segment
    descriptor->ist = 0;
    descriptor->type_attr = flags;
    descriptor->offset_mid = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->offset_high = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->zero = 0;
}

extern void idt_load(uint64_t);

void idt_init() {
    // Clear IDT
    for (int i = 0; i < 256; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attr = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].zero = 0;
    }

    idtr.limit = (uint16_t)(sizeof(struct IDT_Entry) * 256 - 1);
    idtr.base = (uint64_t)&idt;

    // Set up keyboard interrupt (IRQ1 = interrupt 33)
    if (isr_stub_table[33] != 0) {
        idt_set_descriptor(33, isr_stub_table[33], 0x8E);
    }

    idt_load((uint64_t)&idtr);

    // Enable interrupts
    __asm__ volatile("sti");
}
