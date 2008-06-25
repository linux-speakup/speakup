/*
 * originally written by: Kirk Reiser <kirk@braille.uwo.ca>
 * this version considerably modified by David Borowski, david575@rogers.com
 * eventually modified by Samuel Thibault <samuel.thibault@ens-lyon.org>
 *
 * Copyright (C) 1998-99  Kirk Reiser.
 * Copyright (C) 2003 David Borowski.
 * Copyright (C) 2007 Samuel Thibault.
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
#include "spk_priv.h"

#define PROCSPEECH '\n'
#define DRV_VERSION "2.3"
#define SYNTH_CLEAR 0x18

static struct st_string_var stringvars[] = {
	{ CAPS_START, "CAPS_START\n" },
	{ CAPS_STOP, "CAPS_STOP" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "RATE %d\n", 8, 1, 16, 0, 0, 0 },
	{ PITCH, "PITCH %d\n", 8, 0, 16, 0, 0, 0 },
	{ VOL, "VOL %d\n", 8, 0, 16, 0, 0, 0 },
	{ TONE, "TONE %d\n", 8, 0, 16, 0, 0, 0 },
	V_LAST_NUM
};

static struct spk_synth synth_dummy = {
	.name = "dummy",
	.version = DRV_VERSION,
	.long_name = "Dummy",
	.init = "Speakup\n",
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

module_param_named(ser, synth_dummy.ser, int, S_IRUGO);
module_param_named(start, synth_dummy.startup, short, S_IRUGO);

MODULE_PARM_DESC(ser, "Set the serial port for the synthesizer (0-based).");
MODULE_PARM_DESC(start, "Start the synthesizer once it is loaded.");

static int __init dummy_init(void)
{
	return synth_add(&synth_dummy);
}

static void __exit dummy_exit(void)
{
	synth_remove(&synth_dummy);
}

module_init(dummy_init);
module_exit(dummy_exit);
MODULE_AUTHOR("Samuel Thibault <samuel.thibault@ens-lyon.org>");
MODULE_DESCRIPTION("Speakup support for text console");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

