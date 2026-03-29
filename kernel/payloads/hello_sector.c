volatile unsigned short *vga = (unsigned short *)0xB8000;

void entry(void)
{
    const char *msg = "Hello from sector!";
    int i = 0;

    while (msg[i] != '\0') {
        vga[i] = (unsigned short)((0x0F << 8) | msg[i]);
        i++;
    }

    while (1) {
    }
}
