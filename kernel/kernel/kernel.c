
#include <stdint.h>

#include "serial.h"
#include "io.h"
#include "vga.h"
#include "ide.h"
#include "cli.h"
#include "fat.h"

void start_kernel() {
    uint8_t sector0[512];
    int ide_rc;

    serial_init();

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

    cli_loop();
}

