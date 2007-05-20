/* speakup_keyhelp.c
   help module for speakup

  written by David Borowski.

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

#include <linux/version.h>
#include <linux/keyboard.h>
#include "spk_priv.h"

extern u_char *our_keys[];
extern special_func special_handler;
extern special_func help_handler;
#define MAXFUNCS 130
#define MAXKEYS 256
static u_short key_offsets[MAXFUNCS], key_buf[MAXKEYS];
static u_short masks[] = { 32, 16, 8, 4, 2, 1 };
static char help_info[] =
	"press space to leav help, cursor up or down to scroll, or a letter to go to commands in list";
static char *statenames[] = {
	" double", " speakup", " alt",
	" ctrl", " altgr", " shift"
};
static char *keynames[] = {
	"escape", "1", "2", "3", "4",
	"5", "6", "7", "8", "9",
	"0", "minus", "equal", "back space", "tab",
	"q", "w", "e", "r", "t",
	"y", "u", "i", "o", "p",
	"left brace", "right brace", "enter", "left control", "a",
	"s", "d", "f", "g", "h",
	"j", "k", "l", "semicolon", "apostrophe",
	"accent", "left shift", "back slash", "z", "x",
	"c", "v", "b", "n", "m",
	"comma", "dot", "slash", "right shift", "keypad asterisk",
	"left alt", "space", "caps lock", "f1", "f2",
	"f3", "f4", "f5", "f6", "f7",
	"f8", "f9", "f10", "num lock", "scroll lock",
	"keypad 7", "keypad 8", "keypad 9", "keypad minus", "keypad 4",
	"keypad 5", "keypad 6", "keypad plus", "keypad 1", "keypad 2",
	"keypad 3", "keypad 0", "keypad dot", "103rd", "f13",
	"102nd", "f11", "f12", "f14", "f15",
	"f16", "f17", "f18", "f19", "f20",
	"keypad enter", "right control", "keypad slash", "sysrq", "right alt",
	"line feed", "home", "up", "page up", "left",
	"right", "end", "down", "page down", "insert",
	"delete", "macro", "mute", "volume down", "volume up",
	"power", "keypad equal", "keypad plusminus", "pause", "f21",
	"f22", "f23", "f24", "keypad comma", "left meta",
	"right meta", "compose", "stop", "again", "props",
	"undo", "front", "copy", "open", "paste",
	"find", "cut", "help", "menu", "calc",
	"setup", "sleep", "wakeup", "file", "send file",
	"delete file", "transfer", "prog1", "prog2", "www",
	"msdos", "coffee", "direction", "cycle windows", "mail",
	"bookmarks", "computer", "back", "forward", "close cd",
	"eject cd", "eject close cd", "next song", "play pause", "previous song",
	"stop cd", "record", "rewind", "phone", "iso",
	"config", "home page", "refresh", "exit", "move",
	"edit", "scroll up", "scroll down", "keypad left paren", "keypad right paren",
};

static short letter_offsets[26];

static u_char funcvals[] = {
	ATTRIB_BLEEP_DEC, ATTRIB_BLEEP_INC, BLEEPS_DEC, BLEEPS_INC,
	SAY_FIRST_CHAR, SAY_LAST_CHAR, SAY_CHAR, SAY_CHAR_NUM,
	SAY_NEXT_CHAR, SAY_PHONETIC_CHAR, SAY_PREV_CHAR, SPEAKUP_PARKED,
	SPEAKUP_CUT, EDIT_DELIM, EDIT_EXNUM, EDIT_MOST,
	EDIT_REPEAT, EDIT_SOME, SPEAKUP_GOTO, BOTTOM_EDGE,
	LEFT_EDGE, RIGHT_EDGE, TOP_EDGE, SPEAKUP_HELP,
	SAY_LINE, SAY_NEXT_LINE, SAY_PREV_LINE, SAY_LINE_INDENT,
	SPEAKUP_PASTE, PITCH_DEC, PITCH_INC, PUNCT_DEC,
	PUNCT_INC, PUNC_LEVEL_DEC, PUNC_LEVEL_INC, SPEAKUP_QUIET,
	RATE_DEC, RATE_INC, READING_PUNC_DEC, READING_PUNC_INC,
	SAY_ATTRIBUTES, SAY_FROM_LEFT, SAY_FROM_TOP, SAY_POSITION,
	SAY_SCREEN, SAY_TO_BOTTOM, SAY_TO_RIGHT, SPK_KEY,
	SPK_LOCK, SPEAKUP_OFF, SPEECH_KILL, SPELL_DELAY_DEC,
	SPELL_DELAY_INC, SPELL_WORD, SPELL_PHONETIC, TONE_DEC,
	TONE_INC, VOICE_DEC, VOICE_INC, VOL_DEC,
	VOL_INC, CLEAR_WIN, SAY_WIN, SET_WIN,
	ENABLE_WIN, SAY_WORD, SAY_NEXT_WORD, SAY_PREV_WORD, 0
};

static char *funcnames[] = {
	"attribute bleep decrement", "attribute bleep increment",
	"bleeps decrement", "bleeps increment",
	"character, first", "character, last",
	"character, say current",
	"character, say hex and decimal", "character, say next",
	"character, say phonetic", "character, say previous",
	"cursor park", "cut",
	"edit delimiters", "edit exnum",
	"edit most", "edit repeats", "edit some",
	"go to", "go to bottom edge", "go to left edge",
	"go to right edge", "go to top edge", "help",
	"line, say current", "line, say next",
	"line, say previous", "line, say with indent",
	"paste", "pitch decrement", "pitch increment",
	"punctuation decrement", "punctuation increment",
	"punc level decrement", "punc level increment",
	"quiet",
	"rate decrement", "rate increment",
	"reading punctuation decrement", "reading punctuation increment",
	"say attributes",
	"say from left", "say from top",
	"say position", "say screen",
	"say to bottom", "say to right",
	"speakup", "speakup lock",
	"speakup off", "speech kill",
	"spell delay decrement", "spell delay increment",
	"spell word", "spell word phoneticly",
	"tone decrement", "tone increment",
	"voice decrement", "voice increment",
	"volume decrement", "volume increment",
	"window, clear", "window, say",
	"window, set", "window, silence",
	"word, say current", "word, say next",
	"word, say previous", 0
};

static u_char *state_tbl;
static int cur_item = 0, nstates = 0;

static void build_key_data(void)
{
	u_char *kp, counters[MAXFUNCS], ch, ch1;
	u_short *p_key = key_buf, key;
	int i, offset = 1;
	nstates = (int)(state_tbl[-1]);
	memset(counters, 0, sizeof(counters));
	memset(key_offsets, 0, sizeof(key_offsets));
	kp = state_tbl + nstates + 1;
	while (*kp++) { /* count occurrances of each function */
		for (i = 0; i < nstates; i++, kp++) {
			if (!*kp) continue;
			if ((state_tbl[i]&16) != 0 && *kp == SPK_KEY)
				continue;
			counters[*kp]++;
		}
	}
	for (i = 0; i < MAXFUNCS; i++) {
		if (counters[i] == 0) continue;
		key_offsets[i] = offset;
		offset += (counters[i]+1);
		if (offset >= MAXKEYS) break;
	}
/* leave counters set so high keycodes come first.
   this is done so num pad and other extended keys maps are spoken before
   the alpha with speakup type mapping. */
	kp = state_tbl + nstates + 1;
	while ((ch = *kp++)) {
		for (i = 0; i < nstates; i++) {
			ch1 = *kp++;
			if (!ch1) continue;
			if ((state_tbl[i]&16) != 0 && ch1 == SPK_KEY)
				continue;
			key = (state_tbl[i]<<8) + ch;
			counters[ch1]--;
			if (!(offset = key_offsets[ch1])) continue;
			p_key = key_buf + offset + counters[ch1];
			*p_key = key;
		}
	}
}

static void say_key(int key)
{
	int i, state = key>>8;
	key &= 0xff;
	for (i = 0; i < 6; i++) {
		if (state & masks[i])
			synth_write_string(statenames[i]);
	}
	synth_printf(" %s\n", keynames[--key]);
}

static int handle_help(struct vc_data *vc, u_char type, u_char ch, u_short key)
{
	int i, n;
	char *name;
	u_char func, *kp;
	u_short *p_keys, val;
	if (type == KT_LATIN) {
		if (ch == SPACE) {
			special_handler = NULL;
			synth_write_msg("leaving help");
			return 1;
		}
		ch |= 32; /* lower case */
		if (ch < 'a' || ch > 'z') return -1;
		if (letter_offsets[ch-'a'] == -1) {
			synth_printf("no commands for %c\n", ch);
			return 1;
		}
	cur_item	= letter_offsets[ch-'a'];
	} else if (type == KT_CUR) {
		if (ch == 0 && funcnames[cur_item+1] != NULL)
			cur_item++;
		else if (ch == 3 && cur_item > 0)
			cur_item--;
		else return -1;
	} else if (type == KT_SPKUP && ch == SPEAKUP_HELP && !special_handler) {
		special_handler = help_handler;
		synth_write_msg(help_info);
		build_key_data(); /* rebuild each time in case new mapping */
		return 1;
	} else {
		name = NULL;
		if (type != KT_SPKUP) {
			synth_write_msg(keynames[key-1]);
			return 1;
		}
		for (i = 0; funcvals[i] != 0 && !name; i++) {
			if (ch == funcvals[i])
				name = funcnames[i];
		}
		if (!name) return -1;
		kp = our_keys[key]+1;
		for (i = 0; i < nstates; i++) {
			if (ch == kp[i]) break;
		}
		key += (state_tbl[i]<<8);
		say_key(key);
		synth_printf("is %s\n",name);
		return 1;
	}
	name = funcnames[cur_item];
	func = funcvals[cur_item];
	synth_write_string(name);
	if (key_offsets[func] == 0) {
		synth_write_msg(" is unassigned");
		return 1;
	}
	p_keys = key_buf + key_offsets[func];
	for (n = 0; p_keys[n]; n++) {
		val = p_keys[n];
		if (n > 0) synth_write_string("or ");
		say_key(val);
	}
	return 1;
}

static void __exit mod_help_exit(void)
{
	help_handler = 0;
}

static int __init mod_help_init(void)
{
	char start = SPACE;
	int i;
state_tbl = our_keys[0]+SHIFT_TBL_SIZE+2;
	for (i = 0; i < 26; i++) letter_offsets[i] = -1;
	for (i = 0; funcnames[i]; i++) {
		if (start == *funcnames[i]) continue;
		start = *funcnames[i];
		letter_offsets[(start&31)-1] = i;
	}
	help_handler = handle_help;
	return 0;
}

module_init(mod_help_init);
module_exit(mod_help_exit);
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup keyboard help MODULE");
MODULE_LICENSE("GPL");
