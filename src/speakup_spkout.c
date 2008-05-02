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

#define MY_SYNTH synth_spkout
#define DRV_VERSION "1.8"
#define SYNTH_CLEAR 0x18
#define PROCSPEECH '\r'

static const char *synth_immediate(const char *buf);
static void do_catch_up(unsigned long data);
static void synth_flush(void);
static int synth_is_alive(void);
static unsigned char get_index(void);

static const char init_string[] = "\005W1\005I2\005C3";

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

struct spk_synth synth_spkout = {"spkout", DRV_VERSION, "Speakout",
	 init_string, 500, 50, 50, 5000, 0, 0, SYNTH_CHECK,
	stringvars, numvars, serial_synth_probe, spk_serial_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, NULL,
	get_index, {"\x05[%c", 1, 5, 1} };

static void do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+speakup_info.jiffy_delta;
	u_char ch;
	synth_stop_timer();
	while (speakup_info.buff_out < speakup_info.buff_in) {
		ch = *speakup_info.buff_out;
		if (ch == 0x0a)
			ch = PROCSPEECH;
		if (!spk_serial_out(ch)) {
			synth_delay(speakup_info.full_time);
			return;
		}
		speakup_info.buff_out++;
		if (jiffies >= jiff_max && ch == SPACE) {
			spk_serial_out(PROCSPEECH);
			synth_delay(speakup_info.delay_time);
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
			outb(ch, speakup_info.port_tts);
		else
			return buf;
		buf++;
	}
	return 0;
}

static void synth_flush(void)
{
	while ((inb(speakup_info.port_tts + UART_LSR) & BOTH_EMPTY) != BOTH_EMPTY);
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

static int synth_is_alive(void)
{
	if (speakup_info.alive)
		return 1;
	if (wait_for_xmitr() > 0) {
		/* restart */
		speakup_info.alive = 1;
		synth_printf("%s",MY_SYNTH.init);
		return 2;
	} else
		pr_warn("%s Out: can't restart synth\n", MY_SYNTH.long_name);
	return 0;
}

module_param_named(start, MY_SYNTH.flags, short, S_IRUGO);

static int __init spkout_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit spkout_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(spkout_init);
module_exit(spkout_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Speak Out synthesizers");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

