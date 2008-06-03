#include <linux/kthread.h>
#include <linux/wait.h>

#include "spk_types.h"
#include "speakup.h"
#include "spk_priv.h"

wait_queue_head_t speakup_event;

int speakup_thread(void *data)
{
	init_waitqueue_head(&speakup_event);
	while ( ! kthread_should_stop()) {
		wait_event_interruptible(speakup_event,
			(kthread_should_stop() || ! synth_buffer_empty()));
		if (! synth_buffer_empty())
			synth_catch_up((unsigned long ) 0);
	}
	return 0;
}
