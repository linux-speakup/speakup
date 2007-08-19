/*
 *  speakup glue layer
 *
 *  Copyright (C) 2006  Daniel Drake <dsd@gentoo.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/vt_kern.h>
#include <linux/spkglue.h>

static const struct spkglue_funcs *funcs = NULL;

void spkglue_allocate(struct vc_data *vc)
{
	if (funcs && funcs->allocate)
		funcs->allocate(vc);
}

int spkglue_key(struct vc_data *vc, int shift_state, int keycode,
		unsigned short keysym, int up_flag)
{
	if (funcs && funcs->key)
		return funcs->key(vc, shift_state, keycode, keysym, up_flag);
	else
		return 0;
}

void spkglue_bs(struct vc_data *vc)
{
	if (funcs && funcs->bs)
		funcs->bs(vc);
}

void spkglue_con_write(struct vc_data *vc, const char *str, int len)
{
	if (funcs && funcs->con_write)
		funcs->con_write(vc, str, len);
}

void spkglue_con_update(struct vc_data *vc)
{
	if (funcs && funcs->con_update)
		funcs->con_update(vc);
}

void spkglue_register(const char *name, const struct spkglue_funcs *_funcs)
{
	printk(KERN_INFO "spkglue: registered '%s' provider\n", name);
	funcs = _funcs;
}
EXPORT_SYMBOL_GPL(spkglue_register);

void spkglue_unregister(void)
{
	printk(KERN_INFO "spkglue: unregistered\n");
	funcs = NULL;
}
EXPORT_SYMBOL_GPL(spkglue_unregister);

