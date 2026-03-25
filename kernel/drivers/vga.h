#ifndef VGA_H
#define VGA_H

void vga_putchar(char c);
void vga_print(const char *str);
void vga_backspace(void);
void vga_clear(void);
int vga_get_cursor_pos(void);

#endif
