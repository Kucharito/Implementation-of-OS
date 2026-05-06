#include <stdint.h>
#include "io.h"
#include "serial.h"

#define PIC1        0x20
#define PIC2        0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA    (PIC2 + 1)

#define PIC_EOI     0x20

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_remap(void)
{
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

    serial_print("[pic] Remapping interrupts\n");

    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    (void)a1;
    (void)a2;

    // Mask all IRQs except IRQ0 (timer) to avoid unhandled interrupts.
    outb(PIC1_DATA, 0xFE);
    outb(PIC2_DATA, 0xFF);
    
    serial_print("[pic] Remapping done\n");
}