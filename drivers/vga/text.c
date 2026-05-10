#include "vfs.h"
#include "pio.h"
#include <string.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_BUFFER ((uint16_t *)0xB8000)

struct vga_cursor {
	uint8_t x;
	uint8_t y;
};

struct vga_file_priv {
	struct vga_cursor *cursor;
	uint8_t color;
};

/* shared cursor state for all VGA text files */
static struct vga_cursor vga_cursor;

/* per-file private data */
static struct vga_file_priv stdout_priv;
static struct vga_file_priv stderr_priv;

/* static file structs */
static struct file vga_stdout_file;
static struct file vga_stderr_file;

static int vga_text_write(struct file *file, const void *buf, size_t nbyte);
static void vga_update_cursor(struct vga_cursor *cursor);

static struct vfs_ops vga_ops = {
	.write = vga_text_write,
	.read  = NULL,
};

static void vga_update_cursor(struct vga_cursor *cursor)
{
	uint16_t pos = cursor->y * VGA_WIDTH + cursor->x;

	out(0x3D4, 0x0E);
	out(0x3D5, pos >> 8);
	out(0x3D4, 0x0F);
	out(0x3D5, pos & 0xFF);
}

static int vga_text_write(struct file *file, const void *buf, size_t nbyte)
{
	struct vga_file_priv *priv = (struct vga_file_priv *)file->private_data;
	const char *str = (const char *)buf;
	uint16_t *vga_buf = VGA_BUFFER;
	size_t i;
	int written = 0;

	for (i = 0; i < nbyte; i++) {
		char c = str[i];

		switch (c) {
		case '\n':
			priv->cursor->y++;
			priv->cursor->x = 0;
			break;
		case '\r':
			priv->cursor->x = 0;
			break;
		case '\b':
			if (priv->cursor->x > 0)
				priv->cursor->x--;
			break;
		default:
			vga_buf[priv->cursor->y * VGA_WIDTH + priv->cursor->x] =
				(uint16_t)c | ((uint16_t)priv->color << 8);
			priv->cursor->x++;
			if (priv->cursor->x >= VGA_WIDTH) {
				priv->cursor->x = 0;
				priv->cursor->y++;
			}
			break;
		}

		if (priv->cursor->y >= VGA_HEIGHT) {
			memmove(VGA_BUFFER, VGA_BUFFER + VGA_WIDTH,
				VGA_WIDTH * (VGA_HEIGHT - 1) * sizeof(uint16_t));

			uint16_t blank = ' ' | ((uint16_t)priv->color << 8);
			int x;
			for (x = 0; x < VGA_WIDTH; x++)
				vga_buf[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;

			priv->cursor->y = VGA_HEIGHT - 1;
		}

		written++;
	}

	vga_update_cursor(priv->cursor);
	return written;
}

static void vga_text_clear(void)
{
	uint16_t blank = ' ' | (0x0F << 8);
	int i;
	for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
		VGA_BUFFER[i] = blank;
}

static void vga_text_init(void)
{
	vga_text_clear();

	vga_cursor.x = 0;
	vga_cursor.y = 0;
	vga_update_cursor(&vga_cursor);

	stdout_priv.cursor = &vga_cursor;
	stdout_priv.color  = 0x0F; /* white on black */

	stderr_priv.cursor = &vga_cursor;
	stderr_priv.color  = 0x04; /* red on black */

	vga_stdout_file.ops          = &vga_ops;
	vga_stdout_file.private_data = &stdout_priv;

	vga_stderr_file.ops          = &vga_ops;
	vga_stderr_file.private_data = &stderr_priv;

	vfs_stdout = &vga_stdout_file;
	vfs_stderr = &vga_stderr_file;
}
VFS_DRIVER_INIT(vga_text_init);
