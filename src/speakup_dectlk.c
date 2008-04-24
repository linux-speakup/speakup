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

#define MY_SYNTH synth_dectlk
#define DRV_VERSION "1.11"
#define SYNTH_CLEAR 0x03
#define PROCSPEECH 0x0b
#define synth_full() (inb_p(speakup_info.port_tts) == 0x13)

static int synth_probe(void);
static const char *synth_immediate(const char *buf);
static void do_catch_up(unsigned long data);
static void synth_flush(void);
static int synth_is_alive(void);
void read_buff_add(u_char c);
static unsigned char get_index(void);

static int in_escape;
static int is_flushing;
atomic_t dectest = ATOMIC_INIT(0);
static const char init_string[] = "[:dv ap 100][:error sp]";

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

struct spk_synth synth_dectlk = { "dectlk", DRV_VERSION, "Dectalk Express",
	init_string, 500, 50, 50, 1000, 0, 0, SYNTH_CHECK,
	stringvars, numvars, synth_probe, spk_serial_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, read_buff_add,
	get_index, {"[:in re %d] ", 1, 8, 1} };

static int is_indnum(u_char *ch)
{
	if ((*ch > 47) && (*ch < 58)) {
		*ch = *ch - 48;
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

void read_buff_add(u_char c)
{
	static int ind = -1;

	if (c == 0x01) {
		is_flushing = 0;
		atomic_set(&dectest, 0);
	} else if (is_indnum(&c)) {
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

static void do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+speakup_info.jiffy_delta;
	u_char ch;
	static u_char last = '\0';
	synth_stop_timer();
	if (is_flushing) {
		if (--is_flushing == 0)
			pr_warn("flush timeout\n");
		else {
			synth_delay(speakup_info.delay_time);
			return;
		}
	}
	while (speakup_info.buff_out < speakup_info.buff_in) {
		ch = *speakup_info.buff_out;
		if (ch == '\n')
			ch = 0x0D;
		if (synth_full() || !spk_serial_out(ch)) {
			synth_delay(speakup_info.full_time);
			return;
		}
		speakup_info.buff_out++;
		if (ch == '[')
			in_escape = 1;
		else if (ch == ']')
			in_escape = 0;
		else if (ch <= SPACE) {
			if (!in_escape && strchr(",.!?;:", last))
				spk_serial_out(PROCSPEECH);
			if (jiffies >= jiff_max) {
				if (!in_escape)
					spk_serial_out(PROCSPEECH);
				synth_delay(speakup_info.delay_time);
				return;
			}
		}
		last = ch;
	}
	if (synth_done() || !in_escape)
	spk_serial_out(PROCSPEECH);
}

static const char *synth_immediate(const char *buf)
{
	u_char ch;
	while ((ch = *buf)) {
		if (ch == '\n')
			ch = PROCSPEECH;
		if (wait_for_xmitr())
			outb(ch, speakup_info.port_tts);
		else
			return buf;
		buf++;
	}
	return 0;
}

static void synth_flush(void)
{
	if (in_escape) {
		/* if in command output ']' so we don't get an error */
		spk_serial_out(']');
	}
	in_escape = 0;
	spk_serial_out(SYNTH_CLEAR);
	is_flushing = 5; /* if no ctl-a in 4, send data anyway */
}

static int serprobe(int index)
{
	struct serial_state *ser;

	if (! speakup_info.port_forced)
		speakup_info.port_forced = 0x3f8;
	ser = spk_serial_init(index);
	if (ser == NULL)
		return -1;
	outb(0x0d, ser->port);
	return 0;

#if 0
	int test=0;
	int timeout = 1000000;
	/* ignore any error results, if port was forced */
	if (speakup_info.port_forced)
		return 0;

	/* check for dectalk express now... */
	if (!synth_immediate("\x03\x03")) {
		do {
			test = spk_serial_in();
			if (test == 0x01)
				return 0;
		} while (--timeout > 0);
	}
	spk_serial_release();
	timeouts = speakup_info.alive = speakup_info.port_tts = 0;	/* not ignoring */
	return -1;
#endif
}

static int synth_probe(void)
{
	int i;
	int failed = 0;
	pr_info("Probing for %s.\n", MY_SYNTH.long_name);
	/* check ttyS0-ttyS3 */
	for (i = SPK_LO_TTY; i <= SPK_HI_TTY; i++) {
		failed = serprobe(i);
		if (failed == 0)
			break; /* found it */
	}
	if (failed) {
		pr_info("%s: not found\n", MY_SYNTH.long_name);
		return -ENODEV;
	}
	pr_info("%s: %03x-%03x, Driver Version %s,\n", MY_SYNTH.long_name,
		speakup_info.port_tts, speakup_info.port_tts + 7, MY_SYNTH.version);
	return 0;
}

static int
synth_is_alive(void)
{
	if (speakup_info.alive)
		return 1;
	if (!speakup_info.alive && wait_for_xmitr() > 0) {
		/* restart */
		speakup_info.alive = 1;
		synth_printf("%s",MY_SYNTH.init);
		return 2;
	} else
		pr_warn("%s: can't restart synth\n", MY_SYNTH.long_name);
	return 0;
}

module_param_named(start, MY_SYNTH.flags, short, S_IRUGO);

static int __init dectlk_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit dectlk_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(dectlk_init);
module_exit(dectlk_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for DECtalk Express synthesizers");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

