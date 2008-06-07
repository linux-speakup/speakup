#include <linux/errno.h>
#include <linux/miscdevice.h>	/* for misc_register, and SYNTH_MINOR */
#include <linux/types.h>

#include "speakup.h"
#include "spk_priv.h"

#ifndef SYNTH_MINOR
#define SYNTH_MINOR 25
#endif

static struct miscdevice synth_device;
static int misc_registered;
static int synth_file_inuse;

static ssize_t speakup_file_write(struct file *fp, const char *buffer,
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

static int speakup_file_ioctl(struct inode *inode, struct file *file,
		   unsigned int cmd, unsigned long arg)
{
	return 0;		/* silently ignore */
}

static ssize_t speakup_file_read(struct file *fp, char *buf, size_t nbytes, loff_t *ppos)
{
	return 0;
}

static int speakup_file_open(struct inode *ip, struct file *fp)
{
	if (synth_file_inuse)
		return -EBUSY;
	else if (synth == NULL)
		return -ENODEV;
	synth_file_inuse++;
	return 0;
}

static int speakup_file_release(struct inode *ip, struct file *fp)
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

void speakup_register_devsynth(void)
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

void speakup_unregister_devsynth(void)
{
	pr_info("speakup: unregistering synth device /dev/synth\n");
	misc_deregister(&synth_device);
	misc_registered = 0;
}
