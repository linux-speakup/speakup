/* Internationalization implementation.  Includes definitions of English
 * string arrays, and the i18n pointer. */

#include "i18n.h"

static char *speakup_msgs   [MSG_LAST_INDEX] = {
	"blank",
	"I'm aLive!",
	"You killed speakup!",
	"hey. That's better!",
	"You turned me off!",
	"parked!",
	"unparked!",
	"mark",
	"cut",
	"mark, cleared",
	"paste",
	"bright ",
	" on blinking ",
	" on ",
	"no window",
	"cursoring off", "cursoring on",
	"highlight tracking", "read windo",
"read all" ,
	"edit done",
	"window already set, clear then reset",
	"error end before start",
	"window cleared",
	"window silenced",
	"window silence disabled",
	"error",
	" goto canceled",
	"go to?",
	"leaving help",
	" is unassigned",
	"press space to leav help, cursor up or down to scroll, \
or a letter to go to commands in list",
	"top, ",
	"bottom, ",
	"left, ",
	"right, ",
	"",

};

char *msg_get(enum msg_index_t index)
{
	char *ch;

	ch = speakup_msgs[index];
	return ch;
}

