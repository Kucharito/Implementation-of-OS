
#include <stdint.h>
#include "io.h"
#include "serial.h"

#define PIT_FREQUENCY 1193180
#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

volatile uint32_t tick = 0;
static int timer_enabled = 0;

void timer_init(uint32_t frequency)
{
    uint32_t divisor;
    uint8_t low, high;
    
    serial_print("[timer] Initializing\n");
    
    // Calculate divisor: base_frequency / desired_frequency
    divisor = PIT_FREQUENCY / frequency;
    
    // Split into low and high bytes
    low = (uint8_t)(divisor & 0xFF);
    high = (uint8_t)((divisor >> 8) & 0xFF);
    
    // Program PIT control word: 0x34
    // Bits 7-6: 00 = Channel 0
    // Bits 5-4: 11 = Access mode (lo/hi bytes)
    // Bits 3-1: 010 = Mode 2 (rate generator)
    // Bit 0: 0 = Binary
    outb(PIT_COMMAND_PORT, 0x34);
    
    // Send divisor (low byte first, then high byte)
    outb(PIT_CHANNEL0_PORT, low);
    outb(PIT_CHANNEL0_PORT, high);
    
    timer_enabled = 1;
    serial_print("[timer] Initialized\n");
}

void timer_callback(void)
{
    if (!timer_enabled)
        return;
        
    tick++;
    // ISR will call pic_send_eoi() after this returns
}