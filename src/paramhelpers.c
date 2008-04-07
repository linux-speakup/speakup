#include <linux/version.h>
#include <linux/console.h>
#include <linux/limits.h>
#include <linux/module.h>

#include "spk_types.h"
#include "spk_priv.h"
#include "speakup.h"

static int set_characters(const char *val, struct kernel_param *kp);
static int get_characters(char *buffer, struct kernel_param *kp);
static int set_chartab(const char *val, struct kernel_param *kp);
static int get_chartab(char *buffer, struct kernel_param *kp);
static int set_keymap(const char *val, struct kernel_param *kp);
static int get_keymap(char *buffer, struct kernel_param *kp);
static int set_silent(const char *val, struct kernel_param *kp);
static int set_synth(const char *val, struct kernel_param *kp);
static int get_synth(char *buffer, struct kernel_param *kp);
static int send_synth_direct(const char *buffer, struct kernel_param *kp);
static int set_bits(const char *val, struct kernel_param *kp);
static int get_bits(char *buffer, struct kernel_param *kp);
static int set_vars(const char *val, struct kernel_param *kp);
static int get_vars(char *buffer, struct kernel_param *kp);

/*
 * The first thing we do is define the parameters.
 */
module_param_call(characters, NULL, get_characters, NULL, 0644);
module_param_call(chartab, NULL, get_chartab, NULL, 0644);
module_param_call(keymap, set_keymap, get_keymap, NULL, 0644);
module_param_call(silent, set_silent, NULL, NULL, 0664);
module_param_call(synth, set_synth, get_synth, NULL, 0664);
module_param_call(synth_direct, send_synth_direct, NULL, NULL, 0664);

module_param_call(delimiters, set_bits, get_bits, NULL, 0664);
module_param_call(ex_num, set_bits, get_bits, NULL, 0664);
module_param_call(punc_all, set_bits, get_bits, NULL, 0664);
module_param_call(punc_most, set_bits, get_bits, NULL, 0664);
module_param_call(punc_some, set_bits, get_bits, NULL, 0664);
module_param_call(repeats, set_bits, get_bits, NULL, 0664);

module_param_call(attrib_bleep, set_vars, get_vars, NULL, 0664);
module_param_call(bell_pos, set_vars, get_vars, NULL, 0664);
module_param_call(bleeps, set_vars, get_vars, NULL, 0664);
module_param_call(bleep_time, set_vars, get_vars, NULL, 0664);
module_param_call(caps_start, set_vars, get_vars, NULL, 0664);
module_param_call(caps_stop, set_vars, get_vars, NULL, 0664);
module_param_call(cursor_time, set_vars, get_vars, NULL, 0664);
module_param_call(delay_time, set_vars, get_vars, NULL, 0644);
module_param_call(full_time, set_vars, get_vars, NULL, 0644);
module_param_call(jiffy_delta, set_vars, get_vars, NULL, 0644);
module_param_call(key_echo, set_vars, get_vars, NULL, 0664);
module_param_call(no_interrupt, set_vars, get_vars, NULL, 0664);
module_param_call(punc_level, set_vars, get_vars, NULL, 0664);
module_param_call(reading_punc, set_vars, get_vars, NULL, 0664);
module_param_call(say_control, set_vars, get_vars, NULL, 0664);
module_param_call(say_word_ctl, set_vars, get_vars, NULL, 0664);
module_param_call(spell_delay, set_vars, get_vars, NULL, 0664);
module_param_call(trigger_time, set_vars, get_vars, NULL, 0644);

module_param_call(freq, set_vars, get_vars, NULL, 0664);
module_param_call(lang, set_vars, get_vars, NULL, 0664);
module_param_call(pitch, set_vars, get_vars, NULL, 0664);
module_param_call(punct, set_vars, get_vars, NULL, 0664);
module_param_call(rate, set_vars, get_vars, NULL, 0664);
module_param_call(tone, set_vars, get_vars, NULL, 0664);
module_param_call(voice, set_vars, get_vars, NULL, 0664);
module_param_call(vol, set_vars, get_vars, NULL, 0664);

/*
 * The set handler for characters goes here.
 */
static int set_characters(const char *val, struct kernel_param *kp)
{
	return 0;
}

/*
 * This is the get handler for characters.
 */
static int get_characters(char *buffer, struct kernel_param *kp)
{
	int i;
	int len = 0;
	char *cp;

	for (i = 0; i < 256; i++) {
		cp = (characters[i]) ? characters[i] : "NULL";
		len += sprintf(buffer + len, "%d\t%s\n", i, cp);
	}
	return len;
}

/*
 * The set handler for chartab goes here.
 */
static int set_chartab(const char *val, struct kernel_param *kp)
{
	return 0;
}

/*
 * This is the get handler for chartab.
 */
static int get_chartab(char *buffer, struct kernel_param *kp)
{
	int i;
	int len = 0;
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
		len += sprintf(buffer + len, "%d\t%s\n", i, cp);
	}
	return len;
}

/*
 * This is the set handler for keymap.
 */
static int set_keymap(const char *val, struct kernel_param *kp)
{
	int i;
	int ret = 0;
	int count;
	char *in_buff, *cp;
	u_char *cp1;
	unsigned long flags;

	count = strlen(val);
	if (count < 1 || count > 1800)
		return -EINVAL;

	in_buff = (char *) val;

	spk_lock(flags);
	if (in_buff[count - 1] == '\n')
		count--;
	in_buff[count] = '\0';
	if (count == 1 && *in_buff == 'd') {
		set_key_info(key_defaults, key_buf);
		spk_unlock(flags);
		return 0;
	}
	cp = in_buff;
	cp1 = (u_char *)in_buff;
	for (i = 0; i < 3; i++) {
		cp = s2uchar(cp, cp1);
		cp1++;
	}
	i = (int)cp1[-2]+1;
	i *= (int)cp1[-1]+1;
	i += 2; /* 0 and last map ver */
	if (cp1[-3] != KEY_MAP_VER || cp1[-1] > 10 ||
			i+SHIFT_TBL_SIZE+4 >= sizeof(key_buf)) {
		pr_warn("i %d %d %d %d\n", i, (int)cp1[-3], (int)cp1[-2], (int)cp1[-1]);
		spk_unlock(flags);
		return -EINVAL;
	}
	while (--i >= 0) {
		cp = s2uchar(cp, cp1);
		cp1++;
		if (!(*cp))
			break;
	}
	if (i != 0 || cp1[-1] != KEY_MAP_VER || cp1[-2] != 0) {
		ret = -EINVAL;
		pr_warn("end %d %d %d %d\n", i, (int)cp1[-3], (int)cp1[-2], (int)cp1[-1]);
	} else {
		if (set_key_info(in_buff, key_buf)) {
			set_key_info(key_defaults, key_buf);
			ret = -EINVAL;
			pr_warn("set key failed\n");
		}
	}
	spk_unlock(flags);
	return ret;
}

/*
 * This is the get handler for keymap.
 */
static int get_keymap(char *buffer, struct kernel_param *kp)
{
	char *cp = buffer;
	int i, n, num_keys, nstates;
	u_char *cp1 = key_buf + SHIFT_TBL_SIZE, ch;
	num_keys = (int)(*cp1);
	nstates = (int)cp1[1];
	cp += sprintf(cp, "%d, %d, %d,\n", KEY_MAP_VER, num_keys, nstates);
	cp1 += 2; /* now pointing at shift states */
/* dump num_keys+1 as first row is shift states + flags,
   each subsequent row is key + states */
	for (n = 0; n <= num_keys; n++) {
		for (i = 0; i <= nstates; i++) {
			ch = *cp1++;
			cp += sprintf(cp, "%d,", (int)ch);
			*cp++ = (i < nstates) ? SPACE : '\n';
		}
	}
	cp += sprintf(cp, "0, %d\n", KEY_MAP_VER);
	return (int)(cp-buffer);
}

/*
 * This is the set handler for silent.
 * There is not a get handler because reading this parameter is not useful.
 */
static int set_silent(const char *val, struct kernel_param *kp)
{
	int count;
	struct vc_data *vc = vc_cons[fg_console].d;
	char ch = 0, shut;
	unsigned long flags;

	count = strlen(val);
	if (count > 0 || count < 3) {
		ch = val[0];
		if (ch == '\n')
			ch = '0';
	}
	if (ch < '0' || ch > '7') {
		pr_warn("silent value '%c' not in range (0,7)\n", ch);
		return -EINVAL;
	}
	spk_lock(flags);
	if (ch&2) {
		shut = 1;
		do_flush();
	} else
		shut = 0;
	if (ch&4)
		shut |= 0x40;
	if (ch&1)
		spk_shut_up |= shut;
	else
		spk_shut_up &= ~shut;
	spk_unlock(flags);
	return 0;
}

/*
 * This is the function which switches synthesizers when the requested
 * synthesizer driver is either built in or loaded.
 */
static int set_synth(const char *val, struct kernel_param *kp)
{
	int count;
	char new_synth_name[10];
	const char *old_synth_name = (synth != NULL) ? synth->name : "none";

	if (! val)
		return -EINVAL;

	count = strlen(val);
	if (count < 2 || count > 9)
		return -EINVAL;
	strncpy(new_synth_name, val, count);
	if (new_synth_name[count - 1] == '\n')
		count--;
	new_synth_name[count] = '\0';
	strlwr(new_synth_name);
	if (!strcmp(new_synth_name, old_synth_name)) {
		pr_warn("%s already in use\n", new_synth_name);
	} else if (synth_init(new_synth_name) != 0) {
		pr_warn("failed to init synth %s\n", new_synth_name);
		return -ENODEV;
	}
	return 0;
}

/*
 * This function shows which synthesizer is active when the user reads the
 * synth_name parameter.
 */
static int get_synth(char *buffer, struct kernel_param *kp)
{
	if (synth == NULL)
		strcpy(synth_name, "none");
	else
		strcpy(synth_name, synth->name);
	return sprintf(buffer, "%s\n", synth_name);
}

/*
 * This is the set handler for synth_direct.
 * There is not a get handler because reading this parameter is not useful.
 */
static int send_synth_direct(const char *val, struct kernel_param *kp)
{
	u_char buf[256];
	int count;
	int bytes;
	const char *ptr = val;

	if (! val)
		return -EINVAL;

	if (synth == NULL)
		return -EPERM;

	count = strlen(val);
	while (count > 0) {
		bytes = min_t(size_t, count, 250);
		strncpy(buf, ptr, bytes);
		buf[bytes] = '\0';
		xlate(buf);
		synth_printf("%s",buf);
		ptr += bytes;
		count -= bytes;
	}
	return 0;
}

/*
 * This is the set handler for the punctuation settings.
 */
static int set_bits(const char *val, struct kernel_param *kp)
{
	struct st_var_header *p_header = var_header_by_name(kp->name);
	struct st_punc_var *var = get_punc_var(p_header->var_id);
	int ret;
	int count;
	char punc_buf[100];
	unsigned long flags;

	ret = 0;
	count = strlen(val);
	if (count < 1 || count > 99)
		return -EINVAL;

	strncpy(punc_buf, val, count);

	if (punc_buf[count - 1] == '\n')
		count--;
	punc_buf[count] = '\0';

	spk_lock(flags);

	if (*punc_buf == 'd' || *punc_buf == 'r')
		count = set_mask_bits(0, var->value, 3);
	else
		count = set_mask_bits(punc_buf, var->value, 3);

	spk_unlock(flags);
	if (count < 0)
		ret = count;
	return ret;
}

/*
 * This is the get handler for the punctuation settings.
 */
static int get_bits(char *buffer, struct kernel_param *kp)
{
	int i;
	struct st_var_header *p_header = var_header_by_name(kp->name);
	struct st_punc_var *var = get_punc_var(p_header->var_id);
	const struct st_bits_data *pb = &punc_info[var->value];
	short mask = pb->mask;
	char *cp = buffer;
	
	for (i = 33; i < 128; i++) {
		if (!(spk_chartab[i]&mask))
			continue;
		*cp++ = (char)i;
	}
	*cp++ = '\n';
	return cp-buffer;
}

/*
 * This function is called when a user echos a value to one of the
 * variable parameters.
 */
static int set_vars(const char *val, struct kernel_param *kp)
{
	struct st_var_header *param;
	int ret;
	int len;
	char *cp;
	struct st_num_var *var_data;
	short value;

	if (!val)
		return -EINVAL;

	param = var_header_by_name(kp->name);
	if (param == NULL)
		return -EINVAL;

	ret = 0;
	len = strlen(val);
	cp = xlate((char *) val);
	switch (param->var_type) {
	case VAR_NUM:
	case VAR_TIME:
		if (*cp == 'd' || *cp == 'r' || *cp == '\0')
			len = E_DEFAULT;
		else if (*cp == '+' || *cp == '-')
			len = E_INC;
		else
			len = E_SET;
		speakup_s2i(cp, &value);
		ret = set_num_var(value, param, len);
		if (ret == E_RANGE) {
			var_data = param->data;
			pr_warn("value for %s out of range, expect %d to %d\n",
				kp->name, (int)var_data->low, (int)var_data->high);
		}
		break;
	case VAR_STRING:
		ret = set_string_var(val, param, len);
		if (ret == E_TOOLONG)
			pr_warn("value too long for %s\n", kp->name);
		break;
	default:
		pr_warn("%s unknown type %d\n",
			param->name, (int)param->var_type);
	break;
	}
	if (ret == SET_DEFAULT)
		pr_info("%s reset to default value\n", kp->name);
	return 0;
}

/*
 * This function is called when a user reads one of the variable parameters.
 */
static int get_vars(char *buffer, struct kernel_param *kp)
{
	int rv = 0;
	struct st_var_header *param;
	struct st_num_var *n_var;
		char *cp1;
	char *cp;
	char ch;

	if (buffer == NULL)
		return -EINVAL;

	param = var_header_by_name(kp->name);
	if (param == NULL)
		return -EINVAL;

	n_var = (struct st_num_var *) param->data;
	switch (param->var_type) {
		case VAR_NUM:
		case VAR_TIME:
			rv = sprintf(buffer, "%i", (int) n_var->value);
			break;
		case VAR_STRING:
			cp1 = buffer;
			*cp1++ = '"';
			for (cp = (char *)param->p_val; (ch = *cp); cp++) {
				if (ch >= ' ' && ch < '~')
					*cp1++ = ch;
				else
					cp1 += sprintf(cp1, "\\""x%02x", ch);
			}
		*cp1++ = '"';
		*cp1++ = '\n';
		*cp1 = '\0';
		rv = cp1-buffer;
			break;
		default:
			rv = sprintf("Bad parameter  %s, type %i\n",
				param->name, param->var_type);
			break;
	}
	return rv;
}
