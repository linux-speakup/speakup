#include <linux/console.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h> /* for in_atomic */
#include <linux/types.h>
#include <linux/wait.h>

#include "speakup.h"
#include "spk_priv.h"

#define synthBufferSize 8192	/* currently 8K bytes */

static u_char synth_buffer[synthBufferSize];	/* guess what this is for! */
static u_char *buff_in = synth_buffer;
static u_char *buff_out = synth_buffer;
static u_char *buffer_end = synth_buffer+synthBufferSize-1;

void speakup_start_ttys(void)
{
	int i;

	if (!in_atomic())
		lock_kernel();
	else if (!current->lock_depth)
		return;
	for (i = 0; i < MAX_NR_CONSOLES; i++) {
		if (speakup_console[i] && speakup_console[i]->tty_stopped)
			continue;
		if ((vc_cons[i].d != NULL) && (vc_cons[i].d->vc_tty != NULL))
			start_tty(vc_cons[i].d->vc_tty);
	}
	if (!in_atomic())
		unlock_kernel();
	return;
}

static void speakup_stop_ttys(void)
{
	int i;

	if (!in_atomic())
		lock_kernel();
	else if (!current->lock_depth)
		return;
	for (i = 0; i < MAX_NR_CONSOLES; i++)
		if ((vc_cons[i].d != NULL) && (vc_cons[i].d->vc_tty != NULL))
			stop_tty(vc_cons[i].d->vc_tty);
	if (!in_atomic())
		unlock_kernel();
	return;
}

static int synth_buffer_free(void)
{
	int bytesFree;

	if (buff_in >= buff_out)
		bytesFree = synthBufferSize - (buff_in - buff_out);
	else
		bytesFree = buff_out - buff_in;
	return bytesFree;
}

int synth_buffer_empty(void)
{
	return (buff_in == buff_out);
}
EXPORT_SYMBOL_GPL(synth_buffer_empty);

void synth_buffer_add(char ch)
{
	if (synth_buffer_free() <= 100) {
		synth_start();
		speakup_stop_ttys();
	}
	*buff_in++ = ch;
	if (buff_in > buffer_end)
		buff_in = synth_buffer;
}

char synth_buffer_getc(void)
{
	char ch;

	if (buff_out == buff_in)
		return 0;
	ch = *buff_out++;
	if (buff_out > buffer_end)
		buff_out = synth_buffer;
	return ch;
}
EXPORT_SYMBOL_GPL(synth_buffer_getc);

void synth_buffer_clear(void)
{
	buff_in = buff_out = synth_buffer;
	return;
}

