/*
 * written by David Borowski
 *
 * Copyright (C) 2003 David Borowski.
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
 *
 * specificly written as a driver for the speakup screenreview
 * package it's not a general device driver.
 * This driver is for the Keynote Gold internal synthesizer.
 */
#include <linux/jiffies.h>

#include "spk_priv.h"

#define DRV_VERSION "2.2"
#define SYNTH_IO_EXTENT	0x04
#define SWAIT udelay(70)
#define synth_writable() (inb_p(synth_port) & 0x10)
#define synth_readable() (inb_p(synth_port) & 0x10)
#define synth_full() ((inb_p(synth_port) & 0x80) == 0)
#define PROCSPEECH 0x1f
#define SYNTH_CLEAR 0x03

static int synth_probe(struct spk_synth *synth);
static void keynote_release(void);
static const char *synth_immediate(struct spk_synth *synth, const char *buf);
static void do_catch_up(struct spk_synth *synth, unsigned long data);
static void synth_flush(struct spk_synth *synth);

static int synth_port;
static int port_forced;
static unsigned int synth_portlist[] = { 0x2a8, 0 };

static struct st_string_var stringvars[] = {
	{ CAPS_START, "[f130]" },
	{ CAPS_STOP, "[f90]" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "\04%c ", 8, 0, 10, 81, -8, 0 },
	{ PITCH, "[f%d]", 5, 0, 9, 40, 10, 0 },
	V_LAST_NUM
};

static struct spk_synth synth_keypc = {
	.name = "keypc",
	.version = DRV_VERSION,
	.long_name = "Keynote PC",
	.init = "[t][n7,1][n8,0]",
	.procspeech = PROCSPEECH,
	.clear = SYNTH_CLEAR,
	.delay = 500,
	.trigger = 50,
	.jiffies = 50,
	.full = 1000,
	.flush_wait = 0,
	.startup = SYNTH_START,
	.checkval = SYNTH_CHECK,
	.string_vars = stringvars,
	.num_vars = numvars,
	.probe = synth_probe,
	.release = keynote_release,
	.synth_immediate = synth_immediate,
	.catch_up = do_catch_up,
	.start = NULL,
	.flush = synth_flush,
	.is_alive = spk_synth_is_alive_nop,
	.synth_adjust = NULL,
	.read_buff_add = NULL,
	.get_index = NULL,
	.indexing = {
		.command = NULL,
		.lowindex = 0,
		.highindex = 0,
		.currindex = 0,
	}
};

static int oops(void)
{
	int s1, s2, s3, s4;
	s1 = inb_p(synth_port);
	s2 = inb_p(synth_port+1);
	s3 = inb_p(synth_port+2);
	s4 = inb_p(synth_port+3);
	pr_warn("synth timeout %d %d %d %d\n", s1, s2, s3, s4);
	return 0;
}

static const char *synth_immediate(struct spk_synth *synth, const char *buf)
{
	u_char ch;
	int timeout;
	while ((ch = *buf)) {
		if (ch == '\n')
			ch = PROCSPEECH;
		if (synth_full())
			return buf;
		timeout = 1000;
		while (synth_writable())
			if (--timeout <= 0)
				return (char *) oops();
		outb_p(ch, synth_port);
		udelay(70);
		buf++;
	}
	return 0;
}

static void do_catch_up(struct spk_synth *synth, unsigned long data)
{
	u_char ch;
	int timeout;
	unsigned long flags;

	spk_lock(flags);
	while (! synth_buffer_empty() && ! speakup_info.flushing) {
		spk_unlock(flags);
		if (synth_full())
			msleep(speakup_info.delay_time);
		timeout = 1000;
		while (synth_writable())
			if (--timeout <= 0)
				break;
		if (timeout <= 0) {
			oops();
			break;
		}
		spk_lock(flags);
		ch = synth_buffer_getc();
		spk_unlock(flags);
		if (ch == '\n')
			ch = PROCSPEECH;
		outb_p(ch, synth_port);
		SWAIT;
		spk_lock(flags);
	}
	synth_done();
	spk_unlock(flags);
	timeout = 1000;
	while (synth_writable())
		if (--timeout <= 0)
			break;
	if (timeout <= 0)
		oops();
	else
		outb_p(PROCSPEECH, synth_port);
}

static void synth_flush(struct spk_synth *synth)
{
	outb_p(SYNTH_CLEAR, synth_port);
}

static int synth_probe(struct spk_synth *synth)
{
	unsigned int port_val = 0;
	int i = 0;
	pr_info("Probing for %s.\n", synth->long_name);
	if (port_forced) {
		synth_port = port_forced;
		pr_info("probe forced to %x by kernel command line\n",
				synth_port);
		if (synth_request_region(synth_port-1, SYNTH_IO_EXTENT)) {
			pr_warn("sorry, port already reserved\n");
			return -EBUSY;
		}
		port_val = inb(synth_port);
	} else {
		for (i = 0; synth_portlist[i]; i++) {
			if (synth_request_region(synth_portlist[i],
						SYNTH_IO_EXTENT)) {
				pr_warn("request_region: failed with 0x%x, %d\n",
					synth_portlist[i], SYNTH_IO_EXTENT);
				continue;
			}
			port_val = inb(synth_portlist[i]);
			if (port_val == 0x80) {
				synth_port = synth_portlist[i];
				break;
			}
		}
	}
	if (port_val != 0x80) {
		pr_info("%s: not found\n", synth->long_name);
		synth_release_region(synth_portlist[i], SYNTH_IO_EXTENT);
		synth_port = 0;
		return -ENODEV;
	}
	pr_info("%s: %03x-%03x, driver version %s,\n", synth->long_name,
		synth_port, synth_port+SYNTH_IO_EXTENT-1,
		synth->version);
	return 0;
}

static void keynote_release(void)
{
	if (synth_port)
		synth_release_region(synth_port, SYNTH_IO_EXTENT);
	synth_port = 0;
}

module_param_named(port, port_forced, int, S_IRUGO);
module_param_named(start, synth_keypc.startup, short, S_IRUGO);

MODULE_PARM_DESC(port, "Set the port for the synthesizer (override probing).");
MODULE_PARM_DESC(start, "Start the synthesizer once it is loaded.");

static int __init keypc_init(void)
{
	return synth_add(&synth_keypc);
}

static void __exit keypc_exit(void)
{
	synth_remove(&synth_keypc);
}

module_init(keypc_init);
module_exit(keypc_exit);
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Keynote Gold PC synthesizers");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

