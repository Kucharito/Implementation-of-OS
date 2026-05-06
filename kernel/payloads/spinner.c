volatile unsigned short *vga = (unsigned short *)0xB8000;

#ifndef ROW
#define ROW 0
#endif

#ifndef SPIN_ID
#define SPIN_ID 65
#endif

static void busy_delay(volatile int n)
{
    while (n-- > 0) {
        __asm__ volatile ("");
    }
}

void entry(void)
{
    static const char spin[] = "|/-\\";
    volatile unsigned int scratch[32];
    unsigned int idx = 0;

    for (int i = 0; i < 32; i++) {
        scratch[i] = (unsigned int)i;
    }

    while (1) {
        scratch[idx & 31] ^= idx;

        unsigned int off = (unsigned int)(ROW * 80);
        vga[off + 0] = (unsigned short)((0x1E << 8) | spin[idx & 3]);
        vga[off + 2] = (unsigned short)((0x1F << 8) | (char)SPIN_ID);

        idx++;
        busy_delay(50000);
    }
}