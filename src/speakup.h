#ifndef _SPEAKUP_H
#define _SPEAKUP_H

extern u_char *our_keys[];
extern special_func special_handler;
extern int handle_help(struct vc_data *vc, u_char type, u_char ch, u_short key);

#endif
