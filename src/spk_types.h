#ifndef SPEAKUP_TYPES_H
#define SPEAKUP_TYPES_H

/*
 * This file includes all of the typedefs and structs used in speakup.
 */

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
#include <linux/io.h>		/* for inb_p, outb_p, inb, outb, etc... */

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

typedef int (*special_func)(struct vc_data *vc, u_char type, u_char ch,
		u_short key);

#define COLOR_BUFFER_SIZE 160

struct spk_highlight_color_track{
	/* Count of each background color */
	unsigned int bgcount[8];
	/* Buffer for characters drawn with each background color */
	char highbuf[8][COLOR_BUFFER_SIZE];
	/* Current index into highbuf */
	unsigned int highsize[8];
	/* Reading Position for each color */
	u_long rpos[8], rx[8], ry[8];
	/* Real Cursor Y Position */
	ulong cy;
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

struct st_var_header {
	char *name;
	short var_id;
	short var_type;
	short proc_mode;
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

struct st_punc_var {
	short var_id;
	short value;
};

struct st_string_var {
	short var_id;
	char *default_val;
};

struct st_bits_data { /* punc, repeats, word delim bits */
	char *name;
	char *value;
	short mask;
};

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
	char procspeech;
	char clear;
	short delay;
	short trigger;
	short jiffies;
	short full;
	short flush_wait;
	int ser;
	short flags;
	short startup;
	const int checkval; /* for validating a proper synth module */
	struct st_string_var *string_vars;
	struct st_num_var *num_vars;
	int (*probe)(struct spk_synth *synth);
	void (*release)(void);
	const char *(*synth_immediate)(struct spk_synth *synth, const char *buff);
	void (*catch_up)(struct spk_synth *synth, u_long data);
	void (*start)(void);
	void (*flush)(struct spk_synth *synth);
	int (*is_alive)(struct spk_synth *synth);
	int (*synth_adjust)(struct st_var_header *var);
	void (*read_buff_add)(u_char);
	unsigned char (*get_index)(void);
	struct synth_indexing indexing;
};

struct speakup_info_t {
	spinlock_t spinlock;
	int port_tts;
	short delay_time;
	short jiffy_delta;
	short full_time;
	int alive;
	volatile u_char *buff_in;
	volatile u_char *buff_out;
};

/* FIXME: for mainline inclusion, just make the source code use proper names. */
#define declare_timer(name) struct timer_list name;
/* FIXME: couldn't this just be mod_timer? */
#define start_timer(name) if (!timer_pending(&name)) add_timer(&name)
#define stop_timer(name) del_timer(&name)

#define declare_sleeper(name) wait_queue_head_t name
#define init_sleeper(name) 	init_waitqueue_head(&name)

#endif
