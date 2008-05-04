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
#include <asm/uaccess.h> /* for copy_from_user */

#ifndef SYNTH_MINOR
#define SYNTH_MINOR 25
#endif

#include "spk_priv.h"
#include "speakup.h"
#include "serialio.h"

static struct serial_state rs_table[] = {
	SERIAL_PORT_DFNS
};


#define synthBufferSize 8192	/* currently 8K bytes */
#define MAXSYNTHS       16      /* Max number of synths in array. */
static struct spk_synth *synths[MAXSYNTHS];
struct spk_synth *synth = NULL;
static int synth_timer_active;	/* indicates when a timer is set */
	static struct miscdevice synth_device;
static int misc_registered;
char pitch_buff[32] = "";
declare_sleeper(synth_sleeping_list);
static int module_status;
static declare_timer(synth_timer);
int quiet_boot;
u_char synth_buffer[synthBufferSize];	/* guess what this is for! */
static u_char *buffer_highwater = synth_buffer+synthBufferSize-100;
u_char *buffer_end = synth_buffer+synthBufferSize-1;
static irqreturn_t synth_readbuf_handler(int irq, void *dev_id);
static struct serial_state *serstate;
static int timeouts;

short synth_trigger_time = 50;
struct speakup_info_t speakup_info = {
	.delay_time = 500,
	.jiffy_delta = 50,
	.full_time = 1000,
	.buff_in = synth_buffer,
	.buff_out = synth_buffer,
};
EXPORT_SYMBOL_GPL(speakup_info);

static void start_serial_interrupt(int irq);
static void speakup_register_devsynth(void);
static int do_synth_init(struct spk_synth *in_synth);

struct serial_state *spk_serial_init(int index)
{
	int baud = 9600, quot = 0;
	unsigned int cval = 0;
	int cflag = CREAD | HUPCL | CLOCAL | B9600 | CS8;
	struct serial_state *ser = NULL;

	ser = rs_table + index;
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
	if (synth_request_region(ser->port, 8)) {
		/* try to take it back. */
		__release_region(&ioport_resource, ser->port, 8);
		if (synth_request_region(ser->port, 8))
			return NULL;
	}

	/*	Disable UART interrupts, set DTR and RTS high
	 *	and set speed. */
	outb(cval | UART_LCR_DLAB, ser->port + UART_LCR);	/* set DLAB */
	outb(quot & 0xff, ser->port + UART_DLL);	/* LS of divisor */
	outb(quot >> 8, ser->port + UART_DLM);		/* MS of divisor */
	outb(cval, ser->port + UART_LCR);		/* reset DLAB */

	/* Turn off Interrupts */
	outb(0, ser->port + UART_IER);
	outb(UART_MCR_DTR | UART_MCR_RTS, ser->port + UART_MCR);

	/* If we read 0xff from the LSR, there is no UART here. */
	if (inb(ser->port + UART_LSR) == 0xff) {
		synth_release_region(ser->port, 8);
		serstate = NULL;
		return NULL;
	}

	mdelay(1);
	speakup_info.port_tts = ser->port;
	serstate = ser;

	start_serial_interrupt(ser->irq);

	return ser;
}
EXPORT_SYMBOL_GPL(spk_serial_init);

int serial_synth_probe(void)
{
	struct serial_state *ser;
	int failed = 0;
	
	if ((param_ser >= SPK_LO_TTY) && (param_ser <= SPK_HI_TTY)) {
		ser = spk_serial_init(param_ser);
		if (ser == NULL) {
			failed = -1;
		} else {
		outb_p(0, ser->port);
		mdelay(1);
		outb_p('\r', ser->port);
	}
	} else {
		failed = -1;
		pr_warn("ttyS%i is an invalid port\n", param_ser);
	}
	if (failed) {
		pr_info("%s: not found\n", synth->long_name);
		return -ENODEV;
	}
	pr_info("%s: ttyS%i, Driver Version %s\n", synth->long_name, param_ser, synth->version);
	return 0;
}
EXPORT_SYMBOL_GPL(serial_synth_probe);

static void start_serial_interrupt(int irq)
{
	int rv;

	if (synth->read_buff_add == NULL)
		return;

	rv = request_irq(irq, synth_readbuf_handler, IRQF_SHARED,
			 "serial", (void *) synth_readbuf_handler);

	if (rv)
		printk(KERN_ERR "Unable to request Speakup serial I R Q\n");
	/* Set MCR */
	outb(UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2,
			speakup_info.port_tts + UART_MCR);
	/* Turn on Interrupts */
	outb(UART_IER_MSI|UART_IER_RLSI|UART_IER_RDI,
			speakup_info.port_tts + UART_IER);
	inb(speakup_info.port_tts+UART_LSR);
	inb(speakup_info.port_tts+UART_RX);
	inb(speakup_info.port_tts+UART_IIR);
	inb(speakup_info.port_tts+UART_MSR);
	outb(1, speakup_info.port_tts + UART_FCR);	/* Turn FIFO On */
}

static void stop_serial_interrupt(void)
{
	if (speakup_info.port_tts == 0)
		return;

	if (synth->read_buff_add == NULL)
		return;

	/* Turn off interrupts */
	outb(0, speakup_info.port_tts+UART_IER);
	/* Free IRQ */
	free_irq(serstate->irq, (void *) synth_readbuf_handler);
}

int wait_for_xmitr(void)
{
	int check, tmout = SPK_XMITR_TIMEOUT;
	if ((speakup_info.alive) && (timeouts >= NUM_DISABLE_TIMEOUTS)) {
		speakup_info.alive = 0;
		timeouts = 0;
		return 0;
	}
	do {
		/* holding register empty? */
		check = inb_p(speakup_info.port_tts + UART_LSR);
		if (--tmout == 0) {
			pr_warn("%s: timed out\n", synth->long_name);
			timeouts++;
			return 0;
		}
	} while ((check & BOTH_EMPTY) != BOTH_EMPTY);
	tmout = SPK_XMITR_TIMEOUT;
	do {
		/* CTS */
		check = inb_p(speakup_info.port_tts + UART_MSR);
		if (--tmout == 0) {
			timeouts++;
			return 0;
		}
	} while ((check & UART_MSR_CTS) != UART_MSR_CTS);
	timeouts = 0;
	return 1;
}
EXPORT_SYMBOL_GPL(wait_for_xmitr);

unsigned char spk_serial_in(void)
{
	int c;
	int lsr;
	int tmout = SPK_SERIAL_TIMEOUT;

	do {
		lsr = inb_p(speakup_info.port_tts + UART_LSR);
		if (--tmout == 0) {
			pr_warn("time out while waiting for input.\n");
			return 0xff;
		}
	} while (!(lsr & UART_LSR_DR));
	c = inb_p(speakup_info.port_tts + UART_RX);
	return (unsigned char) c;
}
EXPORT_SYMBOL_GPL(spk_serial_in);

int spk_serial_out(const char ch)
{
	if (speakup_info.alive && wait_for_xmitr()) {
		outb_p(ch, speakup_info.port_tts);
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(spk_serial_out);

void spk_serial_release(void)
{
	if (speakup_info.port_tts == 0)
		return;
	synth_release_region(speakup_info.port_tts, 8);
	speakup_info.port_tts = 0;
}
EXPORT_SYMBOL_GPL(spk_serial_release);

static irqreturn_t synth_readbuf_handler(int irq, void *dev_id)
{
/*printk(KERN_ERR "in irq\n"); */
/*pr_warn("in IRQ\n"); */
	int c;
	while (inb_p(speakup_info.port_tts + UART_LSR) & UART_LSR_DR) {

		c = inb_p(speakup_info.port_tts+UART_RX);
		synth->read_buff_add((u_char) c);
/*printk(KERN_ERR "c = %d\n", c); */
/*pr_warn("C = %d\n", c); */
	}
	return IRQ_HANDLED;
}

/* sleep for ms milliseconds */
void
synth_delay(int val)
{
	if (val == 0)
		return;
	synth_timer.expires = jiffies + val;
	start_timer(synth_timer);
	synth_timer_active++;
}
EXPORT_SYMBOL_GPL(synth_delay);

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
EXPORT_SYMBOL_GPL(synth_stop_timer);

int synth_done(void)
{
	speakup_info.buff_out = speakup_info.buff_in = synth_buffer;
	if (waitqueue_active(&synth_sleeping_list)) {
		wake_up_interruptible(&synth_sleeping_list);
		return 0;
	}
	return 1;
}
EXPORT_SYMBOL_GPL(synth_done);

static void synth_start(void)
{
	if (!speakup_info.alive)
		synth_done();
	else if (synth->start)
		synth->start();
	else if (synth_timer_active == 0)
		synth_delay(synth_trigger_time);
}

void do_flush(void)
{
	synth_stop_timer();
	speakup_info.buff_out = speakup_info.buff_in = synth_buffer;
	if (speakup_info.alive) {
		synth->flush();
		if (synth->flush_wait)
			synth_delay((synth->flush_wait * HZ) / 1000);
		if (pitch_shift) {
			synth_printf("%s",pitch_buff);
			pitch_shift = 0;
		}
	}
	if (waitqueue_active(&synth_sleeping_list))
		wake_up_interruptible(&synth_sleeping_list);
}

void
synth_buffer_add(char ch)
{
	if (speakup_info.buff_in >= buffer_highwater) {
		synth_start();
		if (!waitqueue_active(&synth_sleeping_list))
			interruptible_sleep_on(&synth_sleeping_list);
		if (speakup_info.buff_in >= buffer_end)
			return;
	}
	*speakup_info.buff_in++ = ch;
}

void
synth_write(const char *buf, size_t count)
{
	while (count--)
		synth_buffer_add(*buf++);
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
EXPORT_SYMBOL_GPL(synth_printf);

static int index_count = 0;
static int sentence_count = 0;

void
reset_index_count(int sc)
{
	static int first = 1;
	if (first)
		first = 0;
	else
		synth->get_index();
	index_count = 0;
	sentence_count = sc;
}

int
synth_supports_indexing(void)
{
	if (synth->get_index != NULL)
		return 1;
	return 0;
}

void
synth_insert_next_index(int sent_num)
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

void
get_index_count(int *linecount, int *sentcount)
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

/* called by: speakup_dev_init() */
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

static void synth_catch_up(u_long data)
{
	unsigned long flags;
	spk_lock(flags);
	if (synth->catch_up)
		synth->catch_up(data);
	spk_unlock(flags);
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
	if (synth->probe() < 0) {
		pr_warn("%s: device probe failed\n", in_synth->name);
		synth = NULL;
		return -ENODEV;
	}
	synth_time_vars[0].default_val = synth->delay;
	synth_time_vars[1].default_val = synth->trigger;
	synth_time_vars[2].default_val = synth->jiffies;
	synth_time_vars[3].default_val = synth->full;
	synth_timer.function = synth_catch_up;
	synth_timer.entry.prev = NULL;
	init_timer(&synth_timer);
	for (n_var = synth_time_vars; n_var->var_id >= 0; n_var++)
		speakup_register_var(n_var);
	speakup_info.alive = 1;
	synth_printf("%s",synth->init);
	for (s_var = synth->string_vars; s_var->var_id >= 0; s_var++)
		speakup_register_var((struct st_num_var *) s_var);
	for (n_var = synth->num_vars; n_var->var_id >= 0; n_var++)
		speakup_register_var(n_var);
	if (!quiet_boot)
		synth_printf("%s found\n", synth->long_name);
	synth_flags = synth->flags;
	return 0;
}

void
synth_release(void)
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
	synth_dummy_catchup((unsigned long) NULL);
	synth_timer.function = synth_dummy_catchup;
	stop_serial_interrupt();
	synth->release();
	synth = NULL;
}

/* called by: all_driver_init() */
int synth_add(struct spk_synth *in_synth)
{
	int i;
	int status=0;
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
	if (in_synth->flags) 
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

/* called by: speakup_init() */
int speakup_dev_init(char *synth_name)
{
	pr_warn("synth name on entry is: %s\n", synth_name); 
	synth_init(synth_name);
	speakup_register_devsynth();
	return 0;
}

void speakup_remove(void)
{
	int i;

	for (i = 0; i < MAXVARS; i++) {
		speakup_unregister_var(i);
}
	pr_info("speakup: unregistering synth device /dev/synth\n");
	misc_deregister(&synth_device);
	misc_registered = 0;
}

/* provide a file to users, so people can send to /dev/synth */

static ssize_t
speakup_file_write(struct file *fp, const char *buffer,
		   size_t nbytes, loff_t *ppos)
{
	size_t count = nbytes;
	const char *ptr = buffer;
	int bytes;
	unsigned long flags;
	u_char buf[256];
	if (synth == NULL)
		return -ENODEV;
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
	return 0;		/* silently ignore */
}

static ssize_t
speakup_file_read(struct file *fp, char *buf, size_t nbytes, loff_t *ppos)
{
	return 0;
}

static int synth_file_inuse;

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
	.read = speakup_file_read,
	.write = speakup_file_write,
	.ioctl = speakup_file_ioctl,
	.open = speakup_file_open,
	.release = speakup_file_release,
};

static void speakup_register_devsynth(void)
{
	if (misc_registered != 0)
		return;
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
