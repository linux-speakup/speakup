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

#ifndef __SPKGLUE_H__
#define __SPKGLUE_H__

struct spkglue_funcs {
	void (*allocate)(struct vc_data *);
	int (*key)(struct vc_data *, int, int, unsigned short, int);
	void (*bs)(struct vc_data *);
	void (*con_write)(struct vc_data *, const char *, int);
	void (*con_update)(struct vc_data *);
};

void spkglue_register(const char *name, const struct spkglue_funcs *_funcs);
void spkglue_unregister(void);

#ifdef CONFIG_SPEAKUP

int spkglue_key(struct vc_data *vc, int shift_state, int keycode,
		u_short keysym, int up_flag);
void spkglue_allocate(struct vc_data *vc);
void spkglue_bs(struct vc_data *vc);
void spkglue_con_write(struct vc_data *vc, const char *str, int len);
void spkglue_con_update(struct vc_data *vc);

#else

static inline int spkglue_key(struct vc_data *vc, int shift_state, int keycode,
			      u_short keysym, int up_flag)
{
	return 0;
}

static inline void spkglue_allocate(struct vc_data *vc) { }
static inline void spkglue_bs(struct vc_data *vc) { }
static inline void spkglue_con_write(struct vc_data *vc, const char *str,
				     int len) { }
static inline void spkglue_con_update(struct vc_data *vc) { }

#endif

#endif

