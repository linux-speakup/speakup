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
#include <linux/unistd.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include "spk_priv.h"
#include "serialio.h"

#define DRV_VERSION "2.4"
#define SYNTH_CLEAR 0x03
#define PROCSPEECH 0x0b
#define synth_full() (inb_p(speakup_info.port_tts) == 0x13)

static void do_catch_up(struct spk_synth *synth, unsigned long data);
static void synth_flush(struct spk_synth *synth);
static void read_buff_add(u_char c);
static unsigned char get_index(void);

static int in_escape;
static int is_flushing;

static struct st_string_var stringvars[] = {
	{ CAPS_START, "[:dv ap 200]" },
	{ CAPS_STOP, "[:dv ap 100]" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "[:ra %d]", 9, 0, 18, 150, 25, 0 },
	{ PITCH, "[:dv ap %d]", 80, 0, 200, 20, 0, 0 },
	{ VOL, "[:dv gv %d]", 13, 0, 14, 0, 5, 0 },
	{ PUNCT, "[:pu %c]", 0, 0, 2, 0, 0, "nsa" },
	{ VOICE, "[:n%c]", 0, 0, 9, 0, 0, "phfdburwkv" },
	V_LAST_NUM
};

static struct spk_synth synth_dectlk = {
	.name = "dectlk",
	.version = DRV_VERSION,
	.long_name = "Dectalk Express",
	.init = "[:dv ap 100][:error sp]",
	.procspeech = PROCSPEECH,
	.clear = SYNTH_CLEAR,
	.delay = 500,
	.trigger = 50,
	.jiffies = 50,
	.full = 1000,
	.flush_wait = 0,
	.flags = SF_DEC,
	.startup = SYNTH_START,
	.checkval = SYNTH_CHECK,
	.string_vars = stringvars,
	.num_vars = numvars,
	.probe = serial_synth_probe,
	.release = spk_serial_release,
	.synth_immediate = spk_synth_immediate,
	.catch_up = do_catch_up,
	.start = NULL,
	.flush = synth_flush,
	.is_alive = spk_synth_is_alive_restart,
	.synth_adjust = NULL,
	.read_buff_add = read_buff_add,
	.get_index = get_index,
	.indexing = {
		.command = "[:in re %d] ",
		.lowindex = 1,
		.highindex = 8,
		.currindex = 1,
	}
};

static int is_indnum(u_char *ch)
{
	if ((*ch >= '0') && (*ch <= '9')) {
		*ch = *ch - '0';
		return 1;
	}
	return 0;
}

static u_char lastind = 0;

static unsigned char get_index(void)
{
	u_char rv;
	rv = lastind;
	lastind = 0;
	return rv;
}

static void read_buff_add(u_char c)
{
	static int ind = -1;

	if (c == 0x01)
		is_flushing = 0;
	else if (is_indnum(&c)) {
		if (ind == -1)
			ind = c;
		else
			ind = ind * 10 + c;
	} else if ((c > 31) && (c < 127)) {
		if (ind != -1)
			lastind = (u_char)ind;
		ind = -1;
	}
}

static void do_catch_up(struct spk_synth *synth, unsigned long data)
{
	static u_char ch = 0;
	static u_char last = '\0';
	unsigned long flags;

	spk_lock(flags);
	if (is_flushing) {
		if (--is_flushing == 0)
			pr_warn("flush timeout\n");
		else {
			spk_unlock(flags);
			msleep(speakup_info.delay_time);
			spk_lock(flags);
		}
	}
	while (! synth_buffer_empty() && ! speakup_info.flushing) {
		if (! ch)
			ch = synth_buffer_peek();
		if (ch == '\n')
			ch = 0x0D;
		if (synth_full() || !spk_serial_out(ch)) {
			spk_unlock(flags);
			msleep(speakup_info.delay_time);
			spk_lock(flags);
		} else {
			synth_buffer_getc();
		}
		if (ch == '[')
			in_escape = 1;
		else if (ch == ']')
			in_escape = 0;
		else if (ch <= SPACE) {
			if (!in_escape && strchr(",.!?;:", last))
				spk_serial_out(PROCSPEECH);
		}
		last = ch;
		ch = 0;
	}
	synth_done();
	if (!in_escape)
		spk_serial_out(PROCSPEECH);
	spk_unlock(flags);
}

static void synth_flush(struct spk_synth *synth)
{
	if (in_escape) {
		/* if in command output ']' so we don't get an error */
		spk_serial_out(']');
	}
	in_escape = 0;
	spk_serial_out(SYNTH_CLEAR);
	is_flushing = 5; /* if no ctl-a in 4, send data anyway */
}

module_param_named(ser, synth_dectlk.ser, int, S_IRUGO);
module_param_named(start, synth_dectlk.startup, short, S_IRUGO);

MODULE_PARM_DESC(ser, "Set the serial port for the synthesizer (0-based).");
MODULE_PARM_DESC(start, "Start the synthesizer once it is loaded.");

static int __init dectlk_init(void)
{
	return synth_add(&synth_dectlk);
}

static void __exit dectlk_exit(void)
{
	synth_remove(&synth_dectlk);
}

module_init(dectlk_init);
module_exit(dectlk_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for DECtalk Express synthesizers");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

