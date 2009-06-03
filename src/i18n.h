#ifndef I18N_H
#define I18N_H
/* Internationalization declarations */

#define CHARACTERS 0
#define MSGS 1
#define COLORS 2
#define HELP 3
#define EDGENAMES 4

#define BLANK 0
#define IAM_ALIVE 1
#define YOU_KILLED_SPEAKUP 2
#define HEY_THATS_BETTER 3
#define YOU_TURNED_ME_OFF 4


/* I'll start it here to keep the indices and messages in sync. */

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
#endif
