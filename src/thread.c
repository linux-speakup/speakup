#include <linux/kthread.h>
#include <linux/wait.h>

#include "spk_types.h"
#include "speakup.h"
#include "spk_priv.h"

DECLARE_WAIT_QUEUE_HEAD(speakup_event);
EXPORT_SYMBOL_GPL(speakup_event);

int speakup_thread(void *data)
{
	unsigned long flags;
	int should_break;

	while ( ! kthread_should_stop()) {
		DEFINE_WAIT(wait);
		while(1) {
			spk_lock(flags);
			prepare_to_wait(&speakup_event, &wait, TASK_INTERRUPTIBLE);
			should_break = (kthread_should_stop() ||
				speakup_info.flushing ||
				(synth && synth->catch_up && !synth_buffer_empty()));
			spk_unlock(flags);
			if (should_break || signal_pending(current))
				break;
			schedule();
		}
		finish_wait(&speakup_event, &wait);

		if (synth && synth->catch_up && speakup_info.alive) {
			/* It is up to the callee to take the lock, so that it
			 * can sleep whenever it likes */
			synth->catch_up(synth);
		}

		speakup_start_ttys();
	}
	return 0;
}
