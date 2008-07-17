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
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/timer.h>

#include "spk_priv.h"
#include "serialio.h"
#include "speakup.h"

#define DRV_VERSION "2.9"
#define SYNTH_CLEAR 0x03
#define PROCSPEECH 0x0b
#define synth_full() (inb_p(speakup_info.port_tts) == 0x13)

static void do_catch_up(struct spk_synth *synth);
static void synth_flush(struct spk_synth *synth);

static int in_escape;

static struct var_t vars[] = {
	{ CAPS_START, .u.s = {"[:dv ap 222]" }},
	{ CAPS_STOP, .u.s = {"[:dv ap 100]" }},
	{ RATE, .u.n = {"[:ra %d]", 7, 0, 9, 150, 25, NULL }},
	{ PITCH, .u.n = {"[:dv ap %d]", 100, 0, 100, 0, 0, NULL }},
	{ VOL, .u.n = {"[:dv gv %d]", 13, 0, 16, 0, 5, NULL }},
	{ PUNCT, .u.n = {"[:pu %c]", 0, 0, 2, 0, 0, "nsa" }},
	{ VOICE, .u.n = {"[:n%c]", 0, 0, 9, 0, 0, "phfdburwkv" }},
	V_LAST_VAR
};

static struct spk_synth synth_decext = {
	.name = "decext",
	.version = DRV_VERSION,
	.long_name = "Dectalk External",
	.init = "[:pe -380]",
	.procspeech = PROCSPEECH,
	.clear = SYNTH_CLEAR,
	.delay = 500,
	.trigger = 50,
	.jiffies = 50,
	.full = 1000,
	.flags = SF_DEC,
	.startup = SYNTH_START,
	.checkval = SYNTH_CHECK,
	.vars = vars,
	.probe = serial_synth_probe,
	.release = spk_serial_release,
	.synth_immediate = spk_synth_immediate,
	.catch_up = do_catch_up,
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

static void do_catch_up(struct spk_synth *synth)
{
	u_char ch;
	static u_char last = '\0';
	unsigned long flags;
	struct var_t *delay_time;
	DEFINE_WAIT(wait);

	while (1) {
		spk_lock(flags);
		prepare_to_wait(&speakup_event, &wait, TASK_INTERRUPTIBLE);
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
		if (ch == '\n')
			ch = 0x0D;
		if (synth_full() || !spk_serial_out(ch)) {
			delay_time = get_var(DELAY);
			schedule_timeout(msecs_to_jiffies(delay_time->u.n.value));
			continue;
		}
		spk_lock(flags);
		synth_buffer_getc();
		spk_unlock(flags);
		if (ch == '[')
			in_escape = 1;
		else if (ch == ']')
			in_escape = 0;
		else if (ch <= SPACE) {
			if (!in_escape && strchr(",.!?;:", last))
				spk_serial_out(PROCSPEECH);
		}
		last = ch;
	}
	finish_wait(&speakup_event, &wait);
	if (!in_escape)
		spk_serial_out(PROCSPEECH);
}

static void synth_flush(struct spk_synth *synth)
{
	in_escape = 0;
	spk_synth_immediate(synth, "\033P;10z\033\\");
}

module_param_named(ser, synth_decext.ser, int, S_IRUGO);
module_param_named(start, synth_decext.startup, short, S_IRUGO);

MODULE_PARM_DESC(ser, "Set the serial port for the synthesizer (0-based).");
MODULE_PARM_DESC(start, "Start the synthesizer once it is loaded.");

static int __init decext_init(void)
{
	return synth_add(&synth_decext);
}

static void __exit decext_exit(void)
{
	synth_remove(&synth_decext);
}

module_init(decext_init);
module_exit(decext_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for DECtalk External synthesizers");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

