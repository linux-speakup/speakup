#include "spk_types.h"
#include "spk_priv.h"
#include "speakup.h"

static struct st_var_header var_headers[] = {
  { "version", VERSION, VAR_PROC, USER_R, 0, 0, 0 },
  { "synth_name", SYNTH, VAR_PROC, USER_RW, 0, 0, 0 },
  { "keymap", KEYMAP, VAR_PROC, USER_RW, 0, 0, 0 },
  { "silent", SILENT, VAR_PROC, USER_W, 0, 0, 0 },
  { "punc_some", PUNC_SOME, VAR_PROC, USER_RW, 0, 0, 0 },
  { "punc_most", PUNC_MOST, VAR_PROC, USER_RW, 0, 0, 0 },
  { "punc_all", PUNC_ALL, VAR_PROC, USER_R, 0, 0, 0 },
  { "delimiters", DELIM, VAR_PROC, USER_RW, 0, 0, 0 },
  { "repeats", REPEATS, VAR_PROC, USER_RW, 0, 0, 0 },
  { "ex_num", EXNUMBER, VAR_PROC, USER_RW, 0, 0, 0 },
  { "characters", CHARS, VAR_PROC, USER_RW, 0, 0, 0 },
  { "synth_direct", SYNTH_DIRECT, VAR_PROC, USER_W, 0, 0, 0 },
  { "caps_start", CAPS_START, VAR_STRING, USER_RW, 0, str_caps_start, 0 },
  { "caps_stop", CAPS_STOP, VAR_STRING, USER_RW, 0, str_caps_stop, 0 },
  { "delay_time", DELAY, VAR_TIME, ROOT_W, 0, &speakup_info.delay_time, 0 },
  { "trigger_time", TRIGGER, VAR_TIME, ROOT_W, 0, &synth_trigger_time, 0 },
  { "jiffy_delta", JIFFY, VAR_TIME, ROOT_W, 0, &speakup_info.jiffy_delta, 0 },
  { "full_time", FULL, VAR_TIME, ROOT_W, 0, &speakup_info.full_time, 0 },
  { "spell_delay", SPELL_DELAY, VAR_NUM, USER_RW, 0, &spell_delay, 0 },
  { "bleeps", BLEEPS, VAR_NUM, USER_RW, 0, &bleeps, 0 },
  { "attrib_bleep", ATTRIB_BLEEP, VAR_NUM, USER_RW, 0, &attrib_bleep, 0 },
  { "bleep_time", BLEEP_TIME, VAR_TIME, USER_RW, 0, &bleep_time, 0 },
  { "cursor_time", CURSOR_TIME, VAR_TIME, USER_RW, 0, &cursor_timeout, 0 },
  { "punc_level", PUNC_LEVEL, VAR_NUM, USER_RW, 0, &punc_level, 0 },
  { "reading_punc", READING_PUNC, VAR_NUM, USER_RW, 0, &reading_punc, 0 },
  { "say_control", SAY_CONTROL, VAR_NUM, USER_RW, 0, &say_ctrl, 0 },
  { "say_word_ctl", SAY_WORD_CTL, VAR_NUM, USER_RW, 0, &say_word_ctl, 0 },
  { "no_interrupt", NO_INTERRUPT, VAR_NUM, USER_RW, 0, &no_intr, 0 },
  { "key_echo", KEY_ECHO, VAR_NUM, USER_RW, 0, &key_echo, 0 },
  { "bell_pos", BELL_POS, VAR_NUM, USER_RW, 0, &bell_pos, 0 },
  { "rate", RATE, VAR_NUM, USER_RW, 0, 0, 0 },
  { "pitch", PITCH, VAR_NUM, USER_RW, 0, 0, 0 },
  { "vol", VOL, VAR_NUM, USER_RW, 0, 0, 0 },
  { "tone", TONE, VAR_NUM, USER_RW, 0, 0, 0 },
  { "punct", PUNCT, VAR_NUM, USER_RW, 0, 0, 0 },
  { "voice", VOICE, VAR_NUM, USER_RW, 0, 0, 0 },
  { "freq", FREQ, VAR_NUM, USER_RW, 0, 0, 0 },
  { "lang", LANG, VAR_NUM, USER_RW, 0, 0, 0 },
  { "chartab", CHARTAB, VAR_PROC, USER_RW, 0, 0, 0 },
};

static struct st_var_header *var_ptrs[MAXVARS] = { 0, 0, 0 };

static struct st_punc_var punc_vars[] = {
 { PUNC_SOME, 1 },
 { PUNC_MOST, 2 },
 { PUNC_ALL, 3 },
 { DELIM, 4 },
 { REPEATS, 5 },
 { EXNUMBER, 6 },
 { -1, -1 },
};

int chartab_get_value(char *keyword)
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

void speakup_register_var(struct st_num_var *var)
{
	static char nothing[2] = "\0";
	int i, var_id = var->var_id;
	struct st_var_header *p_header;
	struct st_string_var *s_var;

	if (var_id < 0 || var_id >= MAXVARS)
		return;
	if (var_ptrs[0] == 0) {
		for (i = 0; i < MAXVARS; i++) {
			p_header = &var_headers[i];
			var_ptrs[p_header->var_id] = p_header;
			p_header->data = 0;
		}
	}
	p_header = var_ptrs[var_id];
	if (p_header->data != 0)
		return;
	p_header->data = var;
	switch (p_header->var_type) {
	case VAR_STRING:
		s_var = (struct st_string_var *) var;
		set_string_var(nothing, p_header, 0);
		break;
	case VAR_NUM:
	case VAR_TIME:
		set_num_var(0, p_header, E_DEFAULT);
		break;
	}
	return;
}

void speakup_unregister_var(short var_id)
{
	struct st_var_header *p_header;
	if (var_id < 0 || var_id >= MAXVARS)
		return;
	p_header = var_ptrs[var_id];
	p_header->data = 0;
}

struct st_var_header *get_var_header(short var_id)
{
	struct st_var_header *p_header;
	if (var_id < 0 || var_id >= MAXVARS)
		return NULL;
	p_header = var_ptrs[var_id];
	if (p_header->data == NULL)
		return NULL;
	return p_header;
}

struct st_var_header *var_header_by_name(const char *name)
{
	int i;
	struct st_var_header *where = NULL;

	if (name != NULL) {
		i = 0;
		while ((i < MAXVARS) && (where == NULL)) {
			if (strcmp(name, var_ptrs[i]->name) == 0)
				where = var_ptrs[i];
			else
				i++;
		}
	}
	return where;
}

struct st_punc_var *get_punc_var(short var_id)
{
	struct st_punc_var *rv = NULL;
	struct st_punc_var *where;

	where = punc_vars;
	while ((where->var_id != -1) && (rv == NULL)) {
		if (where->var_id == var_id)
			rv = where;
		else
			where++;
	}
	return rv;
}

/* handlers for setting vars */
int set_num_var(short input, struct st_var_header *var, int how)
{
	short val, ret = 0;
	short *p_val = var->p_val;
	int l;
	char buf[32], *cp;
	struct st_num_var *var_data = var->data;
	if (var_data == NULL)
		return E_UNDEF;
	if (how == E_DEFAULT) {
		val = var_data->default_val;
		ret = SET_DEFAULT;
	} else {
		if (how == E_SET)
			val = input;
		else
			val = var_data->value;
		if (how == E_INC)
			val += input;
		else if (how == E_DEC)
			val -= input;
		if (val < var_data->low || val > var_data->high)
			return E_RANGE;
	}
	var_data->value = val;
	if (var->var_type == VAR_TIME && p_val != 0) {
		*p_val = (val * HZ + 1000 - HZ) / 1000;
		return ret;
	}
	if (p_val != 0)
		*p_val = val;
	if (var->var_id == PUNC_LEVEL) {
		punc_mask = punc_masks[val];
		return ret;
	}
	if (var_data->multiplier != 0)
		val *= var_data->multiplier;
	val += var_data->offset;
	if (var->var_id < FIRST_SYNTH_VAR || synth == NULL)
		return ret;
	if (synth->synth_adjust != NULL) {
		int status = synth->synth_adjust(var);
		return (status != 0) ? status : ret;
	}
	if (!var_data->synth_fmt)
		return ret;
	if (var->var_id == PITCH)
		cp = pitch_buff;
	else
		cp = buf;
	if (!var_data->out_str)
		l = sprintf(cp, var_data->synth_fmt, (int)val);
	else
		l = sprintf(cp, var_data->synth_fmt, var_data->out_str[val]);
	synth_printf("%s", cp);
	return ret;
}

int set_string_var(const char *page, struct st_var_header *var, int len)
{
	int ret = 0;
	struct st_string_var *var_data = var->data;
	if (var_data == NULL)
		return E_UNDEF;
	if (len > MAXVARLEN)
		return -E_TOOLONG;
	if (!len) {
	if (!var_data->default_val)
		return 0;
		ret = SET_DEFAULT;
		if (!var->p_val)
			var->p_val = var_data->default_val;
		if (var->p_val != var_data->default_val)
			strcpy((char *)var->p_val, var_data->default_val);
		} else if (var->p_val)
			strcpy((char *)var->p_val, page);
	else
		return -E_TOOLONG;
	return ret;
}

/* set_mask_bits sets or clears the punc/delim/repeat bits,
 * if input is null uses the defaults.
 * values for how: 0 clears bits of chars supplied,
 * 1 clears allk, 2 sets bits for chars */
int set_mask_bits(const char *input, const int which, const int how)
{
	u_char *cp;
	short mask = punc_info[which].mask;
	if (how&1) {
		for (cp = (u_char *)punc_info[3].value; *cp; cp++)
			spk_chartab[*cp] &= ~mask;
	}
	cp = (u_char *)input;
	if (cp == 0)
		cp = punc_info[which].value;
	else {
		for ( ; *cp; cp++) {
			if (*cp < SPACE)
				break;
			if (mask < PUNC) {
				if (!(spk_chartab[*cp]&PUNC))
					break;
			} else if (spk_chartab[*cp]&B_NUM)
				break;
		}
		if (*cp)
			return -EINVAL;
		cp = (u_char *)input;
	}
	if (how&2) {
		for ( ; *cp; cp++)
			if (*cp > SPACE)
				spk_chartab[*cp] |= mask;
	} else {
		for ( ; *cp; cp++)
			if (*cp > SPACE)
				spk_chartab[*cp] &= ~mask;
	}
	return 0;
}
