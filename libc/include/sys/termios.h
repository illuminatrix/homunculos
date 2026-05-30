#ifndef _SYS_TERMIOS_H
#define _SYS_TERMIOS_H 1

#include <sys/cdefs.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NCCS 19

typedef uint32_t tcflag_t;
typedef unsigned char cc_t;

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_cc[NCCS];
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

/* Special control characters */
#define VINTR     0
#define VQUIT     1
#define VERASE    2
#define VKILL     3
#define VEOF      4
#define VTIME     5
#define VMIN      6
#define VSWTC     7
#define VSTART    8
#define VSTOP     9
#define VSUSP    10
#define VEOL     11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE  14
#define VLNEXT   15
#define VEOL2    16

/* TCSETS action argument */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#ifdef __cplusplus
}
#endif

#endif
