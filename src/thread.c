#include <linux/kthread.h>
#include <linux/wait.h>

#include "spk_types.h"
#include "speakup.h"
#include "spk_priv.h"

wait_queue_head_t speakup_event;

int speakup_thread(void *data)
{
	int i;
	struct st_spk_t *first_console = kzalloc(sizeof(*first_console),
		GFP_KERNEL);

	spin_lock_init(&speakup_info.spinlock);
	speakup_open(vc_cons[fg_console].d, first_console);
	for (i = 0; vc_cons[i].d; i++)
		speakup_allocate(vc_cons[i].d);
	speakup_dev_init(synth_name);
	init_waitqueue_head(&speakup_event);
	while ( ! kthread_should_stop()) {
		wait_event_interruptible(speakup_event,
			(kthread_should_stop() || ! synth_buffer_empty()));
		if (! synth_buffer_empty())
			synth_catch_up((unsigned long ) 0);
	}
	return 0;
}
