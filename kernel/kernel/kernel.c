
#include <stdint.h>

#include "serial.h"
#include "io.h"
#include "vga.h"
#include "ide.h"
#include "cli.h"
#include "fat.h"
#include "pic.h"
#include "timer.h"
#include "partition.h"
#include "scheduler.h"

void idt_load(void);
void pic_remap(void);
void timer_init(uint32_t frequency);

void start_kernel() {
    uint8_t sector0[512];
    int ide_rc;

    // pic_remap();
    // idt_load();
    // timer_init(100);  // 100 Hz = 10ms per interrupt

    serial_init();
    partition_init();
    scheduler_init();

    vga_print("AdamOS\n");
    vga_print("Testing VGA output\n");
    vga_print("Testing serial output\n");

    serial_print("AdamOS\n");
    serial_print("Testing serial output\n");
    serial_print("[kernel] COM1 ready\n");

    vga_print("Testing ATA/IDE Reading sector 0...\n");
    serial_print("Testing ATA/IDE Reading sector 0...\n");

    ide_rc = ide_read_sector(0, sector0);
    if (ide_rc == 0) {
        vga_print("ATA/IDE read OK\n");
        serial_print("ATA/IDE read OK\n");
    } else {
        vga_print("ATA/IDE read FAIL\n");
        serial_print("ATA/IDE read FAIL\n");
    }

    vga_print("Initializing FAT16...\n");
    serial_print("Initializing FAT16...\n");
    fat16_init();

    vga_print("Starting CLI...\n");
    serial_print("Starting CLI...\n");
    
    // Enable interrupts (STI) - allow timer interrupts
    serial_print("[kernel] About to enable interrupts\n");
    serial_print("[kernel] BEFORE STI\n");
    // enable_interrupts();
    serial_print("[kernel] AFTER STI\n");
    serial_print("[kernel] Interrupts enabled\n");

    cli_loop();
}

/**/

