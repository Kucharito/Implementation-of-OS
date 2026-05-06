

.RECIPEPREFIX := >

all: run

QEMU ?= /usr/bin/qemu-system-i386
QEMU_CLEAN_ENV = env -u LD_LIBRARY_PATH -u LD_PRELOAD -u GTK_PATH -u GIO_MODULE_DIR
CFLAGS = -g -m32 -ffreestanding -fno-pie -fno-stack-protector -Iarch -Idrivers -Ikomponenty -Ischeduler -I../fat
PAYLOAD_CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -Os -fomit-frame-pointer -fno-asynchronous-unwind-tables -fno-unwind-tables
PAYLOAD_LDFLAGS = -m elf_i386 --gc-sections

kernel.bin: kernel_entry.o kernel.o idt.o isr.o isr_c.o pic.o timer.o interrupts.o serial.o vga.o keyboard.o ide.o cli.o partition.o scheduler.o fat_kernel_adapter.o fat_state.o fat_common.o fat_disk.o fat_init.o fat_search.o fat_tree.o fat_dir.o fat_file.o
>ld -m elf_i386 -o $@ -Ttext 0x1000 $^ --oformat binary

kernel_entry.o: kernel_entry.asm
>nasm $< -f elf -o $@

kernel.o: kernel/kernel.c arch/io.h drivers/serial.h drivers/vga.h drivers/keyboard.h
>cc $(CFLAGS) -c $< -o $@

idt.o: arch/idt.c
>cc $(CFLAGS) -c $< -o $@

isr.o : arch/isr.S
>nasm $< -f elf -o $@

isr_c.o: arch/isr.c
>cc $(CFLAGS) -c $< -o $@

interrupts.o: arch/interrupts.c arch/interrupts.h arch/io.h
>cc $(CFLAGS) -c $< -o $@

pic.o: arch/pic.c arch/pic.h arch/io.h
>cc $(CFLAGS) -c $< -o $@

timer.o: arch/timer.c arch/timer.h arch/io.h
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

partition.o: komponenty/partition.c komponenty/partition.h
>cc $(CFLAGS) -c $< -o $@

scheduler.o: scheduler/scheduler.c scheduler/scheduler.h komponenty/partition.h
>cc $(CFLAGS) -c $< -o $@

fat_kernel_adapter.o: komponenty/fat_kernel_adapter.c drivers/ide.h drivers/vga.h drivers/serial.h
>cc $(CFLAGS) -c $< -o $@

fat_state.o: ../fat/fat_state.c ../fat/fat_internal.h ../fat/fat.h
>cc $(CFLAGS) -c $< -o $@

fat_common.o: ../fat/fat_common.c ../fat/fat_internal.h ../fat/fat.h
>cc $(CFLAGS) -c $< -o $@

fat_disk.o: ../fat/fat_disk.c ../fat/fat_internal.h ../fat/fat.h
>cc $(CFLAGS) -c $< -o $@

fat_init.o: ../fat/fat_init.c ../fat/fat_internal.h ../fat/fat.h
>cc $(CFLAGS) -c $< -o $@

fat_search.o: ../fat/fat_search.c ../fat/fat_internal.h ../fat/fat.h
>cc $(CFLAGS) -c $< -o $@

fat_tree.o: ../fat/fat_tree.c ../fat/fat_internal.h ../fat/fat.h
>cc $(CFLAGS) -c $< -o $@

fat_dir.o: ../fat/fat_dir.c ../fat/fat_internal.h ../fat/fat.h
>cc $(CFLAGS) -c $< -o $@

fat_file.o: ../fat/fat_file.c ../fat/fat_internal.h ../fat/fat.h
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

payloads/scroll_text.o: payloads/scroll_text.c
>cc $(PAYLOAD_CFLAGS) -c $< -o $@

payloads/scroll_text.elf: payloads/scroll_text.o payloads/payload.ld
>ld $(PAYLOAD_LDFLAGS) -T payloads/payload.ld payloads/scroll_text.o -o $@

payloads/scroll_text.bin: payloads/scroll_text.elf
>objcopy -O binary $< $@

inject-hello: os-image.bin payloads/hello_sector.bin
>cp os-image.bin os-image-ide.bin
>dd if=payloads/hello_sector.bin of=os-image-ide.bin bs=512 seek=5 conv=notrunc

inject-scroll: os-image.bin payloads/scroll_text.bin
>cp os-image.bin os-image-ide.bin
>dd if=payloads/scroll_text.bin of=os-image-ide.bin bs=512 seek=7 conv=notrunc

run: inject-hello
>$(QEMU_CLEAN_ENV) $(QEMU) -boot order=a -drive format=raw,file=os-image.bin,if=floppy -drive format=raw,file=os-image-ide.bin,if=ide

run-serial: inject-hello
>$(QEMU_CLEAN_ENV) $(QEMU) -boot order=a -drive format=raw,file=os-image.bin,if=floppy -drive format=raw,file=os-image-ide.bin,if=ide -serial stdio

run-scroll: inject-scroll
>$(QEMU_CLEAN_ENV) $(QEMU) -boot order=a -drive format=raw,file=os-image.bin,if=floppy -drive format=raw,file=os-image-ide.bin,if=ide

run-scroll-serial: inject-scroll
>$(QEMU_CLEAN_ENV) $(QEMU) -boot order=a -drive format=raw,file=os-image.bin,if=floppy -drive format=raw,file=os-image-ide.bin,if=ide -serial stdio

fat-image.bin: ../fat/sd.img mbr.bin kernel.bin
>cp ../fat/sd.img fat-image.bin
>dd if=mbr.bin of=fat-image.bin bs=1 count=446 conv=notrunc
>dd if=kernel.bin of=fat-image.bin bs=512 seek=1 conv=notrunc

sync-sdimg: ../fat/sd.img mbr.bin kernel.bin
>dd if=mbr.bin of=../fat/sd.img bs=1 count=446 conv=notrunc
>dd if=kernel.bin of=../fat/sd.img bs=512 seek=1 conv=notrunc

run-fat: fat-image.bin
>$(QEMU_CLEAN_ENV) $(QEMU) -boot order=c -drive format=raw,file=fat-image.bin,if=ide

run-fat-serial: fat-image.bin
>$(QEMU_CLEAN_ENV) $(QEMU) -boot order=c -drive format=raw,file=fat-image.bin,if=ide -serial stdio

echo: os-image.bin
>xxd $<

clean:
>$(RM) *.bin *.o *.dis
