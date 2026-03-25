volatile unsigned short* vga = (unsigned short*)0xB8000;

void entry()
{
    const char* msg = "Hello from sector!";

    for (int i = 0; msg[i]; i++)
        vga[i] = (0x0F << 8) | msg[i];

    while (1);
}