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

#define DRV_VERSION "1.8"
#define synth_readable() (inb_p(synth_port_control) & SYNTH_READABLE)
#define synth_writable() (inb_p(synth_port_control) & SYNTH_WRITABLE)
#define synth_full() (inb_p(speakup_info.port_tts) == 'F')
#define PROCSPEECH '\r'

static int synth_probe(struct spk_synth *synth);
static void accent_release(void);
static const char *synth_immediate(struct spk_synth *synth, const char *buf);
static void do_catch_up(struct spk_synth *synth, unsigned long data);
static void synth_flush(struct spk_synth *synth);

static int synth_port_control;
static int port_forced;
static unsigned int synth_portlist[] = { 0x2a8, 0 };

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

static struct spk_synth synth_acntpc = {
	.name = "acntpc",
	.version = DRV_VERSION,
	.long_name = "Accent PC",
	.init = "\033=X \033Oi\033T2\033=M\033N1\n",
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
	.release = accent_release,
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

static const char *synth_immediate(struct spk_synth *synth, const char *buf)
{
	u_char ch;
	while ((ch = *buf)) {
		if (ch == '\n')
			ch = PROCSPEECH;
		if (synth_full())
			return buf;
		while (synth_writable())
			cpu_relax();
		outb_p(ch, speakup_info.port_tts);
		buf++;
	}
	return 0;
}

static void do_catch_up(struct spk_synth *synth, unsigned long data)
{
	unsigned long jiff_max = jiffies+speakup_info.jiffy_delta;
	u_char ch;
	synth_stop_timer();
	while (speakup_info.buff_out < speakup_info.buff_in) {
		if (synth_full()) {
			synth_delay(speakup_info.full_time);
			return;
		}
		while (synth_writable())
			cpu_relax();
		ch = *speakup_info.buff_out++;
		if (ch == '\n')
			ch = PROCSPEECH;
		outb_p(ch, speakup_info.port_tts);
		if (jiffies >= jiff_max && ch == SPACE) {
			while (synth_writable())
				cpu_relax();
			outb_p(PROCSPEECH, speakup_info.port_tts);
			synth_delay(speakup_info.delay_time);
			return;
		}
	}
	while (synth_writable())
		cpu_relax();
	outb_p(PROCSPEECH, speakup_info.port_tts);
	synth_done();
}

static void synth_flush(struct spk_synth *synth)
{
	outb_p(SYNTH_CLEAR, speakup_info.port_tts);
}

static int synth_probe(struct spk_synth *synth)
{
	unsigned int port_val = 0;
	int i = 0;
	pr_info("Probing for %s.\n", synth->long_name);
	if (port_forced) {
		speakup_info.port_tts = port_forced;
		pr_info("probe forced to %x by kernel command line\n",
				speakup_info.port_tts);
		if (synth_request_region(speakup_info.port_tts-1,
					SYNTH_IO_EXTENT)) {
			pr_warn("sorry, port already reserved\n");
			return -EBUSY;
		}
		port_val = inw(speakup_info.port_tts-1);
		synth_port_control = speakup_info.port_tts-1;
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
				speakup_info.port_tts = synth_port_control+1;
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
		synth_port_control, synth_port_control+SYNTH_IO_EXTENT-1,
		synth->version);
	return 0;
}

static void accent_release(void)
{
	if (speakup_info.port_tts)
		synth_release_region(speakup_info.port_tts-1, SYNTH_IO_EXTENT);
	speakup_info.port_tts = 0;
}

module_param_named(port, port_forced, int, S_IRUGO);
module_param_named(start, synth_acntpc.startup, short, S_IRUGO);

MODULE_PARM_DESC(port, "Set the port for the synthesizer (override probing).");
MODULE_PARM_DESC(start, "Start the synthesizer once it is loaded.");

static int __init acntpc_init(void)
{
	return synth_add(&synth_acntpc);
}

static void __exit acntpc_exit(void)
{
	synth_remove(&synth_acntpc);
}

module_init(acntpc_init);
module_exit(acntpc_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Accent PC synthesizer");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

