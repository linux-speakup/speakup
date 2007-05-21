/* spk_priv.h
   review functions for the speakup screen review package.
   originally written by: Kirk Reiser and Andy Berdan.

  extensively modified by David Borowski.

    Copyright (C) 1998  Kirk Reiser.
    Copyright (C) 2003  David Borowski.

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
*/
#ifndef _SPEAKUP_PRIVATE_H
#define _SPEAKUP_PRIVATE_H

#include <linux/version.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/wait.h>		/* for wait_queue */
#include <linux/init.h> /* for __init */
#include <linux/module.h>
#include <linux/vt_kern.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif
#include <asm/io.h>		/* for inb_p, outb_p, inb, outb, etc... */
#include "keyinfo.h"

#define SHIFT_TBL_SIZE 64
/* proc permissions */
#define USER_R (S_IFREG|S_IRUGO)
#define USER_W (S_IFREG|S_IWUGO)
#define USER_RW (S_IFREG|S_IRUGO|S_IWUGO)
#define ROOT_W (S_IFREG|S_IRUGO|S_IWUSR)

#define V_LAST_STRING { -1, 0 }
#define V_LAST_NUM { -1, 0, 0, 0, 0, 0, 0, 0 }
#define TOGGLE_0 0, 0, 0, 1, 0, 0, 0
#define TOGGLE_1 0, 1, 0, 1, 0, 0, 0
#define MAXVARLEN 15
#define SPACE 0x20
#define SYNTH_OK 0x0001
#define B_ALPHA 0x0002
#define ALPHA 0x0003
#define B_CAP 0x0004
#define A_CAP 0x0007
#define B_NUM 0x0008
#define NUM 0x0009
#define ALPHANUM (B_ALPHA|B_NUM)
#define SOME 0x0010
#define MOST 0x0020
#define PUNC 0x0040
#define A_PUNC 0x0041
#define B_WDLM 0x0080
#define WDLM 0x0081
#define B_EXNUM 0x0100
#define CH_RPT 0x0200
#define B_CTL 0x0400
#define A_CTL (B_CTL+SYNTH_OK)
#define B_SYM 0x0800
#define B_CAPSYM (B_CAP|B_SYM)
#define IS_WDLM(x) (spk_chartab[((u_char)x)]&B_WDLM)
#define IS_CHAR(x, type) (spk_chartab[((u_char)x)]&type)
#define IS_TYPE(x, type) ((spk_chartab[((u_char)x)]&type) == type)
#define SET_DEFAULT -4
#define E_RANGE -3
#define E_TOOLONG -2
#define E_UNDEF -1

enum {
	VAR_NUM = 0,
	VAR_TIME,
	VAR_STRING,
	VAR_PROC
};

enum {
	E_DEFAULT = 0,
	E_SET,
	E_INC,
	E_DEC
};

typedef int (*special_func)(struct vc_data *vc, u_char type, u_char ch, u_short key);

struct st_var_header {
	char *name;
	short var_id, var_type, proc_mode;
	void *proc_entry;
	void *p_val; /* ptr to programs variable to store value */
	void *data; /* ptr to the vars data */
};

struct st_num_var {
	short var_id;
	char *synth_fmt;
	short default_val, low, high;
	short offset, multiplier; /* for fiddling rates etc. */
	char *out_str; /* if synth needs char representation of number */
	short value; /* current value */
};

struct st_string_var {
	short var_id;
	char *default_val;
};

struct st_proc_var {
	short var_id;
	int (*read_proc)(char *page, char **start, off_t off, int count,
			 int *eof, void *data);
	int (*write_proc)(struct file *file, const char *buffer, u_long count,
			  void *data);
	short value;
};

struct st_bits_data { /* punc, repeats, word delim bits */
	char *name;
	char *value;
	short mask;
};

extern struct st_proc_var spk_proc_vars[];
char *speakup_s2i(char *, short *);
int speakup_register_var(struct st_num_var *var);
extern struct st_var_header *get_var_header(short var_id);
extern int set_num_var(short val, struct st_var_header *var, int how);

#define COLOR_BUFFER_SIZE 160
struct spk_highlight_color_track{
	unsigned int bgcount[8];	// Count of each background color
	char highbuf[8][COLOR_BUFFER_SIZE];	// Buffer for characters drawn with each background color
	unsigned int highsize[8];	// Current index into highbuf
	u_long rpos[8],rx[8],ry[8];	// Reading Position for each color
	ulong cy;			// Real Cursor Y Position
};

struct st_spk_t {
	u_long reading_x, cursor_x;
	u_long reading_y, cursor_y;
	u_long reading_pos, cursor_pos;
	u_long go_x, go_pos;
	u_long w_top, w_bottom, w_left, w_right;
	u_char w_start, w_enabled;
	u_char reading_attr, old_attr;
	char parked, shut_up;
	struct spk_highlight_color_track ht;
};

/* now some defines to make these easier to use. */
#define spk_shut_up speakup_console[vc->vc_num]->shut_up
#define spk_killed (speakup_console[vc->vc_num]->shut_up & 0x40)
#define spk_x speakup_console[vc->vc_num]->reading_x
#define spk_cx speakup_console[vc->vc_num]->cursor_x
#define spk_y speakup_console[vc->vc_num]->reading_y
#define spk_cy speakup_console[vc->vc_num]->cursor_y
#define spk_pos (speakup_console[vc->vc_num]->reading_pos)
#define spk_cp speakup_console[vc->vc_num]->cursor_pos
#define goto_pos (speakup_console[vc->vc_num]->go_pos)
#define goto_x (speakup_console[vc->vc_num]->go_x)
#define win_top (speakup_console[vc->vc_num]->w_top)
#define win_bottom (speakup_console[vc->vc_num]->w_bottom)
#define win_left (speakup_console[vc->vc_num]->w_left)
#define win_right (speakup_console[vc->vc_num]->w_right)
#define win_start (speakup_console[vc->vc_num]->w_start)
#define win_enabled (speakup_console[vc->vc_num]->w_enabled)
#define spk_attr speakup_console[vc->vc_num]->reading_attr
#define spk_old_attr speakup_console[vc->vc_num]->old_attr
#define spk_parked speakup_console[vc->vc_num]->parked
#define SYNTH_CHECK 20030716 /* today's date ought to do for check value */
/* synth flags, for odd synths */
#define SF_DEC 1 /* to fiddle puncs in alpha strings so it doesn't spell */

struct synth_indexing {
	char *command;
	unsigned char lowindex;
	unsigned char highindex;
	unsigned char currindex;
};

struct spk_synth {
	const char *name;
	const char *version;
	const char *long_name;
	const char *init;
	short delay, trigger, jiffies, full, flush_wait, flags;
	const int checkval; /* for validating a proper synth module */
	struct st_string_var *string_vars;
	struct st_num_var *num_vars;
	int (*probe)(void);
	void (*release)(void);
	const char *(*synth_immediate)(const char *buff);
	void (*catch_up)(u_long data);
	void (*start)(void);
	void (*flush)(void);
	int (*is_alive)(void);
	int (*synth_adjust)(struct st_var_header *var);
	void (*read_buff_add)(u_char);
	unsigned char (*get_index)(void);
	struct synth_indexing indexing;
};

extern struct spk_synth *synth;
int synth_request_region(u_long, u_long);
int synth_release_region(u_long, u_long);
void spk_serial_release(void);
extern int synth_port_tts, synth_port_forced;

/* FIXME: for mainline inclusion, just make the source code use proper names. */
#define declare_timer(name) struct timer_list name;
/* FIXME: couldn't this just be mod_timer? */
#define start_timer(name) if (!timer_pending(&name)) add_timer(&name)
#define stop_timer(name) del_timer(&name)
#define declare_sleeper(name) wait_queue_head_t name
#define init_sleeper(name) 	init_waitqueue_head(&name)
extern declare_sleeper(synth_sleeping_list);

/* Protect speakup machinery */
extern spinlock_t spk_spinlock;
/* Protect speakup synthesizer list */
extern struct mutex spk_mutex;
/* Speakup needs to disable the keyboard IRQ */
#define spk_lock(flags) spin_lock_irqsave(&spk_spinlock, flags)
#define spk_unlock(flags) spin_unlock_irqrestore(&spk_spinlock, flags)

extern char str_caps_start[], str_caps_stop[];
extern short no_intr, say_ctrl, say_word_ctl, punc_level;
extern short reading_punc, attrib_bleep, bleeps;
extern short bleep_time, bell_pos;
extern short spell_delay, key_echo, punc_mask;
extern short synth_jiffy_delta, synth_delay_time;
extern short synth_trigger_time, synth_full_time;
extern short cursor_timeout, pitch_shift, synth_flags;
extern int synth_alive, quiet_boot;
extern u_char synth_buffer[]; /* guess what this is for! */
extern volatile u_char *synth_buff_in, *synth_buff_out;
int synth_init(char *name);
void synth_release(void);
int synth_add(struct spk_synth *in_synth);
void synth_remove(struct spk_synth *in_synth);
struct serial_state * spk_serial_init(int index);
void synth_delay(int ms);
void synth_stop_timer(void);
int synth_done(void);
void do_flush(void);
void synth_buffer_add(char ch);
void synth_write(const char *buf, size_t count);
void synth_putc(char buf);
void synth_printf(const char *buf, ...);
void synth_write_string(const char *buf);
void synth_write_msg(const char *buf);
int synth_supports_indexing(void);
int speakup_dev_init(void);

#ifndef pr_warn
#define pr_warn(fmt,arg...) printk(KERN_WARNING fmt,##arg)
#endif

#endif
