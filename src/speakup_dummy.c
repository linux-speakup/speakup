/*
 * originially written by: Kirk Reiser <kirk@braille.uwo.ca>
* this version considerably modified by David Borowski, david575@rogers.com
* eventually modified by Samuel Thibault <samuel.thibault@ens-lyon.org>

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
#include "serialio.h"

#define MY_SYNTH synth_dummy

static int wait_for_xmitr(void)
{
	static int timeouts = 0;	/* sequential number of timeouts */
	int check, tmout = SPK_XMITR_TIMEOUT;
	if ((synth_alive) && (timeouts >= NUM_DISABLE_TIMEOUTS)) {
		synth_alive = 0;
		timeouts = 0;
		return 0;
	}
	do {
		check = inb(synth_port_tts + UART_LSR);
		if (--tmout == 0) {
			pr_warn("DUMMY: timed out\n");
			timeouts++;
			return 0;
		}
	} while ((check & BOTH_EMPTY) != BOTH_EMPTY);
	tmout = SPK_XMITR_TIMEOUT;
	do {
		check = inb(synth_port_tts + UART_MSR);
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
		outb(ch, synth_port_tts);
		return 1;
	}
	return 0;
}

static void do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+synth_jiffy_delta;
	u_char ch;
	synth_stop_timer();
	while (synth_buff_out < synth_buff_in) {
		ch = *synth_buff_out;
		if (!spk_serial_out(ch)) {
			synth_delay(synth_full_time);
			return;
		}
		synth_buff_out++;
		if (jiffies >= jiff_max && ch == ' ') {
			spk_serial_out(' ');
			synth_delay(synth_delay_time);
			return;
		}
	}
	spk_serial_out('\n');
	synth_done();
}

static const char *synth_immediate(const char *buf)
{
	u_char ch;
	while ((ch = *buf)) {
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
}

static int serprobe(int index)
{
	struct serial_state *ser = spk_serial_init(index);
	if (ser == NULL)
		return -1;
	outb('\r', ser->port);
	if (synth_port_forced)
		return 0;
	/* check for serial console now... */
	if (!synth_immediate("probing\n"))
		return 0;
	spk_serial_release();
	synth_alive = 0;
	return -1;
}

static int synth_probe(void)
{
	int i = 0, failed = 0;
	pr_info("Probing for %s.\n", synth->long_name);
	for (i = SPK_LO_TTY; i <= SPK_HI_TTY; i++) {
		failed = serprobe(i);
		if (failed == 0)
			break; /* found it */
	}
	if (failed) {
		pr_info("%s: not found\n", synth->long_name);
		return -ENODEV;
	}
	pr_info("%s: %03x-%03x, Driver version %s,\n", synth->long_name,
		synth_port_tts, synth_port_tts + 7, synth->version);
	return 0;
}

static int synth_is_alive(void)
{
	if (synth_alive)
		return 1;
	if (!synth_alive && wait_for_xmitr() > 0) {
		/* restart */
		synth_alive = 1;
		synth_write_string(synth->init);
		return 2;
	}
	pr_warn("%s: can't restart synth\n", synth->long_name);
	return 0;
}

static const char init_string[] = "\x05Z\x05\x43";

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

struct spk_synth synth_dummy = {"dummy", "1.1", "Dummy",
	init_string, 500, 50, 50, 5000, 0, 0, SYNTH_CHECK,
	stringvars, numvars, synth_probe, spk_serial_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, NULL, NULL,
	{NULL, 0, 0, 0} };

static int __init dummy_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit dummy_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(dummy_init);
module_exit(dummy_exit);
MODULE_AUTHOR("Samuel Thibault <samuel.thibault@ens-lyon.org>");
MODULE_DESCRIPTION("Speakup support for text console");
MODULE_LICENSE("GPL");

