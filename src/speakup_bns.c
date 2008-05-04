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
#include "serialio.h"

#define MY_SYNTH synth_bns
#define DRV_VERSION "1.8"
#define SYNTH_CLEAR 0x18
#define PROCSPEECH '\r'

static const char *synth_immediate(const char *buf);
static void do_catch_up(unsigned long data);
static void synth_flush(void);
static int synth_is_alive(void);

static const char init_string[] = "\x05Z\x05\x43";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "\x05\x31\x32P" },
	{ CAPS_STOP, "\x05\x38P" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "\x05%dE", 8, 1, 16, 0, 0, 0 },
	{ PITCH, "\x05%dP", 8, 0, 16, 0, 0, 0 },
	{ VOL, "\x05%dV", 8, 0, 16, 0, 0, 0 },
	{ TONE, "\x05%dT", 8, 0, 16, 0, 0, 0 },
	V_LAST_NUM
};

static struct spk_synth synth_bns = {
	.name = "bns",
	.version = DRV_VERSION,
	.long_name = "Braille 'N Speak",
	.init = init_string,
	.delay = 500,
	.trigger = 50,
	.jiffies = 50,
	.full = 5000,
	.flush_wait = 0,
	.flags = SYNTH_START,
	.checkval = SYNTH_CHECK,
	.string_vars = stringvars,
	.num_vars = numvars,
	.probe = serial_synth_probe,
	.release = spk_serial_release,
	.synth_immediate = synth_immediate,
	.catch_up = do_catch_up,
	.start = NULL,
	.flush = synth_flush,
	.is_alive = synth_is_alive,
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

static void do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+speakup_info.jiffy_delta;
	u_char ch;
	synth_stop_timer();
	while (speakup_info.buff_out < speakup_info.buff_in) {
		ch = *speakup_info.buff_out;
		if (ch == '\n')
			ch = PROCSPEECH;
		if (!spk_serial_out(ch)) {
			synth_delay(speakup_info.full_time);
			return;
		}
		speakup_info.buff_out++;
		if (jiffies >= jiff_max && ch == ' ') {
			spk_serial_out(PROCSPEECH);
			synth_delay(speakup_info.delay_time);
			return;
		}
	}
	spk_serial_out(PROCSPEECH);
	synth_done();
}

static const char *synth_immediate(const char *buf)
{
	u_char ch;
	while ((ch = *buf)) {
		if (ch == '\n')
			ch = PROCSPEECH;
		if (wait_for_xmitr())
			outb(ch, speakup_info.port_tts);
		else
			return buf;
		buf++;
	}
	return 0;
}

static void synth_flush(void)
{
	spk_serial_out(SYNTH_CLEAR);
}

static int synth_is_alive(void)
{
	if (speakup_info.alive)
		return 1;
	if (!speakup_info.alive && wait_for_xmitr() > 0) {
		/* restart */
		speakup_info.alive = 1;
		synth_printf("%s", MY_SYNTH.init);
		return 2;
	}
	pr_warn("%s: can't restart synth\n", MY_SYNTH.long_name);
	return 0;
}

module_param_named(start, MY_SYNTH.flags, short, S_IRUGO);

static int __init bns_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit bns_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(bns_init);
module_exit(bns_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Braille 'n Speak synthesizers");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

