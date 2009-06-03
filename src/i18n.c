/* Internationalization implementation.  Includes definitions of English
 * string arrays, and the i18n pointer. */
#include "i18n.h"
char *english_messages [] ={
	"blank",
	"I'm aLive!",
	"You killed speakup!",
	"hey. That's better!",
	"You turned me off!",

};

char *english_edges[] = { "top, ", "bottom, ", "left, ", "right, ", "" };
char **english_strings [] = {NULL, english_messages, NULL, NULL, english_edges};

char ***i18n = english_strings;
