/*
 * originally written by: Kirk Reiser <kirk@braille.uwo.ca>
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
 *
 * specificly written as a driver for the speakup screenreview
 * s not a general device driver.
 */
#include <linux/jiffies.h>

#include "spk_priv.h"
#include "serialio.h"
#include "speakup_dtlk.h" /* local header file for LiteTalk values */

#define MY_SYNTH synth_ltlk
#define DRV_VERSION "1.8"
#define PROCSPEECH 0x0d
#define synth_full() (!(inb(speakup_info.port_tts + UART_MSR) & UART_MSR_CTS))

static int synth_probe(void);
static void synth_flush(void);
static unsigned char get_index(void);

static const char init_string[] = "\01@\x01\x31y\n\0";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "\x01+35p" },
	{ CAPS_STOP, "\x01-35p" },
	V_LAST_STRING
};

static struct st_num_var numvars[] = {
	{ RATE, "\x01%ds", 8, 0, 9, 0, 0, 0 },
	{ PITCH, "\x01%dp", 50, 0, 99, 0, 0, 0 },
	{ VOL, "\x01%dv", 5, 0, 9, 0, 0, 0 },
	{ TONE, "\x01%dx", 1, 0, 2, 0, 0, 0 },
	{ PUNCT, "\x01%db", 7, 0, 15, 0, 0, 0 },
	{ VOICE, "\x01%do", 0, 0, 7, 0, 0, 0 },
	{ FREQ, "\x01%df", 5, 0, 9, 0, 0, 0 },
	V_LAST_NUM
};

static struct spk_synth synth_ltlk = {
	.name = "ltlk",
	.version = DRV_VERSION,
	.long_name = "LiteTalk",
	.init = init_string,
	.procspeech = PROCSPEECH,
	.delay = 500,
	.trigger = 50,
	.jiffies = 50,
	.full = 5000,
	.flush_wait = 0,
	.flags = SYNTH_START,
	.checkval = SYNTH_CHECK,
	.string_vars = stringvars,
	.num_vars = numvars,
	.probe = synth_probe,
	.release = spk_serial_release,
	.synth_immediate = spk_synth_immediate,
	.catch_up = spk_do_catch_up,
	.start = NULL,
	.flush = synth_flush,
	.is_alive = spk_synth_is_alive_restart,
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

static void synth_flush(void)
{
	spk_serial_out(SYNTH_CLEAR);
}

static unsigned char get_index(void)
{
	int c, lsr;/*, tmout = SPK_SERIAL_TIMEOUT; */
	lsr = inb(speakup_info.port_tts + UART_LSR);
	if ((lsr & UART_LSR_DR) == UART_LSR_DR) {
		c = inb(speakup_info.port_tts + UART_RX);
		return (unsigned char) c;
	}
	return 0;
}

/* interrogate the LiteTalk and print its settings */
static void synth_interrogate(void)
{
	unsigned char *t, i;
	unsigned char buf[50], rom_v[20];
	spk_synth_immediate(&MY_SYNTH, "\x18\x01?");
	for (i = 0; i < 50; i++) {
		buf[i] = spk_serial_in();
		if (i > 2 && buf[i] == 0x7f)
			break;
	}
	t = buf+2;
	for (i = 0; *t != '\r'; t++) {
		rom_v[i] = *t;
		if (i++ > 48)
			break;
	}
	rom_v[i] = 0;
	pr_info("%s: ROM version: %s\n", MY_SYNTH.long_name, rom_v);
}

static int synth_probe(void)
{
	int failed = 0;

	failed = serial_synth_probe();
	if (failed == 0)
		synth_interrogate();
	return failed;
}

module_param_named(start, MY_SYNTH.flags, short, S_IRUGO);

static int __init ltlk_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit ltlk_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(ltlk_init);
module_exit(ltlk_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for DoubleTalk LT/LiteTalk synthesizers");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

