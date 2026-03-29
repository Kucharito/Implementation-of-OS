volatile unsigned short *vga = (unsigned short *)0xB8000;

static unsigned char inb(unsigned short p)
{
    unsigned char v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(p));
    return v;
}

void entry(void)
{
    static const char msg[] = "  AdamOS marquee demo - load + run from disk sector - press Q to return to CLI  ";
    int offset = 0;
    int i;
    int key;
    int wait;
    int msg_len = (int)(sizeof(msg) - 1);

    while (1) {
        if (inb(0x64) & 1) {
            key = (int)inb(0x60);
            if ((key & 0x80) == 0 && key == 0x10) {
                return;
            }
        }

        for (i = 0; i < 80; i++) {
            char ch = msg[(offset + i) % msg_len];
            vga[i] = (unsigned short)((0x1E << 8) | (unsigned char)ch);
        }

        for (i = 80; i < 80 * 25; i++) {
            vga[i] = (unsigned short)((0x1F << 8) | ' ');
        }

        offset++;
        if (offset >= msg_len) {
            offset = 0;
        }

        for (wait = 0; wait < 80000; wait++) {
            __asm__ volatile ("");
        }
    }
}
