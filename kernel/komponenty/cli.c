#include "cli.h"
#include "keyboard.h"
#include "vga.h"

typedef unsigned int u32;
typedef unsigned char u8;

int ide_read_sector(unsigned int lba, void *buffer);
int ide_write_sector(unsigned int lba, const void *buffer);

#define CLI_MAX_LINE_LENGTH 128
#define CLI_MAX_ARGS 8
#define CLI_DEFAULT_BUFFER_ADDR 0x00010000u
#define CLI_DEFAULT_DUMP_LEN 128u

static int cli_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int cli_streq(const char *a, const char *b)
{
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int cli_parse_u32(const char *s, u32 *out)
{
    u32 value = 0;
    u32 base = 10;
    int i = 0;
    int any = 0;

    if (s == 0 || out == 0 || s[0] == '\0') {
        return -1;
    }

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        i = 2;
    }

    while (s[i] != '\0') {
        char c = s[i];
        u32 d;

        if (c >= '0' && c <= '9') {
            d = (u32)(c - '0');
        } else if (base == 16 && c >= 'a' && c <= 'f') {
            d = (u32)(10 + c - 'a');
        } else if (base == 16 && c >= 'A' && c <= 'F') {
            d = (u32)(10 + c - 'A');
        } else {
            return -1;
        }

        value = value * base + d;
        any = 1;
        i++;
    }

    if (!any) {
        return -1;
    }

    *out = value;
    return 0;
}

static void cli_print_hex_nibble(u8 n)
{
    if (n < 10) {
        vga_putchar((char)('0' + n));
    } else {
        vga_putchar((char)('A' + (n - 10)));
    }
}

static void cli_print_hex8(u8 value)
{
    cli_print_hex_nibble((u8)((value >> 4) & 0x0F));
    cli_print_hex_nibble((u8)(value & 0x0F));
}

static void cli_print_hex32(u32 value)
{
    int shift;
    vga_print("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        cli_print_hex_nibble((u8)((value >> shift) & 0x0F));
    }
}

static void cli_hexdump(u32 addr, u32 len)
{
    u8 *ptr = (u8 *)addr;
    u32 i;

    for (i = 0; i < len; i++) {
        if ((i % 16u) == 0u) {
            if (i != 0u) {
                vga_putchar('\n');
            }
            cli_print_hex32(addr + i);
            vga_print(": ");
        }

        cli_print_hex8(ptr[i]);
        vga_putchar(' ');
    }
    vga_putchar('\n');
}

void cli_readline(char * buffer, int max_length)
{
    int pos = 0;
    int start_cursor_pos;
    if(buffer == 0 || max_length <= 0) {
        return;
    }
    start_cursor_pos = vga_get_cursor_pos();
    while(1){
        char ch = keyboard_getchar();
        if (ch == '\n') {
            vga_putchar('\n');
            break;
        }
        if (ch == '\b') {
            if(pos > 0 && vga_get_cursor_pos() > start_cursor_pos) {
                pos--;
                vga_backspace();
            }
            continue;
        }
        if (pos < max_length -1 && ch >= 32 && ch <= 126) {
            buffer[pos++] = (char)ch;
            vga_putchar((char)ch);
        }
    }
    buffer[pos] = '\0';
}

int cli_parse(char *line, char* argv[], int max_args)
{
    int argc = 0;
    int i = 0;

    if(line == 0 || argv == 0 || max_args <= 0 ){
        return 0;
    }
    while(line[i]!='\0' && argc < max_args){
        while(line[i]!='\0' && cli_is_space(line[i])){
            line[i] = '\0';
            i++;
        }
        if(line[i]=='\0'){
            break;
        }
        argv[argc++]= &line[i];
        while(line[i]!='\0' && !cli_is_space(line[i])){
            i++;
        }
    }
    return argc;

}

static void cli_execute(int argc, char *argv[]){
    u32 lba;
    u32 addr;
    int rc;

    if (argc == 0) {
        return;
    }

    if (cli_streq(argv[0], "help")) {
        vga_print("Commands:\n");
        vga_print("help\n");
        vga_print("clear\n");
        vga_print("read <lba>\n");
        vga_print("write <lba>\n");
        vga_print("load <lba> <addr>\n");
        vga_print("dump <addr>\n");
        vga_print("run <addr>\n");
        return;
    }

    if (cli_streq(argv[0], "clear")) {
        vga_clear();
        return;
    }

    if (cli_streq(argv[0], "read")) {
        if (argc != 2 || cli_parse_u32(argv[1], &lba) != 0) {
            vga_print("Usage: read <lba>\n");
            return;
        }

        rc = ide_read_sector(lba, (void *)CLI_DEFAULT_BUFFER_ADDR);
        if (rc == 0) {
            vga_print("Read OK, buffer=");
            cli_print_hex32(CLI_DEFAULT_BUFFER_ADDR);
            vga_putchar('\n');
        } else {
            vga_print("Read FAIL\n");
        }
        return;
    }

    if (cli_streq(argv[0], "write")) {
        if (argc != 2 || cli_parse_u32(argv[1], &lba) != 0) {
            vga_print("Usage: write <lba>\n");
            return;
        }

        rc = ide_write_sector(lba, (const void *)CLI_DEFAULT_BUFFER_ADDR);
        if (rc == 0) {
            vga_print("Write OK\n");
        } else {
            vga_print("Write FAIL\n");
        }
        return;
    }

    if (cli_streq(argv[0], "load")) {
        if (argc != 3 || cli_parse_u32(argv[1], &lba) != 0 || cli_parse_u32(argv[2], &addr) != 0) {
            vga_print("Usage: load <lba> <addr>\n");
            return;
        }

        rc = ide_read_sector(lba, (void *)addr);
        if (rc == 0) {
            vga_print("Load OK\n");
        } else {
            vga_print("Load FAIL\n");
        }
        return;
    }

    if (cli_streq(argv[0], "dump")) {
        if (argc != 2 || cli_parse_u32(argv[1], &addr) != 0) {
            vga_print("Usage: dump <addr>\n");
            return;
        }
        cli_hexdump(addr, CLI_DEFAULT_DUMP_LEN);
        return;
    }

    if (cli_streq(argv[0], "run")) {
        void (*func)(void);

        if (argc != 2 || cli_parse_u32(argv[1], &addr) != 0) {
            vga_print("Usage: run <addr>\n");
            return;
        }

        vga_print("Running code at ");
        cli_print_hex32(addr);
        vga_putchar('\n');

        func = (void (*)(void))addr;
        func();

        vga_print("Returned from code\n");
        return;
    }

    vga_print("Unknown command: ");
    vga_print(argv[0]);
    vga_print("\n");
}

void cli_loop(){
    char line[CLI_MAX_LINE_LENGTH];
    char *argv[CLI_MAX_ARGS];
    int argc;
    while(1){
        vga_print("> ");
        cli_readline(line, CLI_MAX_LINE_LENGTH);
        argc = cli_parse(line, argv, CLI_MAX_ARGS);
        cli_execute(argc, argv);
    }
}


