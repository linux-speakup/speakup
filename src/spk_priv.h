/* spk_priv.h
   review functions for the speakup screen review package.
   originally written by: Kirk Reiser and Andy Berdan.

  extensively modified by David Borowski.

    Copyright (C) 1998  Kirk Reiser.
    Copyright (C) 2003  David Borowski.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef _SPEAKUP_PRIVATE_H
#define _SPEAKUP_PRIVATE_H

#include "spk_types.h"
#include "spk_priv_keyinfo.h"

#ifndef pr_warn
#define pr_warn(fmt, arg...) printk(KERN_WARNING fmt, ##arg)
#endif

#define V_LAST_STRING { -1, 0 }
#define V_LAST_NUM { -1, 0, 0, 0, 0, 0, 0, 0 }
#define SPACE 0x20
#define SYNTH_CHECK 20030716 /* today's date ought to do for check value */
/* synth flags, for odd synths */
#define SF_DEC 1 /* to fiddle puncs in alpha strings so it doesn't spell */
#ifdef MODULE
#define SYNTH_START 1
#else
#define SYNTH_START 0
#endif

#define KT_SPKUP 15

extern struct serial_state *spk_serial_init(int index);
extern int serial_synth_probe(void);
extern int wait_for_xmitr(void);
extern unsigned char spk_serial_in(void);
extern int spk_serial_out(const char ch);
extern void spk_serial_release(void);
extern const char *spk_synth_immediate(struct spk_synth *synth, const char *buff);
extern void spk_do_catch_up(struct spk_synth *synth, unsigned long data);
extern int spk_synth_is_alive_nop(struct spk_synth *synth);
extern int spk_synth_is_alive_restart(struct spk_synth *synth);
extern void synth_delay(int ms);
extern void synth_stop_timer(void);
extern int synth_done(void);
extern void synth_printf(const char *buf, ...);
extern int synth_request_region(u_long, u_long);
extern int synth_release_region(u_long, u_long);
extern int synth_add(struct spk_synth *in_synth);
extern void synth_remove(struct spk_synth *in_synth);

extern struct speakup_info_t speakup_info;

/* Protect the whole speakup machinery, must be taken at each kernel->speakup
 * transition and released at all corresponding speakup->kernel transitions
 * (flags must be the same variable between lock/trylock and unlock). */
#if 0
/* Speakup needs to disable the keyboard IRQ, hence _irqsave/restore */
#define spk_lock(flags) spin_lock_irqsave(&speakup_info.spinlock, flags)
#define spk_trylock(flags) spin_trylock_irqsave(&speakup_info.spinlock, flags)
#define spk_unlock(flags) spin_unlock_irqrestore(&speakup_info.spinlock, flags)
#else
/* For now, we can't just disable IRQs because Speakup may have to wait for the
 * completion of the talk, so for now just be IRQ and SMP -unsafe (but at least
 * preempt-safe).  In the future we should probably just run a kernel thread. */
#define spk_lock(flags) do { (void) flags; preempt_disable(); } while (0)
#define spk_trylock(flags) ({ (void) flags; preempt_disable(); 1; })
#define spk_unlock(flags) do { (void) flags; preempt_enable(); } while (0)
#endif

#endif
