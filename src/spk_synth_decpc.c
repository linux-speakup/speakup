/*
 * This is the DECtalk PC speakup driver
 *
 * Some constants from DEC's DOS driver:
 *      Copyright (c) by Digital Equipment Corp.
 *
 * 386BSD DECtalk PC driver:
 *      Copyright (c) 1996 Brian Buhrow <buhrow@lothlorien.nfbcal.org>
 *
 * Linux DECtalk PC driver:
 *      Copyright (c) 1997 Nicolas Pitre <nico@cam.org>
 *
 * speakup DECtalk PC driver:
 *      Copyright (c) 2003 David Borowski <david575@golden.net>
 *
 * All rights reserved.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "spk_priv.h"

#define	MODULE_init		0x0dec		/* module in boot code */
#define	MODULE_self_test	0x8800		/* module in self-test */
#define	MODULE_reset		0xffff		/* reinit the whole module */

#define	MODE_mask		0xf000		/* mode bits in high nibble */
#define	MODE_null		0x0000
#define	MODE_test		0x2000		/* in testing mode */
#define	MODE_status		0x8000
#define	STAT_int		0x0001		/* running in interrupt mode */
#define	STAT_tr_char	0x0002		/* character data to transmit */
#define	STAT_rr_char	0x0004		/* ready to receive char data */
#define	STAT_cmd_ready	0x0008		/* ready to accept commands */
#define	STAT_dma_ready	0x0010		/* dma command ready */
#define	STAT_digitized	0x0020		/* spc in digitized mode */
#define	STAT_new_index	0x0040		/* new last index ready */
#define	STAT_new_status	0x0080		/* new status posted */
#define	STAT_dma_state	0x0100		/* dma state toggle */
#define	STAT_index_valid	0x0200		/* indexs are valid */
#define	STAT_flushing	0x0400		/* flush in progress */
#define	STAT_self_test	0x0800		/* module in self test */
#define	MODE_ready		0xc000		/* module ready for next phase */
#define	READY_boot		0x0000
#define	READY_kernel	0x0001
#define	MODE_error		0xf000

#define	CMD_mask			0xf000		/* mask for command nibble */
#define	CMD_null			0x0000		/* post status */
#define	CMD_control		0x1000		/* hard control command */
#define	CTRL_mask		0x0F00	/*   mask off control nibble */
#define	CTRL_data		0x00FF	/*   madk to get data byte */
#define	CTRL_null		0x0000	/*   null control */
#define	CTRL_vol_up		0x0100	/*   increase volume */
#define	CTRL_vol_down		0x0200	/*   decrease volume */
#define	CTRL_vol_set		0x0300	/*   set volume */
#define	CTRL_pause		0x0400	/*   pause spc */
#define	CTRL_resume		0x0500	/*   resume spc clock */
#define	CTRL_resume_spc		0x0001	/*   resume spc soft pause */
#define	CTRL_flush		0x0600	/*   flush all buffers */
#define	CTRL_int_enable	0x0700	/*   enable status change ints */
#define	CTRL_buff_free		0x0800	/*   buffer remain count */
#define	CTRL_buff_used		0x0900	/*   buffer in use */
#define	CTRL_speech		0x0a00	/*   immediate speech change */
#define		CTRL_SP_voice		0x0001	/*      voice change */
#define		CTRL_SP_rate		0x0002	/*      rate change */
#define		CTRL_SP_comma		0x0003	/*      comma pause change */
#define		CTRL_SP_period		0x0004	/*      period pause change */
#define		CTRL_SP_rate_delta	0x0005	/*	  delta rate change */
#define		CTRL_SP_get_param	0x0006	/*      return the desired parameter */
#define	CTRL_last_index	0x0b00	/*   get last index spoken */
#define	CTRL_io_priority	0x0c00	/*   change i/o priority */
#define	CTRL_free_mem		0x0d00	/*   get free paragraphs on module */
#define	CTRL_get_lang		0x0e00	/*   return bit mask of loaded languages */
#define	CMD_test			0x2000		/* self-test request */
#define	TEST_mask		0x0F00	/* isolate test field */
#define	TEST_null		0x0000	/* no test requested */
#define	TEST_isa_int		0x0100	/* assert isa irq */
#define	TEST_echo		0x0200	/* make data in == data out */
#define	TEST_seg			0x0300	/* set peek/poke segment */
#define	TEST_off			0x0400	/* set peek/poke offset */
#define	TEST_peek		0x0500	/* data out == *peek */
#define	TEST_poke		0x0600	/* *peek == data in */
#define	TEST_sub_code		0x00FF	/* user defined test sub codes */
#define	CMD_id			0x3000		/* return software id */
#define	ID_null			0x0000	/* null id */
#define	ID_kernel		0x0100	/* kernel code executing */
#define	ID_boot			0x0200	/* boot code executing */
#define	CMD_dma			0x4000		/* force a dma start */
#define	CMD_reset		0x5000		/* reset module status */
#define	CMD_sync			0x6000		/* kernel sync command */
#define	CMD_char_in		0x7000		/* single character send */
#define	CMD_char_out		0x8000		/* single character get */
#define	CHAR_count_1		0x0100	/*    one char in cmd_low */
#define	CHAR_count_2		0x0200	/*	the second in data_low */
#define	CHAR_count_3		0x0300	/*	the third in data_high */
#define	CMD_spc_mode		0x9000		/* change spc mode */
#define	CMD_spc_to_text	0x0100	/*   set to text mode */
#define	CMD_spc_to_digit	0x0200	/*   set to digital mode */
#define	CMD_spc_rate		0x0400	/*   change spc data rate */
#define	CMD_error		0xf000		/* severe error */

enum {	PRIMARY_DIC	= 0, USER_DIC, COMMAND_DIC, ABBREV_DIC };

#define	DMA_single_in		0x01
#define	DMA_single_out		0x02
#define	DMA_buff_in		0x03
#define	DMA_buff_out		0x04
#define	DMA_control		0x05
#define	DT_MEM_ALLOC		0x03
#define	DT_SET_DIC		0x04
#define	DT_START_TASK		0x05
#define	DT_LOAD_MEM		0x06
#define	DT_READ_MEM		0x07
#define	DT_DIGITAL_IN		0x08
#define	DMA_sync		0x06
#define	DMA_sync_char		0x07

#define MY_SYNTH synth_dec_pc
#define PROCSPEECH 0x0b
#define SYNTH_IO_EXTENT 8

static int synth_portlist[] = { 0x340, 0x350, 0x240, 0x250, 0 };
static int in_escape = 0, is_flushing = 0;
static int dt_stat, dma_state = 0;

static int dt_getstatus(void)
{
	dt_stat = inb_p(synth_port_tts)|(inb_p(synth_port_tts+1)<<8);
	return dt_stat;
}

static void dt_sendcmd(u_int cmd)
{
	outb_p(cmd & 0xFF, synth_port_tts);
	outb_p((cmd>>8) & 0xFF, synth_port_tts+1);
}

static int dt_waitbit(int bit)
{
	int timeout = 100;
	while (--timeout > 0) {
		if((dt_getstatus() & bit) == bit) return 1;
		udelay(50);
	}
	return 0;
}

static int dt_wait_dma(void)
{
	int timeout = 100, state = dma_state;
	if(! dt_waitbit(STAT_dma_ready)) return 0;
	while (--timeout > 0) {
		if((dt_getstatus()&STAT_dma_state) == state) return 1;
		udelay(50);
	}
	dma_state = dt_getstatus() & STAT_dma_state;
	return 1;
}

static int dt_ctrl(u_int cmd)
{
	int timeout = 10;
	if (!dt_waitbit(STAT_cmd_ready)) return -1;
	outb_p(0, synth_port_tts+2);
	outb_p(0, synth_port_tts+3);
	dt_getstatus();
	dt_sendcmd(CMD_control|cmd);
	outb_p(0, synth_port_tts+6);
	while (dt_getstatus() & STAT_cmd_ready) {
		udelay(20);
		if (--timeout == 0) break;
	}
	dt_sendcmd(CMD_null);
	return 0;
}

static void synth_flush(void)
{
	int timeout = 10;
	if (is_flushing) return;
	is_flushing = 4;
	in_escape = 0;
	while (dt_ctrl(CTRL_flush)) {
		if (--timeout == 0) break;
udelay(50);
	}
	for (timeout = 0; timeout < 10; timeout++) {
		if (dt_waitbit(STAT_dma_ready)) break;
udelay(50);
	}
	outb_p(DMA_sync, synth_port_tts+4);
	outb_p(0, synth_port_tts+4);
	udelay(100);
	for (timeout = 0; timeout < 10; timeout++) {
		if (!(dt_getstatus() & STAT_flushing)) break;
udelay(50);
	}
	dma_state = dt_getstatus() & STAT_dma_state;
	dma_state ^= STAT_dma_state;
	is_flushing = 0;
}

static int dt_sendchar(char ch)
{
	if (!dt_wait_dma()) return -1;
	if (!(dt_stat & STAT_rr_char)) return -2;
	outb_p(DMA_single_in, synth_port_tts+4);
	outb_p(ch, synth_port_tts+4);
	dma_state ^= STAT_dma_state;
	return 0;
}

static int testkernel(void)
{
	int status = 0;
	if (dt_getstatus() == 0xffff) {
		status = -1;
		goto oops;
	}
	dt_sendcmd(CMD_sync);
	if (!dt_waitbit(STAT_cmd_ready)) status = -2;
	else if (dt_stat&0x8000) {
		return 0;
	} else if (dt_stat == 0x0dec)
		pr_warn("dec_pc at 0x%x, software not loaded\n", synth_port_tts);
	status = -3;
oops:	synth_release_region(synth_port_tts, SYNTH_IO_EXTENT);
	synth_port_tts = 0;
	return status;
}

static void do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+synth_jiffy_delta;
	u_char ch;
	static u_char last='\0';
	synth_stop_timer();
	while (synth_buff_out < synth_buff_in) {
		ch = *synth_buff_out;
		if (ch == '\n') ch = 0x0D;
		if (dt_sendchar(ch)) {
			synth_delay(synth_full_time);
			return;
		}
		synth_buff_out++;
		if (ch == '[') in_escape = 1;
		else if (ch == ']') in_escape = 0;
		else if (ch <= SPACE) {
			if (!in_escape && strchr(",.!?;:", last))
				dt_sendchar(PROCSPEECH);
			if (jiffies >= jiff_max) {
				if (!in_escape)
					dt_sendchar(PROCSPEECH);
				synth_delay(synth_delay_time);
				return;
			}
		}
		last = ch;
	}
	if (synth_done() || !in_escape)
	dt_sendchar(PROCSPEECH);
}

static const char *synth_immediate(const char *buf)
{
	u_char ch;
	while ((ch = *buf)) {
		if (ch == '\n') ch = PROCSPEECH;
		if (dt_sendchar(ch))
			return buf;
		buf++;
	}
	return 0;
}

static int synth_probe(void)
{
	int i=0, failed=0;
	pr_info("Probing for %s.\n", synth->long_name);
	for (i=0; synth_portlist[i]; i++) {
		if (synth_request_region(synth_portlist[i], SYNTH_IO_EXTENT)) {
			pr_warn("request_region: failed with 0x%x, %d\n",
				synth_portlist[i], SYNTH_IO_EXTENT);
			continue;
		}
		synth_port_tts = synth_portlist[i];
		if ((failed = testkernel()) == 0) break;
	}
	if (failed) {
		pr_info("%s: not found\n", synth->long_name);
		return -ENODEV;
	}
	pr_info("%s: %03x-%03x, Driver Version %s,\n", synth->long_name,
		synth_port_tts, synth_port_tts + 7, synth->version);
	return 0;
}

static void dtpc_release(void)
{
	if (synth_port_tts)
		synth_release_region(synth_port_tts, SYNTH_IO_EXTENT);
	synth_port_tts = 0;
}

static int synth_is_alive(void)
{
	synth_alive = 1;
	return 1;
}

static const char init_string[] = "[:pe -380]";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "[:dv ap 200]" },
	{ CAPS_STOP, "[:dv ap 100]" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "[:ra %d]", 9, 0, 18, 150, 25, 0 },
	{ PITCH, "[:dv ap %d]", 80, 0, 100, 20, 0, 0 },
	{ VOL, "[:vo se %d]", 5, 0, 9, 5, 10, 0 },
	{ PUNCT, "[:pu %c]", 0, 0, 2, 0, 0, "nsa" },
	{ VOICE, "[:n%c]", 0, 0, 9, 0, 0, "phfdburwkv" },
	V_LAST_NUM
};

struct spk_synth synth_dec_pc = { "decpc", "1.1", "Dectalk PC",
	init_string, 500, 50, 50, 1000, 0, SF_DEC, SYNTH_CHECK,
	stringvars, numvars, synth_probe, dtpc_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, NULL, NULL,
	{NULL,0,0,0} };

static int __init decpc_init(void)
{
	int status = do_synth_init(&MY_SYNTH);
	if (status != 0)
		return status;
	synth_add(&MY_SYNTH);
	return 0;
}

static void __exit decpc_exit(void)
{
	if (synth == &MY_SYNTH)
		synth_release();
	synth_remove(&MY_SYNTH);
}

module_init(decpc_init);
module_exit(decpc_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for DECtalk PC synthesizers");
MODULE_LICENSE("GPL");

