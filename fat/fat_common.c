#include "fat_internal.h"

/* Porovna dva nulou ukoncene retazce na presnu zhodu. */
int str_eq(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }
    return (*a == 0 && *b == 0);
}

/* Extract next '/'-separated path component and advance input cursor. */
int parse_path_component(const char **path, char *component, unsigned int max_len)
{
    unsigned int i = 0;
    while (**path == '/')
        (*path)++;
    if (**path == 0)
    {
        component[0] = 0;
        return 0;
    }
    while (**path && **path != '/')
    {
        if (i + 1 < max_len)
            component[i++] = **path;
        (*path)++;
    }
    component[i] = 0;
    return 1;
}

/* Minimalna nahrada memcpy pouzivana v celom projekte. */
void *k_memcpy(void *dest, const void *src, unsigned int n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;

    for (unsigned int i = 0; i < n; i++)
        d[i] = s[i];

    return dest;
}

/* Return string length without the trailing '\0'. */
unsigned int k_strlen(const char *str)
{
    unsigned int len = 0;
    while (str[len])
        len++;
    return len;
}

/* Minimalna nahrada memset pouzivana v celom projekte. */
void *k_memset(void *dest, int value, unsigned int n)
{
    unsigned char *d = dest;
    for (unsigned int i = 0; i < n; i++)
    {
        d[i] = (unsigned char)value;
    }
    return dest;
}

/* Vypise cely C-retazec cez aktivny konzolovy backend. */
void print_string(const char *s)
{
    console_write(s, k_strlen(s));
}

/* Vypise nezaporne cislo v desiatkovej sustave na konzolu. */
void print_dec(unsigned int value)
{
    char buffer[16];
    int i = 0;

    if (value == 0)
    {
        console_putc('0');
        return;
    }

    while (value > 0)
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i--)
        console_putc(buffer[i]);
}

/* Vypise cislo vo formate 2 cifier (s uvodnou nulou ak treba). */
void print_2d(unsigned int v)
{
    if (v < 10)
        console_putc('0');
    print_dec(v);
}

/* Spocita pocet desiatkovych cifier nezaporneho cisla. */
static unsigned int digits10(unsigned int v)
{
    unsigned int d = 1;
    while (v >= 10)
    {
        v /= 10;
        d++;
    }
    return d;
}

/* Zarovna desiatkovy vypis doprava na sirku doplnenim medzier. */
void print_dec_width(unsigned int v, unsigned int w)
{
    unsigned int d = digits10(v);
    while (d < w)
    {
        console_putc(' ');
        d++;
    }
    print_dec(v);
}