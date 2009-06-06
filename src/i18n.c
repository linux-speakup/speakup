/* Internationalization implementation.  Includes definitions of English
 * string arrays, and the i18n pointer. */

#include <linux/module.h>
#include <linux/string.h>
#include "i18n.h"

static char *speakup_msgs[MSG_LAST_INDEX];
static char *speakup_default_msgs   [MSG_LAST_INDEX] = {
	[MSG_BLANK] = "blank",
	[MSG_IAM_ALIVE] = "I'm aLive!",
	[MSG_YOU_KILLED_SPEAKUP] = "You killed speakup!",
	[MSG_HEY_THATS_BETTER] = "hey. That's better!",
	[MSG_YOU_TURNED_ME_OFF] = "You turned me off!",
	[MSG_PARKED] = "parked!",
	[MSG_UNPARKED] = "unparked!",
	[MSG_MARK] = "mark",
	[MSG_CUT] = "cut",
	[MSG_MARK_CLEARED] = "mark, cleared",
	[MSG_PASTE] = "paste",
	[MSG_BRIGHT] = "bright ",
	[MSG_ON_BLINKING] = " on blinking ",
	[MSG_ON] = " on ",
	[MSG_NO_WINDOW] = "no window",
	[MSG_CURSORING_OFF] = "cursoring off",
	[MSG_CURSORING_ON] = "cursoring on",
	[MSG_HIGHLIGHT_TRACKING] = "highlight tracking",
	[MSG_READ_WINDOW] = "read windo",
	[MSG_READ_ALL] = "read all" ,
	[MSG_EDIT_DONE] = "edit done",
	[MSG_WINDOW_ALREADY_SET] = "window already set, clear then reset",
	[MSG_END_BEFORE_START] = "error end before start",
	[MSG_WINDOW_CLEARED] = "window cleared",
	[MSG_WINDOW_SILENCED] = "window silenced",
	[MSG_WINDOW_SILENCE_DISABLED] = "window silence disabled",
	[MSG_ERROR] = "error",
	[MSG_GOTO_CANCELED] = " goto canceled",
	[MSG_GOTO] = "go to?",
	[MSG_LEAVING_HELP] = "leaving help",
	[MSG_IS_UNASSIGNED] = " is unassigned",
	[MSG_HELP_INFO] = "press space to leav help, cursor up or down to scroll, or a letter to go to commands in list",
	[MSG_EDGE_TOP] = "top, ",
	[MSG_EDGE_BOTTOM] = "bottom, ",
	[MSG_EDGE_LEFT] = "left, ",
	[MSG_EDGE_RIGHT] = "right, ",
	[MSG_EDGE_QUIET] = "",

	/* Color names. */
	[MSG_COLOR_BLACK] = "black",
	[MSG_COLOR_BLUE] = "blue",
	[MSG_COLOR_GREEN] = "green",
	[MSG_COLOR_CYAN] = "cyan",
	[MSG_COLOR_RED] = "red",
	[MSG_COLOR_MAGENTA] = "magenta",
	[MSG_COLOR_YELLOW] = "yellow",
	[MSG_COLOR_WHITE] = "white",
	[MSG_COLOR_GREY] = "grey",

	/* Names of key states. */
	[MSG_STATE_DOUBLE] = " double",
	[MSG_STATE_SPEAKUP] = " speakup",
	[MSG_STATE_ALT] = " alt",
	[MSG_STATE_CONTROL] = " ctrl",
	[MSG_STATE_ALTGR] = " altgr",
	[MSG_STATE_SHIFT] = " shift",

	/* Key names. */
	[MSG_KEYNAME_ESC] = "escape",
	[MSG_KEYNAME_1] = "1",
	[MSG_KEYNAME_2] = "2",
	[MSG_KEYNAME_3] = "3",
	[MSG_KEYNAME_4] = "4",
	[MSG_KEYNAME_5] = "5",
	[MSG_KEYNAME_6] = "6",
	[MSG_KEYNAME_7] = "7",
	[MSG_KEYNAME_8] = "8",
	[MSG_KEYNAME_9] = "9",
	[MSG_KEYNAME_0] = "0",
	[MSG_KEYNAME_DASH] = "minus",
	[MSG_KEYNAME_EQUAL] = "equal",
	[MSG_KEYNAME_BS] = "back space",
	[MSG_KEYNAME_TAB] = "tab",
	[MSG_KEYNAME_Q] = "q",
	[MSG_KEYNAME_W] = "w",
	[MSG_KEYNAME_E] = "e",
	[MSG_KEYNAME_R] = "r",
	[MSG_KEYNAME_T] = "t",
	[MSG_KEYNAME_Y] = "y",
	[MSG_KEYNAME_U] = "u",
	[MSG_KEYNAME_I] = "i",
	[MSG_KEYNAME_O] = "o",
	[MSG_KEYNAME_P] = "p",
	[MSG_KEYNAME_LEFTBRACE] = "left brace",
	[MSG_KEYNAME_RIGHTBRACE] = "right brace",
	[MSG_KEYNAME_ENTER] = "enter",
	[MSG_KEYNAME_LEFTCTRL] = "left control",
	[MSG_KEYNAME_A] = "a",
	[MSG_KEYNAME_S] = "s",
	[MSG_KEYNAME_D] = "d",
	[MSG_KEYNAME_F] = "f",
	[MSG_KEYNAME_G] = "g",
	[MSG_KEYNAME_H] = "h",
	[MSG_KEYNAME_J] = "j",
	[MSG_KEYNAME_K] = "k",
	[MSG_KEYNAME_L] = "l",
	[MSG_KEYNAME_SEMICOLON] = "semicolon",
	[MSG_KEYNAME_SINGLEQUOTE] = "apostrophe",
	[MSG_KEYNAME_GRAVE] = "accent",
	[MSG_KEYNAME_LEFTSHFT] = "left shift",
	[MSG_KEYNAME_BACKSLASH] = "back slash",
	[MSG_KEYNAME_Z] = "z",
	[MSG_KEYNAME_X] = "x",
	[MSG_KEYNAME_C] = "c",
	[MSG_KEYNAME_V] = "v",
	[MSG_KEYNAME_B] = "b",
	[MSG_KEYNAME_N] = "n",
	[MSG_KEYNAME_M] = "m",
	[MSG_KEYNAME_COMMA] = "comma",
	[MSG_KEYNAME_DOT] = "dot",
	[MSG_KEYNAME_SLASH] = "slash",
	[MSG_KEYNAME_RIGHTSHFT] = "right shift",
	[MSG_KEYNAME_KPSTAR] = "keypad asterisk",
	[MSG_KEYNAME_LEFTALT] = "left alt",
	[MSG_KEYNAME_SPACE] = "space",
	[MSG_KEYNAME_CAPSLOCK] = "caps lock",
	[MSG_KEYNAME_F1] = "f1",
	[MSG_KEYNAME_F2] = "f2",
	[MSG_KEYNAME_F3] = "f3",
	[MSG_KEYNAME_F4] = "f4",
	[MSG_KEYNAME_F5] = "f5",
	[MSG_KEYNAME_F6] = "f6",
	[MSG_KEYNAME_F7] = "f7",
	[MSG_KEYNAME_F8] = "f8",
	[MSG_KEYNAME_F9] = "f9",
	[MSG_KEYNAME_F10] = "f10",
	[MSG_KEYNAME_NUMLOCK] = "num lock",
	[MSG_KEYNAME_SCROLLLOCK] = "scroll lock",
	[MSG_KEYNAME_KP7] = "keypad 7",
	[MSG_KEYNAME_KP8] = "keypad 8",
	[MSG_KEYNAME_KP9] = "keypad 9",
	[MSG_KEYNAME_KPMINUS] = "keypad minus",
	[MSG_KEYNAME_KP4] = "keypad 4",
	[MSG_KEYNAME_KP5] = "keypad 5",
	[MSG_KEYNAME_KP6] = "keypad 6",
	[MSG_KEYNAME_KPPLUS] = "keypad plus",
	[MSG_KEYNAME_KP1] = "keypad 1",
	[MSG_KEYNAME_KP2] = "keypad 2",
	[MSG_KEYNAME_KP3] = "keypad 3",
	[MSG_KEYNAME_KP0] = "keypad 0",
	[MSG_KEYNAME_KPDOT] = "keypad dot",
	[MSG_KEYNAME_103RD] = "103rd",
	[MSG_KEYNAME_F13] = "f13",
	[MSG_KEYNAME_102ND] = "102nd",
	[MSG_KEYNAME_F11] = "f11",
	[MSG_KEYNAME_F12] = "f12",
	[MSG_KEYNAME_F14] = "f14",
	[MSG_KEYNAME_F15] = "f15",
	[MSG_KEYNAME_F16] = "f16",
	[MSG_KEYNAME_F17] = "f17",
	[MSG_KEYNAME_F18] = "f18",
	[MSG_KEYNAME_F19] = "f19",
	[MSG_KEYNAME_F20] = "f20",
	[MSG_KEYNAME_KPENTER] = "keypad enter",
	[MSG_KEYNAME_RIGHTCTRL] = "right control",
	[MSG_KEYNAME_KPSLASH] = "keypad slash",
	[MSG_KEYNAME_SYSRQ] = "sysrq",
	[MSG_KEYNAME_RIGHTALT] = "right alt",
	[MSG_KEYNAME_LF] = "line feed",
	[MSG_KEYNAME_HOME] = "home",
	[MSG_KEYNAME_UP] = "up",
	[MSG_KEYNAME_PGUP] = "page up",
	[MSG_KEYNAME_LEFT] = "left",
	[MSG_KEYNAME_RIGHT] = "right",
	[MSG_KEYNAME_END] = "end",
	[MSG_KEYNAME_DOWN] = "down",
	[MSG_KEYNAME_PGDN] = "page down",
	[MSG_KEYNAME_INS] = "insert",
	[MSG_KEYNAME_DEL] = "delete",
	[MSG_KEYNAME_MACRO] = "macro",
	[MSG_KEYNAME_MUTE] = "mute",
	[MSG_KEYNAME_VOLDOWN] = "volume down",
	[MSG_KEYNAME_VOLUP] = "volume up",
	[MSG_KEYNAME_POWER] = "power",
	[MSG_KEYNAME_KPEQUAL] = "keypad equal",
	[MSG_KEYNAME_KPPLUSDASH] = "keypad plusminus",
	[MSG_KEYNAME_PAUSE] = "pause",
	[MSG_KEYNAME_F21] = "f21",
	[MSG_KEYNAME_F22] = "f22",
	[MSG_KEYNAME_F23] = "f23",
	[MSG_KEYNAME_F24] = "f24",
	[MSG_KEYNAME_KPCOMMA] = "keypad comma",
	[MSG_KEYNAME_LEFTMETA] = "left meta",
	[MSG_KEYNAME_RIGHTMETA] = "right meta",
	[MSG_KEYNAME_COMPOSE] = "compose",
	[MSG_KEYNAME_STOP] = "stop",
	[MSG_KEYNAME_AGAIN] = "again",
	[MSG_KEYNAME_PROPS] = "props",
	[MSG_KEYNAME_UNDO] = "undo",
	[MSG_KEYNAME_FRONT] = "front",
	[MSG_KEYNAME_COPY] = "copy",
	[MSG_KEYNAME_OPEN] = "open",
	[MSG_KEYNAME_PASTE] = "paste",
	[MSG_KEYNAME_FIND] = "find",
	[MSG_KEYNAME_CUT] = "cut",
	[MSG_KEYNAME_HELP] = "help",
	[MSG_KEYNAME_MENU] = "menu",
	[MSG_KEYNAME_CALC] = "calc",
	[MSG_KEYNAME_SETUP] = "setup",
	[MSG_KEYNAME_SLEEP] = "sleep",
	[MSG_KEYNAME_WAKEUP] = "wakeup",
	[MSG_KEYNAME_FILE] = "file",
	[MSG_KEYNAME_SENDFILE] = "send file",
	[MSG_KEYNAME_DELFILE] = "delete file",
	[MSG_KEYNAME_XFER] = "transfer",
	[MSG_KEYNAME_PROG1] = "prog1",
	[MSG_KEYNAME_PROG2] = "prog2",
	[MSG_KEYNAME_WWW] = "www",
	[MSG_KEYNAME_MSDOS] = "msdos",
	[MSG_KEYNAME_COFFEE] = "coffee",
	[MSG_KEYNAME_DIRECTION] = "direction",
	[MSG_KEYNAME_CYCLEWINDOWS] = "cycle windows",
	[MSG_KEYNAME_MAIL] = "mail",
	[MSG_KEYNAME_BOOKMARKS] = "bookmarks",
	[MSG_KEYNAME_COMPUTER] = "computer",
	[MSG_KEYNAME_BACK] = "back",
	[MSG_KEYNAME_FORWARD] = "forward",
	[MSG_KEYNAME_CLOSECD] = "close cd",
	[MSG_KEYNAME_EJECTCD] = "eject cd",
	[MSG_KEYNAME_EJECTCLOSE] = "eject close cd",
	[MSG_KEYNAME_NEXTSONG] = "next song",
	[MSG_KEYNAME_PLAYPAUSE] = "play pause",
	[MSG_KEYNAME_PREVSONG] = "previous song",
	[MSG_KEYNAME_STOPCD] = "stop cd",
	[MSG_KEYNAME_RECORD] = "record",
	[MSG_KEYNAME_REWIND] = "rewind",
	[MSG_KEYNAME_PHONE] = "phone",
	[MSG_KEYNAME_ISO] = "iso",
	[MSG_KEYNAME_CONFIG] = "config",
	[MSG_KEYNAME_HOMEPG] = "home page",
	[MSG_KEYNAME_REFRESH] = "refresh",
	[MSG_KEYNAME_EXIT] = "exit",
	[MSG_KEYNAME_MOVE] = "move",
	[MSG_KEYNAME_EDIT] = "edit",
	[MSG_KEYNAME_SCROLLUP] = "scroll up",
	[MSG_KEYNAME_SCROLLDN] = "scroll down",
	[MSG_KEYNAME_KPLEFTPAR] = "keypad left paren",
	[MSG_KEYNAME_KPRIGHTPAR] = "keypad right paren",

	/* Function names. */
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
	"word, say previous",
};

char *msg_get(enum msg_index_t index)
{
	char *ch;

	ch = speakup_msgs[index];
	return ch;
}

/*
 * Function: msg_set
 * Description: Add a user-supplied message to the user_messages array.
 * The message text is copied to a memory area allocated with kmalloc.
 * If the function fails, then user_messages is untouched.
 * Arguments:
 * - index: a message number, as found in i18n.h.
 * - message: NUL-terminated text of message.
 * Return value: pointer to new message on success, NULL on failure.
 * Failure conditions:
 * - Unable to allocate memory.
 * - Illegal index.
*/

char *msg_set(enum msg_index_t index, char *text)
{
	char *newstr = NULL;

	if((index >= MSG_FIRST_INDEX) && (index < MSG_LAST_INDEX)) {
		newstr = kmalloc(strlen(text) + 1, GFP_KERNEL);
		if(newstr != NULL) {
			strcpy(newstr, text);
			if((speakup_msgs[index] != speakup_default_msgs[index])
			    && (speakup_msgs[index] != NULL))
				kfree(speakup_msgs[index]);
			speakup_msgs[index] = newstr;
		}
	}

	return newstr;
}

/* Called at initialization time, to establish default messages. */
void reset_default_msgs(void) {
	enum msg_index_t index;
	for(index = MSG_FIRST_INDEX; index < MSG_LAST_INDEX; index++)
		speakup_msgs[index] = speakup_default_msgs[index];
}

/* Free user-supplied strings when module is unloaded: */
void free_user_strings(void) {
	enum msg_index_t index;
	for(index = MSG_FIRST_INDEX; index < MSG_LAST_INDEX; index++) {
		if((speakup_msgs[index] != speakup_default_msgs[index])
		    && (speakup_msgs[index] != NULL)) {
			kfree(speakup_msgs[index]);
			speakup_msgs[index] = NULL;
		}
	}
}
