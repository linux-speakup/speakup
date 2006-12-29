/* speakup_sftsynth.c - speakup driver to register and make available
 * a user space device for software synthesizers.  written by: Kirk
 * Reiser <kirk@braille.uwo.ca>

		Copyright (C) 2003  Kirk Reiser.

		This program is free software; you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation; either version 2 of the License, or
		(at your option) any later version.

		This program is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
		GNU General Public License for more details.

		You should have received a copy of the GNU General Public License
		along with this program; if not, write to the Free Software
		Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 * this code is specificly written as a driver for the speakup screenreview
 * package and is not a general device driver.  */

#include <linux/unistd.h>
#include <asm/semaphore.h>
#include <linux/miscdevice.h>   /* for misc_register, and SYNTH_MINOR */
#include <linux/poll.h>  // for poll_wait()
#include "spk_priv.h"

#define MY_SYNTH synth_sftsyn
#define SOFTSYNTH_MINOR 26  // might as well give it one more than /dev/synth
#define PROCSPEECH 0x0d
#define CLEAR_SYNTH 0x18

static struct miscdevice synth_device;
static int misc_registered = 0;
static int dev_opened = 0;
static DECLARE_MUTEX(sem);
DECLARE_WAIT_QUEUE_HEAD ( wait_on_output );


static int softsynth_open (struct inode *inode, struct file *fp)
{
        if (dev_opened) return -EBUSY;
	//if ((fp->f_flags & O_ACCMODE) != O_RDONLY ) return -EPERM;
	dev_opened++;
        return 0;
}

static int softsynth_close (struct inode *inode, struct file *fp)
{
        fp->f_op = NULL;
	dev_opened = 0;
	return 0;
}

static ssize_t softsynth_read (struct file *fp, char *buf, size_t count, loff_t *pos)
{
	int chars_sent=0;

        if (down_interruptible( &sem )) return -ERESTARTSYS;
        while (synth_buff_in == synth_buff_out) {
	  up ( &sem );
	  if (fp->f_flags & O_NONBLOCK)
	    return -EAGAIN;
	  if (wait_event_interruptible( wait_on_output, (synth_buff_in > synth_buff_out)))
	    return -ERESTARTSYS;
	  if (down_interruptible( &sem )) return -ERESTARTSYS;
	}

	chars_sent = (count > synth_buff_in-synth_buff_out) 
	  ? synth_buff_in-synth_buff_out : count;
	if (copy_to_user(buf, (char *) synth_buff_out, chars_sent)) {
	  up ( &sem);
	  return -EFAULT;
	}
	synth_buff_out += chars_sent;
	*pos += chars_sent;
	if (synth_buff_out >= synth_buff_in) {
	  synth_done();
	  *pos = 0;
	}
	up ( &sem );
	return chars_sent;
}

static int last_index=0;

static ssize_t softsynth_write (struct file *fp, const char *buf, size_t count, loff_t *pos)
{
	int i;
	char indbuf[5];
	if (down_interruptible( &sem))
		return -ERESTARTSYS;

	if (copy_from_user(indbuf,buf,count))
	{
		up(&sem);
		return -EFAULT;
	}
	up(&sem);
	last_index=0;
	for (i=0;i<strlen(indbuf);i++)
		last_index=last_index*10+indbuf[i]-48;
	return count;
}

static unsigned int softsynth_poll (struct file *fp, struct poll_table_struct *wait)
{
        poll_wait(fp, &wait_on_output, wait);

	if (synth_buff_out < synth_buff_in)
	  return (POLLIN | POLLRDNORM);
	return 0;
}

static void 
softsynth_flush( void )
{
	synth_write( "\x18", 1 );
}

static unsigned char get_index( void )
{
	int rv;
	rv=last_index;
	last_index=0;
	return rv;
}

static struct file_operations softsynth_fops = {
        poll:softsynth_poll,
        read:softsynth_read,
	write:softsynth_write,
        open:softsynth_open,
        release:softsynth_close,
};
 

static int 
softsynth_probe( void )
{

        if ( misc_registered != 0 ) return 0;
	memset( &synth_device, 0, sizeof( synth_device ) );
        synth_device.minor = SOFTSYNTH_MINOR;
        synth_device.name = "softsynth";
        synth_device.fops = &softsynth_fops;
        if ( misc_register ( &synth_device ) ) {
	  pr_warn("Couldn't initialize miscdevice /dev/softsynth.\n" );
	  return -ENODEV;
        }

        misc_registered = 1;
	pr_info("initialized device: /dev/softsynth, node (MAJOR 10, MINOR 26)\n" );
	return 0;
}

static void 
softsynth_release(void)
{
        misc_deregister( &synth_device );
	misc_registered = 0;
	pr_info("unregistered /dev/softsynth\n");
}

static void 
softsynth_start ( void )
{
        wake_up_interruptible ( &wait_on_output );
}

static int 
softsynth_is_alive( void )
{
	if ( synth_alive ) return 1;
	return 0;
}

static const char init_string[] = "\01@\x01\x31y\n";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "\x01+3p" },
	{ CAPS_STOP, "\x01-3p" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "\x01%ds", 5, 0, 9, 0, 0, 0 },
	{ PITCH, "\x01%dp", 5, 0, 9, 0, 0, 0 },
	{ VOL, "\x01%dv", 5, 0, 9, 0, 0, 0 },
	{ TONE, "\x01%dx", 1, 0, 2, 0, 0, 0 },
	{ PUNCT, "\x01%db", 7, 0, 15, 0, 0, 0 },
	{ VOICE, "\x01%do", 0, 0, 7, 0, 0, 0 },
	{ FREQ, "\x01%df", 5, 0, 9, 0, 0, 0 },
	V_LAST_NUM
};

struct spk_synth synth_sftsyn = { "sftsyn", "0.3", "software synth",
	init_string, 0, 0, 0, 0, 0, 0, SYNTH_CHECK,
	stringvars, numvars, softsynth_probe, softsynth_release, NULL,
	NULL, softsynth_start, softsynth_flush, softsynth_is_alive, NULL, NULL,
	get_index, {"\x01%di",1,5,1} };

static void __exit mod_synth_exit( void )
{
        if ( synth == &MY_SYNTH ) 
	  synth_release( );
	synth_remove( &MY_SYNTH );
}

static int __init mod_synth_init( void )
{
	int status = do_synth_init( &MY_SYNTH );
	if ( status != 0 ) return status;
	synth_add( &MY_SYNTH );
	return 0;
}

module_init( mod_synth_init );
module_exit( mod_synth_exit );
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_DESCRIPTION("Synthesizer driver module for speakup for the  synth->long_name");
MODULE_LICENSE( "GPL" );
