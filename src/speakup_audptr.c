/*
 * originially written by: Kirk Reiser <kirk@braille.uwo.ca>
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

#define DRV_VERSION "1.10"
#define SYNTH_CLEAR 0x18 /* flush synth buffer */
#define PROCSPEECH '\r' /* start synth processing speech char */

static int synth_probe(struct spk_synth *synth);
static void synth_flush(struct spk_synth *synth);

static struct st_string_var stringvars[] = {
	{ CAPS_START, "\x05[f99]" },
	{ CAPS_STOP, "\x05[f80]" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "\x05[r%d]", 10, 0, 20, -100, 10, 0 },
	{ PITCH, "\x05[f%d]", 80, 39, 4500, 0, 0, 0 },
	{ VOL, "\x05[g%d]", 21, 0, 40, 0, 0, 0 },
	{ TONE, "\x05[s%d]", 9, 0, 63, 0, 0, 0 },
	{ PUNCT, "\x05[A%c]", 0, 0, 3, 0, 0, "nmsa" },
	V_LAST_NUM
};

static struct spk_synth synth_audptr = {
	.name = "audptr",
	.version = DRV_VERSION,
	.long_name = "Audapter",
	.init = "\x05[D1]\x05[Ol]",
	.procspeech = PROCSPEECH,
	.clear = SYNTH_CLEAR,
	.delay = 400,
	.trigger = 5,
	.jiffies = 30,
	.full = 5000,
	.flush_wait = 0,
	.startup = SYNTH_START,
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
	.get_index = NULL,
	.indexing = {
		.command = NULL,
		.lowindex = 0,
		.highindex = 0,
		.currindex = 0,
	}
};

static void synth_flush(struct spk_synth *synth)
{
	while (spk_serial_tx_busy())
		cpu_relax();
	outb(SYNTH_CLEAR, speakup_info.port_tts);
	spk_serial_out(PROCSPEECH);
}

static void synth_version(struct spk_synth *synth)
{
	unsigned char test = 0;
	char synth_id[40] = "";
	spk_synth_immediate(synth, "\x05[Q]");
	synth_id[test] = spk_serial_in();
	if (synth_id[test] == 'A') {
		do {
			/* read version string from synth */
			synth_id[++test] = spk_serial_in();
		} while (synth_id[test] != '\n' && test < 32);
		synth_id[++test] = 0x00;
	}
	if (synth_id[0] == 'A')
		pr_info("%s version: %s", synth->long_name, synth_id);
}

static int synth_probe(struct spk_synth *synth)
{
	int failed = 0;

	failed = serial_synth_probe(synth);
	if (failed == 0)
		synth_version(synth);
	return 0;
}

module_param_named(ser, synth_audptr.ser, int, S_IRUGO);
module_param_named(start, synth_audptr.startup, short, S_IRUGO);

MODULE_PARM_DESC(ser, "Set the serial port for the synthesizer (0-based).");
MODULE_PARM_DESC(start, "Start the synthesizer once it is loaded.");

static int __init audptr_init(void)
{
	return synth_add(&synth_audptr);
}

static void __exit audptr_exit(void)
{
	synth_remove(&synth_audptr);
}

module_init(audptr_init);
module_exit(audptr_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Audapter synthesizer");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

