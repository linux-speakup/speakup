/* speakup_soft.c - speakup driver to register and make available
 * a user space device for software synthesizers.  written by: Kirk
 * Reiser <kirk@braille.uwo.ca>

 * Copyright (C) 2003  Kirk Reiser.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 * this code is specificly written as a driver for the speakup screenreview
 * package and is not a general device driver.  */

#include <linux/unistd.h>
#include <linux/miscdevice.h> /* for misc_register, and SYNTH_MINOR */
#include <linux/poll.h> /* for poll_wait() */
#include "spk_priv.h"

#define MY_SYNTH synth_soft
#define DRV_VERSION "0.10"
#define SOFTSYNTH_MINOR 26 /* might as well give it one more than /dev/synth */
#define PROCSPEECH 0x0d
#define CLEAR_SYNTH 0x18

static int softsynth_probe(void);
static void softsynth_release(void);
static void softsynth_start(void);
static void softsynth_flush(void);
static int softsynth_is_alive(void);
static unsigned char get_index(void);

static struct miscdevice synth_device;
static int misc_registered;
static int dev_opened;
DECLARE_WAIT_QUEUE_HEAD(wait_on_output);

static const char init_string[] = "\01@\x01\x31y\n";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "\x01+3p" },
	{ CAPS_STOP, "\x01-3p" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "\x01%ds", 5, 0, 9, 0, 0, 0 },
	{ PITCH, "\x01%dp", 5, 0, 9, 0, 0, 0 },
	{ VOL, "\x01%dv", 5, 0, 9, 0, 0, 0 },
	{ TONE, "\x01%dx", 1, 0, 2, 0, 0, 0 },
	{ PUNCT, "\x01%db", 7, 0, 15, 0, 0, 0 },
	{ VOICE, "\x01%do", 0, 0, 7, 0, 0, 0 },
	{ FREQ, "\x01%df", 5, 0, 9, 0, 0, 0 },
	V_LAST_NUM
};

struct spk_synth synth_soft = {
	.name = "soft",
	.version = DRV_VERSION,
	.long_name = "software synth",
	.init = init_string,
	.delay = 0,
	.trigger = 0,
	.jiffies = 0,
	.full = 0,
	.flush_wait = 0,
	.flags = SYNTH_START,
	.checkval = SYNTH_CHECK,
	.string_vars = stringvars,
	.num_vars = numvars,
	.probe = softsynth_probe,
	.release = softsynth_release,
	.synth_immediate = NULL,
	.catch_up = NULL,
	.start = softsynth_start,
	.flush = softsynth_flush,
	.is_alive = softsynth_is_alive,
	.synth_adjust = NULL,
	.read_buff_add = NULL,
	.get_index = get_index,
	.indexing = {
		.command = "\x01%di",
		.lowindex = 1,
		.highindex = 5,
		.currindex = 1,
	}
};

static int softsynth_open(struct inode *inode, struct file *fp)
{
	if (dev_opened)
		return -EBUSY;
	/*if ((fp->f_flags & O_ACCMODE) != O_RDONLY) */
	/*	return -EPERM; */
	dev_opened++;
	return 0;
}

static int softsynth_close(struct inode *inode, struct file *fp)
{
	fp->f_op = NULL;
	dev_opened = 0;
	return 0;
}

static ssize_t softsynth_read(struct file *fp, char *buf, size_t count,
			      loff_t *pos)
{
	int chars_sent = 0;
	unsigned long flags;
	DEFINE_WAIT(wait);

	spk_lock(flags);
	while (speakup_info.buff_in == speakup_info.buff_out) {
		prepare_to_wait(&wait_on_output, &wait, TASK_INTERRUPTIBLE);
		spk_unlock(flags);
		if (fp->f_flags & O_NONBLOCK) {
			finish_wait(&wait_on_output, &wait);
			return -EAGAIN;
		}
		if (signal_pending(current)) {
			finish_wait(&wait_on_output, &wait);
			return -ERESTARTSYS;
		}
		schedule();
		spk_lock(flags);
	}
	finish_wait(&wait_on_output, &wait);

	chars_sent = (count > speakup_info.buff_in-speakup_info.buff_out)
		? speakup_info.buff_in-speakup_info.buff_out : count;
	if (copy_to_user(buf, (char *) speakup_info.buff_out, chars_sent)) {
		spk_unlock(flags);
		return -EFAULT;
	}
	speakup_info.buff_out += chars_sent;
	*pos += chars_sent;
	if (speakup_info.buff_out >= speakup_info.buff_in) {
		synth_done();
		*pos = 0;
	}
	spk_unlock(flags);
	return chars_sent;
}

static int last_index = 0;

static ssize_t softsynth_write(struct file *fp, const char *buf, size_t count,
			       loff_t *pos)
{
	char indbuf[5];
	if (count >= sizeof(indbuf))
		return -EINVAL;

	if (copy_from_user(indbuf, buf, count))
		return -EFAULT;
	indbuf[4] = 0;

	last_index = simple_strtoul(indbuf, NULL, 0);
	return count;
}

static unsigned int softsynth_poll(struct file *fp,
		struct poll_table_struct *wait)
{
	unsigned long flags;
	int ret = 0;
	poll_wait(fp, &wait_on_output, wait);

	spk_lock(flags);
	if (speakup_info.buff_out < speakup_info.buff_in)
		ret = POLLIN | POLLRDNORM;
	spk_unlock(flags);
	return ret;
}

static void softsynth_flush(void)
{
	synth_printf("%c",'\x18');
}

static unsigned char get_index(void)
{
	int rv;
	rv = last_index;
	last_index = 0;
	return rv;
}

static struct file_operations softsynth_fops = {
	.poll = softsynth_poll,
	.read = softsynth_read,
	.write = softsynth_write,
	.open = softsynth_open,
	.release = softsynth_close,
};


static int softsynth_probe(void)
{

	if (misc_registered != 0)
		return 0;
	memset(&synth_device, 0, sizeof(synth_device));
	synth_device.minor = SOFTSYNTH_MINOR;
	synth_device.name = "softsynth";
	synth_device.fops = &softsynth_fops;
	if (misc_register(&synth_device)) {
		pr_warn("Couldn't initialize miscdevice /dev/softsynth.\n");
		return -ENODEV;
	}

	misc_registered = 1;
	pr_info("initialized device: /dev/softsynth, node (MAJOR 10, MINOR 26)\n");
	return 0;
}

static void softsynth_release(void)
{
	misc_deregister(&synth_device);
	misc_registered = 0;
	pr_info("unregistered /dev/softsynth\n");
}

static void softsynth_start(void)
{
	wake_up_interruptible(&wait_on_output);
}

static int softsynth_is_alive(void)
{
	if (speakup_info.alive)
		return 1;
	return 0;
}

module_param_named(start, MY_SYNTH.flags, short, S_IRUGO);


static int __init soft_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit soft_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(soft_init);
module_exit(soft_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_DESCRIPTION("Speakup userspace software synthesizer support");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

