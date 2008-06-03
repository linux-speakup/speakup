#include <linux/delay.h>
#include <linux/kthread.h>

#include "spk_types.h"
#include "speakup.h"
#include "spk_priv.h"

int speakup_thread(void *data)
{
	while ( ! kthread_should_stop()) {
		msleep_interruptible(50);
		if (! synth_buffer_empty())
			synth_catch_up((unsigned long ) 0);
	}
	return 0;
}