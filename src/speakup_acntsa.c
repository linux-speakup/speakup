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

 * this code is specificly written as a driver for the speakup screenreview
 * package and is not a general device driver.
 */

#include <linux/jiffies.h>

#include "spk_priv.h"
#include "serialio.h"
#include "speakup_acnt.h" /* local header file for Accent values */

#define MY_SYNTH synth_acntsa
#define DRV_VERSION "1.3"
#define synth_full() (inb_p(synth_port_tts) == 'F')
#define PROCSPEECH '\r'

static int synth_probe(void);
static const char *synth_immediate(const char *buf);
static void do_catch_up(unsigned long data);
static void synth_flush(void);
static int synth_is_alive(void);

static int timeouts;	/* sequential number of timeouts */

static const char init_string[] = "\033T2\033=M\033Oi\033N1\n";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "\033P8" },
	{ CAPS_STOP, "\033P5" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "\033R%c", 9, 0, 17, 0, 0, "0123456789abcdefgh" },
	{ PITCH, "\033P%d", 5, 0, 9, 0, 0, 0 },
	{ VOL, "\033A%d", 9, 0, 9, 0, 0, 0 },
	{ TONE, "\033V%d", 5, 0, 9, 0, 0, 0 },
	V_LAST_NUM
};

struct spk_synth synth_acntsa = { "acntsa", DRV_VERSION, "Accent-SA",
	init_string, 400, 5, 30, 1000, 0, 0, SYNTH_CHECK,
	stringvars, numvars, synth_probe, spk_serial_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, NULL, NULL,
	{NULL, 0, 0, 0} };

static int
wait_for_xmitr(void)
{
	int check, tmout = SPK_XMITR_TIMEOUT;
	if ((synth_alive) && (timeouts >= NUM_DISABLE_TIMEOUTS)) {
		synth_alive = 0;
		return 0;
	}
	do {
		/* holding register empty? */
		check = inb_p(synth_port_tts + UART_LSR);
		if (--tmout == 0) {
			pr_warn("%s: timed out\n", MY_SYNTH.long_name);
			timeouts++;
			return 0;
		}
	} while ((check & BOTH_EMPTY) != BOTH_EMPTY);
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

static void
do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+synth_jiffy_delta;
	u_char ch;
	synth_stop_timer();
	while (synth_buff_out < synth_buff_in) {
		ch = *synth_buff_out;
		if (ch == '\n')
			ch = PROCSPEECH;
		if (!spk_serial_out(ch)) {
			synth_delay(synth_full_time);
			return;
		}
		synth_buff_out++;
		if (jiffies >= jiff_max && ch == ' ') {
			spk_serial_out(PROCSPEECH);
			synth_delay(synth_delay_time);
			return;
		}
	}
	spk_serial_out(PROCSPEECH);
	synth_done();
}

static const char *synth_immediate(const char *buff)
{
	u_char ch;
	while ((ch = *buff)) {
		if (ch == 0x0a)
			ch = PROCSPEECH;
		if (wait_for_xmitr())
			outb(ch, synth_port_tts);
		else
			return buff;
		buff++;
	}
	return 0;
}

static void synth_flush(void)
{
	spk_serial_out(SYNTH_CLEAR);
}

static int serprobe(int index)
{
	struct serial_state *ser = spk_serial_init(index);
	if (ser == NULL)
		return -1;
	outb(0x0d, ser->port);
	/*	mdelay(1); */
	/* ignore any error results, if port was forced */
	if (synth_port_forced)
		return 0;
	/* check for accent s.a now... */
	if (!synth_immediate("\x18"))
		return 0;
	spk_serial_release();
	timeouts = synth_alive = 0;	/* not ignoring */
	return -1;
}

static int synth_probe(void)
{
	int i = 0, failed = 0;
	pr_info("Probing for %s.\n", MY_SYNTH.long_name);
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
		synth_port_tts, synth_port_tts + 7, MY_SYNTH.version);
	synth_immediate("\033=R\r");
	mdelay(100);
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
		synth_printf("%s",MY_SYNTH.init);
		return 2;
	}
	pr_warn("%s: can't restart synth\n", MY_SYNTH.long_name);
	return 0;
}

module_param_named(start, MY_SYNTH.flags, short, S_IRUGO);

static int __init acntsa_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit acntsa_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(acntsa_init);
module_exit(acntsa_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Accent SA synthesizer");
MODULE_LICENSE("GPL");

