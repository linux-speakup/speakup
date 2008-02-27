/*
 * originally written by: Kirk Reiser <kirk@braille.uwo.ca>
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
#include "speakup_dtlk.h" /* local header file for LiteTalk values */

#define MY_SYNTH synth_ltlk
#define DRV_VERSION "1.3"
#define PROCSPEECH 0x0d
#define synth_full() (!(inb(synth_port_tts + UART_MSR) & UART_MSR_CTS))

static int synth_probe(void);
static const char *synth_immediate(const char *buf);
static void do_catch_up(unsigned long data);
static void synth_flush(void);
static int synth_is_alive(void);
static unsigned char get_index(void);

static const char init_string[] = "\01@\x01\x31y\n\0";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "\x01+35p" },
	{ CAPS_STOP, "\x01-35p" },
	V_LAST_STRING
};

static struct st_num_var numvars[] = {
	{ RATE, "\x01%ds", 8, 0, 9, 0, 0, 0 },
	{ PITCH, "\x01%dp", 50, 0, 99, 0, 0, 0 },
	{ VOL, "\x01%dv", 5, 0, 9, 0, 0, 0 },
	{ TONE, "\x01%dx", 1, 0, 2, 0, 0, 0 },
	{ PUNCT, "\x01%db", 7, 0, 15, 0, 0, 0 },
	{ VOICE, "\x01%do", 0, 0, 7, 0, 0, 0 },
	{ FREQ, "\x01%df", 5, 0, 9, 0, 0, 0 },
	V_LAST_NUM
};

struct spk_synth synth_ltlk = { "ltlk", DRV_VERSION, "LiteTalk",
	init_string, 500, 50, 50, 5000, 0, 0, SYNTH_CHECK,
	stringvars, numvars, synth_probe, spk_serial_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, NULL, get_index,
	{"\x01%di", 1, 5, 1} };

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
		/* holding register empty? */
		check = inb(synth_port_tts + UART_LSR);
		if (--tmout == 0) {
			pr_warn("%s: register timed out\n", MY_SYNTH.long_name);
			timeouts++;
			return 0;
		}
	} while ((check & BOTH_EMPTY) != BOTH_EMPTY);
	tmout = SPK_XMITR_TIMEOUT;
	do {
		/* CTS */
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

static unsigned char spk_serial_in(void)
{
	int c, lsr, tmout = SPK_SERIAL_TIMEOUT;
	do {
		lsr = inb(synth_port_tts + UART_LSR);
		if (--tmout == 0) {
			pr_warn("time out while waiting for input.\n");
			return 0xff;
		}
	} while ((lsr & UART_LSR_DR) != UART_LSR_DR);
	c = inb(synth_port_tts + UART_RX);
	return (unsigned char) c;
}

static void do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+synth_jiffy_delta;
	u_char ch;
	synth_stop_timer();
	while (synth_buff_out < synth_buff_in) {
		ch = *synth_buff_out;
		if (ch == 0x0a)
			ch = PROCSPEECH;
		if (!spk_serial_out(ch)) {
			synth_delay(synth_full_time);
			return;
		}
		synth_buff_out++;
		if (jiffies >= jiff_max && ch == SPACE) {
			spk_serial_out(PROCSPEECH);
			synth_delay(synth_delay_time);
			return;
		}
	}
	spk_serial_out(PROCSPEECH);
	synth_done();
}

static const char *synth_immediate(const char *buf)
{
	u_char ch;
	while ((ch = *buf)) {
		if (ch == 0x0a)
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
	spk_serial_out(SYNTH_CLEAR);
}

static unsigned char get_index(void)
{
	int c, lsr;/*, tmout = SPK_SERIAL_TIMEOUT; */
	lsr = inb(synth_port_tts + UART_LSR);
	if ((lsr & UART_LSR_DR) == UART_LSR_DR) {
		c = inb(synth_port_tts + UART_RX);
		return (unsigned char) c;
	}
	return 0;
}

/* interrogate the LiteTalk and print its settings */
static void synth_interrogate(void)
{
	unsigned char *t, i;
	unsigned char buf[50], rom_v[20];
	synth_immediate("\x18\x01?");
	for (i = 0; i < 50; i++) {
		buf[i] = spk_serial_in();
		if (i > 2 && buf[i] == 0x7f)
			break;
	}
	t = buf+2;
	for (i = 0; *t != '\r'; t++) {
		rom_v[i] = *t;
		if (i++ > 48)
			break;
	}
	rom_v[i] = 0;
	pr_info("%s: ROM version: %s\n", MY_SYNTH.long_name, rom_v);
}

static int serprobe(int index)
{
	struct serial_state *ser = spk_serial_init(index);
	if (ser == NULL)
		return -1;
	outb(0, ser->port);
	mdelay(1);
	outb('\r', ser->port);
	/* ignore any error results, if port was forced */
	if (synth_port_forced)
		return 0;
	/* check for device... */
	if (!synth_immediate("\x18"))
		return 0;
	spk_serial_release();
	synth_alive = 0; /* try next port */
	return -1;
}

static int synth_probe(void)
{
	int i, failed = 0;
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
	synth_interrogate();
	pr_info("%s: at %03x-%03x, driver %s\n", MY_SYNTH.long_name,
		synth_port_tts, synth_port_tts + 7, MY_SYNTH.version);
	return 0;
}

static int synth_is_alive(void)
{
	if (synth_alive)
		return 1;
	if (!synth_alive && wait_for_xmitr() > 0) {
		/* restart */
		synth_alive = 1;
		synth_printf("%s",MY_SYNTH.init);
		return 2;
	} else
		pr_warn("%s: can't restart synth\n", MY_SYNTH.long_name);
	return 0;
}

module_param_named(start, MY_SYNTH.flags, short, S_IRUGO);

static int __init ltlk_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit ltlk_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(ltlk_init);
module_exit(ltlk_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for DoubleTalk LT/LiteTalk synthesizers");
MODULE_LICENSE("GPL");

