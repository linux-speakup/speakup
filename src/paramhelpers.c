#include <linux/console.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/ctype.h>

#include "spk_types.h"
#include "spk_priv.h"
#include "speakup.h"

static int set_keymap(const char *val, struct kernel_param *kp);
static int get_keymap(char *buffer, struct kernel_param *kp);

/*
 * The first thing we do is define the parameters.
 */
module_param_call(keymap, set_keymap, get_keymap, NULL, 0644);

char *strlwr(char *s)
{
	char *p;
	if (s == NULL)
		return NULL;

	for (p = s; *p; p++)
		*p = tolower(*p);
	return s;
}

char *speakup_s2i(char *start, int *dest)
{
	int val;
	char ch = *start;
	if (ch == '-' || ch == '+')
		start++;
	if (*start < '0' || *start > '9')
		return start;
	val = (*start) - '0';
	start++;
	while (*start >= '0' && *start <= '9') {
		val *= 10;
		val += (*start) - '0';
		start++;
	}
	if (ch == '-')
		*dest = -val;
	else
		*dest = val;
	return start;
}

char *s2uchar(char *start, char *dest)
{
	int val = 0;
	while (*start && *start <= SPACE)
		start++;
	while (*start >= '0' && *start <= '9') {
		val *= 10;
		val += (*start) - '0';
		start++;
	}
	if (*start == ',')
		start++;
	*dest = (u_char)val;
	return start;
}

char *xlate(char *s)
{
	static const char finds[] = "nrtvafe";
	static const char subs[] = "\n\r\t\013\001\014\033";
	static const char hx[] = "0123456789abcdefABCDEF";
	char *p = s, *p1, *p2, c;
	int num;
	while ((p = strchr(p, '\\'))) {
		p1 = p+1;
		p2 = strchr(finds, *p1);
		if (p2) {
			*p++ = subs[p2-finds];
			p1++;
		} else if (*p1 >= '0' && *p1 <= '7') {
			num = (*p1++)&7;
			while (num < 256 && *p1 >= '0' && *p1 <= '7') {
				num <<= 3;
				num = (*p1++)&7;
			}
			*p++ = num;
		} else if (*p1 == 'x' &&
				strchr(hx, p1[1]) && strchr(hx, p1[2])) {
			p1++;
			c = *p1++;
			if (c > '9')
				c = (c - '7') & 0x0f;
			else
				c -= '0';
			num = c << 4;
			c = *p1++;
			if (c > '9')
				c = (c-'7')&0x0f;
			else
				c -= '0';
			num += c;
			*p++ = num;
		} else
			*p++ = *p1++;
		p2 = p;
		while (*p1)
			*p2++ = *p1++;
		*p2 = '\0';
	}
	return s;
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
		pr_warn("i %d %d %d %d\n", i,
				(int)cp1[-3], (int)cp1[-2], (int)cp1[-1]);
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
		pr_warn("end %d %d %d %d\n", i,
				(int)cp1[-3], (int)cp1[-2], (int)cp1[-1]);
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
	cp += sprintf(cp, "0, %d", KEY_MAP_VER);
	return (int)(cp-buffer);
}
