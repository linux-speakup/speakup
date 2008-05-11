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

#define DRV_VERSION "1.11"
#define SYNTH_CLEAR 0x18
#define PROCSPEECH '\r'

static void do_catch_up(struct spk_synth *synth, unsigned long data);

static struct st_string_var stringvars[] = {
	{ CAPS_START, "cap, " },
	{ CAPS_STOP, "" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "@W%d", 6, 1, 9, 0, 0, 0 },
	{ PITCH, "@F%x", 10, 0, 15, 0, 0, 0 },
	{ VOL, "@A%x", 10, 0, 15, 0, 0, 0 },
	{ VOICE, "@V%d", 1, 1, 6, 0, 0, 0 },
	{ LANG, "@=%d,", 1, 1, 4, 0, 0, 0 },
	V_LAST_NUM
};

static struct spk_synth synth_apollo = {
	.name = "apollo",
	.version = DRV_VERSION,
	.long_name = "Apollo",
	.init = "@R3@D0@K1\r",
	.procspeech = PROCSPEECH,
	.clear = SYNTH_CLEAR,
	.delay = 500,
	.trigger = 50,
	.jiffies = 50,
	.full = 5000,
	.flush_wait = 0,
	.startup = SYNTH_START,
	.checkval = SYNTH_CHECK,
	.string_vars = stringvars,
	.num_vars = numvars,
	.probe = serial_synth_probe,
	.release = spk_serial_release,
	.synth_immediate = spk_synth_immediate,
	.catch_up = do_catch_up,
	.start = NULL,
	.flush = spk_synth_flush,
	.is_alive = spk_synth_is_alive_restart,
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

static void do_catch_up(struct spk_synth *synth, unsigned long data)
{
	unsigned long jiff_max = jiffies+speakup_info.jiffy_delta;
	u_char ch;
	synth_stop_timer();
	while (speakup_info.buff_out < speakup_info.buff_in) {
		ch = *speakup_info.buff_out;
		if (!spk_serial_out(ch)) {
			outb(UART_MCR_DTR, speakup_info.port_tts + UART_MCR);
			outb(UART_MCR_DTR | UART_MCR_RTS,
					speakup_info.port_tts + UART_MCR);
			synth_delay(speakup_info.full_time);
			return;
		}
		speakup_info.buff_out++;
		if (jiffies >= jiff_max && ch == SPACE) {
			spk_serial_out(PROCSPEECH);
			synth_delay(speakup_info.delay_time);
			return;
		}
	}
	spk_serial_out(PROCSPEECH);
	synth_done();
}

module_param_named(ser, synth_apollo.ser, int, S_IRUGO);
module_param_named(start, synth_apollo.startup, short, S_IRUGO);

MODULE_PARM_DESC(ser, "Set the serial port for the synthesizer (0-based).");
MODULE_PARM_DESC(start, "Start the synthesizer once it is loaded.");

static int __init apollo_init(void)
{
	return synth_add(&synth_apollo);
}

static void __exit apollo_exit(void)
{
	synth_remove(&synth_apollo);
}

module_init(apollo_init);
module_exit(apollo_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Apollo II synthesizer");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

