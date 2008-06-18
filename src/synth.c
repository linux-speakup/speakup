#include <linux/types.h>
#include <linux/ctype.h>	/* for isdigit() and friends */
#include <linux/fs.h>
#include <linux/mm.h>		/* for verify_area */
#include <linux/errno.h>	/* for -EBUSY */
#include <linux/ioport.h>	/* for check_region, request_region */
#include <linux/interrupt.h>
#include <linux/delay.h>	/* for loops_per_sec */
#include <linux/wait.h>		/* for wait_queue */
#include <linux/kmod.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h> /* for copy_from_user */

#include "spk_priv.h"
#include "speakup.h"
#include "serialio.h"

#define MAXSYNTHS       16      /* Max number of synths in array. */
static struct spk_synth *synths[MAXSYNTHS];
struct spk_synth *synth = NULL;
char pitch_buff[32] = "";
static int module_status;
int quiet_boot;

short synth_trigger_time = 50;
struct speakup_info_t speakup_info = {
	.spinlock = SPIN_LOCK_UNLOCKED,
	.delay_time = 500,
	.jiffy_delta = 50,
	.full_time = 1000,
};
EXPORT_SYMBOL_GPL(speakup_info);

static int do_synth_init(struct spk_synth *in_synth);

int serial_synth_probe(struct spk_synth *synth)
{
	struct serial_state *ser;
	int failed = 0;

	if ((synth->ser >= SPK_LO_TTY) && (synth->ser <= SPK_HI_TTY)) {
		ser = spk_serial_init(synth->ser);
		if (ser == NULL) {
			failed = -1;
		} else {
		outb_p(0, ser->port);
		mdelay(1);
		outb_p('\r', ser->port);
	}
	} else {
		failed = -1;
		pr_warn("ttyS%i is an invalid port\n", synth->ser);
	}
	if (failed) {
		pr_info("%s: not found\n", synth->long_name);
		return -ENODEV;
	}
	pr_info("%s: ttyS%i, Driver Version %s\n",
			synth->long_name, synth->ser, synth->version);
	return 0;
}
EXPORT_SYMBOL_GPL(serial_synth_probe);

void spk_do_catch_up(struct spk_synth *synth, unsigned long data)
{
	static u_char ch = 0;
	unsigned long flags;

	spk_lock(flags);
	while (! synth_buffer_empty()) {
		if (! ch)
			ch = synth_buffer_getc();
		if (ch == '\n')
			ch = synth->procspeech;
		if (!spk_serial_out(ch)) {
			spk_unlock(flags);
			msleep(speakup_info.full_time);
			spk_lock(flags);
		}
		ch = 0;
	}
	spk_serial_out(synth->procspeech);
	synth_done();
	spk_unlock(flags);
}
EXPORT_SYMBOL_GPL(spk_do_catch_up);

const char *spk_synth_immediate(struct spk_synth *synth, const char *buff)
{
	u_char ch;
	while ((ch = *buff)) {
		if (ch == '\n')
			ch = synth->procspeech;
		if (wait_for_xmitr())
			outb(ch, speakup_info.port_tts);
		else
			return buff;
		buff++;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(spk_synth_immediate);

void spk_synth_flush(struct spk_synth *synth)
{
	spk_serial_out(synth->clear);
}
EXPORT_SYMBOL_GPL(spk_synth_flush);

int spk_synth_is_alive_nop(struct spk_synth *synth)
{
	speakup_info.alive = 1;
	return 1;
}
EXPORT_SYMBOL_GPL(spk_synth_is_alive_nop);

int spk_synth_is_alive_restart(struct spk_synth *synth)
{
	if (speakup_info.alive)
		return 1;
	if (!speakup_info.alive && wait_for_xmitr() > 0) {
		/* restart */
		speakup_info.alive = 1;
		synth_printf("%s", synth->init);
		return 2; /* reenabled */
	}
	pr_warn("%s: can't restart synth\n", synth->long_name);
	return 0;
}
EXPORT_SYMBOL_GPL(spk_synth_is_alive_restart);

static void thread_wake_up(u_long data)
{
	wake_up_interruptible(&speakup_event);
}

static DEFINE_TIMER(thread_timer, thread_wake_up, 0, 0);

void synth_done(void)
{
	synth_buffer_clear();
	speakup_start_ttys();
	return;
}
EXPORT_SYMBOL_GPL(synth_done);

void synth_start(void)
{
	if (!speakup_info.alive)
		synth_done();
	else if (synth->start)
		synth->start();
	if (!timer_pending(&thread_timer))
		mod_timer(&thread_timer, jiffies + (HZ * synth_trigger_time) / 1000);
}

void do_flush(void)
{
	do_flush_flag = 1;
	synth_buffer_clear();
	if (speakup_info.alive) {
		if (pitch_shift) {
			synth_printf("%s", pitch_buff);
			pitch_shift = 0;
		}
	}
	speakup_start_ttys();
	wake_up_interruptible(&speakup_event);
}

void synth_write(const char *buf, size_t count)
{
	while (count--)
		synth_buffer_add(*buf++);
	synth_start();
}

void synth_printf(const char *fmt, ...)
{
	va_list args;
	unsigned char buf[80], *p;
	int r;

	va_start(args, fmt);
	r = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if (r > sizeof(buf) - 1)
		r = sizeof(buf) - 1;

	p = buf;
	while (r--)
		synth_buffer_add(*p++);
	synth_start();
}
EXPORT_SYMBOL_GPL(synth_printf);

static int index_count = 0;
static int sentence_count = 0;

void reset_index_count(int sc)
{
	static int first = 1;
	if (first)
		first = 0;
	else
		synth->get_index();
	index_count = 0;
	sentence_count = sc;
}

int synth_supports_indexing(void)
{
	if (synth->get_index != NULL)
		return 1;
	return 0;
}

void synth_insert_next_index(int sent_num)
{
	int out;
	if (speakup_info.alive) {
		if (sent_num == 0) {
			synth->indexing.currindex++;
			index_count++;
			if (synth->indexing.currindex >
					synth->indexing.highindex)
				synth->indexing.currindex =
					synth->indexing.lowindex;
		}

		out = synth->indexing.currindex * 10 + sent_num;
		synth_printf(synth->indexing.command, out, out);
	}
}

void get_index_count(int *linecount, int *sentcount)
{
	int ind = synth->get_index();
	if (ind) {
		sentence_count = ind % 10;

		if ((ind / 10) <= synth->indexing.currindex)
			index_count = synth->indexing.currindex-(ind/10);
		else
			index_count = synth->indexing.currindex-synth->indexing.lowindex
				+ synth->indexing.highindex-(ind/10)+1;

	}
	*sentcount = sentence_count;
	*linecount = index_count;
}

static struct resource synth_res;

int synth_request_region(unsigned long start, unsigned long n)
{
	struct resource *parent = &ioport_resource;
	memset(&synth_res, 0, sizeof(synth_res));
	synth_res.name = synth->name;
	synth_res.start = start;
	synth_res.end = start + n - 1;
	synth_res.flags = IORESOURCE_BUSY;
	return request_resource(parent, &synth_res);
}
EXPORT_SYMBOL_GPL(synth_request_region);

int synth_release_region(unsigned long start, unsigned long n)
{
	return release_resource(&synth_res);
}
EXPORT_SYMBOL_GPL(synth_release_region);

static struct st_num_var synth_time_vars[] = {
	{ DELAY, 0, 100, 100, 2000, 0, 0, 0 },
	{ TRIGGER, 0, 20, 10, 200, 0, 0, 0 },
	{ JIFFY, 0, 50, 20, 200, 0, 0, 0 },
	{ FULL, 0, 400, 200, 10000, 0, 0, 0 },
	V_LAST_NUM
};

/* called by: speakup_init() */
int synth_init(char *synth_name)
{
	int i;
	int ret = 0;
	struct spk_synth *synth = NULL;

	if (synth_name == NULL)
		return 0;

	if (strcmp(synth_name, "none") == 0) {
		mutex_lock(&spk_mutex);
		synth_release();
		mutex_unlock(&spk_mutex);
		return 0;
	}

	mutex_lock(&spk_mutex);
	/* First, check if we already have it loaded. */
	for (i = 0; synths[i] != NULL && i < MAXSYNTHS; i++)
		if (strcmp(synths[i]->name, synth_name) == 0)
			synth = synths[i];

	/* If we got one, initialize it now. */
	if (synth)
		ret = do_synth_init(synth);
	mutex_unlock(&spk_mutex);

	return ret;
}

/* called by: synth_add() */
static int do_synth_init(struct spk_synth *in_synth)
{
	struct st_num_var *n_var;
	struct st_string_var *s_var;

	synth_release();
	if (in_synth->checkval != SYNTH_CHECK)
		return -EINVAL;
	synth = in_synth;
	pr_warn("synth probe\n");
	if (synth->probe(synth) < 0) {
		pr_warn("%s: device probe failed\n", in_synth->name);
		synth = NULL;
		return -ENODEV;
	}
	synth_time_vars[0].default_val = synth->delay;
	synth_time_vars[1].default_val = synth->trigger;
	synth_time_vars[2].default_val = synth->jiffies;
	synth_time_vars[3].default_val = synth->full;
	for (n_var = synth_time_vars; n_var->var_id >= 0; n_var++)
		speakup_register_var(n_var);
	speakup_info.alive = 1;
	synth_printf("%s", synth->init);
	for (s_var = synth->string_vars; s_var->var_id >= 0; s_var++)
		speakup_register_var((struct st_num_var *) s_var);
	for (n_var = synth->num_vars; n_var->var_id >= 0; n_var++)
		speakup_register_var(n_var);
	if (!quiet_boot)
		synth_printf("%s found\n", synth->long_name);
	synth_flags = synth->flags;
	wake_up_interruptible(&speakup_event);
	return 0;
}

void synth_release(void)
{
	struct st_num_var *n_var;
	struct st_string_var *s_var;
	if (synth == NULL)
		return;
	pr_info("releasing synth %s\n", synth->name);
	for (s_var = synth->string_vars; s_var->var_id >= 0; s_var++)
		speakup_unregister_var(s_var->var_id);
	for (n_var = synth_time_vars; n_var->var_id >= 0; n_var++)
		speakup_unregister_var(n_var->var_id);
	for (n_var = synth->num_vars; n_var->var_id >= 0; n_var++)
		speakup_unregister_var(n_var->var_id);
	stop_serial_interrupt();
	synth->release();
	synth = NULL;
}

/* called by: all_driver_init() */
int synth_add(struct spk_synth *in_synth)
{
	int i;
	int status = 0;
	mutex_lock(&spk_mutex);
	for (i = 0; synths[i] != NULL && i < MAXSYNTHS; i++)
		/* synth_remove() is responsible for rotating the array down */
		if (in_synth == synths[i]) {
			mutex_unlock(&spk_mutex);
			return 0;
		}
	if (i == MAXSYNTHS) {
		pr_warn("Error: attempting to add a synth past end of array\n");
		mutex_unlock(&spk_mutex);
		return -1;
	}
	synths[i++] = in_synth;
	synths[i] = NULL;
	if (in_synth->startup)
		status = do_synth_init(in_synth);
	mutex_unlock(&spk_mutex);
	return status;
}
EXPORT_SYMBOL_GPL(synth_add);

void synth_remove(struct spk_synth *in_synth)
{
	int i;
	mutex_lock(&spk_mutex);
	if (synth == in_synth)
		synth_release();
	for (i = 0; synths[i] != NULL; i++) {
		if (in_synth == synths[i])
			break;
	}
	for ( ; synths[i] != NULL; i++) /* compress table */
		synths[i] = synths[i+1];
	module_status = 0;
	mutex_unlock(&spk_mutex);
}
EXPORT_SYMBOL_GPL(synth_remove);

short punc_masks[] = { 0, SOME, MOST, PUNC, PUNC|B_SYM };
