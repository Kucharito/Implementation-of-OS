#include "serial.h"
#include "io.h"

#define COM1_BASE 0x3F8

#define REG_DATA            0
#define REG_INTERRUPT_EN    1
#define REG_FIFO_CTRL       2
#define REG_LINE_CTRL       3
#define REG_MODEM_CTRL      4
#define REG_LINE_STATUS     5

static int serial_transmit_empty(void) {
    return inb(COM1_BASE + REG_LINE_STATUS) & 0x20;
}

void serial_init(void) {
    outb(COM1_BASE + REG_INTERRUPT_EN, 0x00); /* Vypni prerusenia */
    outb(COM1_BASE + REG_LINE_CTRL, 0x80);    /* Zapni DLAB */
    outb(COM1_BASE + REG_DATA, 0x03);         /* Nizsi bajt delicky (38400 baud) */
    outb(COM1_BASE + REG_INTERRUPT_EN, 0x00); /* Vyssi bajt delicky */
    outb(COM1_BASE + REG_LINE_CTRL, 0x03);    /* 8 bitov, bez parity, jeden stop bit */
    outb(COM1_BASE + REG_FIFO_CTRL, 0xC7);    /* Zapni FIFO, vycisti RX/TX fronty */
    outb(COM1_BASE + REG_MODEM_CTRL, 0x0B);   /* Zapnute IRQ, nastavene RTS/DSR */
}

void serial_putchar(char c) {
    if (c == '\n') {
        serial_putchar('\r');
    }

    while (!serial_transmit_empty()) {
    }
    outb(COM1_BASE + REG_DATA, (unsigned char)c);
}

void serial_print(const char *str) {
    int i = 0;
    while (str[i] != '\0') {
        serial_putchar(str[i]);
        i++;
    }
}
