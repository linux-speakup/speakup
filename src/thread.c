#include <linux/kthread.h>
#include <linux/wait.h>

#include "spk_types.h"
#include "speakup.h"
#include "spk_priv.h"

DECLARE_WAIT_QUEUE_HEAD(speakup_event);

int speakup_thread(void *data)
{
	while ( ! kthread_should_stop()) {
		wait_event_interruptible(speakup_event,
			(kthread_should_stop() ||
			 (synth->catch_up && !synth_buffer_empty())));

		if (synth->catch_up && !synth_buffer_empty())
			/* It is up to the callee to take the lock, so that it
			 * can sleep whenever it likes */
			synth->catch_up(synth, 0);

		speakup_start_ttys();
	}
	return 0;
}
