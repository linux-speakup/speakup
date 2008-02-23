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

#define MY_SYNTH synth_apollo
#define DRV_VERSION "1.3"
#define SYNTH_CLEAR 0x18
#define PROCSPEECH '\r'

static int timeouts;	/* sequential number of timeouts */

static int wait_for_xmitr(void)
{
	int check, tmout = SPK_XMITR_TIMEOUT;
	if ((synth_alive) && (timeouts >= NUM_DISABLE_TIMEOUTS)) {
		synth_alive = 0;
		timeouts = 0;
		return 0;
	}
	do {
		check = inb(synth_port_tts + UART_LSR);
		if (--tmout == 0) {
			pr_warn("APOLLO: timed out\n");
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
 /* int timer = 9000000; */
	if (synth_alive && wait_for_xmitr()) {
		outb(ch, synth_port_tts);
		/*while (inb(synth_port_tts+UART_MSR) & UART_MSR_CTS)
		 *	if (--timer == 0)
		 *		break;*/
		/*	outb(UART_MCR_DTR, synth_port_tts + UART_MCR);*/
		return 1;
	}
	return 0;
}

/*
static unsigned char spk_serial_in(void)
{
	int c, lsr, tmout = SPK_SERIAL_TIMEOUT;
	do {
		lsr = inb(synth_port_tts + UART_LSR);
		if (--tmout == 0)
			return 0xff;
	} while (!(lsr & UART_LSR_DR));
	c = inb(synth_port_tts + UART_RX);
	return (unsigned char) c;
}
*/

static void do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+synth_jiffy_delta;
	u_char ch;
	synth_stop_timer();
	while (synth_buff_out < synth_buff_in) {
		ch = *synth_buff_out;
		if (!spk_serial_out(ch)) {
			outb(UART_MCR_DTR, synth_port_tts + UART_MCR);
			outb(UART_MCR_DTR | UART_MCR_RTS,
					synth_port_tts + UART_MCR);
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
	spk_serial_out(SYNTH_CLEAR);
}

static int serprobe(int index)
{
	struct serial_state *ser = spk_serial_init(index);
	if (ser == NULL)
		return -1;
	outb(0x0d, ser->port); /* wake it up if older BIOS */
	mdelay(1);
	synth_port_tts = ser->port;
	if (synth_port_forced)
		return 0;
	/* check for apollo now... */
	if (!synth_immediate("\x18"))
		return 0;
	pr_warn("port %x failed\n", synth_port_tts);
	spk_serial_release();
	timeouts = synth_alive = synth_port_tts = 0;
	return -1;
}

static int synth_probe(void)
{
	int i, failed = 0;
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
		synth_printf("%s",synth->init);
		return 2; /* reenabled */
	} else
		pr_warn("%s: can't restart synth\n", synth->long_name);
	return 0;
}

static const char init_string[] = "@R3@D0@K1\r";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "cap, " },
	{ CAPS_STOP, "" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "@W%d", 6, 1, 9, 0, 0, 0 },
	{ PITCH, "@F%x", 10, 0, 15, 0, 0, 0 },
	{ VOL, "@A%x", 10, 0, 15, 0, 0, 0 },
	{ VOICE, "@V%d", 1, 1, 6, 0, 0, 0 },
	{ LANG, "@=%d,", 1, 1, 4, 0, 0, 0 },
	V_LAST_NUM
};

struct spk_synth synth_apollo = {"apollo", DRV_VERSION, "Apollo",
	init_string, 500, 50, 50, 5000, 0, 0, SYNTH_CHECK,
	stringvars, numvars, synth_probe, spk_serial_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, NULL, NULL,
	{NULL, 0, 0, 0} };

module_param_named(start, MY_SYNTH.flags, short, S_IRUGO);

static int __init apollo_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit apollo_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(apollo_init);
module_exit(apollo_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Apollo II synthesizer");
MODULE_LICENSE("GPL");

