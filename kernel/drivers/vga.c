#include "io.h"
#include "vga.h"

#define VIDEO_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR 0x0F

#define VGA_CTRL_REGISTER 0x3D4
#define VGA_DATA_REGISTER 0x3D5
#define VGA_CUR_LOW_REG 0x0F
#define VGA_CUR_HIGH_REG 0x0E

static int vga_get_cursor_offset(void) {
	int offset;

	outb(VGA_CTRL_REGISTER, VGA_CUR_LOW_REG);
	offset = inb(VGA_DATA_REGISTER);
	outb(VGA_CTRL_REGISTER, VGA_CUR_HIGH_REG);
	offset |= inb(VGA_DATA_REGISTER) << 8;

	return offset * 2;
}

static void vga_set_cursor_offset(int offset) {
	int cell = offset / 2;
	outb(VGA_CTRL_REGISTER, VGA_CUR_LOW_REG);
	outb(VGA_DATA_REGISTER, (unsigned char)(cell & 0xFF));
	outb(VGA_CTRL_REGISTER, VGA_CUR_HIGH_REG);
	outb(VGA_DATA_REGISTER, (unsigned char)((cell >> 8) & 0xFF));
}

static int vga_row_start_offset(int row) {
	return row * VGA_WIDTH * 2;
}

static int vga_handle_scroll(int offset) {
	unsigned char *vmem = (unsigned char *)VIDEO_ADDRESS;
	int row;
	int col;

	if (offset < VGA_WIDTH * VGA_HEIGHT * 2) {
		return offset;
	}

	for (row = 1; row < VGA_HEIGHT; row++) {
		for (col = 0; col < VGA_WIDTH * 2; col++) {
			vmem[vga_row_start_offset(row - 1) + col] = vmem[vga_row_start_offset(row) + col];
		}
	}

	for (col = 0; col < VGA_WIDTH; col++) {
		int pos = vga_row_start_offset(VGA_HEIGHT - 1) + col * 2;
		vmem[pos] = ' ';
		vmem[pos + 1] = VGA_COLOR;
	}

	return offset - VGA_WIDTH * 2;
}

void vga_putchar(char c) {
	unsigned char *vmem = (unsigned char *)VIDEO_ADDRESS;
	int offset = vga_get_cursor_offset();

	if (c == '\n') {
		int row = offset / (VGA_WIDTH * 2);
		offset = vga_row_start_offset(row + 1);
	}
	else if(c == '\b') {
		if(offset > 0){
			offset = offset -2;
			vmem[offset] = ' ';
			vmem[offset + 1] = VGA_COLOR;
			vga_set_cursor_offset(offset);
		}
	}
	else {
		vmem[offset] = (unsigned char)c;
		vmem[offset + 1] = VGA_COLOR;
		offset += 2;
	}

	if (offset >= VGA_WIDTH * VGA_HEIGHT * 2) {
		offset = vga_handle_scroll(offset);
	}

	vga_set_cursor_offset(offset);
}

void vga_print(const char *str) {
	int i = 0;
	while (str[i] != '\0') {
		vga_putchar(str[i]);
		i++;
	}
}
void vga_backspace(void)
{
	unsigned char *vmem = (unsigned char *)VIDEO_ADDRESS;
	int offset = vga_get_cursor_offset();
	if(offset <= 0){
		return;
	}
	offset = offset -2;
	vmem[offset] = ' ';
	vmem[offset + 1] = VGA_COLOR;
	vga_set_cursor_offset(offset);
}

void vga_clear(void){
	unsigned char *vmem = (unsigned char *)VIDEO_ADDRESS;
	int row;
	int col;
	for (row = 0; row < VGA_HEIGHT;row++)
	{
		for (col = 0; col< VGA_WIDTH;col++)
		{
			int pos = vga_row_start_offset(row) + col * 2;
			vmem[pos] = ' ';
			vmem[pos + 1] = VGA_COLOR;
		}
	}
	vga_set_cursor_offset(0);
}

int vga_get_cursor_pos(void) {
	return vga_get_cursor_offset();
}
