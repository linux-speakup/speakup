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
#include <linux/unistd.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include "spk_priv.h"
#include "serialio.h"

#define MY_SYNTH synth_dectlk
#define DRV_VERSION "1.5"
#define SYNTH_CLEAR 0x03
#define PROCSPEECH 0x0b
#define synth_full() (inb_p(synth_port_tts) == 0x13)

static int timeouts;
static int in_escape, is_flushing;
atomic_t dectest = ATOMIC_INIT(0);

static int wait_for_xmitr(void)
{
	int check, tmout = SPK_XMITR_TIMEOUT;
	if ((synth_alive) && (timeouts >= NUM_DISABLE_TIMEOUTS)) {
		synth_alive = 0;
		timeouts = 0;
		return 0;
	}
	do {
		/* holding register empty? */
		check = inb_p(synth_port_tts + UART_LSR);
		if (--tmout == 0) {
			pr_warn("%s: timed out\n", synth->long_name);
			timeouts++;
			return 0;
		}
	} while ((check & BOTH_EMPTY) != BOTH_EMPTY);
	/*} while ((check & UART_LSR_THRE) != UART_LSR_THRE); */
	tmout = SPK_XMITR_TIMEOUT;
	do {
		/* CTS */
		check = inb_p(synth_port_tts + UART_MSR);
		if (--tmout == 0) {
			timeouts++;
			return 0;
		}
	} while ((check & UART_MSR_CTS) != UART_MSR_CTS);
	timeouts = 0;
	return 1;
}

static int spk_serial_out(const char ch)
{
	if (synth_alive && wait_for_xmitr()) {
		outb_p(ch, synth_port_tts);

		return 1;
	}
	return 0;
}

static u_char spk_serial_in(void)
{
	int c = 0;

	while (inb_p(synth_port_tts + UART_LSR) & UART_LSR_DR) {
		c = inb_p(synth_port_tts+UART_RX);
	}
	return (u_char) c;
}

static int is_indnum(u_char *ch)
{
	if ((*ch > 47) && (*ch < 58)) {
		*ch = *ch - 48;
		return 1;
	}
	return 0;
}

static const char *synth_immediate(const char *);

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
	unsigned long jiff_max = jiffies+synth_jiffy_delta;
	u_char ch;
	static u_char last = '\0';
	synth_stop_timer();
	if (is_flushing) {
		if (--is_flushing == 0)
			pr_warn("flush timeout\n");
		else {
			synth_delay(synth_delay_time);
			return;
		}
	}
	while (synth_buff_out < synth_buff_in) {
		ch = *synth_buff_out;
		if (ch == '\n')
			ch = 0x0D;
		if (synth_full() || !spk_serial_out(ch)) {
			synth_delay(synth_full_time);
			return;
		}
		synth_buff_out++;
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
				synth_delay(synth_delay_time);
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
			outb(ch, synth_port_tts);
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
	struct serial_state *ser = spk_serial_init(index);
	/*u_char test, timeout = 10000; */
	int test=0, timeout = 1000000;
	if (ser == NULL)
		return -1;
	outb(0x0d, ser->port);
	/* ignore any error results, if port was forced */
	if (synth_port_forced)
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
	timeouts = synth_alive = synth_port_tts = 0;	/* not ignoring */
	return -1;
}

static int synth_probe(void)
{
	int i = 0, failed = 0;
	pr_info("Probing for %s.\n", synth->long_name);
	/* check ttyS0-ttyS3 */
	for (i = SPK_LO_TTY; i <= SPK_HI_TTY; i++) {
		failed = serprobe(i);
		if (failed == 0)
			break; /* found it */
	}
	if (failed) {
		pr_info("%s: not found\n", synth->long_name);
		return -ENODEV;
	}
	pr_info("%s: %03x-%03x, Driver Version %s,\n", synth->long_name,
		synth_port_tts, synth_port_tts + 7, synth->version);
	return 0;
}

static int
synth_is_alive(void)
{
	if (synth_alive)
		return 1;
	if (!synth_alive && wait_for_xmitr() > 0) {
		/* restart */
		synth_alive = 1;
		synth_printf("%s",synth->init);
		return 2;
	} else
		pr_warn("%s: can't restart synth\n", synth->long_name);
	return 0;
}

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

