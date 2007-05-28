/*
 * originially written by: Kirk Reiser <kirk@braille.uwo.ca>
* this version considerably modified by David Borowski, david575@rogers.com

		Copyright (C) 1998-99  Kirk Reiser.
		Copyright (C) 2003 David Borowski.

		This program is free software; you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation; either version 2 of the License, or
		(at your option) any later version.

		This program is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
		GNU General Public License for more details.

		You should have received a copy of the GNU General Public License
		along with this program; if not, write to the Free Software
		Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 * this code is specificly written as a driver for the speakup screenreview
 * package and is not a general device driver.
		*/
#include <linux/jiffies.h>

#include "spk_priv.h"
#include "serialio.h"
#include "spk_synth_dtlk.h" /* local header file for DoubleTalk values */

#define MY_SYNTH synth_dtlk
#define PROCSPEECH 0x00
#define synth_readable() ((synth_status = inb_p(synth_port_tts)) & TTS_READABLE)
#define synth_full() ((synth_status = inb_p(synth_port_tts)) & TTS_ALMOST_FULL)

static int synth_lpc;
static unsigned int synth_portlist[] =
		{ 0x25e, 0x29e, 0x2de, 0x31e, 0x35e, 0x39e, 0 };
static u_char synth_status = 0;

static void spk_out(const char ch)
{
	int tmout = 100000;
	while (((synth_status = inb_p(synth_port_tts)) & TTS_WRITABLE) == 0);
	outb_p(ch, synth_port_tts);
	while ((((synth_status = inb_p(synth_port_tts)) & TTS_WRITABLE) != 0)
		&& (--tmout != 0));
}

static void do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+synth_jiffy_delta;
	u_char ch;
	synth_stop_timer();
	synth_status = inb_p(synth_port_tts);
	while (synth_buff_out < synth_buff_in) {
		if (synth_status & TTS_ALMOST_FULL) {
			synth_delay(synth_full_time);
			return;
		}
		ch = *synth_buff_out++;
		if (ch == '\n') ch = PROCSPEECH;
		spk_out(ch);
		if (jiffies >= jiff_max && ch == SPACE) {
			spk_out(PROCSPEECH);
			synth_delay(synth_delay_time);
			return;
		}
	}
	spk_out(PROCSPEECH);
	synth_done();
}

static const char *synth_immediate(const char *buf)
{
	u_char ch;
	synth_status = inb_p(synth_port_tts);
	while ((ch = (u_char)*buf)) {
		if (synth_status & TTS_ALMOST_FULL)
			return buf;
		if (ch == '\n') ch = PROCSPEECH;
		spk_out(ch);
		buf++;
	}
	return 0;
}

static unsigned char get_index(void)
{
	int c, lsr;//, tmout = SPK_SERIAL_TIMEOUT;
	lsr = inb_p(synth_port_tts + UART_LSR);
	if ((lsr & UART_LSR_DR) == UART_LSR_DR)
	{
		c = inb_p(synth_port_tts + UART_RX);
		return (unsigned char) c;
	}
	return 0;
}

static void synth_flush(void)
{
	outb_p(SYNTH_CLEAR, synth_port_tts);
	while (((synth_status = inb_p(synth_port_tts)) & TTS_WRITABLE) != 0);
}

static char synth_read_tts(void)
{
	u_char ch;
	while (((synth_status = inb_p(synth_port_tts)) & TTS_READABLE) == 0);
	ch = synth_status & 0x7f;
	outb_p(ch, synth_port_tts);
	while ((inb_p(synth_port_tts) & TTS_READABLE) != 0);
	return (char) ch;
}

/* interrogate the DoubleTalk PC and return its settings */
static struct synth_settings * synth_interrogate(void)
{
	u_char *t;
	static char buf[sizeof(struct synth_settings) + 1];
	int total, i;
	static struct synth_settings status;
	synth_immediate("\x18\x01?");
	for (total = 0, i = 0; i < 50; i++) {
		buf[total] = synth_read_tts();
		if (total > 2 && buf[total] == 0x7f) break;
		if (total < sizeof(struct synth_settings)) total++;
	}
	t = buf;
	status.serial_number = t[0] + t[1]*256; /* serial number is little endian */
	t += 2;
	for (i = 0; *t != '\r'; t++) {
		status.rom_version[i] = *t;
		if (i < sizeof(status.rom_version)-1) i++;
	}
	status.rom_version[i] = 0;
	t++;
	status.mode = *t++;
	status.punc_level = *t++;
	status.formant_freq = *t++;
	status.pitch = *t++;
	status.speed = *t++;
	status.volume = *t++;
	status.tone = *t++;
	status.expression = *t++;
	status.ext_dict_loaded = *t++;
	status.ext_dict_status = *t++;
	status.free_ram = *t++;
	status.articulation = *t++;
	status.reverb = *t++;
	status.eob = *t++;
	return &status;
}

static int synth_probe(void)
{
		unsigned int port_val = 0;
	int i = 0;
	struct synth_settings *sp;
	pr_info("Probing for DoubleTalk.\n");
	if (synth_port_forced) {
		synth_port_tts = synth_port_forced;
		pr_info("probe forced to %x by kernel command line\n", synth_port_tts);
		if (synth_request_region(synth_port_tts-1, SYNTH_IO_EXTENT)) {
			pr_warn("sorry, port already reserved\n");
			return -EBUSY;
		}
		port_val = inw(synth_port_tts-1);
		synth_lpc = synth_port_tts-1;
	} else {
		for (i=0; synth_portlist[i]; i++) {
			if (synth_request_region(synth_portlist[i], SYNTH_IO_EXTENT))
				continue;
			port_val = inw(synth_portlist[i]);
			if ((port_val &= 0xfbff) == 0x107f) {
				synth_lpc = synth_portlist[i];
				synth_port_tts = synth_lpc+1;
				break;
			}
			synth_release_region(synth_portlist[i], SYNTH_IO_EXTENT);
		}
	}
	if ((port_val &= 0xfbff) != 0x107f) {
		pr_info("DoubleTalk PC: not found\n");
		return -ENODEV;
	}
	while (inw_p(synth_lpc) != 0x147f); /* wait until it's ready */
	sp = synth_interrogate();
	pr_info("%s: %03x-%03x, ROM ver %s, s/n %u, driver: %s\n",
		synth->long_name, synth_lpc, synth_lpc+SYNTH_IO_EXTENT - 1,
	 sp->rom_version, sp->serial_number, synth->version);
	//	synth_alive = 1;
	return 0;
}

static int synth_is_alive(void)
{
	return 1;	/* I'm *INVINCIBLE* */
}

static void
dtlk_release(void)
{
	if (synth_port_tts)
		synth_release_region(synth_port_tts-1, SYNTH_IO_EXTENT);
	synth_port_tts = 0;
}

static const char init_string[] = "\x01@\x01\x31y";

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

struct spk_synth synth_dtlk = {"dtlk", "1.1", "DoubleTalk PC",
	init_string, 500, 30, 50, 1000, 0, 0, SYNTH_CHECK,
	stringvars, numvars, synth_probe, dtlk_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, NULL, get_index,
	{"\x01%di",1,5,1} };

static int __init dtlk_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit dtlk_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(dtlk_init);
module_exit(dtlk_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for DoubleTalk PC synthesizers");
MODULE_LICENSE("GPL");

