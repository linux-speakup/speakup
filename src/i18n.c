/* Internationalization implementation.  Includes definitions of English
 * string arrays, and the i18n pointer. */
#include "i18n.h"
char *english_messages [] ={
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

};

char *english_edges[] = { "top, ", "bottom, ", "left, ", "right, ", "" };
char **english_strings [] = {NULL, english_messages, NULL, NULL, english_edges};

char ***i18n = english_strings;
