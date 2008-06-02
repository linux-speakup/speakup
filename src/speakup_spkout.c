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
 * specificly written as a driver for the speakup screenreview
 * s not a general device driver.
 */
#include "spk_priv.h"
#include "serialio.h"

#define DRV_VERSION "2.1"
#define SYNTH_CLEAR 0x18
#define PROCSPEECH '\r'

static void synth_flush(struct spk_synth *synth);

static struct st_string_var stringvars[] = {
	{ CAPS_START, "\x05P+" },
	{ CAPS_STOP, "\x05P-" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "\x05R%d", 7, 0, 9, 0, 0, 0 },
	{ PITCH, "\x05P%d", 3, 0, 9, 0, 0, 0 },
	{ VOL, "\x05V%d", 9, 0, 9, 0, 0, 0 },
	{ TONE, "\x05T%c", 8, 0, 25, 65, 0, 0 },
	{ PUNCT, "\x05M%c", 0, 0, 3, 0, 0, "nsma" },
	V_LAST_NUM
};

static struct spk_synth synth_spkout = {
	.name = "spkout",
	.version = DRV_VERSION,
	.long_name = "Speakout",
	.init = "\005W1\005I2\005C3",
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
	.flush = synth_flush,
	.is_alive = spk_synth_is_alive_restart,
	.synth_adjust = NULL,
	.read_buff_add = NULL,
	.get_index = spk_serial_in_nowait,
	.indexing = {
		.command = "\x05[%c",
		.lowindex = 1,
		.highindex = 5,
		.currindex = 1,
	}
};

static void synth_flush(struct spk_synth *synth)
{
	while (spk_serial_tx_busy())
		cpu_relax();
	outb(SYNTH_CLEAR, speakup_info.port_tts);
}

module_param_named(ser, synth_spkout.ser, int, S_IRUGO);
module_param_named(start, synth_spkout.startup, short, S_IRUGO);

MODULE_PARM_DESC(ser, "Set the serial port for the synthesizer (0-based).");
MODULE_PARM_DESC(start, "Start the synthesizer once it is loaded.");

static int __init spkout_init(void)
{
	return synth_add(&synth_spkout);
}

static void __exit spkout_exit(void)
{
	synth_remove(&synth_spkout);
}

module_init(spkout_init);
module_exit(spkout_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Speak Out synthesizers");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

