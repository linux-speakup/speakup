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
#define PARKED 5
#define UNPARKED 6
#define MARK 7
#define CUT 8
#define MARK_CLEARED 9
#define PASTE 10
#define BRIGHT 11
#define ON_BLINKING 12
#define MSG_ON 13
#define NO_WINDOW 14
#define CURSORING_OFF 15
#define CURSORING_ON 16
#define HIGHLIGHT_TRACKING 17
#define READ_WINDOW 18
#define READ_ALL 19
#define EDIT_DONE 20
#define WINDOW_ALREADY_SET 21
#define END_BEFORE_START 22
#define WINDOW_CLEARED 23
#define WINDOW_SILENCED 24
#define WINDOW_SILENCE_DISABLED 25
#define MSG_ERROR 26
#define GOTO_CANCELED 27
#define MSG_GOTO 28
#define LEAVING_HELP 29
#define IS_UNASSIGNED 30
#define HELP_INFO 31
/* For completeness.
#define EDGE_TOP 32
#define EDGE_BOTTOM 33
#define EDGE_LEFT 34
#define EDGE_RIGHT 35
#define EDGE_QUIET 36

/* Dummy indices. */
#define CURSOR_MSGS_START 15
#define EDGE_MSGS_START 32

extern char *speakup_msgs[];
#endif
