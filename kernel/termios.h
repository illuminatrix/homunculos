#ifndef TERMIOS_H
#define TERMIOS_H

#include <stdint.h>

#define NCCS 19

struct termios {
	uint32_t c_iflag;
	uint32_t c_oflag;
	uint32_t c_cflag;
	uint32_t c_lflag;
	unsigned char c_cc[NCCS];
};

/* ioctl commands */
#define TCGETS  0x5401
#define TCSETS  0x5402
#define TCSETSW 0x5403
#define TCSETSF 0x5404

/* c_lflag bits */
#define ISIG   0x0001
#define ICANON 0x0002
#define ECHO   0x0008

/* c_iflag bits */
#define ICRNL  0x0100

extern struct termios tty_termios;

#endif
