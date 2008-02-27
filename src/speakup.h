#ifndef _SPEAKUP_H
#define _SPEAKUP_H

#include "spk_types.h"

#define SHIFT_TBL_SIZE 64

/* proc permissions */
#define USER_R (S_IFREG|S_IRUGO)
#define USER_W (S_IFREG|S_IWUGO)
#define USER_RW (S_IFREG|S_IRUGO|S_IWUGO)
#define ROOT_W (S_IFREG|S_IRUGO|S_IWUSR)

#define TOGGLE_0 0, 0, 0, 1, 0, 0, 0
#define TOGGLE_1 0, 1, 0, 1, 0, 0, 0
#define MAXVARLEN 15

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

extern char *speakup_s2i(char *, short *);
extern int speakup_register_var(struct st_num_var *var);
extern struct st_var_header *get_var_header(short var_id);
extern int set_num_var(short val, struct st_var_header *var, int how);
extern special_func special_handler;
extern int handle_help(struct vc_data *vc, u_char type, u_char ch, u_short key);
extern int synth_init(char *name);
extern void synth_release(void);

extern void do_flush(void);
extern void synth_buffer_add(char ch);
extern void synth_write(const char *buf, size_t count);
extern int synth_supports_indexing(void);
extern int speakup_dev_init(char *);

extern declare_sleeper(synth_sleeping_list);

/* Protect speakup synthesizer list */
extern struct mutex spk_mutex;
extern struct spk_synth *synth;
extern struct st_proc_var spk_proc_vars[];
extern u_char *our_keys[];
extern char str_caps_start[], str_caps_stop[];
extern short no_intr, say_ctrl, say_word_ctl, punc_level;
extern short reading_punc, attrib_bleep, bleeps;
extern short bleep_time, bell_pos;
extern short spell_delay, key_echo, punc_mask;
extern short synth_trigger_time;
extern short cursor_timeout, pitch_shift, synth_flags;
extern int quiet_boot;

#endif
