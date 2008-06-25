#include <linux/kthread.h>
#include <linux/wait.h>

#include "spk_types.h"
#include "speakup.h"
#include "spk_priv.h"

DECLARE_WAIT_QUEUE_HEAD(speakup_event);

int speakup_thread(void *data)
{
	unsigned long flags;

	while ( ! kthread_should_stop()) {
		wait_event_interruptible(speakup_event,
			(kthread_should_stop() ||
			speakup_info.flushing ||
			 (synth && synth->catch_up && !synth_buffer_empty())));

		if (speakup_info.flushing) {
			spk_lock(flags);
			if (speakup_info.alive)
				synth->flush(synth);
			spk_unlock(flags);
			speakup_info.flushing = 0;
		}
		if (synth && synth->catch_up && !synth_buffer_empty()) {
			/* It is up to the callee to take the lock, so that it
			 * can sleep whenever it likes */
			synth->catch_up(synth, 0);
		}

		speakup_start_ttys();
	}
	return 0;
}
