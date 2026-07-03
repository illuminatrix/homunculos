#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

int
vsprintf(char* str, const char* restrict format, va_list args)
{
    int written = 0;
    size_t amount;
    bool rejected_bad_specifier = false;

    while ( *format != '\0' ) {
        if ( *format != '%' ) {
        copy_str:
            amount = 1;
            while ( format[amount] && format[amount] != '%' )
                amount++;
            memcpy(&str[written], format, amount);
            format += amount;
            written += amount;
            continue;
        }

        const char* format_begun_at = format;

        if ( *(++format) == '%' )
            goto copy_str;

        if ( rejected_bad_specifier ) {
        incomprehensible_conversion:
            rejected_bad_specifier = true;
            format = format_begun_at;
            goto copy_str;
        }

        if ( *format == 'c' ) {
            format++;
            char c = (char) va_arg(args, int /* char promotes to int */);
            memcpy(&str[written++], &c, sizeof(c));
        }
        else if ( *format == 's' ) {
            format++;
            const char* s = va_arg(args, const char*);
            memcpy(&str[written], s, strlen(s));
            written += strlen(s);
        }
        else if ( *format == 'd' ) {
            format++;
            char chr[] = "0123456789";
            int number = va_arg(args, int);
            char buf[12];
            int pos = 12;

            if (number < 0) {
                str[written++] = '-';
                number = -number;
            }

            buf[--pos] = chr[number % 10];
            while ((number /= 10) > 0)
                buf[--pos] = chr[number % 10];

            memcpy(&str[written], &buf[pos], 12 - pos);
            written += 12 - pos;
        }
        else if ( *format == 'o' ) {
            format++;
            char chr[] = "01234567";
            unsigned int number = va_arg(args, unsigned int);
            char buf[12];
            int pos = 12;

            buf[--pos] = chr[number & 7];
            while ((number >>= 3) > 0)
                buf[--pos] = chr[number & 7];

            memcpy(&str[written], &buf[pos], 12 - pos);
            written += 12 - pos;
        }
        else if ( *format == 'u' ) {
            format++;
            char chr[] = "0123456789";
            unsigned int number = va_arg(args, unsigned int);
            char buf[12];
            int pos = 12;

            buf[--pos] = chr[number % 10];
            while ((number /= 10) > 0)
                buf[--pos] = chr[number % 10];

            memcpy(&str[written], &buf[pos], 12 - pos);
            written += 12 - pos;
        }
        else if ( *format == 'x' ) {
            format++;
            char chr[] = "0123456789abcdef";
            unsigned int number = va_arg(args, unsigned int);
            char buf[9];
            int pos = 9;

            buf[--pos] = chr[number & 15];
            while ((number >>= 4) > 0)
                buf[--pos] = chr[number & 15];

            memcpy(&str[written], &buf[pos], 9 - pos);
            written += 9 - pos;
        }
        else {
            goto incomprehensible_conversion;
        }
    }

	if (written > 0)
	    str[written] = '\0';

    return written;
}
