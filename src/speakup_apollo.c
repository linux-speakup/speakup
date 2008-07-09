/*
 * originally written by: Kirk Reiser <kirk@braille.uwo.ca>
* this version considerably modified by David Borowski, david575@rogers.com
 *
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
 * this code is specificly written as a driver for the speakup screenreview
 * package and is not a general device driver.
 */
#include <linux/jiffies.h>

#include "spk_priv.h"
#include "serialio.h"

#define DRV_VERSION "2.6"
#define SYNTH_CLEAR 0x18
#define PROCSPEECH '\r'

static void do_catch_up(struct spk_synth *synth);

static struct var_t vars[] = {
	{ CAPS_START, .u.s = {"cap, " }},
	{ CAPS_STOP, .u.s = {"" }},
	{ RATE, .u.n = {"@W%d", 6, 1, 9, 0, 0, NULL }},
	{ PITCH, .u.n = {"@F%x", 10, 0, 15, 0, 0, NULL }},
	{ VOL, .u.n = {"@A%x", 10, 0, 15, 0, 0, NULL }},
	{ VOICE, .u.n = {"@V%d", 1, 1, 6, 0, 0, NULL }},
	{ LANG, .u.n = {"@=%d,", 1, 1, 4, 0, 0, NULL }},
	V_LAST_VAR
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
	.vars = vars,
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

static void do_catch_up(struct spk_synth *synth)
{
	u_char ch;
	unsigned long flags;
	struct var_t *full_time;

	while (1) {
		spk_lock(flags);
		if (speakup_info.flushing) {
			speakup_info.flushing = 0;
			spk_unlock(flags);
			synth->flush(synth);
			continue;
		}
		if (synth_buffer_empty()) {
			spk_unlock(flags);
			break;
		}
		ch = synth_buffer_peek();
		spk_unlock(flags);
		if (!spk_serial_out(ch)) {
			outb(UART_MCR_DTR, speakup_info.port_tts + UART_MCR);
			outb(UART_MCR_DTR | UART_MCR_RTS,
					speakup_info.port_tts + UART_MCR);
			full_time = get_var(FULL);
			msleep(full_time->u.n.value);
			continue;
		}
		spk_lock(flags);
		synth_buffer_getc();
		spk_unlock(flags);
	}
	spk_serial_out(PROCSPEECH);
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

