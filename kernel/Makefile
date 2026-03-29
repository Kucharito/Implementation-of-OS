

.RECIPEPREFIX := >

all: run

QEMU ?= /usr/bin/qemu-system-i386
QEMU_CLEAN_ENV = env -u LD_LIBRARY_PATH -u LD_PRELOAD -u GTK_PATH -u GIO_MODULE_DIR
CFLAGS = -g -m32 -ffreestanding -fno-pie -fno-stack-protector -Iarch -Idrivers -Ikomponenty
PAYLOAD_CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector
PAYLOAD_LDFLAGS = -m elf_i386

kernel.bin: kernel_entry.o kernel.o serial.o vga.o keyboard.o ide.o cli.o
>ld -m elf_i386 -o $@ -Ttext 0x1000 $^ --oformat binary

kernel_entry.o: kernel_entry.asm
>nasm $< -f elf -o $@

kernel.o: kernel/kernel.c arch/io.h drivers/serial.h drivers/vga.h drivers/keyboard.h
>cc $(CFLAGS) -c $< -o $@

serial.o: drivers/serial.c drivers/serial.h arch/io.h
>cc $(CFLAGS) -c $< -o $@

vga.o: drivers/vga.c drivers/vga.h arch/io.h
>cc $(CFLAGS) -c $< -o $@

keyboard.o: drivers/keyboard.c drivers/keyboard.h arch/io.h
>cc $(CFLAGS) -c $< -o $@

ide.o: drivers/ide.c drivers/ide.h arch/io.h
>cc $(CFLAGS) -c $< -o $@

cli.o: komponenty/cli.c komponenty/cli.h drivers/keyboard.h drivers/vga.h
>cc $(CFLAGS) -c $< -o $@

kernel.dis: kernel.bin
>ndisasm -b 32 $< > $@

mbr.bin: mbr.asm
>nasm $< -f bin -o $@

os-image.bin: mbr.bin kernel.bin
>cat $^ > $@
>truncate -s 1474560 $@

payloads/hello_sector.o: payloads/hello_sector.c
>cc $(PAYLOAD_CFLAGS) -c $< -o $@

payloads/hello_sector.elf: payloads/hello_sector.o payloads/payload.ld
>ld $(PAYLOAD_LDFLAGS) -T payloads/payload.ld payloads/hello_sector.o -o $@

payloads/hello_sector.bin: payloads/hello_sector.elf
>objcopy -O binary $< $@

inject-hello: os-image.bin payloads/hello_sector.bin
>cp os-image.bin os-image-ide.bin
>dd if=payloads/hello_sector.bin of=os-image-ide.bin bs=512 seek=5 conv=notrunc

run: inject-hello
>$(QEMU_CLEAN_ENV) $(QEMU) -boot order=a -drive format=raw,file=os-image.bin,if=floppy -drive format=raw,file=os-image-ide.bin,if=ide

run-serial: inject-hello
>$(QEMU_CLEAN_ENV) $(QEMU) -boot order=a -drive format=raw,file=os-image.bin,if=floppy -drive format=raw,file=os-image-ide.bin,if=ide -serial stdio

echo: os-image.bin
>xxd $<

clean:
>$(RM) *.bin *.o *.dis
