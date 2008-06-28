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
		DEFINE_WAIT(wait);
		while(1) {
			spk_lock(flags);
			prepare_to_wait(&speakup_event, &wait, TASK_INTERRUPTIBLE);
			if (kthread_should_stop() ||
				speakup_info.flushing ||
				(synth && synth->catch_up && !synth_buffer_empty())) {
				spk_unlock(flags);
				break;
			}
			spk_unlock(flags);
			if (signal_pending(current))
				break;
			schedule();
		}
		finish_wait(&speakup_event, &wait);

		spk_lock(flags);
		if (speakup_info.flushing) {
			speakup_info.flushing = 0;
			if (speakup_info.alive)
				spk_unlock(flags);
				synth->flush(synth);
			else
				spk_unlock(flags);
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
