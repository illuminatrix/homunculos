#include "vfs.h"
#include "irq.h"
#include "pio.h"
#include "interrupt.h"

#define KBD_BUFFER_SIZE 256

#define PS2_DATA  0x60
#define PS2_CMD   0x64

static volatile char kbd_buffer[KBD_BUFFER_SIZE];
static volatile int kbd_head;
static volatile int kbd_tail;

static int kbd_shift;
static int kbd_caps;

/* Track which keys are currently held down (1 bit per scancode 0-127) */
static uint8_t kbd_key_state[16];

static struct file kbd_file;

static const char kbd_scan_normal[128] = {
	  0,  0x1B, '1',  '2',  '3',  '4',  '5',  '6',
	'7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
	'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
	'o',  'p',  '[',  ']',  '\n',   0,  'a',  's',
	'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
   '\'',  '`',    0, '\\',  'z',  'x',  'c',  'v',
	'b',  'n',  'm',  ',',  '.',  '/',    0,  '*',
	  0,  ' ',    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
};

static const char kbd_scan_shift[128] = {
	  0,  0x1B, '!',  '@',  '#',  '$',  '%',  '^',
	'&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
	'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
	'O',  'P',  '{',  '}',  '\n',   0,  'A',  'S',
	'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
   '\"',  '~',    0,  '|',  'Z',  'X',  'C',  'V',
	'B',  'N',  'M',  '<',  '>',  '?',    0,  '*',
	  0,  ' ',    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
};

static void kbd_push(char c)
{
	int next = (kbd_head + 1) % KBD_BUFFER_SIZE;
	if (next != kbd_tail)
	{
		kbd_buffer[kbd_head] = c;
		kbd_head = next;
	}
}

static int kbd_pop(void)
{
	if (kbd_head == kbd_tail)
		return -1;
	char c = kbd_buffer[kbd_tail];
	kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
	return c;
}

static void kbd_process_scancode(uint8_t scancode)
{
	if (scancode & 0x80)
	{
		scancode &= 0x7F;
		/* Key released - clear key state */
		kbd_key_state[scancode / 8] &= ~(1 << (scancode % 8));
		if (scancode == 0x2A || scancode == 0x36)
			kbd_shift = 0;
		return;
	}

	/* Typematic repeat: suppress if key is already held down */
	if (kbd_key_state[scancode / 8] & (1 << (scancode % 8)))
		return;

	/* Mark key as held down */
	kbd_key_state[scancode / 8] |= (1 << (scancode % 8));

	if (scancode == 0x2A || scancode == 0x36)
	{
		kbd_shift = 1;
		return;
	}

	if (scancode == 0x3A)
	{
		kbd_caps = !kbd_caps;
		return;
	}

	char c;
	if (kbd_shift)
		c = kbd_scan_shift[scancode];
	else
		c = kbd_scan_normal[scancode];

	if (c >= 'a' && c <= 'z' && kbd_caps)
		c -= 0x20;

	if (c)
		kbd_push(c);
}

static void kbd_irq_handler(void)
{
	uint8_t scancode = in(PS2_DATA);
	kbd_process_scancode(scancode);
}

static int kbd_poll_one(void)
{
	if (!(in(PS2_CMD) & 1))
		return -1;

	uint8_t scancode = in(PS2_DATA);
	kbd_process_scancode(scancode);

	return kbd_pop();
}

static int kbd_read(struct file *file, void *buf, size_t nbyte)
{
	(void)file;
	char *cbuf = (char *)buf;
	size_t i;

	for (i = 0; i < nbyte; i++)
	{
		int c = kbd_pop();
		if (c < 0)
			c = kbd_poll_one();
		if (c < 0)
			break;
		cbuf[i] = (char)c;
	}

	return (int)i;
}

static struct vfs_ops kbd_ops = {
	.write = NULL,
	.read  = kbd_read,
};

static int ps2_wait_read(void)
{
	int tries = 10000;
	while (tries-- && !(in(PS2_CMD) & 1));
	return tries >= 0;
}

static int ps2_wait_write(void)
{
	int tries = 10000;
	while (tries-- && (in(PS2_CMD) & 2));
	return tries >= 0;
}
static void ps2_keyboard_init(void)
{
	int i;
	kbd_head = 0;
	kbd_tail = 0;
	kbd_shift = 0;
	kbd_caps = 0;
	for (i = 0; i < 16; i++)
		kbd_key_state[i] = 0;

	irq_request(1, kbd_irq_handler);

	/* Enable the PS/2 keyboard interface on the controller */
	ps2_wait_write();
	out(PS2_CMD, 0xAE);

	/* Enable keyboard scanning */
	ps2_wait_write();
	out(PS2_DATA, 0xF4);

	kbd_file.ops = &kbd_ops;
	kbd_file.private_data = 0;

	vfs_stdin = &kbd_file;
}
VFS_DRIVER_INIT(ps2_keyboard_init);
