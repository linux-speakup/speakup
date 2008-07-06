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
#include "spk_priv.h"

#define DRV_VERSION "2.5"
#define SYNTH_CLEAR 0x18
#define PROCSPEECH '\r'

static struct var_t vars[] = {
	{ CAPS_START, .u.s = {"\x05\x31\x32P" }},
	{ CAPS_STOP, .u.s = {"\x05\x38P" }},
	{ RATE, .u.n = {"\x05%dE", 8, 1, 16, 0, 0, NULL }},
	{ PITCH, .u.n = {"\x05%dP", 8, 0, 16, 0, 0, NULL }},
	{ VOL, .u.n = {"\x05%dV", 8, 0, 16, 0, 0, NULL }},
	{ TONE, .u.n = {"\x05%dT", 8, 0, 16, 0, 0, NULL }},
	V_LAST_VAR
};

static struct spk_synth synth_bns = {
	.name = "bns",
	.version = DRV_VERSION,
	.long_name = "Braille 'N Speak",
	.init = "\x05Z\x05\x43",
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
	.catch_up = spk_do_catch_up,
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

module_param_named(ser, synth_bns.ser, int, S_IRUGO);
module_param_named(start, synth_bns.startup, short, S_IRUGO);

MODULE_PARM_DESC(ser, "Set the serial port for the synthesizer (0-based).");
MODULE_PARM_DESC(start, "Start the synthesizer once it is loaded.");

static int __init bns_init(void)
{
	return synth_add(&synth_bns);
}

static void __exit bns_exit(void)
{
	synth_remove(&synth_bns);
}

module_init(bns_init);
module_exit(bns_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Braille 'n Speak synthesizers");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

