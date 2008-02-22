/*
 * originially written by: Kirk Reiser <kirk@braille.uwo.ca>
 * this version considerably modified by David Borowski, david575@rogers.com

 * Copyright (C) 1998-99  Kirk Reiser.
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

 * this code is specificly written as a driver for the speakup screenreview
 * package and is not a general device driver.
 */

#include <linux/jiffies.h>

#include "spk_priv.h"
#include "speakup_acnt.h" /* local header file for Accent values */

#define MY_SYNTH synth_acntpc
#define DRV_VERSION "1.2"
#define synth_readable() (inb_p(synth_port_control) & SYNTH_READABLE)
#define synth_writable() (inb_p(synth_port_control) & SYNTH_WRITABLE)
#define synth_full() (inb_p(synth_port_tts) == 'F')
#define PROCSPEECH '\r'


static int synth_port_control;
static unsigned int synth_portlist[] = { 0x2a8, 0 };

static const char *synth_immediate(const char *buf)
{
	u_char ch;
	while ((ch = *buf)) {
	if (ch == '\n')
		ch = PROCSPEECH;
		if (synth_full())
			return buf;
		while (synth_writable());
		outb_p(ch, synth_port_tts);
		buf++;
	}
	return 0;
}

static void do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+synth_jiffy_delta;
	u_char ch;
	synth_stop_timer();
	while (synth_buff_out < synth_buff_in) {
		if (synth_full()) {
			synth_delay(synth_full_time);
			return;
		}
		while (synth_writable());
		ch = *synth_buff_out++;
		if (ch == '\n')
			ch = PROCSPEECH;
		outb_p(ch, synth_port_tts);
		if (jiffies >= jiff_max && ch == SPACE) {
			while (synth_writable());
			outb_p(PROCSPEECH, synth_port_tts);
			synth_delay(synth_delay_time);
			return;
		}
	}
	while (synth_writable());
	outb_p(PROCSPEECH, synth_port_tts);
	synth_done();
}

static void synth_flush(void)
{
	outb_p(SYNTH_CLEAR, synth_port_tts);
}

static int synth_probe(void)
{
	unsigned int port_val = 0;
	int i = 0;
	pr_info("Probing for %s.\n", synth->long_name);
	if (synth_port_forced) {
		synth_port_tts = synth_port_forced;
		pr_info("probe forced to %x by kernel command line\n",
				synth_port_tts);
		if (synth_request_region(synth_port_tts-1, SYNTH_IO_EXTENT)) {
			pr_warn("sorry, port already reserved\n");
			return -EBUSY;
		}
		port_val = inw(synth_port_tts-1);
		synth_port_control = synth_port_tts-1;
	} else {
		for (i = 0; synth_portlist[i]; i++) {
			if (synth_request_region(synth_portlist[i],
						SYNTH_IO_EXTENT)) {
				pr_warn("request_region: failed with 0x%x, %d\n",
					synth_portlist[i], SYNTH_IO_EXTENT);
				continue;
			}
			port_val = inw(synth_portlist[i]) & 0xfffc;
			if (port_val == 0x53fc) {
				/* 'S' and out&input bits */
				synth_port_control = synth_portlist[i];
				synth_port_tts = synth_port_control+1;
				break;
			}
		}
	}
	port_val &= 0xfffc;
	if (port_val != 0x53fc) {
		/* 'S' and out&input bits */
		pr_info("%s: not found\n", synth->long_name);
		synth_release_region(synth_portlist[i], SYNTH_IO_EXTENT);
		synth_port_control = 0;
		return -ENODEV;
	}
	pr_info("%s: %03x-%03x, driver version %s,\n", synth->long_name,
		synth_port_control,	synth_port_control+SYNTH_IO_EXTENT-1,
		synth->version);
	return 0;
}

static void accent_release(void)
{
	if (synth_port_tts)
		synth_release_region(synth_port_tts-1, SYNTH_IO_EXTENT);
	synth_port_tts = 0;
}

static int synth_is_alive(void)
{
	synth_alive = 1;
	return 1;
}

static const char init_string[] = "\033=X \033Oi\033T2\033=M\033N1\n";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "\033P8" },
	{ CAPS_STOP, "\033P5" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "\033R%c", 9, 0, 17, 0, 0, "0123456789abcdefgh" },
	{ PITCH, "\033P%d", 5, 0, 9, 0, 0, 0 },
	{ VOL, "\033A%d", 5, 0, 9, 0, 0, 0 },
	{ TONE, "\033V%d", 5, 0, 9, 0, 0, 0 },
	V_LAST_NUM
};

struct spk_synth synth_acntpc = {"acntpc", DRV_VERSION, "Accent PC",
	init_string, 500, 50, 50, 1000, 0, 0, SYNTH_CHECK,
	stringvars, numvars, synth_probe, accent_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, NULL, NULL,
	{NULL, 0, 0, 0} };

module_param_named(start, MY_SYNTH.flags, short, S_IRUGO);

static int __init acntpc_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit acntpc_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(acntpc_init);
module_exit(acntpc_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Accent PC synthesizer");
MODULE_LICENSE("GPL");

