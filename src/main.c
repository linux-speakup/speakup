/* speakup.c
   review functions for the speakup screen review package.
   originally written by: Kirk Reiser and Andy Berdan.

  extensively modified by David Borowski.

    Copyright (C ) 1998  Kirk Reiser.
    Copyright (C ) 2003  David Borowski.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option ) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/vt.h>
#include <linux/tty.h>
#include <linux/mm.h> /* __get_free_page( ) and friends */
#include <linux/vt_kern.h>
#include <linux/ctype.h>
#include <linux/selection.h>
#include <asm/uaccess.h> /* copy_from|to|user( ) and others */
#include <linux/unistd.h>

#include <linux/keyboard.h>	/* for KT_SHIFT */
#include <linux/kbd_kern.h> /* for vc_kbd_* and friends */
#include <linux/vt_kern.h>
#include <linux/input.h>
#include <linux/kmod.h>
#include <linux/spkglue.h>

#include "spk_priv.h"
#include <linux/bootmem.h>	/* for alloc_bootmem */

/* speakup_*_selection */
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/consolemap.h>

#define SPEAKUP_VERSION "3.0.0"
#define MAX_DELAY ( (500 * HZ ) / 1000 )
#define KEY_MAP_VER 119
#define MINECHOCHAR SPACE

MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("Daniel Drake <dsd@gentoo.org>");
MODULE_DESCRIPTION("Speakup console speech");
MODULE_LICENSE("GPL");
MODULE_VERSION(SPEAKUP_VERSION);

/* these are globals from the kernel code */
extern struct kbd_struct * kbd;
extern int fg_console;
extern short punc_masks[];

special_func special_handler = NULL;
special_func help_handler = NULL;

short pitch_shift = 0, synth_flags = 0;
static char buf[256];
short attrib_bleep = 0, bleeps = 0,  bleep_time = 1;
short no_intr = 0, spell_delay = 0;
short key_echo = 0, cursor_timeout = 120, say_word_ctl = 0;
short say_ctrl = 0, bell_pos = 0;
short punc_mask = 0, punc_level = 0, reading_punc = 0;
char str_caps_start[MAXVARLEN+1] = "\0", str_caps_stop[MAXVARLEN+1] = "\0";
static const struct st_bits_data punc_info[] = {
	{ "none", "", 0 },
	{ "some", "/$%&@", SOME },
	{ "most", "$%&#()=+*/@^<>|\\", MOST },
	{ "all", "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", PUNC },
	{ "delimiters", "", B_WDLM },
	{ "repeats", "()", CH_RPT },
	{ "extended numeric", "", B_EXNUM },
	{ "symbols", "", B_SYM },
	{ 0, 0 }
};
static char mark_cut_flag = 0;
char synth_name[10] = "" /* FIXME */;
#define MAX_KEY 160
u_char *our_keys[MAX_KEY], *shift_table;
static u_char key_buf[600];
static const u_char key_defaults[] = {
#include "speakupmap.h"
};

/* Speakup Cursor Track Variables */
static int cursor_track = 1, prev_cursor_track = 1;

/* cursor track modes, must be ordered same as cursor_msgs */
enum {
	CT_Off = 0,
	CT_On,
	CT_Highlight,
	CT_Window,
	CT_Max
};
#define read_all_mode CT_Max

struct tty_struct *tty;
#define key_handler k_handler
typedef void (*k_handler_fn)(struct vc_data *vc, unsigned char value,
                            char up_flag);
extern k_handler_fn key_handler[16];
static k_handler_fn do_shift, do_spec, do_latin, do_cursor;
EXPORT_SYMBOL_GPL(help_handler);
EXPORT_SYMBOL_GPL(special_handler);
EXPORT_SYMBOL_GPL(our_keys);
EXPORT_SYMBOL_GPL(synth_name);

static void spkup_write(const char *in_buf, int count);
static int set_mask_bits(const char *input, const int which, const int how);

static const char str_ctl[] = "control-";
static const char *colors[] = {
	"black", "blue", "green", "cyan", "red", "magenta", "yellow", "white",
	"grey"
};

static char *phonetic[] = {
	"alpha", "beta", "charley", "delta", "echo", "fox", "gamma", "hotel",
	"india", "juleiet", "keelo", "leema", "mike", "november", "oscar",
	"papa",
	"quebec", "romeo", "seeara", "tango", "uniform", "victer", "wiskey",
	"x ray", "yankee", "zooloo"
};

// array of 256 char pointers (one for each character description )
// initialized to default_chars and user selectable via /proc/speakup/characters
static char *characters[256];

static char *default_chars[256] = {
	"null", "^a", "^b", "^c", "^d", "^e", "^f", "^g",
	"^h", "^i", "^j", "^k", "^l", "^m", "^n", "^o",
	"^p", "^q", "^r", "^s", "^t", "^u", "^v", "^w",
	"^x", "^y", "^z", NULL, NULL, NULL, NULL, NULL,
	"space", "bang!", "quote", "number", "dollar", "percent", "and",
	"tick",
	"left paren", "right paren", "star", "plus", "comma", "dash", "dot",
	"slash",
	"zero", "one", "two", "three", "four", "five", "six", "seven",
	"eight", "nine",
	"colon", "semmy", "less", "equals", "greater", "question", "at",
	"eigh", "b", "c", "d", "e", "f", "g",
	"h", "i", "j", "k", "l", "m", "n", "o",
	"p", "q", "r", "s", "t", "u", "v", "w", "x",
	"y", "zehd", "left bracket", "backslash", "right bracket", "caret",
	"line",
	"accent", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, "left brace", "bar", "right brace", "tihlduh",
	"delta", "see cedilla", "u oomlout", "e acute", /* 128 */
	"eigh circumflex", "eigh oomlout", "eigh grave", "eigh ring", /* 132 */
	"see cedilla", "e circumflex", "e oomlout", "e grave", /* 136 */
	"i oomlout", "i circumflex", "i grave", "eigh oomlout", /* 140 */
	"eigh ring", "e acute", "eigh e dipthong", "eigh e dipthong", /* 144 */
	"o circumflex", "o oomlout", "o grave", "u circumflex", /* 148 */
	"u grave", "y oomlout", "o oomlout", "u oomlout", /* 152 */
	"cents", "pounds", "yen", "peseta", /* 156 */
	"florin", "eigh acute", "i acute", "o acute", /* 160 */
	"u acute", "n tilde", "n tilde", "feminine ordinal", /* 164 */
	"masculin ordinal", "inverted question", "reversed not", "not", /* 168 */
	"half", "quarter", "inverted bang", "much less than", /* 172 */
	"much greater than", "dark shading", "medium shading", /* 176 */
	"light shading", "verticle line", "left tee", /* 179 */
	"double left tee", "left double tee", "double top right", /* 182 */
	"top double right", "double left double tee", /* 185 */
	"double vertical line", "double top double right", /* 187 */
	"double bottom double right", "double bottom right", /* 189 */
	"bottom double right", "top right", "left bottom", /* 191 */
	"up tee", "tee down", "tee right", "horizontal line", /* 194 */
	"cross bars", "tee double right", "double tee right", /* 198 */
	"double left double bottom", "double left double top", /* 201 */
	"double up double tee", "double tee double down", /* 203 */
	"double tee double right", "double horizontal line", /* 205 */
	"double cross bars", "up double tee", "double up tee", /* 207 */
	"double tee down", "tee double down", /* 210 */
	"double left bottom", "left double bottom", /* 212 */
	"double left top", "left double top", /* 214 */
	"double vertical cross", "double horizontal cross", /* 216 */
	"bottom right", "left top", "solid square", /* 218 */
	"solid lower half", "solid left half", "solid right half", /* 221 */
	"solid upper half", "alpha", "beta", "gamma", /* 224 */
	"pie", "sigma", "sigma", "mu", /* 228 */
	"tou", "phigh", "thayta", "ohmega", /* 232 */
	"delta", "infinity", "phigh", "epsilaun", /* 236 */
"intersection", "identical to", "plus or minus", "equal grater than", /* 240 */
	"less than equal", "upper integral", "lower integral", /* 244 */
		"divided by", "almost equal", "degrees", /* 247 */
	"centre dot", "bullet", "square root", /* 250 */
	"power", "squared", "black square", "white space" /* 252 */
};

// array of 256 u_short (one for each character)
// initialized to default_chartab and user selectable via /proc/speakup/chartab
static u_short spk_chartab[256];

static u_short default_chartab[256] = {
 B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, /* 0-7 */
 B_CTL, B_CTL, A_CTL, B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, /* 8-15 */
 B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, /*16-23 */
 B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, B_CTL, /* 24-31 */
WDLM, A_PUNC, PUNC, PUNC, PUNC, PUNC, PUNC, A_PUNC, /*  !"#$%&' */
PUNC, PUNC, PUNC, PUNC, A_PUNC, A_PUNC, A_PUNC, PUNC, /* ( )*+, -./ */
NUM, NUM, NUM, NUM, NUM, NUM, NUM, NUM, /* 01234567 */
NUM, NUM, A_PUNC, PUNC, PUNC, PUNC, PUNC, A_PUNC, /* 89:;<=>? */
PUNC, A_CAP, A_CAP, A_CAP, A_CAP, A_CAP, A_CAP, A_CAP, /* @ABCDEFG */
A_CAP, A_CAP, A_CAP, A_CAP, A_CAP, A_CAP, A_CAP, A_CAP, /* HIJKLMNO */
A_CAP, A_CAP, A_CAP, A_CAP, A_CAP, A_CAP, A_CAP, A_CAP, /* PQRSTUVW */
A_CAP, A_CAP, A_CAP, PUNC, PUNC, PUNC, PUNC, PUNC, /* XYZ[\]^_ */
PUNC, ALPHA, ALPHA, ALPHA, ALPHA, ALPHA, ALPHA, ALPHA, /* `abcdefg */
ALPHA, ALPHA, ALPHA, ALPHA, ALPHA, ALPHA, ALPHA, ALPHA, /* hijklmno */
ALPHA, ALPHA, ALPHA, ALPHA, ALPHA, ALPHA, ALPHA, ALPHA, /* pqrstuvw */
ALPHA, ALPHA, ALPHA, PUNC, PUNC, PUNC, PUNC, 0, /* xyz{|}~ */
B_CAPSYM, B_CAPSYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, /* 128-135 */
B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_CAPSYM, /* 136-143 */
B_CAPSYM, B_CAPSYM, B_SYM, B_CAPSYM, B_SYM, B_SYM, B_SYM, B_SYM, /* 144-151 */
B_SYM, B_SYM, B_CAPSYM, B_CAPSYM, B_SYM, B_SYM, B_SYM, B_SYM, /* 152-159 */
B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_CAPSYM, B_SYM, /* 160-167 */
B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, /* 168-175 */
B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, /* 176-183 */
B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, /* 184-191 */
B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, /* 192-199 */
B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, /* 200-207 */
B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, /* 208-215 */
B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, /* 216-223 */
B_SYM, B_SYM, B_SYM, B_CAPSYM, B_SYM, B_CAPSYM, B_SYM, B_SYM, /* 224-231 */
B_SYM, B_CAPSYM, B_CAPSYM, B_CAPSYM, B_SYM, B_SYM, B_SYM, B_SYM, /* 232-239 */
B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, /* 240-247 */
B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM, B_SYM /* 248-255 */
};

static int spk_keydown = 0;
static u_char spk_lastkey = 0, spk_close_press = 0, keymap_flags = 0;
static u_char last_keycode = 0, this_speakup_key = 0;
static u_long last_spk_jiffy = 0;

struct st_spk_t *speakup_console[MAX_NR_CONSOLES];

static int spk_setup(char *str)
{
	int ints[4];
	str = get_options(str, ARRAY_SIZE (ints), ints);
	if (ints[0] > 0 && ints[1] >= 0)
		synth_port_forced = ints[1];
	return 1;
}

static int spk_ser_setup(char *str)
{
	const int lookup[4] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
	int ints[4];
	str = get_options(str, ARRAY_SIZE (ints), ints);
	if (ints[0] > 0 && ints[1] >= 0)
		synth_port_forced = lookup[ints[1]];
	return 1;
}

static int spk_synth_setup(char *str)
{
	size_t len = min_t(size_t, strlen(str), 9);
	memcpy (synth_name, str, len);
	synth_name[len] = '\0';
	return 1;
}

static int spk_quiet_setup(char *str)
{
	if (strchr("1yt", *str) != NULL)
		quiet_boot = 1;
	return 1;
}

// FIXME convert to module params
__setup("speakup_port=", spk_setup);
__setup("speakup_ser=", spk_ser_setup);
__setup("speakup_synth=", spk_synth_setup);
__setup("speakup_quiet=", spk_quiet_setup);

static unsigned char get_attributes(u16 *pos)
{
	return (u_char)(scr_readw(pos) >> 8);
}

static void speakup_date(struct vc_data *vc)
{
	spk_x = spk_cx = vc->vc_x;
	spk_y = spk_cy = vc->vc_y;
	spk_pos = spk_cp = vc->vc_pos;
	spk_old_attr = spk_attr;
	spk_attr = get_attributes((u_short *) spk_pos);
}

char *strlwr(char *s)
{
	char *p;
	for (p = s; *p; p++)
		if (*p >= CAP_A && *p <= CAP_Z)
			*p |= 32;
	return s;
}

static void bleep(u_short val)
{
	static const short vals[] = {
		350, 370, 392, 414, 440, 466, 491, 523, 554, 587, 619, 659
	};
	short freq;
	int time = bleep_time;
	freq = vals[val%12];
	if (val > 11)
		freq *= (1 << (val/12));
	kd_mksound(freq, time);
}

static void speakup_shut_up(struct vc_data *vc)
{
	if (spk_killed)
		return;
	spk_shut_up |= 0x01;
	spk_parked &= 0xfe;
	speakup_date(vc);
	if (synth != NULL)
		do_flush();
}

static void speech_kill(struct vc_data *vc)
{
	char val = synth->is_alive();
	if (val == 0)
		return;

	/* re-enables synth, if disabled */
	if (val == 2 || spk_killed) {	/* dead */
		spk_shut_up &= ~0x40;
		synth_write_msg("Eyem a Lighve!");
	} else {
		synth_write_msg("You killed speak up!");
		spk_shut_up |= 0x40;
	}
}

static void speakup_off(struct vc_data *vc)
{
	if (spk_shut_up & 0x80) {
		spk_shut_up &= 0x7f;
		synth_write_msg("hey. That's better!" );
	} else {
		spk_shut_up |= 0x80;
		synth_write_msg("You turned me off!" );
	}
	speakup_date(vc);
}

static void speakup_parked(struct vc_data *vc)
{
	if (spk_parked & 0x80) {
		spk_parked = 0;
		synth_write_msg ("unparked!");
	} else {
		spk_parked |= 0x80;
		synth_write_msg ("parked!");
	}
}

/* ------ cut and paste ----- */
/* Don't take this from <ctype.h>: 011-015 on the screen aren't spaces */
#undef isspace
#define isspace(c)      ((c) == ' ')
/* Variables for selection control. */
static struct vc_data *spk_sel_cons;      /* defined in selection.c must not be disallocated */
static volatile int sel_start = -1;     /* cleared by clear_selection */
static int sel_end;
static int sel_buffer_lth;
static char *sel_buffer;

static unsigned char sel_pos(int n)
{
	return inverse_translate(spk_sel_cons, screen_glyph(spk_sel_cons, n));
}

static u16 get_char(struct vc_data *vc, u16 *pos)
{
	u16 ch = ' ';
	if (vc && pos) {
		u16 w = scr_readw(pos);
		u16 c = w & 0xff;
  
		if (w & vc->vc_hi_font_mask)
			c |= 0x100;

		ch = w & 0xff00;		
		ch |= inverse_translate(vc, c);
	}
	return ch;
}

static void speakup_clear_selection(void)
{
	sel_start = -1;
}

/* does screen address p correspond to character at LH/RH edge of screen? */
static int atedge(const int p, int size_row)
{
	return (!(p % size_row) || !((p + 2) % size_row));
}

/* constrain v such that v <= u */
static unsigned short limit(const unsigned short v, const unsigned short u)
{
	return (v > u) ? u : v;
}

static unsigned short xs, ys, xe, ye; /* our region points */

static int speakup_set_selection(struct tty_struct *tty)
{
	int new_sel_start, new_sel_end;
	char *bp, *obp;
	int i, ps, pe;
	struct vc_data *vc = vc_cons[fg_console].d;

	xs = limit(xs, vc->vc_cols - 1);
	ys = limit(ys, vc->vc_rows - 1);
	xe = limit(xe, vc->vc_cols - 1);
	ye = limit(ye, vc->vc_rows - 1);
	ps = ys * vc->vc_size_row + (xs << 1);
	pe = ye * vc->vc_size_row + (xe << 1);

	if (ps > pe) {   /* make sel_start <= sel_end */
		int tmp = ps;
		ps = pe;
		pe = tmp;
	}

	if (spk_sel_cons != vc_cons[fg_console].d) {
	 	speakup_clear_selection();
		spk_sel_cons = vc_cons[fg_console].d;
		printk(KERN_WARNING "Selection: mark console not the same as cut\n");
		return -EINVAL;
	}

	new_sel_start = ps;
	new_sel_end = pe;

	/* select to end of line if on trailing space */
	if (new_sel_end > new_sel_start &&
	    !atedge(new_sel_end, vc->vc_size_row) &&
	    isspace(sel_pos(new_sel_end))) {
		for (pe = new_sel_end + 2; ; pe += 2)
			if (!isspace(sel_pos(pe)) ||
			    atedge(pe, vc->vc_size_row))
				break;
		if (isspace(sel_pos(pe)))
			new_sel_end = pe;
	}
	if ((new_sel_start == sel_start) && (new_sel_end == sel_end))
		return 0; /* no action required */

	sel_start = new_sel_start;
	sel_end = new_sel_end;
	/* Allocate a new buffer before freeing the old one ... */
	bp = kmalloc((sel_end-sel_start)/2+1, GFP_ATOMIC);
	if (!bp) {
		printk(KERN_WARNING "selection: kmalloc() failed\n");
		speakup_clear_selection();
		return -ENOMEM;
	}
	if (sel_buffer)
		kfree(sel_buffer);
	sel_buffer = bp;

	obp = bp;
	for (i = sel_start; i <= sel_end; i += 2) {
		*bp = sel_pos(i);
		if (!isspace(*bp++))
			obp = bp;
		if (! ((i + 2) % vc->vc_size_row)) {
			/* strip trailing blanks from line and add newline,
			   unless non-space at end of line. */
			if (obp != bp) {
				bp = obp;
				*bp++ = '\r';
			}
			obp = bp;
		}
	}
	sel_buffer_lth = bp - sel_buffer;
	return 0;
}

static int speakup_paste_selection(struct tty_struct *tty)
{
	struct vc_data *vc = (struct vc_data *) tty->driver_data;
	int pasted = 0, count;
	DECLARE_WAITQUEUE(wait, current);
	add_wait_queue(&vc->paste_wait, &wait);
	while (sel_buffer && sel_buffer_lth > pasted) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (test_bit(TTY_THROTTLED, &tty->flags)) {
			schedule();
			continue;
		}
		count = sel_buffer_lth - pasted;
		count = min_t(int, count, tty->receive_room);
		tty->ldisc.receive_buf(tty, sel_buffer + pasted, 0, count);
		pasted += count;
	}
	remove_wait_queue(&vc->paste_wait, &wait);
	current->state = TASK_RUNNING;
	return 0;
}

static void speakup_cut(struct vc_data *vc)
{
	static const char err_buf[] = "set selection failed";
	int ret;

	if (!mark_cut_flag) {
		mark_cut_flag = 1;
		xs = spk_x;
		ys = spk_y;
		spk_sel_cons = vc;
		synth_write_msg("mark");
		return;
	}
	xe = (u_short) spk_x;
	ye = (u_short) spk_y;
	mark_cut_flag = 0;
	synth_write_msg ("cut");
	
	speakup_clear_selection();
	ret = speakup_set_selection(tty);

	switch (ret) {
	case 0:
		break; /* no error */
	case -EFAULT :
		pr_warn( "%sEFAULT\n", err_buf );
		break;
	case -EINVAL :
		pr_warn( "%sEINVAL\n", err_buf );
		break;
	case -ENOMEM :
		pr_warn( "%sENOMEM\n", err_buf );
		break;
	}
}

static void speakup_paste(struct vc_data *vc)
{
	if (mark_cut_flag) {
		mark_cut_flag = 0;
		synth_write_msg("mark, cleared");
	} else {
		synth_write_msg ("paste");
		speakup_paste_selection(tty);
	}
}

static void say_attributes(struct vc_data *vc )
{
	int fg = spk_attr & 0x0f;
	int bg = spk_attr>>4;
	if (fg > 8) {
		synth_write_string("bright ");
		fg -= 8;
	}
	synth_write_string(colors[fg]);
	if (bg > 7) {
		synth_write_string(" on blinking ");
		bg -= 8;
	} else
		synth_write_string(" on ");
	synth_write_msg(colors[bg]);
}

static char *blank_msg = "blank";
static char *edges[] = { "top, ", "bottom, ", "left, ", "right, ", "" };
enum {
	edge_top = 1,
	edge_bottom,
	edge_left,
	edge_right,
	edge_quiet
};

static void announce_edge(struct vc_data *vc, int msg_id)
{
	if (bleeps & 1)
		bleep(spk_y);
	if (bleeps & 2)
		synth_write_msg(edges[msg_id-1]);
}

static void speak_char( u_char ch)
{
	char *cp = characters[ch];
	if (cp == NULL) {
		pr_info ("speak_char: cp==NULL!\n");
		return;
	}
	synth_buffer_add(SPACE);
	if (IS_CHAR(ch, B_CAP)) {
		pitch_shift++;
		synth_write_string(str_caps_start);
		synth_write_string(cp);
		synth_write_string(str_caps_stop);
	} else {
		if (*cp == '^') {
			synth_write_string(str_ctl);
			cp++;
		}
		synth_write_string(cp);
	}
	synth_buffer_add(SPACE);
}

static void say_char(struct vc_data *vc)
{
	u_short ch;
	spk_old_attr = spk_attr;
	ch = get_char(vc, (u_short *) spk_pos);
	spk_attr = (ch >> 8);
	if (spk_attr != spk_old_attr) {
		if (attrib_bleep & 1)
			bleep(spk_y);
		if (attrib_bleep & 2)
			say_attributes(vc);
	}
	speak_char(ch & 0xff);
}

static void say_phonetic_char(struct vc_data *vc)
{
	u_short ch;
	spk_old_attr = spk_attr;
	ch = get_char(vc, (u_short *) spk_pos);
	spk_attr = ((ch & 0xff00) >> 8);
	if (IS_CHAR(ch, B_ALPHA)) {
		ch &= 0x1f;
		synth_write_msg(phonetic[--ch] );
	} else {
		if (IS_CHAR(ch, B_NUM))
			synth_write_string("number ");
		speak_char(ch);
	}
}

static void say_prev_char(struct vc_data *vc)
{
	spk_parked |= 0x01;
	if (spk_x == 0) {
		announce_edge(vc, edge_left);
		return;
	}
	spk_x--;
	spk_pos -= 2;
	say_char(vc);
}

static void say_next_char(struct vc_data *vc)
{
	spk_parked |= 0x01;
	if (spk_x == vc->vc_cols - 1) {
		announce_edge(vc, edge_right);
		return;
	}
	spk_x++;
	spk_pos += 2;
	say_char(vc);
}

/* get_word - will first check to see if the character under the
   reading cursor is a space and if say_word_ctl is true it will
   return the word space.  If say_word_ctl is not set it will check to
   see if there is a word starting on the next position to the right
   and return that word if it exists.  If it does not exist it will
   move left to the beginning of any previous word on the line or the
   beginning off the line whichever comes first.. */

static u_long get_word(struct vc_data *vc)
{
	u_long cnt = 0, tmpx = spk_x, tmp_pos = spk_pos;
	char ch;
	u_short attr_ch;
	spk_old_attr = spk_attr;
	ch = (char) get_char(vc, (u_short *) tmp_pos);

/* decided to take out the sayword if on a space (mis-information */
	if (say_word_ctl && ch == SPACE) {
		*buf = '\0';
		synth_write_msg("space");
		return 0;
	} else if ((tmpx < vc->vc_cols - 2)
		   && (ch == SPACE || IS_WDLM(ch ))
		   && ((char) get_char (vc, (u_short * ) tmp_pos+1 ) > SPACE)) {
		tmp_pos += 2;
		tmpx++;
	} else
		while (tmpx > 0 ) {
			ch = (char) get_char(vc, (u_short *) tmp_pos - 1);
			if ((ch == SPACE || IS_WDLM(ch))
			    && ((char) get_char(vc, (u_short *) tmp_pos) > SPACE))
				break;
			tmp_pos -= 2;
			tmpx--;
		}
	attr_ch = get_char(vc, (u_short *) tmp_pos);
	spk_attr = attr_ch >> 8;
	buf[cnt++] = attr_ch & 0xff;
	while (tmpx < vc->vc_cols - 1) {
		tmp_pos += 2;
		tmpx++;
		ch = (char) get_char(vc, (u_short *) tmp_pos);
		if ((ch == SPACE) || (IS_WDLM(buf[cnt-1]) && (ch > SPACE)))
			break;
		buf[cnt++] = ch;
	}
	buf[cnt] = '\0';
	return cnt;
}

static void say_word(struct vc_data *vc)
{
	u_long cnt = get_word(vc );
	u_short saved_punc_mask = punc_mask;
	if (cnt == 0)
		return;
	punc_mask = PUNC;
	buf[cnt++] = SPACE;
	spkup_write(buf, cnt);
	punc_mask = saved_punc_mask;
}

static void say_prev_word(struct vc_data *vc)
{
	char ch;
	u_short edge_said = 0, last_state = 0, state = 0;
	spk_parked |= 0x01;
	if (spk_x == 0) {
		if (spk_y == 0) {
			announce_edge(vc, edge_top);
			return;
		}
		spk_y--;
		spk_x = vc->vc_cols;
		edge_said = edge_quiet;
	}
	while (1) {
		if (spk_x == 0) {
			if (spk_y == 0) {
				edge_said = edge_top;
				break;
			}
			if (edge_said != edge_quiet)
				edge_said = edge_left;
			if (state > 0)
				break;
			spk_y--;
			spk_x = vc->vc_cols - 1;
		} else spk_x--;
			spk_pos -= 2;
		ch = (char) get_char(vc, (u_short *) spk_pos);
		if (ch == SPACE)
			state = 0;
		else if (IS_WDLM(ch))
			state = 1;
		else state = 2;
		if (state < last_state) {
			spk_pos += 2;
			spk_x++;
			break;
		}
		last_state = state;
	}
	if (spk_x == 0 && edge_said == edge_quiet)
		edge_said = edge_left;
	if (edge_said > 0 && edge_said < edge_quiet)
		announce_edge(vc, edge_said);
	say_word(vc);
}

static void say_next_word(struct vc_data *vc)
{
	char ch;
	u_short edge_said = 0, last_state = 2, state = 0;
	spk_parked |= 0x01;
	if (spk_x == vc->vc_cols - 1 && spk_y == vc->vc_rows - 1) {
		announce_edge(vc, edge_bottom);
		return;
	}
	while (1) {
		ch = (char) get_char(vc, (u_short *) spk_pos );
		if (ch == SPACE)
			state = 0;
		else if (IS_WDLM(ch))
			state = 1;
		else state = 2;
		if (state > last_state) break;
		if (spk_x >= vc->vc_cols - 1) {
			if (spk_y == vc->vc_rows - 1) {
				edge_said = edge_bottom;
				break;
			}
			state = 0;
			spk_y++;
			spk_x = 0;
			edge_said = edge_right;
		} else spk_x++;
			spk_pos += 2;
		last_state = state;
	}
	if (edge_said > 0)
		announce_edge(vc, edge_said);
	say_word(vc);
}

static void spell_word(struct vc_data *vc)
{
	static char *delay_str[] = { " ", ", ", ". ", ". . ", ". . . " };
	char *cp = buf, *str_cap = str_caps_stop;
	char *cp1, *last_cap = str_caps_stop;
	u_char ch;
	if (!get_word(vc))
		return;
	while ((ch = (u_char) *cp)) {
		if (cp != buf)
			synth_write_string(delay_str[spell_delay]);
		if (IS_CHAR(ch, B_CAP)) {
			str_cap = str_caps_start;
			if (*str_caps_stop)
				pitch_shift++;
			else /* synth has no pitch */
				last_cap = str_caps_stop;
		} else
			str_cap = str_caps_stop;
		if (str_cap != last_cap) {
			synth_write_string(str_cap);
			last_cap = str_cap;
		}
		if (this_speakup_key == SPELL_PHONETIC
		    && (IS_CHAR(ch, B_ALPHA))) {
			ch &= 31;
			cp1 = phonetic[--ch];
		} else {
			cp1 = characters[ch];
			if (*cp1 == '^') {
				synth_write_string(str_ctl);
				cp1++;
			}
		}
		synth_write_string(cp1);
		cp++;
	}
	if (str_cap != str_caps_stop)
		synth_write_string(str_caps_stop);
}

static int get_line(struct vc_data *vc)
{
	u_long tmp = spk_pos - (spk_x * 2);
	int i = 0;
	spk_old_attr = spk_attr;
	spk_attr = get_attributes((u_short *) spk_pos);
	for (i = 0; i < vc->vc_cols; i++) {
		buf[i] = (u_char) get_char(vc, (u_short *) tmp);
		tmp += 2;
	}
	for (--i; i >= 0; i--)
		if (buf[i] != SPACE)
			break;
	return ++i;
}

static void say_line(struct vc_data *vc)
{
	int i = get_line(vc);
	char *cp;
	char num_buf[8];
	u_short saved_punc_mask = punc_mask;
	if (i == 0) {
		synth_write_msg(blank_msg);
		return;
	}
	buf[i++] = '\n';
	if (this_speakup_key == SAY_LINE_INDENT) {
		for (cp = buf; *cp == SPACE; cp++)
			;
		sprintf(num_buf, "%d, ", (cp - buf) + 1);
		synth_write_string(num_buf);
	}
	punc_mask = punc_masks[reading_punc];
	spkup_write(buf, i);
	punc_mask = saved_punc_mask;
}

static void say_prev_line(struct vc_data *vc)
{
	spk_parked |= 0x01;
	if (spk_y == 0) {
		announce_edge(vc, edge_top);
		return;
	}
	spk_y--;
	spk_pos -= vc->vc_size_row;
	say_line(vc);
}

static void say_next_line(struct vc_data *vc)
{
	spk_parked |= 0x01;
	if (spk_y == vc->vc_rows - 1) {
		announce_edge(vc, edge_bottom);
		return;
	}
	spk_y++;
	spk_pos += vc->vc_size_row;
	say_line(vc);
}

static int say_from_to(struct vc_data *vc, u_long from, u_long to,
		       int read_punc)
{
	int i = 0;
	u_short saved_punc_mask = punc_mask;
	spk_old_attr = spk_attr;
	spk_attr = get_attributes((u_short *) from);
	while (from < to) {
		buf[i++] = (char) get_char(vc, (u_short *) from);
		from += 2;
		if (i >= vc->vc_size_row)
			break;
	}
	for (--i; i >= 0; i--)
		if (buf[i] != SPACE)
			break;
	buf[++i] = SPACE;
	buf[++i] = '\0';
	if (i < 1)
		return i;
	if (read_punc)
		punc_mask = punc_info[reading_punc].mask;
	spkup_write(buf, i);
	if (read_punc)
		punc_mask = saved_punc_mask;
	return i - 1;
}

static void say_line_from_to(struct vc_data *vc, u_long from, u_long to,
			     int read_punc)
{
	u_long start = vc->vc_origin + (spk_y * vc->vc_size_row);
	u_long end = start + (to * 2);
	start += from * 2;
	if (say_from_to(vc, start, end, read_punc) <= 0)
		if (cursor_track != read_all_mode)
			synth_write_msg(blank_msg);
}

// Sentence Reading Commands

void synth_insert_next_index(int);

static int currsentence;
static int numsentences[2];
static char *sentbufend[2];
static char *sentmarks[2][10];
static int currbuf=0;
static int bn;
static char sentbuf[2][256];

static int say_sentence_num(int num , int prev)
{
	bn = currbuf;
	currsentence = num + 1;
	if (prev && --bn == -1)
		bn = 1;

	if (num > numsentences[bn])
		return 0;

	spkup_write(sentmarks[bn][num], sentbufend[bn] - sentmarks[bn][num]);
	return 1;
}

static int get_sentence_buf(struct vc_data *vc, int read_punc)
{
	u_long start, end;
	int i, bn;
	currbuf++;
	if (currbuf == 2)
		currbuf = 0;
	bn = currbuf;
	start = vc->vc_origin + ((spk_y) *vc->vc_size_row);
	end = vc->vc_origin+((spk_y) *vc->vc_size_row) + vc->vc_cols * 2;

	numsentences[bn] = 0;
	sentmarks[bn][0] = &sentbuf[bn][0];
	i = 0;
	spk_old_attr = spk_attr;
	spk_attr = get_attributes((u_short *) start);

	while (start < end) {
		sentbuf[bn][i] = (char) get_char(vc, (u_short *) start);
		if (i > 0) {
			if (sentbuf[bn][i] == SPACE && sentbuf[bn][i-1] == '.'
			    && numsentences[bn] < 9) {
				// Sentence Marker
				numsentences[bn]++;
				sentmarks[bn][numsentences[bn]] =
					&sentbuf[bn][i];
			}
		}
		i++;
		start += 2;
		if (i >= vc->vc_size_row)
			break;
	}

	for (--i; i >= 0; i--)
		if (sentbuf[bn][i] != SPACE)
			break;

	if (i < 1)
		return -1;

	sentbuf[bn][++i] = SPACE;
	sentbuf[bn][++i] = '\0';

	sentbufend[bn] = &sentbuf[bn][i];
	return numsentences[bn];
}

static void say_screen_from_to(struct vc_data *vc, u_long from, u_long to)
{
	u_long start = vc->vc_origin, end;
	if (from > 0)
		start += from * vc->vc_size_row;
	if (to > vc->vc_rows)
		to = vc->vc_rows;
	end = vc->vc_origin + (to * vc->vc_size_row);
	for (from = start; from < end; from = to) {
		to = from + vc->vc_size_row;
		say_from_to(vc, from, to, 1);
	}
}

static void say_screen(struct vc_data *vc)
{
	say_screen_from_to(vc, 0, vc->vc_rows);
}

static void speakup_win_say(struct vc_data *vc)
{
	u_long start, end, from, to;
	if (win_start < 2) {
		synth_write_msg("no window");
		return;
	}
	start = vc->vc_origin + (win_top * vc->vc_size_row);
	end = vc->vc_origin + (win_bottom * vc->vc_size_row);
	while (start <= end) {
		from = start + (win_left * 2);
		to = start + (win_right * 2);
		say_from_to(vc, from, to, 1);
		start += vc->vc_size_row;
	}
}

static void top_edge (struct vc_data *vc)
{
	spk_parked |= 0x01;
	spk_pos = vc->vc_origin + 2 * spk_x;
	spk_y = 0;
	say_line(vc);
}

static void bottom_edge(struct vc_data *vc)
{
	spk_parked |= 0x01;
	spk_pos += (vc->vc_rows - spk_y - 1) * vc->vc_size_row;
	spk_y = vc->vc_rows - 1;
	say_line(vc);
}

static void left_edge(struct vc_data *vc)
{
	spk_parked |= 0x01;
	spk_pos -= spk_x * 2;
	spk_x = 0;
	say_char (vc );
}

static void right_edge(struct vc_data *vc)
{
	spk_parked |= 0x01;
	spk_pos += (vc->vc_cols - spk_x - 1) * 2;
	spk_x = vc->vc_cols - 1;
	say_char(vc);
}

static void say_first_char(struct vc_data *vc)
{
	int i, len = get_line(vc);
	u_char ch;
	spk_parked |= 0x01;
	if (len == 0) {
		synth_write_msg(blank_msg);
		return;
	}
	for (i = 0; i < len; i++)
		if (buf[i] != SPACE)
			break;
	ch = buf[i];
	spk_pos -= (spk_x - i) * 2;
	spk_x = i;
	sprintf(buf, "%d, ", ++i);
	synth_write_string(buf);
	speak_char(ch);
}

static void say_last_char(struct vc_data *vc)
{
	int len = get_line(vc);
	u_char ch;
	spk_parked |= 0x01;
	if (len == 0) {
		synth_write_msg(blank_msg);
		return;
	}
	ch = buf[--len];
	spk_pos -= (spk_x - len) * 2;
	spk_x = len;
	sprintf (buf, "%d, ", ++len);
	synth_write_string(buf);
	speak_char(ch);
}

static void say_position(struct vc_data *vc)
{
	sprintf(buf, "line %ld, col %ld, t t y %d\n", spk_y + 1, spk_x + 1,
		vc->vc_num + 1);
	synth_write_string(buf);
}

// Added by brianb
static void say_char_num(struct vc_data *vc)
{
	u_short ch = get_char(vc, (u_short *) spk_pos);
	ch &= 0xff;
	sprintf(buf, "hex %02x, decimal %d", ch, ch);
	synth_write_msg(buf);
}

/* these are stub functions to keep keyboard.c happy. */

static void say_from_top(struct vc_data *vc)
{
	say_screen_from_to(vc, 0, spk_y);
}

static void say_to_bottom(struct vc_data *vc)
{
	say_screen_from_to(vc, spk_y, vc->vc_rows);
}

static void say_from_left(struct vc_data *vc)
{
	say_line_from_to(vc, 0, spk_x, 1);
}

static void say_to_right(struct vc_data *vc)
{
	say_line_from_to(vc, spk_x, vc->vc_cols, 1);
}

/* end of stub functions. */

static void spkup_write(const char *in_buf, int count)
{
	static int rep_count = 0;
	static u_char ch = '\0', old_ch = '\0';
	static u_short char_type = 0, last_type = 0;
	static u_char *exn_ptr = NULL;
	int in_count = count;
	char rpt_buf[32];
	spk_keydown = 0;
	while ( count-- ) {
		if ( cursor_track == read_all_mode ) {
			// Insert Sentence Index
			if (( in_buf == sentmarks[bn][currsentence] ) &&
			   ( currsentence <= numsentences[bn] ))
				synth_insert_next_index(currsentence++);
		}
		ch = (u_char )*in_buf++;
		char_type = spk_chartab[ch];
		if (ch == old_ch && !(char_type&B_NUM ) ) {
			if (++rep_count > 2 ) continue;
		} else {
			if ( (last_type&CH_RPT) && rep_count > 2 ) {
				sprintf (rpt_buf, " times %d . ", ++rep_count );
				synth_write_string (rpt_buf );
			}
			rep_count = 0;
		}
		if ( !( char_type&B_NUM ) )
				exn_ptr = NULL;
		if (ch == spk_lastkey ) {
			rep_count = 0;
			if ( key_echo == 1 && ch >= MINECHOCHAR )
				speak_char( ch );
		} else if ( ( char_type&B_ALPHA ) ) {
			if ( (synth_flags&SF_DEC) && (last_type&PUNC) )
				synth_buffer_add ( SPACE );
			synth_write( &ch, 1 );
		} else if ( ( char_type&B_NUM ) ) {
			rep_count = 0;
			if ( (last_type&B_EXNUM) && synth_buff_in == exn_ptr+1 ) {
				synth_buff_in--;
				synth_buffer_add( old_ch );
				exn_ptr = NULL;
			}
			synth_write( &ch, 1 );
		} else if ( (char_type&punc_mask) ) {
			speak_char( ch );
			char_type &= ~PUNC; /* for dec nospell processing */
		} else if ( ( char_type&SYNTH_OK ) ) {
/* these are usually puncts like . and , which synth needs for expression.
 * suppress multiple to get rid of long pausesand clear repeat count so if
 *someone has repeats on you don't get nothing repeated count */
			if ( ch != old_ch )
				synth_write( &ch, 1 );
			else rep_count = 0;
		} else {
			if ( ( char_type&B_EXNUM ) )
					exn_ptr = (u_char *)synth_buff_in;
/* send space and record position, if next is num overwrite space */
			if ( old_ch != ch ) synth_buffer_add ( SPACE );
			else rep_count = 0;
		}
		old_ch = ch;
		last_type = char_type;
	}
	spk_lastkey = 0;
	if (in_count > 2 && rep_count > 2 ) {
		if ( (last_type&CH_RPT) ) {
			sprintf (rpt_buf, " repeated %d . ", ++rep_count );
			synth_write_string (rpt_buf );
		}
		rep_count = 0;
	}
}

static char *ctl_key_ids[] = {
	"shift", "altgr", "control", "ault", "l shift", "speakup",
"l control", "r control"
};
#define NUM_CTL_LABELS 8

static void read_all_doc(struct vc_data *vc);
static void cursor_stop_timer(void);

static void handle_shift(struct vc_data *vc, u_char value, char up_flag)
{
	(*do_shift)(vc, value, up_flag);
	if (synth == NULL || up_flag || spk_killed)
		return;
	if (cursor_track == read_all_mode) {
		switch (value) {
		case KVAL(K_SHIFT):
			cursor_stop_timer();
			spk_shut_up &= 0xfe;
			do_flush();
			read_all_doc(vc);
			break;
		case KVAL(K_CTRL):
			cursor_stop_timer();
			cursor_track=prev_cursor_track;
			spk_shut_up &= 0xfe;
			do_flush();
			break;
		}
	} else {
		spk_shut_up &= 0xfe;
		do_flush();
	}
	if (say_ctrl && value < NUM_CTL_LABELS)
		synth_write_string(ctl_key_ids[value]);
}

static void handle_latin(struct vc_data *vc, u_char value, char up_flag)
{
	(*do_latin)(vc, value, up_flag);
	if (up_flag) {
		spk_lastkey = spk_keydown = 0;
		return;
	}
	if (synth == NULL || spk_killed)
		return;
	spk_shut_up &= 0xfe;
	spk_lastkey = value;
	spk_keydown++;
	spk_parked &= 0xfe;
	if (key_echo == 2 && value >= MINECHOCHAR)
		speak_char( value );
}

static int set_key_info(const u_char *key_info, u_char *k_buffer)
{
	int i = 0, states, key_data_len;
	const u_char *cp = key_info;
	u_char *cp1 = k_buffer;
	u_char ch, version, num_keys;
	version = *cp++;
	if (version != KEY_MAP_VER)
		return -1;
	num_keys = *cp;
	states = (int) cp[1];
	key_data_len = (states + 1) * (num_keys + 1);
	if (key_data_len + SHIFT_TBL_SIZE + 4 >= sizeof(key_buf))
		return -2;
	memset(k_buffer, 0, SHIFT_TBL_SIZE);
	memset(our_keys, 0, sizeof(our_keys));
	shift_table = k_buffer;
	our_keys[0] = shift_table;
	cp1 += SHIFT_TBL_SIZE;
	memcpy(cp1, cp, key_data_len + 3);
	/* get num_keys, states and data*/
	cp1 += 2; /* now pointing at shift states */
	for (i = 1; i <= states; i++) {
		ch = *cp1++;
		if (ch >= SHIFT_TBL_SIZE)
			return -3;
		shift_table[ch] = i;
	}
	keymap_flags = *cp1++;
	while ((ch = *cp1)) {
		if (ch >= MAX_KEY)
			return -4;
		our_keys[ch] = cp1;
		cp1 += states + 1;
	}
	return 0;
}

static struct st_num_var spk_num_vars[] = { /* bell must be first to set high limit */
	{ BELL_POS, 0, 0, 0, 0, 0, 0, 0 },
	{ SPELL_DELAY, 0, 0, 0, 5, 0, 0, 0 },
	{ ATTRIB_BLEEP, 0, 1, 0, 3, 0, 0, 0 },
	{ BLEEPS, 0, 3, 0, 3, 0, 0, 0 },
	{ BLEEP_TIME, 0, 30, 1, 200, 0, 0, 0 },
	{ PUNC_LEVEL, 0, 1, 0, 4, 0, 0, 0 },
	{ READING_PUNC, 0, 1, 0, 4, 0, 0, 0 },
	{ CURSOR_TIME, 0, 120, 50, 600, 0, 0, 0 },
	{ SAY_CONTROL, TOGGLE_0 },
	{ SAY_WORD_CTL, TOGGLE_0 },
	{ NO_INTERRUPT, TOGGLE_0 },
	{ KEY_ECHO, 0, 1, 0, 2, 0, 0, 0 },
	V_LAST_NUM
};

static char *cursor_msgs[] = { "cursoring off", "cursoring on",
	"highlight tracking", "read windo",
"read all" };

static void toggle_cursoring(struct vc_data *vc)
{
	if (cursor_track == read_all_mode)
		cursor_track = prev_cursor_track;
	if (++cursor_track >= CT_Max)
		cursor_track = 0;
	synth_write_msg(cursor_msgs[cursor_track]);
}

static void reset_default_chars(void)
{
	int i;
	if (default_chars[(int )'a'] == NULL) /* lowers are null first time */
		for (i = (int )'a'; default_chars[i] == NULL; i++)
			default_chars[i] = default_chars[i - 32];
	else /* free any non-default */
		for (i = 0; i < 256; i++) {
			if (characters[i] != default_chars[i])
				kfree(characters[i]);
		}
	memcpy(characters, default_chars, sizeof(default_chars));
}

static void reset_default_chartab(void)
{
	memcpy(spk_chartab, default_chartab, sizeof(default_chartab));
}

static void handle_cursor(struct vc_data *vc, u_char value, char up_flag);
static void handle_spec(struct vc_data *vc, u_char value, char up_flag);
static void cursor_done(u_long data );

static declare_timer(cursor_timer);

static void __init speakup_open(struct vc_data *vc,
				struct st_spk_t *first_console)
{
	int i;
	struct st_num_var *n_var;

	reset_default_chars();
	reset_default_chartab();
	memset(speakup_console, 0, sizeof(speakup_console));
	if (first_console == NULL)
		return;
	speakup_console[vc->vc_num] = first_console;
	speakup_date(vc);
	pr_info("speakup %s: initialized\n", SPEAKUP_VERSION);
	init_timer(&cursor_timer);
	cursor_timer.entry.prev = NULL;
	cursor_timer.function = cursor_done;
	init_sleeper(synth_sleeping_list);
	strlwr(synth_name);
 	spk_num_vars[0].high = vc->vc_cols;
	for (n_var = spk_num_vars; n_var->var_id >= 0; n_var++)
		speakup_register_var(n_var);
	for (i = 1; punc_info[i].mask != 0; i++)
		set_mask_bits(0, i, 2);
	do_latin = key_handler[KT_LATIN];
	key_handler[KT_LATIN] = handle_latin;
	do_spec = key_handler[KT_SPEC];
	key_handler[KT_SPEC] = handle_spec;
	do_cursor = key_handler[KT_CUR];
	key_handler[KT_CUR] = handle_cursor;
	do_shift = key_handler[KT_SHIFT];
	key_handler[KT_SHIFT] = handle_shift;
	set_key_info(key_defaults, key_buf);
	if (quiet_boot) spk_shut_up |= 0x01;
}

#ifdef CONFIG_PROC_FS

// speakup /proc interface code

/* Usage:
cat /proc/speakup/version

cat /proc/speakup/characters > foo
less /proc/speakup/characters
vi /proc/speakup/characters

cat foo > /proc/speakup/characters
cat > /proc/speakup/characters
echo 39 apostrophe > /proc/speakup/characters
echo 87 w > /proc/speakup/characters
echo 119 w > /proc/speakup/characters
echo defaults > /proc/speakup/characters
echo reset > /proc/speakup/characters


cat /proc/speakup/chartab > foo
less /proc/speakup/chartab
vi /proc/speakup/chartab

cat foo > /proc/speakup/chartab
cat > /proc/speakup/chartab
echo 233 ALPHA > /proc/speakup/chartab
echo 46 A_PUNC > /proc/speakup/chartab
echo defaults > /proc/speakup/chartab
echo reset > /proc/speakup/chartab
*/

// keymap handlers

static int keys_read_proc(char *page, char **start, off_t off, int count,
			  int *eof, void *data)
{
	char *cp = page;
	int i, n, num_keys, nstates;
	u_char *cp1 = key_buf + SHIFT_TBL_SIZE, ch;
	num_keys = (int)(*cp1);
	nstates = (int)cp1[1];
	cp += sprintf( cp, "%d, %d, %d,\n", KEY_MAP_VER,  num_keys, nstates );
	cp1 += 2; /* now pointing at shift states */
/* dump num_keys+1 as first row is shift states + flags,
   each subsequent row is key + states */
	for ( n = 0; n <= num_keys; n++ ) {
		for ( i = 0; i <= nstates; i++ ) {
			ch = *cp1++;
			cp += sprintf( cp, "%d,", (int)ch );
			*cp++ = ( i < nstates ) ? SPACE : '\n';
		}
	}
	cp += sprintf( cp, "0, %d\n", KEY_MAP_VER );
	*start = 0;
	*eof = 1;
	return (int)(cp-page);
}

static char *
s2uchar ( char *start, char *dest )
{
	int val = 0;
	while ( *start && *start <= SPACE ) start++;
	while ( *start >= '0' && *start <= '9' ) {
		val *= 10;
		val += ( *start ) - '0';
		start++;
	}
	if ( *start == ',' ) start++;
	*dest = (u_char)val;
	return start;
}

static int keys_write_proc(struct file *file, const char *buffer, u_long count,
			   void *data)
{
	int i, ret = count;
	char *in_buff, *cp;
	u_char *cp1;
	if (count < 1 || count > 1800 )
		return -EINVAL;
	in_buff = ( char * ) __get_free_page ( GFP_KERNEL );
	if ( !in_buff ) return -ENOMEM;
	if (copy_from_user (in_buff, buffer, count ) ) {
		free_page ( ( unsigned long ) in_buff );
		return -EFAULT;
	}
	if (in_buff[count - 1] == '\n' ) count--;
	in_buff[count] = '\0';
	if ( count == 1 && *in_buff == 'd' ) {
		free_page ( ( unsigned long ) in_buff );
		set_key_info( key_defaults, key_buf );
		return ret;
	}
	cp = in_buff;
	cp1 = (u_char *)in_buff;
	for ( i = 0; i < 3; i++ ) {
		cp = s2uchar( cp, cp1 );
		cp1++;
	}
	i = (int)cp1[-2]+1;
	i *= (int)cp1[-1]+1;
	i+= 2; /* 0 and last map ver */
	if ( cp1[-3] != KEY_MAP_VER || cp1[-1] > 10 ||
			i+SHIFT_TBL_SIZE+4 >= sizeof(key_buf ) ) {
pr_warn( "i %d %d %d %d\n", i, (int)cp1[-3], (int)cp1[-2], (int)cp1[-1] );
		free_page ( ( unsigned long ) in_buff );
		return -EINVAL;
	}
	while ( --i >= 0 ) {
		cp = s2uchar( cp, cp1 );
		cp1++;
		if ( !(*cp) ) break;
	}
	if ( i != 0 || cp1[-1] != KEY_MAP_VER || cp1[-2] != 0 ) {
		ret = -EINVAL;
pr_warn( "end %d %d %d %d\n", i, (int)cp1[-3], (int)cp1[-2], (int)cp1[-1] );
	} else {
		if ( set_key_info( in_buff, key_buf ) ) {
			set_key_info( key_defaults, key_buf );
		ret = -EINVAL;
pr_warn( "set key failed\n" );
		}
	}
	free_page ( ( unsigned long ) in_buff );
	return ret;
}

// this is the handler for /proc/speakup/version
static int version_read_proc(char *page, char **start, off_t off, int count,
			     int *eof, void *data)
{
	int len = sprintf (page, "%s\n", SPEAKUP_VERSION );
	if (synth != NULL)
		len += sprintf(page+len, "synth %s version %s\n", synth->name,
			synth->version);
	*start = 0;
	*eof = 1;
	return len;
}

// this is the read handler for /proc/speakup/characters
static int chars_read_proc(char *page, char **start, off_t off, int count,
			   int *eof, void *data)
{
	int i, len = 0;
	off_t begin = 0;
	char *cp;
	for (i = 0; i < 256; i++) {
		cp = (characters[i]) ? characters[i] : "NULL";
		len += sprintf(page + len, "%d\t%s\n", i, cp);
		if (len + begin > off + count)
			break;
		if (len + begin < off) {
			begin += len;
			len = 0;
		}
	}
	if (i >= 256)
		*eof = 1;
	if (off >= len + begin)
		return 0;
	*start = page + (off - begin);
	return ((count < begin + len - off) ? count : begin + len - off);
}

static volatile int chars_timer_active = 0;	// indicates when timer is set
static declare_timer(chars_timer);

static void chars_stop_timer(void)
{
	if (chars_timer_active)
		stop_timer(chars_timer);
}

static int strings, rejects, updates;

static void show_char_results (u_long data)
{
	int len;
	char buf[80];
	chars_stop_timer();
	len = sprintf(buf, " updated %d of %d character descriptions\n",
		       updates, strings);
	if (rejects)
		sprintf(buf + (len - 1), " with %d reject%s\n",
			 rejects, rejects > 1 ? "s" : "");
	printk(buf);
}

// this is the read handler for /proc/speakup/chartab
static int chartab_read_proc(char *page, char **start, off_t off, int count,
			     int *eof, void *data)
{
	int i, len = 0;
	off_t begin = 0;
	char *cp;
	for (i = 0; i < 256; i++) {	  
		cp = "0";
		if (IS_TYPE(i, B_CTL))
			cp = "B_CTL";
		else if (IS_TYPE(i, WDLM))
			cp = "WDLM";
		else if (IS_TYPE(i, A_PUNC))
			cp = "A_PUNC";
		else if (IS_TYPE(i, PUNC))
			cp = "PUNC";
		else if (IS_TYPE(i, NUM))
			cp = "NUM";
		else if (IS_TYPE(i, A_CAP))
			cp = "A_CAP";
		else if (IS_TYPE(i, ALPHA))
			cp = "ALPHA";
		else if (IS_TYPE(i, B_CAPSYM))
			cp = "B_CAPSYM";
		else if (IS_TYPE(i, B_SYM))
			cp = "B_SYM";

		len += sprintf(page + len, "%d\t%s\n", i, cp);
		if (len + begin > off + count)
			break;
		if (len + begin < off) {
			begin += len;
			len = 0;
		}
	}
	if (i >= 256)
		*eof = 1;
	if (off >= len + begin)
		return 0;
	*start = page + (off - begin);
	return ((count < begin + len - off) ? count : begin + len - off);
}

static int chartab_get_value(char *keyword)
{
	int value = 0;
	
	if (!strcmp(keyword, "ALPHA"))
		value = ALPHA;
	else if (!strcmp(keyword, "B_CTL"))
		value = B_CTL;
	else if (!strcmp(keyword, "WDLM"))
		value = WDLM;
	else if (!strcmp(keyword, "A_PUNC"))
		value = A_PUNC;
	else if (!strcmp(keyword, "PUNC"))
		value = PUNC;
	else if (!strcmp(keyword, "NUM"))
		value = NUM;
	else if (!strcmp(keyword, "A_CAP"))
		value = A_CAP;
	else if (!strcmp(keyword, "B_CAPSYM"))
		value = B_CAPSYM;
	else if (!strcmp(keyword, "B_SYM"))
		value = B_SYM;
	return value;
}

/* this is the write handler for /proc/speakup/silent */
static int silent_write_proc(struct file *file, const char *buffer,
			     u_long count, void *data)
{
	struct vc_data *vc = vc_cons[fg_console].d;
	char ch = 0, shut;
	if (count > 0 || count < 3 ) {
		get_user (ch, buffer );
		if ( ch == '\n' ) ch = '0';
	}
	if ( ch < '0' || ch > '7' ) {
		pr_warn ( "silent value not in range (0,7)\n" );
		return count;
	}
	if ( (ch&2) ) {
		shut = 1;
		do_flush( );
	} else shut = 0;
	if ( (ch&4) ) shut |= 0x40;
	if ( (ch&1) )
		spk_shut_up |= shut;
		else spk_shut_up &= ~shut;
	return count;
}

// this is the write handler for /proc/speakup/characters
static int chars_write_proc(struct file *file, const char *buffer,
			    u_long count, void *data)
{
#define max_desc_len 72
	static int cnt = 0, state = 0;
	static char desc[max_desc_len + 1];
	static u_long jiff_last = 0;
	short i = 0, num;
	int len;
	char ch, *cp, *p_new;
	// reset certain vars if enough time has elapsed since last called
	if (jiffies - jiff_last > 10 ) {
		cnt = state = strings = rejects = updates = 0;
	}
	jiff_last = jiffies;
get_more:
	desc[cnt] = '\0';
	state = 0;
	for (; i < count && state < 2; i++ ) {
		get_user (ch, buffer + i );
		if ( ch == '\n' ) {
			desc[cnt] = '\0';
			state = 2;
		} else if (cnt < max_desc_len )
			desc[cnt++] = ch;
	}
	if (state < 2 ) return count;
	cp = desc;
	while ( *cp && (unsigned char)(*cp) <= SPACE ) cp++;
	if ((!cnt ) || strchr ("dDrR", *cp ) ) {
		reset_default_chars ( );
		pr_info( "character descriptions reset to defaults\n" );
		cnt = 0;
		return count;
	}
	cnt = 0;
	if (*cp == '#' ) goto get_more;
	num = -1;
	cp = speakup_s2i(cp, &num );
	while ( *cp && (unsigned char)(*cp) <= SPACE ) cp++;
	if (num < 0 || num > 255 ) {	// not in range
		rejects++;
		strings++;
		goto get_more;
	}
	if (num >= 27 && num <= 31 ) goto get_more;
	if (!strcmp(cp, characters[num] ) ) {
		strings++;
		goto get_more;
	}
	len = strlen(cp );
	if (characters[num] == default_chars[num] )
		p_new = kmalloc(sizeof (char) * len+1, GFP_KERNEL );
	else if ( strlen(characters[num] ) >= len )
		p_new = characters[num];
	else {
		kfree(characters[num] );
		characters[num] = default_chars[num];
		p_new = kmalloc(sizeof (char) * len+1, GFP_KERNEL );
	}
	if (!p_new ) return -ENOMEM;
	strcpy ( p_new, cp );
	characters[num] = p_new;
	updates++;
	strings++;
	if (i < count ) goto get_more;
	chars_stop_timer ( );
	init_timer (&chars_timer );
	chars_timer.function = show_char_results;
	chars_timer.expires = jiffies + 5;
		start_timer (chars_timer );
	chars_timer_active++;
	return count;
}

// this is the write handler for /proc/speakup/chartab
static int chartab_write_proc(struct file *file, const char *buffer,
			      u_long count, void *data)
{
#define max_desc_len 72
	static int cnt = 0, state = 0;
	static char desc[max_desc_len + 1];
	static u_long jiff_last = 0;
	short i = 0, num;
	char ch, *cp;
	int value=0;
	// reset certain vars if enough time has elapsed since last called
	if (jiffies - jiff_last > 10 ) {
		cnt = state = strings = rejects = updates = 0;
	}
	jiff_last = jiffies;
get_more:
	desc[cnt] = '\0';
	state = 0;
	for (; i < count && state < 2; i++ ) {
		get_user (ch, buffer + i );
		if ( ch == '\n' ) {
			desc[cnt] = '\0';
			state = 2;
		} else if (cnt < max_desc_len )
			desc[cnt++] = ch;
	}
	if (state < 2 ) return count;
	cp = desc;
	while ( *cp && (unsigned char)(*cp) <= SPACE ) cp++;
	if ((!cnt ) || strchr ("dDrR", *cp ) ) {
		reset_default_chartab ( );
		pr_info( "character descriptions reset to defaults\n" );
		cnt = 0;
		return count;
	}
	cnt = 0;
	if (*cp == '#' ) goto get_more;
	num = -1;
	cp = speakup_s2i(cp, &num );
	while ( *cp && (unsigned char)(*cp) <= SPACE ) cp++;
	if (num < 0 || num > 255 ) {	// not in range
		rejects++;
		strings++;
		goto get_more;
	}
	/*	if (num >= 27 && num <= 31 ) goto get_more; */

	value = chartab_get_value (cp);
	if (!value) {	// not in range
		rejects++;
		strings++;
		goto get_more;
	}

	if (value==spk_chartab[num]) {
		strings++;
		goto get_more;
	}

	spk_chartab[num] = value;
	updates++;
	strings++;
	if (i < count ) goto get_more;
	chars_stop_timer ( );
	init_timer (&chars_timer );
	chars_timer.function = show_char_results;
	chars_timer.expires = jiffies + 5;
		start_timer (chars_timer );
	chars_timer_active++;
	return count;
}

static int bits_read_proc(char *page, char **start, off_t off, int count,
			  int *eof, void *data)
{
	int i;
	struct st_var_header *p_header = data;
	struct st_proc_var *var = p_header->data;
	const struct st_bits_data *pb = &punc_info[var->value];
	short mask = pb->mask;
	char *cp = page;
	*start = 0;
	*eof = 1;
	for ( i = 33; i < 128; i++ ) {
		if ( !(spk_chartab[i]&mask ) ) continue;
		*cp++ = (char )i;
	}
	*cp++ = '\n';
	return cp-page;
}

/* set_mask_bits sets or clears the punc/delim/repeat bits,
 * if input is null uses the defaults.
 * values for how: 0 clears bits of chars supplied,
 * 1 clears allk, 2 sets bits for chars */

static int set_mask_bits(const char *input, const int which, const int how)
{
	u_char *cp;
	short mask = punc_info[which].mask;
	if ( how&1 ) {
		for ( cp = (u_char * )punc_info[3].value; *cp; cp++ )
			spk_chartab[*cp] &= ~mask;
	}
	cp = (u_char * )input;
	if ( cp == 0 ) cp = punc_info[which].value;
	else {
		for ( ; *cp; cp++ ) {
			if ( *cp < SPACE ) break;
			if ( mask < PUNC ) {
				if ( !(spk_chartab[*cp]&PUNC) ) break;
			} else if ( (spk_chartab[*cp]&B_NUM) ) break;
		}
		if ( *cp ) return -EINVAL;
		cp = (u_char * )input;
	}
	if ( how&2 ) {
		for ( ; *cp; cp++ )
			if ( *cp > SPACE ) spk_chartab[*cp] |= mask;
	} else {
		for ( ; *cp; cp++ )
			if ( *cp > SPACE ) spk_chartab[*cp] &= ~mask;
	}
	return 0;
}

static const struct st_bits_data *pb_edit = NULL;

static int edit_bits (struct vc_data *vc, u_char type, u_char ch, u_short key )
{
	short mask = pb_edit->mask, ch_type = spk_chartab[ch];
	if ( type != KT_LATIN || (ch_type&B_NUM ) || ch < SPACE ) return -1;
	if ( ch == SPACE ) {
		synth_write_msg( "edit done" );
		special_handler = NULL;
		return 1;
	}
	if ( mask < PUNC && !(ch_type&PUNC) ) return -1;
	spk_chartab[ch] ^= mask;
	speak_char( ch );
	synth_write_msg( (spk_chartab[ch]&mask ) ? " on" : " off" );
	return 1;
}

static int bits_write_proc(struct file *file, const char *buffer, u_long count,
			   void *data)
{
	struct st_var_header *p_header = data;
	struct st_proc_var *var = p_header->data;
	int ret = count;
	char punc_buf[100];
	if (count < 1 || count > 99 )
		return -EINVAL;
	if (copy_from_user (punc_buf, buffer, count ) )
		return -EFAULT;
	if (punc_buf[count - 1] == '\n' )
		count--;
	punc_buf[count] = '\0';
	if ( *punc_buf == 'd' || *punc_buf == 'r' )
		count = set_mask_bits( 0, var->value, 3 );
	else
		count = set_mask_bits( punc_buf, var->value, 3 );
	if ( count < 0 ) return count;
	return ret;
}

// this is the read handler for /proc/speakup/synth
static int synth_read_proc(char *page, char **start, off_t off, int count,
			   int *eof, void *data)
{
	int len;
	if ( synth == NULL ) strcpy( synth_name, "none" );
	else strcpy( synth_name, synth->name );
	len = sprintf (page, "%s\n", synth_name );
	*start = 0;
	*eof = 1;
	return len;
}

// this is the write handler for /proc/speakup/synth
static int synth_write_proc(struct file *file, const char *buffer,
			    u_long count, void *data)
{
	int ret = count;
	char new_synth_name[10];
	const char *old_name = ( synth != NULL ) ? synth->name : "none";
	if (count < 2 || count > 9 )
		return -EINVAL;
	if (copy_from_user (new_synth_name, buffer, count ) )
		return -EFAULT;
	if (new_synth_name[count - 1] == '\n' )
		count--;
	new_synth_name[count] = '\0';
	strlwr (new_synth_name );
	if (!strcmp (new_synth_name, old_name ) ) {
		pr_warn ( "%s already in use\n", new_synth_name );
		return ret;
	}
	if ( synth_init( new_synth_name ) == 0 ) return ret;
	pr_warn( "failed to init synth %s\n", new_synth_name );
	return -ENODEV;
}

struct st_proc_var spk_proc_vars[] = {
	 { VERSION, version_read_proc, 0, 0 },
	 { SILENT, 0, silent_write_proc, 0 },
	 { CHARS, chars_read_proc, chars_write_proc, 0 },
	 { SYNTH, synth_read_proc, synth_write_proc, 0 },
	 { KEYMAP, keys_read_proc, keys_write_proc, 0 },
	 { PUNC_SOME, bits_read_proc, bits_write_proc, 1 },
	 { PUNC_MOST, bits_read_proc, bits_write_proc, 2 },
	 { PUNC_ALL, bits_read_proc, 0, 3 },
	 { DELIM, bits_read_proc, bits_write_proc, 4 },
	 { REPEATS, bits_read_proc, bits_write_proc, 5 },
	 { EXNUMBER, bits_read_proc, bits_write_proc, 6 },
	 { CHARTAB, chartab_read_proc, chartab_write_proc, 0 },
	{ -1, 0, 0, 0 }
};

#endif // CONFIG_PROC_FS

void speakup_allocate(struct vc_data *vc)
{
	int vc_num;

	vc_num = vc->vc_num;
	if ( speakup_console[vc_num] == NULL ) {
		speakup_console[vc_num] = kzalloc(sizeof(struct st_spk_t) + 1,
			GFP_KERNEL);
		if (speakup_console[vc_num] == NULL)
			return;
		speakup_date( vc);
	} else if ( !spk_parked ) speakup_date( vc);
}

static u_char is_cursor = 0;
static u_long old_cursor_pos, old_cursor_x, old_cursor_y;
static int cursor_con;
static int cursor_timer_active = 0;

static void cursor_stop_timer(void)
{
  if (!cursor_timer_active ) return;
		stop_timer ( cursor_timer );
	cursor_timer_active = 0;
}

static void reset_highlight_buffers( struct vc_data * );

//extern void kbd_fakekey(unsigned int);
extern struct input_dev *fakekeydev;

static int read_all_key;

void reset_index_count(int);
void get_index_count(int *, int *);
//int synth_supports_indexing(void);
static void start_read_all_timer( struct vc_data *vc, int command );

enum {RA_NOTHING,RA_NEXT_SENT,RA_PREV_LINE,RA_NEXT_LINE,RA_PREV_SENT,RA_DOWN_ARROW,RA_TIMER,RA_FIND_NEXT_SENT,RA_FIND_PREV_SENT};

static void
kbd_fakekey2(struct vc_data *vc,int v,int command)
{
	cursor_stop_timer();
        (*do_cursor)( vc,v,0);
        (*do_cursor)( vc,v,1);
	start_read_all_timer(vc,command);
}

static void
read_all_doc( struct vc_data *vc)
{
	if ( synth == NULL || spk_shut_up || (vc->vc_num != fg_console ) )
		return;
	if (!synth_supports_indexing())
		return;
	if (cursor_track!=read_all_mode)
		prev_cursor_track=cursor_track;
	cursor_track=read_all_mode;
	reset_index_count(0);
	if (get_sentence_buf(vc,0)==-1)
		kbd_fakekey2(vc,0,RA_DOWN_ARROW);
	else {
		say_sentence_num(0,0);
		synth_insert_next_index(0);
		start_read_all_timer(vc,RA_TIMER);
	}
}

static void
stop_read_all( struct vc_data *vc)
{
	cursor_stop_timer( );
	cursor_track=prev_cursor_track;
	spk_shut_up &= 0xfe;
	do_flush();
}

static void
start_read_all_timer( struct vc_data *vc, int command )
{
	cursor_con = vc->vc_num;
	cursor_timer.expires = jiffies + cursor_timeout;
	read_all_key=command;
	start_timer (cursor_timer );
	cursor_timer_active++;
}

static void
handle_cursor_read_all( struct vc_data *vc,int command )
{
	int indcount,sentcount,rv,sn;

	switch (command)
	{
		case RA_NEXT_SENT:
			// Get Current Sentence
			get_index_count(&indcount,&sentcount);
			//printk("%d %d  ",indcount,sentcount);
			reset_index_count(sentcount+1);
			if (indcount==1)
			{
				if (!say_sentence_num(sentcount+1,0))
				{
					kbd_fakekey2(vc,0,RA_FIND_NEXT_SENT);
					return;
				}
				synth_insert_next_index(0);
			}
			else
			{
				sn=0;
				if (!say_sentence_num(sentcount+1,1))
				{
					sn=1;
					reset_index_count(sn);
				}
				else
					synth_insert_next_index(0);
				if (!say_sentence_num(sn,0))
				{
					kbd_fakekey2(vc,0,RA_FIND_NEXT_SENT);
					return;
				}
				synth_insert_next_index(0);
			}
			start_read_all_timer(vc,RA_TIMER);
			break;
		case RA_PREV_SENT:
			break;
		case RA_NEXT_LINE:
			read_all_doc(vc);
			break;
		case RA_PREV_LINE:
			break;
		case RA_DOWN_ARROW:
			if (get_sentence_buf(vc,0)==-1)
			{
				kbd_fakekey2(vc,0,RA_DOWN_ARROW);
			}
			else
			{
				say_sentence_num(0,0);
				synth_insert_next_index(0);
				start_read_all_timer(vc,RA_TIMER);
			}
			break;
		case RA_FIND_NEXT_SENT:
			rv=get_sentence_buf(vc,0);
			if (rv==-1)
			{
				read_all_doc(vc);
			}
			if (rv==0)
			{
				kbd_fakekey2(vc,0,RA_FIND_NEXT_SENT);
			}
			else
			{
				say_sentence_num(1,0);
				synth_insert_next_index(0);
				start_read_all_timer(vc,RA_TIMER);
			}
			break;
		case RA_FIND_PREV_SENT:
			break;
		case RA_TIMER:
			get_index_count(&indcount,&sentcount);
			if (indcount<2)
			{
				kbd_fakekey2(vc,0,RA_DOWN_ARROW);
			}
			else
			{
				start_read_all_timer(vc,RA_TIMER);
			}
			break;
	}
}

static void handle_cursor(struct vc_data *vc, u_char value, char up_flag)
{
	if (cursor_track == read_all_mode)
	{
		spk_parked &= 0xfe;
		if ( synth == NULL || up_flag || spk_shut_up )
			return;
		cursor_stop_timer();
		spk_shut_up &= 0xfe;
		do_flush();
		start_read_all_timer(vc,value+1);
		return;
	}
	(*do_cursor)(vc, value, up_flag);
	spk_parked &= 0xfe;
	if ( synth == NULL || up_flag || spk_shut_up || cursor_track == CT_Off )
	  return;
	spk_shut_up &= 0xfe;
	if ( no_intr ) do_flush( );
/* the key press flushes if !no_inter but we want to flush on cursor
 * moves regardless of no_inter state */
	is_cursor = value+1;
	old_cursor_pos = vc->vc_pos;
	old_cursor_x = vc->vc_x;
	old_cursor_y = vc->vc_y;
	speakup_console[vc->vc_num]->ht.cy=vc->vc_y;
	cursor_con = vc->vc_num;
	cursor_stop_timer( );
	cursor_timer.expires = jiffies + cursor_timeout;
	if ( cursor_track == CT_Highlight)
		reset_highlight_buffers( vc );
	read_all_key=value+1;
	start_timer (cursor_timer );
	cursor_timer_active++;
}

static void
update_color_buffer( struct vc_data *vc , const char *ic , int len )
{
	int i,bi,hi;
	int vc_num=vc->vc_num;
	
	bi = ( (vc->vc_attr & 0x70) >> 4 ) ;
	hi=speakup_console[vc_num]->ht.highsize[bi];
	
	i=0;
	if (speakup_console[vc_num]->ht.highsize[bi]==0)
	{
		speakup_console[vc_num]->ht.rpos[bi]=vc->vc_pos;
		speakup_console[vc_num]->ht.rx[bi]=vc->vc_x;
		speakup_console[vc_num]->ht.ry[bi]=vc->vc_y;
	}
	while (( hi<COLOR_BUFFER_SIZE ) && ( i < len ))
	{
		if (( ic[i]>32 ) && ( ic[i]<127 ))
		{
			speakup_console[vc_num]->ht.highbuf[bi][hi] = ic[i];
			hi++;
		}
		else if (( ic[i] == 32 ) && ( hi != 0 ))
		{
			if (speakup_console[vc_num]->ht.highbuf[bi][hi-1]!=32)
			{
				speakup_console[vc_num]->ht.highbuf[bi][hi] = ic[i];
				hi++;
			}
		}
		i++;
	}
	speakup_console[vc_num]->ht.highsize[bi]=hi;
}

static void
reset_highlight_buffers( struct vc_data *vc )
{
	int i;
	int vc_num=vc->vc_num;
	for ( i=0 ; i<8 ; i++ )
		speakup_console[vc_num]->ht.highsize[i]=0;
}

static int
count_highlight_color(struct vc_data *vc)
{
	int i,bg;
	int cc;
	int vc_num=vc->vc_num;
	u16 ch;
	u16 *start = (u16 *) vc->vc_origin;

	for ( i=0 ; i<8 ; i++ )
		speakup_console[vc_num]->ht.bgcount[i]=0;		

	for ( i=0 ; i<vc->vc_rows; i++ ) {
		u16 *end = start + vc->vc_cols*2;
		u16 *ptr;
		for ( ptr=start ; ptr<end ; ptr++) {
			ch = get_attributes( ptr );
			bg = ( ch & 0x70 ) >> 4;
			speakup_console[vc_num]->ht.bgcount[bg]++;
		}
		start += vc->vc_size_row;
	}

	cc=0;
	for ( i=0 ; i<8 ; i++ )
		if (speakup_console[vc_num]->ht.bgcount[i]>0)
			cc++;		
	return cc;
}

static int
get_highlight_color( struct vc_data *vc )
{
	int i,j;
	unsigned int cptr[8],tmp;
	int vc_num=vc->vc_num;

	for ( i=0 ; i<8 ; i++ )
		cptr[i]=i;

	for ( i=0 ; i<7 ; i++ )
		for ( j=i+1 ; j<8 ; j++ )
			if ( speakup_console[vc_num]->ht.bgcount[cptr[i]] > speakup_console[vc_num]->ht.bgcount[cptr[j]]) {
				tmp=cptr[i];
				cptr[i]=cptr[j];
				cptr[j]=tmp;
			}

	for ( i=0; i<8; i++ )
		if ( speakup_console[vc_num]->ht.bgcount[cptr[i]] != 0)
			if ( speakup_console[vc_num]->ht.highsize[cptr[i]] > 0)
			{
				return cptr[i];
			}
	return -1;
}

static int
speak_highlight( struct vc_data *vc )
{
	int hc,d;
	int vc_num=vc->vc_num;
	if (count_highlight_color( vc )==1)
		return 0;
	hc=get_highlight_color( vc );
	if ( hc != -1 )
	{
		d=vc->vc_y-speakup_console[vc_num]->ht.cy;
		if ((d==1)||(d==-1))
		{
			if (speakup_console[vc_num]->ht.ry[hc]!=vc->vc_y)
				return 0;
		}
		spk_parked |= 0x01;
		do_flush();
		spkup_write (speakup_console[vc_num]->ht.highbuf[hc] , speakup_console[vc_num]->ht.highsize[hc] );
		spk_pos=spk_cp=speakup_console[vc_num]->ht.rpos[hc];		
		spk_x=spk_cx=speakup_console[vc_num]->ht.rx[hc];		
		spk_y=spk_cy=speakup_console[vc_num]->ht.ry[hc];		
		return 1;
	}
	return 0;
}

static void
cursor_done (u_long data )
{
	struct vc_data *vc = vc_cons[cursor_con].d;
	cursor_stop_timer( );
	if (cursor_con != fg_console ) {
		is_cursor = 0;
		return;
	}
	speakup_date (vc );
	if ( win_enabled ) {
		if ( vc->vc_x >= win_left && vc->vc_x <= win_right &&
		vc->vc_y >= win_top && vc->vc_y <= win_bottom ) {
			spk_keydown = is_cursor = 0;
			return;
		}
	}
	if ( cursor_track == read_all_mode ) {
		handle_cursor_read_all(vc,read_all_key);
		return;
	}
	if ( cursor_track == CT_Highlight) {
		if ( speak_highlight( vc )) {
			spk_keydown = is_cursor = 0;
			return;
		}
	}
	if ( cursor_track == CT_Window) {
		speakup_win_say (vc);
	} else if ( is_cursor == 1 || is_cursor == 4 )
		say_line_from_to (vc, 0, vc->vc_cols, 0 );
	else
		say_char ( vc );
	spk_keydown = is_cursor = 0;
}

/* These functions are the interface to speakup from the actual kernel code. */

void
speakup_bs (struct vc_data *vc )
{
	if (!spk_parked )
		speakup_date (vc );
	if ( spk_shut_up || synth == NULL ) return;
	if ( vc->vc_num == fg_console  && spk_keydown ) {
		spk_keydown = 0;
		if (!is_cursor ) say_char (vc );
	}
}

void
speakup_con_write (struct vc_data *vc, const char *str, int len )
{
	if (spk_shut_up || (vc->vc_num != fg_console ) )
		return;
	if (bell_pos && spk_keydown && (vc->vc_x == bell_pos - 1 ) )
		bleep(3 );
	if (synth == NULL) return;
	if ((is_cursor)||(cursor_track == read_all_mode )) {
		if (cursor_track == CT_Highlight )
			update_color_buffer( vc, str, len);
		return;
	}
	if ( win_enabled ) {
		if ( vc->vc_x >= win_left && vc->vc_x <= win_right &&
		vc->vc_y >= win_top && vc->vc_y <= win_bottom ) return;
	}

	spkup_write (str, len );
}

void
speakup_con_update (struct vc_data *vc )
{
	if ( speakup_console[vc->vc_num] == NULL || spk_parked )
		return;
	speakup_date (vc );
}

static void handle_spec(struct vc_data *vc, u_char value, char up_flag)
{
	int on_off = 2;
	char *label;
	static const char *lock_status[] = { " off", " on", "" };
	(*do_spec)(vc, value, up_flag);
	if ( synth == NULL || up_flag || spk_killed ) return;
	spk_shut_up &= 0xfe;
	if ( no_intr ) do_flush( );
	switch (value ) {
		case KVAL( K_CAPS ):
			label = "caps lock";
			on_off =  (vc_kbd_led(kbd , VC_CAPSLOCK ) );
			break;
		case KVAL( K_NUM ):
			label = "num lock";
			on_off = (vc_kbd_led(kbd , VC_NUMLOCK ) );
			break;
		case KVAL( K_HOLD ):
			label = "scroll lock";
			on_off = (vc_kbd_led(kbd , VC_SCROLLOCK ) );
			break;
	default:
		spk_parked &= 0xfe;
		return;
	}
	synth_write_string ( label );
	synth_write_msg ( lock_status[on_off] );
}

static int
inc_dec_var( u_char value )
{
	struct st_var_header *p_header;
	struct st_num_var *var_data;
	char num_buf[32];
	char *cp = num_buf, *pn;
	int var_id = (int)value - VAR_START;
	int how = (var_id&1) ? E_INC : E_DEC;
	var_id = var_id/2+FIRST_SET_VAR;
	p_header = get_var_header( var_id );
	if ( p_header == NULL ) return -1;
	if ( p_header->var_type != VAR_NUM ) return -1;
	var_data = p_header->data;
	if ( set_num_var( 1, p_header, how ) != 0 )
		return -1;
	if ( !spk_close_press ) {
		for ( pn = p_header->name; *pn; pn++ ) {
			if ( *pn == '_' ) *cp = SPACE;
			else *cp++ = *pn;
		}
	}
	sprintf( cp, " %d ", (int)var_data->value );
	synth_write_string( num_buf );
	return 0;
}

static void
speakup_win_set (struct vc_data *vc )
{
	char info[40];
	if ( win_start > 1 ) {
		synth_write_msg( "window already set, clear then reset" );
		return;
	}
	if ( spk_x < win_left || spk_y < win_top ) {
		synth_write_msg( "error end before start" );
		return;
	}
	if ( win_start && spk_x == win_left && spk_y == win_top ) {
		win_left = 0;
		win_right = vc->vc_cols-1;
		win_bottom = spk_y;
		sprintf( info, "window is line %d", (int)win_top+1 );
	} else {
		if ( !win_start ) {
			win_top = spk_y;
			win_left = spk_x;
		} else {
			win_bottom = spk_y;
			win_right = spk_x;
		}
		sprintf( info, "%s at line %d, column %d",
			(win_start) ? "end" : "start",
			(int)spk_y+1, (int)spk_x+1 );
	}
	synth_write_msg( info );
	win_start++;
}

static void
speakup_win_clear (struct vc_data *vc )
{
	win_top = win_bottom = 0;
	win_left = win_right = 0;
	win_start = 0;
	synth_write_msg( "window cleared" );
}

static void
speakup_win_enable (struct vc_data *vc )
{
	if ( win_start < 2 ) {
		synth_write_msg( "no window" );
		return;
	}
	win_enabled ^= 1;
	if ( win_enabled ) synth_write_msg( "window silenced" );
	else synth_write_msg( "window silence disabled" );
}

static void
speakup_bits (struct vc_data *vc )
{
	int val = this_speakup_key - ( FIRST_EDIT_BITS - 1 );
	if ( special_handler != NULL || val < 1 || val > 6 ) {
		synth_write_msg( "error" );
		return;
	}
	pb_edit = &punc_info[val];
	sprintf( buf, "edit  %s, press space when done", pb_edit->name );
	synth_write_msg( buf );
	special_handler = edit_bits;
}

static int handle_goto (struct vc_data *vc, u_char type, u_char ch, u_short key )
{
	static u_char *goto_buf = "\0\0\0\0\0\0";
	static int num = 0;
	short maxlen, go_pos;
	char *cp;
	if ( type == KT_SPKUP && ch == SPEAKUP_GOTO ) goto do_goto;
	if ( type == KT_LATIN && ch == '\n' ) goto do_goto;
	if ( type != 0 ) goto oops;
	if (ch == 8 ) {
		if ( num == 0 ) return -1;
		ch = goto_buf[--num];
		goto_buf[num] = '\0';
		spkup_write( &ch, 1 );
		return 1;
}
	if ( ch < '+' || ch > 'y' ) goto oops;
	goto_buf[num++] = ch;
	goto_buf[num] = '\0';
	spkup_write( &ch, 1 );
	maxlen = ( *goto_buf >= '0' ) ? 3 : 4;
	if ((ch == '+' || ch == '-' ) && num == 1 ) return 1;
	if (ch >= '0' && ch <= '9' && num < maxlen ) return 1;
	if ( num < maxlen-1 || num > maxlen ) goto oops;
	if ( ch < 'x' || ch > 'y' ) {
oops:
		if (!spk_killed )
			synth_write_msg (" goto canceled" );
		goto_buf[num = 0] = '\0';
		special_handler = NULL;
		return 1;
	}
	cp = speakup_s2i (goto_buf, &go_pos );
	goto_pos = (u_long)go_pos;
	if (*cp == 'x' ) {
		if (*goto_buf < '0' ) goto_pos += spk_x;
		else goto_pos--;
		if (goto_pos < 0 ) goto_pos = 0;
		if (goto_pos >= vc->vc_cols )
			goto_pos = vc->vc_cols-1;
		goto_x = 1;
	} else {
		if (*goto_buf < '0' ) goto_pos += spk_y;
		else goto_pos--;
		if (goto_pos < 0 ) goto_pos = 0;
	if (goto_pos >= vc->vc_rows ) goto_pos = vc->vc_rows-1;
		goto_x = 0;
	}
		goto_buf[num = 0] = '\0';
do_goto:
	special_handler = NULL;
	spk_parked |= 0x01;
	if ( goto_x ) {
		spk_pos -= spk_x * 2;
		spk_x = goto_pos;
		spk_pos += goto_pos * 2;
		say_word( vc );
	} else {
		spk_y = goto_pos;
		spk_pos = vc->vc_origin + ( goto_pos * vc->vc_size_row );
		say_line( vc );
	}
	return 1;
}

static void
speakup_goto (struct vc_data *vc )
{
	if ( special_handler != NULL ) {
		synth_write_msg( "error" );
		return;
	}
	synth_write_msg( "go to?" );
	special_handler = handle_goto;
	return;
}

static void
load_help(struct work_struct *work)
{
	request_module( "speakup_keyhelp" );
	if ( help_handler ) {
		(*help_handler)(0, KT_SPKUP, SPEAKUP_HELP, 0 );
	} else synth_write_string( "help module not found" );
}

static DECLARE_WORK(ld_help, load_help);
#define schedule_help schedule_work

static void
speakup_help (struct vc_data *vc )
{
	if ( help_handler == NULL ) {
/* we can't call request_module from this context so schedule it*/
/* **** note kernel hangs and my wrath will be on you */
		schedule_help (&ld_help);
		return;
	}
	(*help_handler)(vc, KT_SPKUP, SPEAKUP_HELP, 0 );
}

static void
do_nothing (struct vc_data *vc )
{
	return; /* flush done in do_spkup */
}
static u_char key_speakup = 0, spk_key_locked = 0;

static void
speakup_lock (struct vc_data *vc )
{
	if ( !spk_key_locked )
		spk_key_locked = key_speakup = 16;
	else spk_key_locked = key_speakup = 0;
}

typedef void (*spkup_hand )(struct vc_data * );
spkup_hand spkup_handler[] = { /* must be ordered same as defines in speakup.h */
	do_nothing, speakup_goto, speech_kill, speakup_shut_up,
	speakup_cut, speakup_paste, say_first_char, say_last_char,
	say_char, say_prev_char, say_next_char,
	say_word, say_prev_word, say_next_word,
	say_line, say_prev_line, say_next_line,
	top_edge, bottom_edge, left_edge, right_edge,
	        spell_word, spell_word, say_screen,
	say_position, say_attributes,
	speakup_off, speakup_parked, say_line, // this is for indent
	say_from_top, say_to_bottom,
	say_from_left, say_to_right,
	say_char_num, speakup_bits, speakup_bits, say_phonetic_char,
	speakup_bits, speakup_bits, speakup_bits,
	speakup_win_set, speakup_win_clear, speakup_win_enable, speakup_win_say,
	speakup_lock, speakup_help, toggle_cursoring, read_all_doc,  NULL
};

static void do_spkup( struct vc_data *vc,u_char value )
{
	if (spk_killed && value != SPEECH_KILL ) return;
	spk_keydown = 0;
	spk_lastkey = 0;
	spk_shut_up &= 0xfe;
	this_speakup_key = value;
	if (value < SPKUP_MAX_FUNC && spkup_handler[value] ) {
		do_flush( );
		(*spkup_handler[value] )(vc );
	} else {
		if ( inc_dec_var( value ) < 0 )
			bleep( 9 );
	}
}

	static const char *pad_chars = "0123456789+-*/\015,.?()";

int
speakup_key (struct vc_data *vc, int shift_state, int keycode, u_short keysym, int up_flag)
{
	int kh;
	u_char *key_info;
	u_char type = KTYP( keysym ), value = KVAL( keysym ), new_key = 0;
	u_char shift_info, offset;
	tty = vc->vc_tty;
	if ( synth == NULL ) return 0;
	if ( type >= 0xf0 ) type -= 0xf0;
	if ( type == KT_PAD && (vc_kbd_led(kbd , VC_NUMLOCK ) ) ) {
		if ( up_flag ) {
			spk_keydown = 0;
			return 0;
		}
		value = spk_lastkey = pad_chars[value];
		spk_keydown++;
		spk_parked &= 0xfe;
		goto no_map;
	}
	if ( keycode >= MAX_KEY ) goto no_map;
	if ( ( key_info = our_keys[keycode] ) == 0 ) goto no_map;
	// Check valid read all mode keys
        if ( (cursor_track==read_all_mode) && ( !up_flag ))
	{
                switch (value)
                {
                        case KVAL(K_DOWN):
                        case KVAL(K_UP):
                        case KVAL(K_LEFT):
                        case KVAL(K_RIGHT):
                        case KVAL(K_PGUP):
                        case KVAL(K_PGDN):
                                break;
                        default:
				stop_read_all(vc);
                                break;
                }
	}
	shift_info = ( shift_state&0x0f ) + key_speakup;
	offset = shift_table[shift_info];
	if ( offset && ( new_key = key_info[offset] ) ) {
		if ( new_key == SPK_KEY ) {
			if ( !spk_key_locked )
				key_speakup = ( up_flag ) ? 0 : 16;
			if ( up_flag || spk_killed ) return 1;
			spk_shut_up &= 0xfe;
			do_flush( );
			return 1;
		}
		if ( up_flag ) return 1;
		if ( last_keycode == keycode && last_spk_jiffy+MAX_DELAY > jiffies ) {
			spk_close_press = 1;
			offset = shift_table[shift_info+32];
/* double press? */
			if ( offset && key_info[offset] )
				new_key = key_info[offset];
		}
		last_keycode = keycode;
		last_spk_jiffy = jiffies;
		type = KT_SPKUP;
		value = new_key;
	}
no_map:
	if ( type == KT_SPKUP && special_handler == NULL ) {
		do_spkup( vc, new_key );
		spk_close_press = 0;
		return 1;
	}
	if ( up_flag || spk_killed || type == KT_SHIFT ) return 0;
	spk_shut_up &= 0xfe;
	kh=(value==KVAL(K_DOWN))||(value==KVAL(K_UP))||(value==KVAL(K_LEFT))||(value==KVAL(K_RIGHT));
	if ((cursor_track != read_all_mode) || !kh)
		if (!no_intr ) do_flush( );
	if ( special_handler ) {
		int status;
		if ( type == KT_SPEC && value == 1 ) {
			value = '\n';
			type = KT_LATIN;
		} else if ( type == KT_LETTER ) type = KT_LATIN;
		else if ( value == 0x7f ) value = 8; /* make del = backspace */
		status = (*special_handler)(vc, type, value, keycode );
		spk_close_press = 0;
		if ( status < 0 ) bleep( 9 );
		return status;
	}
	last_keycode = 0;
	return 0;
}

extern void speakup_remove(void);

static const struct spkglue_funcs glue_funcs = {
	.allocate = speakup_allocate,
	.key = speakup_key,
	.bs = speakup_bs,
	.con_write = speakup_con_write,
	.con_update = speakup_con_update,
};

static void __exit speakup_exit(void)
{
	int i;

	key_handler[KT_LATIN] = do_latin;
	key_handler[KT_SPEC] = do_spec;
	key_handler[KT_CUR] = do_cursor;
	key_handler[KT_SHIFT] = do_shift;
	spkglue_unregister();
	synth_release();
	speakup_remove();
	for(i = 0; i < 256; i++) {
		if (characters[i] != default_chars[i])
			kfree(characters[i]);
	}
	for(i = 0; speakup_console[i]; i++) {
	  kfree(speakup_console[i]);
	  speakup_console[i] = NULL;
	}
}

static int __init speakup_init(void)
{
	int i;
	struct st_spk_t *first_console = kzalloc(sizeof(struct st_spk_t) + 1,
		GFP_KERNEL);
	speakup_open( vc_cons[fg_console].d, first_console );
for ( i = 0; vc_cons[i].d; i++)
  speakup_allocate(vc_cons[i].d);
	spkglue_register("speakup v" SPEAKUP_VERSION, &glue_funcs);
	speakup_dev_init();
	return 0;
}

module_init(speakup_init);
module_exit(speakup_exit);

