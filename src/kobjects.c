/*
 * Speakup kobject implementation
 *
 * Copyright (C) 2009 William Hubbs
 *
 * This code is based on kobject-example.c, which came with linux 2.6.x.
 *
 * Copyright (C) 2004-2007 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (C) 2007 Novell Inc.
 *
 * Released under the GPL version 2 only.
 *
 */
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>

#include "speakup.h"
#include "spk_priv.h"

/*
 * This is called when a user reads the characters or chartab sys file.
 */
static ssize_t chars_chartab_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int i;
	int len = 0;
	char *cp;

	for (i = 0; i < 256; i++) {
		if(strcmp("characters", attr->attr.name) == 0) {
			len += sprintf(buf + len, "%d\t%s\n", i, characters[i]);
		} else {
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
			else
				cp = "0";
			len += sprintf(buf + len, "%d\t%s\n", i, cp);
		}
	}
	return len;
}

/*
 * Print informational messages or warnings after updating
 * character descriptions.
 */
static void report_char_status(int reset, int received, int used, int rejected)
{
	int len;
	char buf[80];

	if (reset) {
		pr_info("character descriptions reset to defaults\n");
	} else if (received ) {
		len = snprintf(buf, sizeof(buf),
			       " updated %d of %d character descriptions\n",
				       used, received);
		if (rejected)
			snprintf(buf + (len - 1), sizeof(buf) - (len - 1),
				 " with %d reject%s\n",
				 rejected, rejected > 1 ? "s" : "");
		printk(buf);
	}
}

/*
 * This is called when a user changes the characters parameter.
 */
static ssize_t chars_store(struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	const char *linefeed = NULL;
	const char *cp = buf;
	const char *end;
	char *descptr = NULL;
	char *newstr = NULL;
	ssize_t retval = count;
	unsigned long flags;
	unsigned long index = 0;
	int received = 0;
	int used = 0;
	int rejected = 0;
	int reset = 0;
	size_t desc_length = 0;
	int i;

	spk_lock(flags);
	end = buf + count - 1;	/* end is pointer to NUL */

	while (cp < end) {

		while ((cp < end) && (*cp == ' ' || *cp == '\t'))
			cp++;
		if (cp == end)
			break;
		if ((*cp == '\n') || strchr("dDrR", *cp)) {
			reset = 1;
			break;
		}
		linefeed = strchr(cp, '\n');
		received++;
		if (!linefeed) {
			rejected++;
			break;
		}

		if (isdigit(*cp)) {
			index = simple_strtoul(cp, &descptr, 10);

			/* We have at least one digit, so descptr > cp.
			 * The first non-digit may have been *linefeed, so
			 * descptr <= linefeed. */
			cp = linefeed + 1;
			if (index > 255) {
				rejected++;
				continue;
			}

			while ((descptr < linefeed)
			       && (*descptr == ' ' || *descptr == '\t'))
				descptr++;

			desc_length = linefeed - descptr;
			newstr = kmalloc(desc_length + 1, GFP_ATOMIC);
			if (newstr == NULL) {
				retval = -ENOMEM;
				reset = 1;	/* just reset on error. */
				break;
			}

			for (i = 0; i < desc_length; i++)
				newstr[i] = descptr[i];
			newstr[desc_length] = '\0';

			if (characters[index] != default_chars[index])
				kfree(characters[index]);

			characters[index] = newstr;
			used++;
		} else {	/* not "index description" */
			rejected++;
			cp = linefeed + 1;
		}
	}

	if (reset)
		reset_default_chars();

	spk_unlock(flags);

	report_char_status(reset, received, used, rejected);

	return retval;
}

/*
 * This is called when a user changes the chartab parameter.
 */
static ssize_t chartab_store(struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

/*
 * This is called when a user reads the keymap parameter.
 */
static ssize_t keymap_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	char *cp = buf;
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
	return (int)(cp-buf);
}

/*
 * This is called when a user changes the keymap parameter.
 */
static ssize_t keymap_store(struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

/*
 * This is called when a user changes the value of the silent parameter.
 */
static ssize_t silent_store(struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	int len;
	struct vc_data *vc = vc_cons[fg_console].d;
	char ch = 0, shut;
	unsigned long flags;

	len = strlen(buf);
	if (len > 0 || len < 3) {
		ch = buf[0];
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
	return count;
}

/*
 * This is called when a user reads the synth setting.
 */
static ssize_t synth_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	int rv;

	if (synth == NULL)
		rv = sprintf(buf, "%s\n", "none");
	else
		rv = sprintf(buf, "%s\n", synth->name);
	return rv;
}

/*
 * This is called when a user requests to change synthesizers.
 */
static ssize_t synth_store(struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	int len;
	char new_synth_name[10];

	len = strlen(buf);
	if (len < 2 || len > 9)
		return -EINVAL;
	strncpy(new_synth_name, buf, len);
	if (new_synth_name[len - 1] == '\n')
		len--;
	new_synth_name[len] = '\0';
	strlwr(new_synth_name);
	if ((synth != NULL) && (!strcmp(new_synth_name, synth->name))) {
		pr_warn("%s already in use\n", new_synth_name);
	} else if (synth_init(new_synth_name) != 0) {
		pr_warn("failed to init synth %s\n", new_synth_name);
		return -ENODEV;
	}
	return count;
}

/*
 * This is called when text is sent to the synth via the synth_direct file.
 */
static ssize_t synth_direct_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	u_char tmp[256];
	int len;
	int bytes;
	const char *ptr = buf;

	if (! synth)
		return -EPERM;

	len = strlen(buf);
	while (len > 0) {
		bytes = min_t(size_t, len, 250);
		strncpy(tmp, ptr, bytes);
		tmp[bytes] = '\0';
		xlate(tmp);
		synth_printf("%s", tmp);
		ptr += bytes;
		len -= bytes;
	}
	return count;
}

/*
 * This function is called when a user reads the version.
 */
static ssize_t version_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	char *cp;

	cp = buf;
	cp += sprintf(cp, "Speakup version %s\n", SPEAKUP_VERSION);
	if (synth)
		cp += sprintf(cp, "%s synthesizer driver version %s\n",
		synth->name, synth->version);
	return cp - buf;
}

/*
 * This is called when a user reads the punctuation settings.
 */
static ssize_t punc_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	int i;
	char *cp = buf;
	struct st_var_header *p_header;
	struct punc_var_t *var;
	struct st_bits_data *pb;
	short mask;

	p_header = var_header_by_name(attr->attr.name);
	if (p_header == NULL) {
		pr_warn("p_header is null, attr->attr.name is %s\n", attr->attr.name);
		return -EINVAL;
	}

	var = get_punc_var(p_header->var_id);
	if (var == NULL) {
		pr_warn("var is null, p_header->var_id is %i\n",
				p_header->var_id);
		return -EINVAL;
	}

	pb = (struct st_bits_data *) &punc_info[var->value];
	mask = pb->mask;
	for (i = 33; i < 128; i++) {
		if (!(spk_chartab[i]&mask))
			continue;
		*cp++ = (char)i;
	}
	return cp-buf;
}

/*
 * This is called when a user changes the punctuation settings.
 */
static ssize_t punc_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int x;
	struct st_var_header *p_header;
	struct punc_var_t *var;
	char punc_buf[100];
	unsigned long flags;

	x = strlen(buf);
	if (x < 1 || x > 99)
		return -EINVAL;

	p_header = var_header_by_name(attr->attr.name);
	if (p_header == NULL) {
		pr_warn("p_header is null, attr->attr.name is %s\n", attr->attr.name);
		return -EINVAL;
	}

	var = get_punc_var(p_header->var_id);
	if (var == NULL) {
		pr_warn("var is null, p_header->var_id is %i\n",
				p_header->var_id);
		return -EINVAL;
	}

	strncpy(punc_buf, buf, x);

	while (x && punc_buf[x - 1] == '\n')
		x--;
	punc_buf[x] = '\0';

	spk_lock(flags);

	if (*punc_buf == 'd' || *punc_buf == 'r')
		x = set_mask_bits(0, var->value, 3);
	else
		x = set_mask_bits(punc_buf, var->value, 3);

	spk_unlock(flags);
	return count;
}

/*
 * This function is called when a user reads one of the variable parameters.
 */
ssize_t spk_var_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	int rv = 0;
	struct st_var_header *param;
	struct var_t *var;
		char *cp1;
	char *cp;
	char ch;

	param = var_header_by_name(attr->attr.name);
	if (param == NULL)
		return -EINVAL;

	var = (struct var_t *) param->data;
	switch (param->var_type) {
	case VAR_NUM:
	case VAR_TIME:
		if (var)
			rv = sprintf(buf, "%i\n", var->u.n.value);
		else
			rv = sprintf(buf, "0\n");
		break;
	case VAR_STRING:
		if (var) {
			cp1 = buf;
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
			rv = cp1-buf;
		} else {
			rv = sprintf(buf, "\"\"\n");
		}
		break;
	default:
		rv = sprintf(buf, "Bad parameter  %s, type %i\n",
			param->name, param->var_type);
		break;
	}
	return rv;
}
EXPORT_SYMBOL_GPL(spk_var_show);

/*
 * This function is called when a user echos a value to one of the
 * variable parameters.
 */
ssize_t spk_var_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	struct st_var_header *param;
	int ret;
	int len;
	char *cp;
	struct var_t *var_data;
	int value;

	param = var_header_by_name(attr->attr.name);
	if (param == NULL)
		return -EINVAL;
	if (param->data == NULL)
		return 0;
	ret = 0;
	cp = xlate((char *) buf);
	len = strlen(buf); /* xlate may have changed the length of the string */

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
				attr->attr.name,
				var_data->u.n.low, var_data->u.n.high);
		}
		break;
	case VAR_STRING:
		/*
		 * Strip balanced quote and newline character, if present.
		*/
		if((len >= 1) && (buf[len - 1] == '\n'))
			--len;
		if((len >= 2) && (buf[0] == '"') && (buf[len - 1] == '"')) {
			++buf;
			len -= 2;
		}
		cp = (char *) buf; /* non-const pointer to buf */
		cp[len] = '\0'; /* Ensure NUL-termination. */
		ret = set_string_var(buf, param, len);
		if (ret == E_TOOLONG)
			pr_warn("value too long for %s\n",
					attr->attr.name);
		break;
	default:
		pr_warn("%s unknown type %d\n",
			param->name, (int)param->var_type);
	break;
	}
	if (ret == SET_DEFAULT)
		pr_info("%s reset to default value\n", attr->attr.name);
	return count;
}
EXPORT_SYMBOL_GPL(spk_var_store);

/*
 * Functions for reading and writing lists of i18n messages.  Incomplete.
 */

static ssize_t message_show_helper(char *buf, enum msg_index_t first,
	enum msg_index_t last)
{
	size_t bufsize = PAGE_SIZE;
	char *buf_pointer = buf;
	int printed;
	enum msg_index_t cursor;
	int index = 0;
	*buf_pointer = '\0'; /* buf_pointer always looking at a NUL byte. */

	for (cursor = first; cursor <= last; cursor++, index++) {
		if(bufsize <= 1) /* full buffer. */
			break;
		printed = scnprintf(buf_pointer, bufsize, "%d\t%s\n",
			index, msg_get(cursor));
		buf_pointer += printed; /* point to NUL following text. */
		bufsize -= printed;
	}

	return buf_pointer - buf;
}

static ssize_t message_store_helper(const char *buf, size_t count,
	enum msg_index_t first, enum msg_index_t last)
{
	return count; /* Allow the write, and do nothing. */
} /* message_store_helper */

static ssize_t message_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	ssize_t retval = 0;
	struct msg_set *set = find_message_set(attr->attr.name);

	BUG_ON(! set);
	retval = message_show_helper(buf, set->start, set->end);
	return retval;
} /* message_show */

static ssize_t message_store(struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	ssize_t retval = 0;
	struct msg_set *set = find_message_set(attr->attr.name);

	BUG_ON(! set);
	retval = message_store_helper(buf, count, set->start, set->end);
	return retval;
} /* message_store */

/* End i18n-message functions. */

/*
 * Declare the attributes.
 */
static struct kobj_attribute keymap_attribute =
	__ATTR(keymap, ROOT_W, keymap_show, keymap_store);
static struct kobj_attribute silent_attribute =
	__ATTR(silent, USER_W, NULL, silent_store);
static struct kobj_attribute synth_attribute =
	__ATTR(synth, USER_RW, synth_show, synth_store);
static struct kobj_attribute synth_direct_attribute =
	__ATTR(synth_direct, USER_W, NULL, synth_direct_store);
static struct kobj_attribute version_attribute =
	__ATTR_RO(version);

static struct kobj_attribute delimiters_attribute =
	__ATTR(delimiters, USER_RW, punc_show, punc_store);
static struct kobj_attribute ex_num_attribute =
	__ATTR(ex_num, USER_RW, punc_show, punc_store);
static struct kobj_attribute punc_all_attribute =
	__ATTR(punc_all, USER_RW, punc_show, punc_store);
static struct kobj_attribute punc_most_attribute =
	__ATTR(punc_most, USER_RW, punc_show, punc_store);
static struct kobj_attribute punc_some_attribute =
	__ATTR(punc_some, USER_RW, punc_show, punc_store);
static struct kobj_attribute repeats_attribute =
	__ATTR(repeats, USER_RW, punc_show, punc_store);

static struct kobj_attribute attrib_bleep_attribute =
	__ATTR(attrib_bleep, USER_RW, spk_var_show, spk_var_store);
static struct kobj_attribute bell_pos_attribute =
	__ATTR(bell_pos, USER_RW, spk_var_show, spk_var_store);
static struct kobj_attribute bleep_time_attribute =
	__ATTR(bleep_time, USER_RW, spk_var_show, spk_var_store);
static struct kobj_attribute bleeps_attribute =
	__ATTR(bleeps, USER_RW, spk_var_show, spk_var_store);
static struct kobj_attribute cursor_time_attribute =
	__ATTR(cursor_time, USER_RW, spk_var_show, spk_var_store);
static struct kobj_attribute key_echo_attribute =
	__ATTR(key_echo, USER_RW, spk_var_show, spk_var_store);
static struct kobj_attribute no_interrupt_attribute =
	__ATTR(no_interrupt, USER_RW, spk_var_show, spk_var_store);
static struct kobj_attribute punc_level_attribute =
	__ATTR(punc_level, USER_RW, spk_var_show, spk_var_store);
static struct kobj_attribute reading_punc_attribute =
	__ATTR(reading_punc, USER_RW, spk_var_show, spk_var_store);
static struct kobj_attribute say_control_attribute =
	__ATTR(say_control, USER_RW, spk_var_show, spk_var_store);
static struct kobj_attribute say_word_ctl_attribute =
	__ATTR(say_word_ctl, USER_RW, spk_var_show, spk_var_store);
static struct kobj_attribute spell_delay_attribute =
	__ATTR(spell_delay, USER_RW, spk_var_show, spk_var_store);

/*
 * These attributes are i18n related.
 */
static struct kobj_attribute characters_attribute =
	__ATTR(characters, USER_RW, chars_chartab_show, chars_store);
static struct kobj_attribute chartab_attribute =
	__ATTR(chartab, USER_RW, chars_chartab_show, chartab_store);
static struct kobj_attribute ctl_keys_message_attribute =
	__ATTR(ctl_keys_message, USER_RW, message_show, message_store);
static struct kobj_attribute colors_message_attribute =
	__ATTR(colors_message, USER_RW, message_show, message_store);
static struct kobj_attribute fancy_message_attribute =
	__ATTR(fancy_message, USER_RW, message_show, message_store);
static struct kobj_attribute funcnames_message_attribute =
	__ATTR(funcnames_message, USER_RW, message_show, message_store);
static struct kobj_attribute keynames_message_attribute =
	__ATTR(keynames_message, USER_RW, message_show, message_store);
static struct kobj_attribute misc_message_attribute =
	__ATTR(misc_message, USER_RW, message_show, message_store);
static struct kobj_attribute states_message_attribute =
	__ATTR(states_message, USER_RW, message_show, message_store);

/*
 * Create groups of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *main_attrs[] = {
	&keymap_attribute.attr,
	&silent_attribute.attr,
	&synth_attribute.attr,
	&synth_direct_attribute.attr,
	&version_attribute.attr,
	&delimiters_attribute.attr,
	&ex_num_attribute.attr,
	&punc_all_attribute.attr,
	&punc_most_attribute.attr,
	&punc_some_attribute.attr,
	&repeats_attribute.attr,
	&attrib_bleep_attribute.attr,
	&bell_pos_attribute.attr,
	&bleep_time_attribute.attr,
	&bleeps_attribute.attr,
	&cursor_time_attribute.attr,
	&key_echo_attribute.attr,
	&no_interrupt_attribute.attr,
	&punc_level_attribute.attr,
	&reading_punc_attribute.attr,
	&say_control_attribute.attr,
	&say_word_ctl_attribute.attr,
	&spell_delay_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute *i18n_attrs[] = {
	&characters_attribute.attr,
	&chartab_attribute.attr,
	&ctl_keys_message_attribute.attr,
	&colors_message_attribute.attr,
	&fancy_message_attribute.attr,
	&funcnames_message_attribute.attr,
	&keynames_message_attribute.attr,
	&misc_message_attribute.attr,
	&states_message_attribute.attr,
	NULL,
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory.  If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
static struct attribute_group main_attr_group = {
	.attrs = main_attrs,
};

static struct attribute_group i18n_attr_group = {
	.attrs = i18n_attrs,
	.name = "i18n",
};

static struct kobject *accessibility_kobj;
struct kobject *speakup_kobj;

int speakup_kobj_init(void)
{
	int retval;

	/*
	 * Create a simple kobject with the name of "accessibility",
	 * located under /sys/
	 *
	 * As this is a simple directory, no uevent will be sent to
	 * userspace.  That is why this function should not be used for
	 * any type of dynamic kobjects, where the name and number are
	 * not known ahead of time.
	 */
	accessibility_kobj = kobject_create_and_add("accessibility", NULL);
	if (!accessibility_kobj)
		return -ENOMEM;

	speakup_kobj = kobject_create_and_add("speakup", accessibility_kobj);
	if (!speakup_kobj) {
		kobject_put(accessibility_kobj);
		return -ENOMEM;
	}

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(speakup_kobj, &main_attr_group);
	if (retval)
		speakup_kobj_exit();

	retval = sysfs_create_group(speakup_kobj, &i18n_attr_group);
	if (retval)
		speakup_kobj_exit();

	return retval;
}

void speakup_kobj_exit(void)
{
	kobject_put(speakup_kobj);
	kobject_put(accessibility_kobj);
}
