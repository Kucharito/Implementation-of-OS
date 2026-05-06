#include <stdint.h>

#define IDT_ENTRIES 256
#define KERNEL_CODE_SELECTOR 0x08
#define IDT_FLAG_INTERRUPT_GATE 0x8E

typedef struct {
	uint16_t offset_1;
	uint16_t selector;
	uint8_t zero;
	uint8_t type_attr;
	uint16_t offset_2;
} __attribute__((packed)) idt_entry_t;

typedef struct {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed)) idtr_t;

static idt_entry_t idt[IDT_ENTRIES] __attribute__((aligned(16)));
static idtr_t idtr;

void idt_set_gate(int n, uint32_t handler)
{
	if (n < 0 || n >= IDT_ENTRIES)
		return;

	idt[n].offset_1 = (uint16_t)(handler & 0xFFFFu);
	idt[n].selector = KERNEL_CODE_SELECTOR;
	idt[n].zero = 0;
	idt[n].type_attr = IDT_FLAG_INTERRUPT_GATE;
	idt[n].offset_2 = (uint16_t)((handler >> 16) & 0xFFFFu);
}

extern void isr_common_stub(void);

void idt_load(void)
{
	int i;

	for (i = 0; i < IDT_ENTRIES; i++) {
		idt[i].offset_1 = 0;
		idt[i].selector = KERNEL_CODE_SELECTOR;
		idt[i].zero = 0;
		idt[i].type_attr = IDT_FLAG_INTERRUPT_GATE;
		idt[i].offset_2 = 0;
	}

	idtr.limit = (uint16_t)(sizeof(idt) - 1u);
	idtr.base = (uint32_t)(uintptr_t)&idt[0];
	// Vector 32 = IRQ0 (timer) after PIC remapping
	idt_set_gate(32, (uint32_t)(uintptr_t)isr_common_stub);

	__asm__ volatile ("lidt %0" : : "m"(idtr));
}

