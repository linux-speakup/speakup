#define KERNEL
#include <linux/version.h>
#include <linux/types.h>
#include <linux/ctype.h>	/* for isdigit() and friends */
#include <linux/fs.h>
#include <linux/mm.h>		/* for verify_area */
#include <linux/errno.h>	/* for -EBUSY */
#include <linux/ioport.h>	/* for check_region, request_region */
#include <linux/interrupt.h>
#include <linux/delay.h>	/* for loops_per_sec */
#include <linux/wait.h>		/* for wait_queue */
#include <linux/miscdevice.h>	/* for misc_register, and SYNTH_MINOR */
#include <linux/kmod.h>
#include <linux/jiffies.h>

#ifdef __powerpc__
#include <asm-ppc/pc_serial.h> /* for SERIAL_PORT_DFNS */
#endif

#include "spk_priv.h"
#include "serialio.h"

static struct serial_state rs_table[] = {
	SERIAL_PORT_DFNS
};

static struct spk_synth *synths[16];

#define synthBufferSize 8192	/* currently 8K bytes */
struct spk_synth *synth = NULL;
int synth_port_tts = 0, synth_port_forced = 0;
static int synth_timer_active = 0;	/* indicates when a timer is set */
	static struct miscdevice synth_device;
static int misc_registered = 0;
static char pitch_buff[32] = "";
declare_sleeper(synth_sleeping_list);
static int module_status = 0;
static declare_timer(synth_timer);
short synth_delay_time = 500, synth_trigger_time = 50;
short synth_jiffy_delta = 50, synth_full_time = 1000;
int synth_alive = 0, quiet_boot = 0;
u_char synth_buffer[synthBufferSize];	/* guess what this is for! */
static u_char *buffer_highwater = synth_buffer+synthBufferSize-100;
u_char *buffer_end = synth_buffer+synthBufferSize-1;
volatile u_char *synth_buff_in = synth_buffer, *synth_buff_out = synth_buffer;
static irqreturn_t synth_readbuf_handler(int irq, void *dev_id);
static struct serial_state *serstate;

static void speakup_unregister_var(short var_id);
static void start_serial_interrupt(int irq);
static void speakup_register_devsynth(void);
static int do_synth_init(struct spk_synth *in_synth);

static char *xlate(char *s)
{
	static const char finds[] = "nrtvafe";
	static const char subs[] = "\n\r\t\013\001\014\033";
	static const char hx[] = "0123456789abcdefABCDEF";
	char *p = s, *p1, *p2, c;
	int num;
	while ((p = strchr(p, '\\'))) {
		p1 = p+1;
		p2 = strchr(finds, *p1);
		if (p2) {
			*p++ = subs[p2-finds];
			p1++;
		} else if (*p1 >= '0' && *p1 <= '7') {
			num = (*p1++)&7;
			while (num < 256 && *p1 >= '0' && *p1 <= '7') {
				num <<= 3;
				num = (*p1++)&7;
			}
			*p++ = num;
		} else if (*p1 == 'x'&& strchr(hx, p1[1]) && strchr(hx, p1[2])) {
			p1++;
			c = *p1++;
			if (c > '9')
				c = (c-'7')&0x0f;
			else
				c -= '0';
			num = c<<4;
			c = *p1++;
			if (c > '9')
				c = (c-'7')&0x0f;
			else
				c -= '0';
			num += c;
			*p++ = num;
		} else
			*p++ = *p1++;
		p2 = p;
		while (*p1) *p2++ = *p1++;
		*p2 = '\0';
	}
	return s;
}

struct serial_state *spk_serial_init(int index)
{
	int baud = 9600, quot = 0;
	unsigned int cval = 0;
	int i, cflag = CREAD | HUPCL | CLOCAL | B9600 | CS8;
	struct serial_state *ser = NULL;

	if (synth_port_forced) {
		if (index > 0) return NULL;
		pr_info("probe forced to 0x%x by kernel command line\n",
			synth_port_forced);
		for (i=0; i <= SPK_HI_TTY; i++)
			if ((rs_table+i)->port == synth_port_forced) {
				ser = rs_table+i;
				break;
			}
	} else ser = rs_table + index;
	/*	Divisor, bytesize and parity */
	quot = ser->baud_base / baud;
	cval = cflag & (CSIZE | CSTOPB);
#if defined(__powerpc__) || defined(__alpha__)
	cval >>= 8;
#else /* !__powerpc__ && !__alpha__ */
	cval >>= 4;
#endif /* !__powerpc__ && !__alpha__ */
	if (cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(cflag & PARODD))
		cval |= UART_LCR_EPAR;
	if (synth_request_region(ser->port, 8)) { // try to take it back.
		__release_region(&ioport_resource, ser->port, 8);
		if (synth_request_region(ser->port, 8)) return NULL;
	}

	/*	Disable UART interrupts, set DTR and RTS high
	 *	and set speed. */
	outb(cval | UART_LCR_DLAB, ser->port + UART_LCR);	/* set DLAB */
	outb(quot & 0xff, ser->port + UART_DLL);	/* LS of divisor */
	outb(quot >> 8, ser->port + UART_DLM);		/* MS of divisor */
	outb(cval, ser->port + UART_LCR);		/* reset DLAB */

	// Turn off Interrupts
	outb(0, ser->port + UART_IER);
	outb(UART_MCR_DTR | UART_MCR_RTS, ser->port + UART_MCR);

	/* If we read 0xff from the LSR, there is no UART here. */
	if (inb(ser->port + UART_LSR) == 0xff) {
		synth_release_region(ser->port, 8);
		serstate=NULL;
		return NULL;
	}

	mdelay(1);
	synth_port_tts = ser->port;
	serstate=ser;

	start_serial_interrupt(ser->irq);

	return ser;
}

static void start_serial_interrupt(int irq)
{
	int rv;

	if (synth->read_buff_add == NULL)
		return;

	rv = request_irq(irq, synth_readbuf_handler, SA_SHIRQ,
			 "serial", (void *) synth_readbuf_handler);

	if (rv)
	{
		printk(KERN_ERR "Unable to request Speakup serial I R Q\n");
	}
	// Set MCR
	outb(UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2, synth_port_tts + UART_MCR);
	// Turn on Interrupts
	outb(UART_IER_MSI|UART_IER_RLSI|UART_IER_RDI,synth_port_tts+ UART_IER);
	inb(synth_port_tts+UART_LSR);
	inb(synth_port_tts+UART_RX);
	inb(synth_port_tts+UART_IIR);
	inb(synth_port_tts+UART_MSR);
	outb(1, synth_port_tts + UART_FCR);	/* Turn FIFO On */
}

static void stop_serial_interrupt(void)
{
	if (synth_port_tts == 0) return;

	if (synth->read_buff_add == NULL)
		return;

	// Turn off interrupts
	outb(0,synth_port_tts+UART_IER);
	// Free IRQ
	free_irq(serstate->irq, (void *) synth_readbuf_handler);
}

void spk_serial_release(void)
{
	if (synth_port_tts == 0) return;
	synth_release_region(synth_port_tts, 8);
	synth_port_tts = 0;
}

static irqreturn_t synth_readbuf_handler(int irq, void *dev_id)
{
//printk(KERN_ERR "in irq\n");
//pr_warn("in IRQ\n");
	int c;
	while (inb_p(synth_port_tts + UART_LSR) & UART_LSR_DR)
	{

		c=inb_p(synth_port_tts+UART_RX);
		synth->read_buff_add((u_char) c);
//printk(KERN_ERR "c = %d\n",c);
//pr_warn("C = %d\n",c);
	}
	return IRQ_HANDLED;
}

/* sleep for ms milliseconds */
void
synth_delay(int val)
{
	if (val == 0) return;
	synth_timer.expires = jiffies + val;
	start_timer(synth_timer);
	synth_timer_active++;
}

static void synth_dummy_catchup(unsigned long data)
{
	synth_stop_timer();
	synth_done();
} /* a bogus catchup if no synth */

void
synth_stop_timer(void)
{
	if (synth_timer_active)
		stop_timer(synth_timer);
	synth_timer_active = 0;
}

int synth_done(void)
{
	synth_buff_out = synth_buff_in = synth_buffer;
	if (waitqueue_active(&synth_sleeping_list)) {
		wake_up_interruptible(&synth_sleeping_list);
		return 0;
	}
	return 1;
}

static void synth_start(void)
{
	if (!synth_alive)
		synth_done();
	else if (synth->start)
		synth->start();
	else if (synth_timer_active == 0)
		synth_delay(synth_trigger_time);
}

void do_flush(void)
{
	synth_stop_timer();
	synth_buff_out = synth_buff_in = synth_buffer;
	if (synth_alive) {
		synth->flush();
		if (synth->flush_wait)
			synth_delay((synth->flush_wait * HZ) / 1000);
		if (pitch_shift) {
			synth_write_string(pitch_buff);
			pitch_shift = 0;
		}
	}
	if (waitqueue_active(&synth_sleeping_list))
		wake_up_interruptible(&synth_sleeping_list);
}

void
synth_buffer_add(char ch)
{
	if (synth_buff_in >= buffer_highwater) {
		synth_start();
		if (!waitqueue_active(&synth_sleeping_list))
			interruptible_sleep_on(&synth_sleeping_list);
		if (synth_buff_in >= buffer_end) return;
	}
	*synth_buff_in++ = ch;
}

void
synth_write(const char *buf, size_t count)
{
	while (count--)
		synth_buffer_add(*buf++);
	synth_start();
}

void
synth_putc(char ch)
{
	synth_buffer_add(ch);
	synth_start();
}

void
synth_printf(const char *fmt, ...)
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

void
synth_write_string(const char *buf)
{
	while (*buf)
		synth_buffer_add(*buf++);
	synth_start();
}

void
synth_write_msg(const char *buf)
{
	while (*buf)
		synth_buffer_add(*buf++);
	synth_buffer_add('\n');
	synth_start();
}

static int index_count=0;
static int sentence_count=0;

void
reset_index_count(int sc)
{
	static int first = 1;
	if (first)
		first=0;
	else
		synth->get_index();
	index_count=0;
	sentence_count=sc;
}

int
synth_supports_indexing(void)
{
	if (synth->get_index!=NULL)
		return 1;
	return 0;
}

void
synth_insert_next_index(int sent_num)
{
	int out;
	if (synth_alive) {
		if (sent_num==0)
		{
			synth->indexing.currindex++;
			index_count++;
			if (synth->indexing.currindex>synth->indexing.highindex)
				synth->indexing.currindex=synth->indexing.lowindex;
		}

		out=synth->indexing.currindex*10+sent_num;
		synth_printf(synth->indexing.command,out,out);
	}
}

void
get_index_count(int *linecount,int *sentcount)
{
	int ind=synth->get_index();
	if (ind)
	{
		sentence_count=ind%10;

		if ((ind/10)<=synth->indexing.currindex)
			index_count = synth->indexing.currindex-(ind/10);
		else
			index_count = synth->indexing.currindex-synth->indexing.lowindex + synth->indexing.highindex-(ind/10)+1;

	}
	*sentcount=sentence_count;
	*linecount=index_count;
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

int synth_release_region(unsigned long start, unsigned long n)
{
	return release_resource(&synth_res);
}

#ifdef CONFIG_PROC_FS

// /proc/synth-specific code

#include <asm/uaccess.h>
#include <linux/limits.h>

// this is the write handler for /proc/speakup/synth-specific/direct
static int spk_direct_write_proc(struct file *file, const char *buffer,
				 u_long count, void *data)
{
	u_char buf[256];
	int ret = count, bytes;
	const char *ptr = buffer;
	if (synth == NULL) return -EPERM;
	while (count > 0) {
		bytes = min_t(size_t, count, 250);
		if (copy_from_user(buf, ptr, bytes))
			return -EFAULT;
		buf[bytes] = '\0';
		xlate(buf);
		synth_write_string(buf);
		ptr += bytes;
		count -= bytes;
	}
	return ret;
}

struct st_proc_var synth_direct = { SYNTH_DIRECT, 0, spk_direct_write_proc, 0 };

#endif

static struct st_num_var synth_time_vars[] = {
	{ DELAY, 0, 100, 100, 2000, 0, 0, 0 },
	{ TRIGGER, 0, 20, 10, 200, 0, 0, 0 },
	{ JIFFY, 0, 50, 20, 200, 0, 0, 0 },
	{ FULL, 0, 400, 200, 10000, 0, 0, 0 },
	V_LAST_NUM
};

static struct spk_synth *do_load_synth(const char *synth_name)
{
	int i;

	if (request_module("speakup_%s", synth_name))
		return NULL;

	for (i = 0; synths[i] != NULL; i++) {
		if (strcmp(synths[i]->name, synth_name) == 0)
			return synths[i];
	}

	return NULL;
}

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
	for (i = 0; synths[i] != NULL; i++)
		if (strcmp(synths[i]->name, synth_name) == 0)
			synth = synths[i];

	/* No synth loaded matching this one, try loading it. */
	if (!synth)
		synth = do_load_synth(synth_name);

	/* If we got one, initialize it now. */
	if (synth)
		ret = do_synth_init(synth);
	mutex_unlock(&spk_mutex);

	return ret;
}

static void synth_catch_up(u_long data)
{
	unsigned long flags;
	spk_lock(flags);
	if (synth->catch_up)
		synth->catch_up(data);
	spk_unlock(flags);
}

static int do_synth_init(struct spk_synth *in_synth)
{
	struct st_num_var *n_var;
	struct st_string_var *s_var;
	unsigned long flags;

	synth_release();
	if (in_synth->checkval != SYNTH_CHECK) return -EINVAL;
	synth = in_synth;
	pr_warn("synth probe\n");
	spk_lock(flags);
	if (synth->probe() < 0) {
		spk_unlock(flags);
		pr_warn("%s: device probe failed\n", in_synth->name);
		synth = NULL;
		return -ENODEV;
	}
	spk_unlock(flags);
	synth_time_vars[0].default_val = synth->delay;
	synth_time_vars[1].default_val = synth->trigger;
	synth_time_vars[2].default_val = synth->jiffies;
	synth_time_vars[3].default_val = synth->full;
	synth_timer.function = synth_catch_up;
	synth_timer.entry.prev = NULL;
	init_timer(&synth_timer);
	for (n_var = synth_time_vars; n_var->var_id >= 0; n_var++)
		speakup_register_var(n_var);
	synth_alive = 1;
	synth_write_string(synth->init);
	for (s_var = synth->string_vars; s_var->var_id >= 0; s_var++)
		speakup_register_var((struct st_num_var *) s_var);
	for (n_var = synth->num_vars; n_var->var_id >= 0; n_var++)
		speakup_register_var(n_var);
	if (!quiet_boot)
		synth_printf("%s found\n",synth->long_name);
#ifdef CONFIG_PROC_FS
	speakup_register_var((struct st_num_var *) &synth_direct);
#endif
	synth_flags = synth->flags;
	return 0;
}

void
synth_release(void)
{
	struct st_num_var *n_var;
	struct st_string_var *s_var;
	unsigned long flags;
	if (synth == NULL) return;
	pr_info("releasing synth %s\n", synth->name);
	for (s_var = synth->string_vars; s_var->var_id >= 0; s_var++)
		speakup_unregister_var(s_var->var_id);
	for (n_var = synth_time_vars; n_var->var_id >= 0; n_var++)
		speakup_unregister_var(n_var->var_id);
	for (n_var = synth->num_vars; n_var->var_id >= 0; n_var++)
		speakup_unregister_var(n_var->var_id);
#ifdef CONFIG_PROC_FS
	speakup_unregister_var(SYNTH_DIRECT);
#endif
	synth_dummy_catchup((unsigned long) NULL);
	synth_timer.function = synth_dummy_catchup;
	spk_lock(flags);
	stop_serial_interrupt();
	synth->release();
	spk_unlock(flags);
	synth = NULL;
}

int synth_add(struct spk_synth *in_synth)
{
	int i;
	int status;
	mutex_lock(&spk_mutex);
	status = do_synth_init(in_synth);
	if (status != 0) {
		mutex_unlock(&spk_mutex);
		return status;
	}
	for (i = 0; synths[i] != NULL; i++)
		if (in_synth == synths[i]) {
			mutex_unlock(&spk_mutex);
			return 0;
		}
	BUG_ON(i == ARRAY_SIZE(synths) - 1);
	synths[i++] = in_synth;
	synths[i] = NULL;
	mutex_unlock(&spk_mutex);
	return 0;
}

void synth_remove(struct spk_synth *in_synth)
{
	int i;
	mutex_lock(&spk_mutex);
	if (synth == in_synth)
		synth_release();
	for (i = 0; synths[i] != NULL; i++) {
		if (in_synth == synths[i]) break;
	}
	for ( ; synths[i] != NULL; i++) /* compress table */
		synths[i] = synths[i+1];
	module_status = 0;
	mutex_unlock(&spk_mutex);
}

static struct st_var_header var_headers[] = {
  { "version", VERSION, VAR_PROC, USER_R, 0, 0, 0 },
  { "synth_name", SYNTH, VAR_PROC, USER_RW, 0, 0, 0 },
  { "keymap", KEYMAP, VAR_PROC, USER_RW, 0, 0, 0 },
  { "silent", SILENT, VAR_PROC, USER_W, 0, 0, 0 },
  { "punc_some", PUNC_SOME, VAR_PROC, USER_RW, 0, 0, 0 },
  { "punc_most", PUNC_MOST, VAR_PROC, USER_RW, 0, 0, 0 },
  { "punc_all", PUNC_ALL, VAR_PROC, USER_R, 0, 0, 0 },
  { "delimiters", DELIM, VAR_PROC, USER_RW, 0, 0, 0 },
  { "repeats", REPEATS, VAR_PROC, USER_RW, 0, 0, 0 },
  { "ex_num", EXNUMBER, VAR_PROC, USER_RW, 0, 0, 0 },
  { "characters", CHARS, VAR_PROC, USER_RW, 0, 0, 0 },
  { "synth_direct", SYNTH_DIRECT, VAR_PROC, USER_W, 0, 0, 0 },
  { "caps_start", CAPS_START, VAR_STRING, USER_RW, 0, str_caps_start, 0 },
  { "caps_stop", CAPS_STOP, VAR_STRING, USER_RW, 0, str_caps_stop, 0 },
  { "delay_time", DELAY, VAR_TIME, ROOT_W, 0, &synth_delay_time, 0 },
  { "trigger_time", TRIGGER, VAR_TIME, ROOT_W, 0, &synth_trigger_time, 0 },
  { "jiffy_delta", JIFFY, VAR_TIME, ROOT_W, 0, &synth_jiffy_delta, 0 },
  { "full_time", FULL, VAR_TIME, ROOT_W, 0, &synth_full_time, 0 },
  { "spell_delay", SPELL_DELAY, VAR_NUM, USER_RW, 0, &spell_delay, 0 },
  { "bleeps", BLEEPS, VAR_NUM, USER_RW, 0, &bleeps, 0 },
  { "attrib_bleep", ATTRIB_BLEEP, VAR_NUM, USER_RW, 0, &attrib_bleep, 0 },
  { "bleep_time", BLEEP_TIME, VAR_TIME, USER_RW, 0, &bleep_time, 0 },
  { "cursor_time", CURSOR_TIME, VAR_TIME, USER_RW, 0, &cursor_timeout, 0 },
  { "punc_level", PUNC_LEVEL, VAR_NUM, USER_RW, 0, &punc_level, 0 },
  { "reading_punc", READING_PUNC, VAR_NUM, USER_RW, 0, &reading_punc, 0 },
  { "say_control", SAY_CONTROL, VAR_NUM, USER_RW, 0, &say_ctrl, 0 },
  { "say_word_ctl", SAY_WORD_CTL, VAR_NUM, USER_RW, 0, &say_word_ctl, 0 },
  { "no_interrupt", NO_INTERRUPT, VAR_NUM, USER_RW, 0, &no_intr, 0 },
  { "key_echo", KEY_ECHO, VAR_NUM, USER_RW, 0, &key_echo, 0 },
  { "bell_pos", BELL_POS, VAR_NUM, USER_RW, 0, &bell_pos, 0 },
  { "rate", RATE, VAR_NUM, USER_RW, 0, 0, 0 },
  { "pitch", PITCH, VAR_NUM, USER_RW, 0, 0, 0 },
  { "vol", VOL, VAR_NUM, USER_RW, 0, 0, 0 },
  { "tone", TONE, VAR_NUM, USER_RW, 0, 0, 0 },
  { "punct", PUNCT, VAR_NUM, USER_RW, 0, 0, 0 },
  { "voice", VOICE, VAR_NUM, USER_RW, 0, 0, 0 },
  { "freq", FREQ, VAR_NUM, USER_RW, 0, 0, 0 },
  { "lang", LANG, VAR_NUM, USER_RW, 0, 0, 0 },
  { "chartab", CHARTAB, VAR_PROC, USER_RW, 0, 0, 0 },
};

static struct st_var_header *var_ptrs[MAXVARS] = { 0, 0, 0 };

char *
speakup_s2i(char *start, short *dest)
{
	int val;
	char ch = *start;
	if (ch == '-' || ch == '+') start++;
	if (*start < '0' || *start > '9') return start;
	val = (*start) - '0';
	start++;
	while (*start >= '0' && *start <= '9') {
		val *= 10;
		val += (*start) - '0';
		start++;
	}
	if (ch == '-') *dest = -val;
	else *dest = val;
	return start;
}

short punc_masks[] = { 0, SOME, MOST, PUNC, PUNC|B_SYM };

// handlers for setting vars
int set_num_var(short input, struct st_var_header *var, int how)
{
	short val, ret = 0;
	short *p_val = var->p_val;
	int l;
	char buf[32], *cp;
	struct st_num_var *var_data = var->data;
	if (var_data == NULL)
		return E_UNDEF;
	if (how == E_DEFAULT) {
		val = var_data->default_val;
		ret = SET_DEFAULT;
	} else {
		if (how == E_SET) val = input;
		else val = var_data->value;
		if (how == E_INC) val += input;
		else if (how == E_DEC) val -= input;
		if (val < var_data->low || val > var_data->high)
			return E_RANGE;
	}
	var_data->value = val;
	if (var->var_type == VAR_TIME && p_val != 0) {
		*p_val = (val * HZ + 1000 - HZ) / 1000;
		return ret;
	}
	if (p_val != 0) *p_val = val;
	if (var->var_id == PUNC_LEVEL) {
		punc_mask = punc_masks[val];
		return ret;
	}
	if (var_data->multiplier != 0)
		val *= var_data->multiplier;
	val += var_data->offset;
	if (var->var_id < FIRST_SYNTH_VAR || synth == NULL) return ret;
	if (synth->synth_adjust != NULL) {
		int status = synth->synth_adjust(var);
		return (status != 0) ? status : ret;
	}
	if (!var_data->synth_fmt) return ret;
	if (var->var_id == PITCH) cp = pitch_buff;
	else cp = buf;
	if (!var_data->out_str)
		l = sprintf(cp, var_data->synth_fmt, (int)val);
	else l = sprintf(cp, var_data->synth_fmt, var_data->out_str[val]);
	synth_write_string(cp);
	return ret;
}

static int set_string_var(char *page, struct st_var_header *var, int len)
{
	int ret = 0;
	struct st_string_var *var_data = var->data;
	if (var_data == NULL) return E_UNDEF;
	if (len > MAXVARLEN)
		return -E_TOOLONG;
	if (!len) {
	if (!var_data->default_val) return 0;
		ret = SET_DEFAULT;
		if (!var->p_val) var->p_val = var_data->default_val;
		if (var->p_val != var_data->default_val)
			strcpy((char *)var->p_val, var_data->default_val);
		} else if (var->p_val)
			strcpy((char *)var->p_val, page);
	else return -E_TOOLONG;
	return ret;
}

struct st_var_header *get_var_header(short var_id)
{
	struct st_var_header *p_header;
	if (var_id < 0 || var_id >= MAXVARS) return NULL;
	p_header = var_ptrs[var_id];
	if (p_header->data == NULL) return NULL;
	return p_header;
}

#ifdef CONFIG_PROC_FS
// this is the write handler for /proc/speakup vars
static int speakup_vars_write_proc(struct file *file, const char *buffer,
				   u_long count, void *data)
{
	struct st_var_header *p_header = data;
	int len = count, ret = 0;
	char *page = (char *) __get_free_page(GFP_KERNEL);
	char *v_name = p_header->name, *cp;
	struct st_num_var *var_data;
	short value;
	if (!page) return -ENOMEM;
	if (copy_from_user(page, buffer, count)) {
		ret = -EFAULT;
		goto out;
	}
	if (page[len - 1] == '\n') --len;
	page[len] = '\0';
	cp = xlate(page);
	switch(p_header->var_type) {
		case VAR_NUM:
		case VAR_TIME:
			if (*cp == 'd' || *cp == 'r' || *cp == '\0')
				len = E_DEFAULT;
			else if (*cp == '+' || *cp == '-') len = E_INC;
			else len = E_SET;
			speakup_s2i(cp, &value);
			ret = set_num_var(value, p_header, len);
			if (ret != E_RANGE) break;
			var_data = p_header->data;
			pr_warn("value for %s out of range, expect %d to %d\n",
			v_name, (int)var_data->low, (int)var_data->high);
			break;
		case VAR_STRING:
			len = strlen(page);
			ret = set_string_var(page, p_header, len);
			if (ret != E_TOOLONG) break;
			pr_warn("value too long for %s\n", v_name);
			break;
		default:
			pr_warn("%s unknown type %d\n",
				p_header->name, (int)p_header->var_type);
		break;
	}
out:
	if (ret == SET_DEFAULT)
		pr_info("%s reset to default value\n", v_name);
	free_page((unsigned long) page);
	return count;
}

// this is the read handler for /proc/speakup vars
static int speakup_vars_read_proc(char *page, char **start, off_t off,
				  int count, int *eof, void *data)
{
	struct st_var_header *var = data;
	struct st_num_var *n_var = var->data;
	char ch, *cp, *cp1;
	*start = 0;
	*eof = 1;
	switch(var->var_type) {
		case VAR_NUM:
		case VAR_TIME:
			return sprintf(page, "%d\n", (int)n_var->value);
			break;
		case VAR_STRING:
			cp1 = page;
			*cp1++ = '"';
			for (cp = (char *)var->p_val; (ch = *cp); cp++) {
				if (ch >= ' ' && ch < '~')
					*cp1++ = ch;
				else
					cp1 += sprintf(cp1, "\\""x%02x", ch);
			}
			*cp1++ = '"';
			*cp1++ = '\n';
			*cp1 = '\0';
			return cp1-page;
			break;
		default:
			return sprintf(page, "oops bad type %d\n",
				(int)var->var_type);
	}
	return 0;
}

static const char spk_dir[] = "speakup";
static struct proc_dir_entry *dir_ent = 0;

static int spk_make_proc(struct st_var_header *p_header)
{
	struct proc_dir_entry *ent = p_header->proc_entry;
	char *name = p_header->name;
	struct st_proc_var *p_var;
	if (dir_ent == 0 || p_header->proc_mode == 0 || ent != 0) return 0;
	ent = create_proc_entry(name, p_header->proc_mode, dir_ent);
	if (!ent) {
		pr_warn("Unable to create /proc/%s/%s entry.\n",
			spk_dir, name);
		return -1;
	}
	if (p_header->var_type == VAR_PROC) {
		p_var = (struct st_proc_var *) p_header->data;
		if (p_header->proc_mode&S_IRUSR)
			ent->read_proc = p_var->read_proc;
		if (p_header->proc_mode&S_IWUSR)
			ent->write_proc = p_var->write_proc;
	} else {
		if (p_header->proc_mode&S_IRUSR)
			ent->read_proc = speakup_vars_read_proc;
		if (p_header->proc_mode&S_IWUSR)
			ent->write_proc = speakup_vars_write_proc;
	}
	ent->data = (void *)p_header;
	p_header->proc_entry = (void *) ent;
	return 0;
}

#endif

int speakup_register_var(struct st_num_var *var)
{
	static char nothing[2] = "\0";
	int i, var_id = var->var_id;
	struct st_var_header *p_header;
	struct st_string_var *s_var;
	if (var_id < 0 || var_id >= MAXVARS)
		return -1;
	if (var_ptrs[0] == 0) {
		for (i = 0; i < MAXVARS; i++) {
			p_header = &var_headers[i];
			var_ptrs[p_header->var_id] = p_header;
			p_header->data = 0;
		}
	}
	p_header = var_ptrs[var_id];
	if (p_header->data != 0) return 0;
	p_header->data = var;
	switch (p_header->var_type) {
		case VAR_STRING:
			s_var = (struct st_string_var *) var;
			set_string_var(nothing, p_header, 0);
			break;
		case VAR_NUM:
		case VAR_TIME:
			set_num_var(0, p_header, E_DEFAULT);
			break;
	}
#ifdef CONFIG_PROC_FS
	return spk_make_proc(p_header);
#else
	return 0;
#endif
}

static void speakup_unregister_var(short var_id)
{
	struct st_var_header *p_header;
	if (var_id < 0 || var_id >= MAXVARS) return;
	p_header = var_ptrs[var_id];
	p_header->data = 0;
#ifdef CONFIG_PROC_FS
	if (dir_ent != 0 && p_header->proc_entry != 0)
		remove_proc_entry(p_header->name, dir_ent);
	p_header->proc_entry = 0;
#endif
}

extern char synth_name[];

int speakup_dev_init(void)
{
	int i;
	struct st_var_header *p_header;
	struct st_proc_var *pv = spk_proc_vars;

	//pr_warn("synth name on entry is: %s\n", synth_name);
	synth_init(synth_name);
	speakup_register_devsynth();
#ifdef CONFIG_PROC_FS
	dir_ent = create_proc_entry(spk_dir, S_IFDIR, 0);
	if (!dir_ent) {
		pr_warn("Unable to create /proc/%s entry.\n", spk_dir);
		return -1;
	}
	while(pv->var_id >= 0) {
		speakup_register_var((void *) pv);
		pv++;
	}
		for (i = 0; i < MAXVARS; i++) {
			p_header = &var_headers[i];
		if (p_header->data != 0) spk_make_proc(p_header);
	}
#endif
	return 0;
}

void speakup_remove(void)
{
	int i;

	for (i = 0; i < MAXVARS; i++)
		speakup_unregister_var(i);
	pr_info("speakup: unregistering synth device /dev/synth\n");
	misc_deregister(&synth_device);
	misc_registered = 0;
#ifdef CONFIG_PROC_FS
	if (dir_ent != 0)
		remove_proc_entry(spk_dir, NULL);
#endif
}

// provide a file to users, so people can send to /dev/synth

static ssize_t
speakup_file_write(struct file *fp, const char *buffer,
		   size_t nbytes, loff_t * ppos)
{
	size_t count = nbytes;
	const char *ptr = buffer;
	int bytes;
	unsigned long flags;
	u_char buf[256];
	if (synth == NULL) return -ENODEV;
	while (count > 0) {
		bytes = min_t(size_t, count, sizeof(buf));
		if (copy_from_user(buf, ptr, bytes))
			return -EFAULT;
		count -= bytes;
		ptr += bytes;
		spk_lock(flags);
		synth_write(buf, bytes);
		spk_unlock(flags);
	}
	return (ssize_t) nbytes;
}

static int
speakup_file_ioctl(struct inode *inode, struct file *file,
		   unsigned int cmd, unsigned long arg)
{
	return 0;		// silently ignore
}

static ssize_t
speakup_file_read(struct file *fp, char *buf, size_t nbytes, loff_t * ppos)
{
	return 0;
}

static int synth_file_inuse = 0;

static int
speakup_file_open(struct inode *ip, struct file *fp)
{
	if (synth_file_inuse)
		return -EBUSY;
	else if (synth == NULL)
		return -ENODEV;
	synth_file_inuse++;
	return 0;
}

static int
speakup_file_release(struct inode *ip, struct file *fp)
{
	synth_file_inuse = 0;
	return 0;
}

static struct file_operations synth_fops = {
	.read=speakup_file_read,
	.write=speakup_file_write,
	.ioctl=speakup_file_ioctl,
	.open=speakup_file_open,
	.release=speakup_file_release,
};

static void speakup_register_devsynth(void)
{
	if (misc_registered != 0) return;
	misc_registered = 1;
	memset(&synth_device, 0, sizeof(synth_device));
/* zero it so if register fails, deregister will not ref invalid ptrs */
	synth_device.minor = SYNTH_MINOR;
	synth_device.name = "synth";
	synth_device.fops = &synth_fops;
	if (misc_register(&synth_device))
		pr_warn("Couldn't initialize miscdevice /dev/synth.\n");
	else
		pr_info("initialized device: /dev/synth, node (MAJOR 10, MINOR 25)\n");
}

/* exported symbols needed by synth modules */
EXPORT_SYMBOL_GPL(spk_serial_init);
EXPORT_SYMBOL_GPL(spk_serial_release);
EXPORT_SYMBOL_GPL(synth);
EXPORT_SYMBOL_GPL(synth_alive);
EXPORT_SYMBOL_GPL(synth_buffer);
EXPORT_SYMBOL_GPL(synth_buff_in);
EXPORT_SYMBOL_GPL(synth_buff_out);
EXPORT_SYMBOL_GPL(synth_delay);
EXPORT_SYMBOL_GPL(synth_delay_time);
EXPORT_SYMBOL_GPL(synth_done);
EXPORT_SYMBOL_GPL(synth_full_time);
EXPORT_SYMBOL_GPL(synth_jiffy_delta);
EXPORT_SYMBOL_GPL(synth_port_forced);
EXPORT_SYMBOL_GPL(synth_port_tts);
EXPORT_SYMBOL_GPL(synth_request_region);
EXPORT_SYMBOL_GPL(synth_release_region);
EXPORT_SYMBOL_GPL(synth_add);
EXPORT_SYMBOL_GPL(synth_remove);
EXPORT_SYMBOL_GPL(synth_stop_timer);
EXPORT_SYMBOL_GPL(synth_write);
EXPORT_SYMBOL_GPL(synth_putc);
EXPORT_SYMBOL_GPL(synth_printf);
EXPORT_SYMBOL_GPL(synth_write_string);
EXPORT_SYMBOL_GPL(synth_write_msg);
