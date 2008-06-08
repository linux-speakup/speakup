#include <linux/interrupt.h> /* for in_atomic */
#include <linux/types.h>
#include <linux/wait.h>

#include "speakup.h"
#include "spk_priv.h"

#define synthBufferSize 8192	/* currently 8K bytes */

static int synth_buffer_free(void);

extern wait_queue_head_t synth_sleeping_list;

static u_char synth_buffer[synthBufferSize];	/* guess what this is for! */
static u_char *buff_in = synth_buffer;
static u_char *buff_out = synth_buffer;
static u_char *buffer_end = synth_buffer+synthBufferSize-1;

void synth_buffer_add(char ch)
{
	if (synth_buffer_free() <= 100) {
		synth_start();
		/* Sleep if we can, otherwise drop the character. */
		if (!in_atomic())
			interruptible_sleep_on(&synth_sleeping_list);
		else
			return;
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

static int synth_buffer_free(void)
{
	int bytesFree;

	if (buff_in > buff_out)
		bytesFree = synthBufferSize - (buff_in - buff_out);
	else if (buff_in < buff_out)
		bytesFree = buff_out - buff_in;
	else
		bytesFree = synthBufferSize;
	return bytesFree;
}

int synth_buffer_empty(void)
{
	return (buff_in == buff_out);
}
EXPORT_SYMBOL_GPL(synth_buffer_empty);

void synth_buffer_clear(void)
{
	buff_in = buff_out = synth_buffer;
	return;
}

