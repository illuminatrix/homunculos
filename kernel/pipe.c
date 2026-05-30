#include "pipe.h"
#include "vfs.h"
#include "task.h"
#include "scheduler.h"
#include <stdint.h>
#include <string.h>

#define PIPE_BUF_SIZE 4096
#define PIPE_MAX_PIPES 16

struct pipe {
	uint8_t buf[PIPE_BUF_SIZE];
	int read_pos;
	int write_pos;
	int bytes;
	int read_refs;
	int write_refs;
	int reader_pid;
	int writer_pid;
};

static struct pipe pipe_pool[PIPE_MAX_PIPES];
static int pipe_used[PIPE_MAX_PIPES];

static struct pipe *pipe_alloc(void)
{
	int i;
	for (i = 0; i < PIPE_MAX_PIPES; i++) {
		if (!pipe_used[i]) {
			pipe_used[i] = 1;
			struct pipe *p = &pipe_pool[i];
			memset(p, 0, sizeof(struct pipe));
			p->read_refs = 1;
			p->write_refs = 1;
			p->reader_pid = -1;
			p->writer_pid = -1;
			return p;
		}
	}
	return 0;
}

static void pipe_free(struct pipe *p)
{
	int i;
	for (i = 0; i < PIPE_MAX_PIPES; i++) {
		if (&pipe_pool[i] == p) {
			pipe_used[i] = 0;
			return;
		}
	}
}

static int pipe_read_op(struct file *f, void *buf, size_t nbyte)
{
	struct pipe *p = (struct pipe *)f->private_data;
	int total = 0;
	char *dst = (char *)buf;

	if (!p)
		return -1;

	while (p->bytes == 0) {
		if (!p->write_refs)
			return 0;
		p->reader_pid = scheduler_get_current()->pid;
		task_block();
	}

	while (nbyte > 0 && p->bytes > 0) {
		*dst++ = p->buf[p->read_pos];
		p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
		p->bytes--;
		total++;
		nbyte--;
	}

	if (p->writer_pid >= 0) {
		int pid = p->writer_pid;
		p->writer_pid = -1;
		task_wake(pid);
	}

	return total;
}

static int pipe_write_op(struct file *f, const void *buf, size_t nbyte)
{
	struct pipe *p = (struct pipe *)f->private_data;
	int total = 0;
	const char *src = (const char *)buf;

	if (!p)
		return -1;

	while (nbyte > 0) {
		while (p->bytes >= PIPE_BUF_SIZE) {
			if (!p->read_refs)
				return -1;
			p->writer_pid = scheduler_get_current()->pid;
			task_block();
		}

		p->buf[p->write_pos] = *src++;
		p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
		p->bytes++;
		total++;
		nbyte--;
	}

	if (p->reader_pid >= 0) {
		int pid = p->reader_pid;
		p->reader_pid = -1;
		task_wake(pid);
	}

	return total;
}

static int pipe_close_read(struct file *f)
{
	struct pipe *p = (struct pipe *)f->private_data;
	if (!p)
		return -1;
	p->read_refs--;
	if (p->read_refs == 0) {
		if (p->writer_pid >= 0) {
			int pid = p->writer_pid;
			p->writer_pid = -1;
			task_wake(pid);
		}
		if (!p->write_refs)
			pipe_free(p);
	}
	return 0;
}

static int pipe_close_write(struct file *f)
{
	struct pipe *p = (struct pipe *)f->private_data;
	if (!p)
		return -1;
	p->write_refs--;
	if (p->write_refs == 0) {
		if (p->reader_pid >= 0) {
			int pid = p->reader_pid;
			p->reader_pid = -1;
			task_wake(pid);
		}
		if (!p->read_refs)
			pipe_free(p);
	}
	return 0;
}

static struct vfs_ops pipe_read_ops = {
	.read  = pipe_read_op,
	.write = 0,
	.ioctl = 0,
	.close = pipe_close_read,
};

static struct vfs_ops pipe_write_ops = {
	.read  = 0,
	.write = pipe_write_op,
	.ioctl = 0,
	.close = pipe_close_write,
};

int sys_pipe(int *fds)
{
	struct task *current = scheduler_get_current();
	struct pipe *p;
	struct file *rf, *wf;
	int rfd, wfd;
	int i;

	if (!current)
		return -1;

	p = pipe_alloc();
	if (!p)
		return -1;

	rf = vfs_alloc_file();
	if (!rf) {
		p->read_refs = 0;
		p->write_refs = 0;
		pipe_free(p);
		return -1;
	}
	rf->ops = &pipe_read_ops;
	rf->private_data = p;
	rf->pos = 0;
	rf->refcount = 1;

	wf = vfs_alloc_file();
	if (!wf) {
		rf->ops = 0;
		p->read_refs = 0;
		p->write_refs = 0;
		pipe_free(p);
		return -1;
	}
	wf->ops = &pipe_write_ops;
	wf->private_data = p;
	wf->pos = 0;
	wf->refcount = 1;

	rfd = -1;
	wfd = -1;
	for (i = 0; i < VFS_MAX_FD; i++) {
		if (!current->fd_table[i]) {
			if (rfd < 0) {
				rfd = i;
			} else if (wfd < 0) {
				wfd = i;
				break;
			}
		}
	}

	if (wfd < 0) {
		vfs_close_file(rf);
		vfs_close_file(wf);
		return -1;
	}

	current->fd_table[rfd] = rf;
	current->fd_table[wfd] = wf;

	fds[0] = rfd;
	fds[1] = wfd;
	return 0;
}

int sys_dup2(int oldfd, int newfd)
{
	struct task *current = scheduler_get_current();
	struct file *f;

	if (!current)
		return -1;
	if (oldfd < 0 || oldfd >= VFS_MAX_FD)
		return -1;
	if (newfd < 0 || newfd >= VFS_MAX_FD)
		return -1;

	f = current->fd_table[oldfd];
	if (!f)
		return -1;

	if (oldfd == newfd)
		return newfd;

	if (current->fd_table[newfd])
		vfs_close_file(current->fd_table[newfd]);

	f->refcount++;
	current->fd_table[newfd] = f;
	return newfd;
}
