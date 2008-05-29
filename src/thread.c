#include <linux/delay.h>
#include <linux/kthread.h>

#include "spk_types.h"
#include "speakup.h"
#include "spk_priv.h"

int speakup_thread(void *data)
{
	while ( ! kthread_should_stop()) {
		if (speakup_info.buff_in != speakup_info.buff_out)
			synth_catch_up((unsigned long ) 0);
		msleep_interruptible(50);
	}
	return 0;
}
