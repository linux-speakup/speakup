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
 *
 * specificly written as a driver for the speakup screenreview
 * s not a general device driver.
 */
#include <linux/jiffies.h>

#include "spk_priv.h"
#include "serialio.h"

#define DRV_VERSION "1.8"
#define SYNTH_CLEAR 0x18
#define PROCSPEECH '\r'

static void synth_flush(struct spk_synth *synth);
static unsigned char get_index(void);

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
	.flags = SYNTH_START,
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
	.get_index = get_index,
	.indexing = {
		.command = "\x05[%c",
		.lowindex = 1,
		.highindex = 5,
		.currindex = 1,
	}
};

static void synth_flush(struct spk_synth *synth)
{
	while ((inb(speakup_info.port_tts + UART_LSR) & BOTH_EMPTY) != BOTH_EMPTY)
		cpu_relax();
	outb(SYNTH_CLEAR, speakup_info.port_tts);
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

module_param_named(start, synth_spkout.flags, short, S_IRUGO);

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

