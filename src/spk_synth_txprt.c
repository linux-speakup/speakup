/*
 * originially written by: Kirk Reiser <kirk@braille.uwo.ca>
* this version considerably modified by David Borowski, david575@rogers.com

		Copyright (C) 1998-99  Kirk Reiser.
		Copyright (C) 2003 David Borowski.

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
 * package and is not a general device driver.
		*/
#include "spk_priv.h"
#include "serialio.h"

#define MY_SYNTH synth_txprt
#define DRV_VERSION "1.2"
#define SYNTH_CLEAR 0x18
#define PROCSPEECH '\r' /* process speech char */

static int timeouts = 0;	/* sequential number of timeouts */

static int wait_for_xmitr( void )
{
	int check, tmout = SPK_XMITR_TIMEOUT;
	if ( ( synth_alive ) && ( timeouts >= NUM_DISABLE_TIMEOUTS ) ) {
		synth_alive = 0; 
		timeouts = 0;
		return 0; 
	}
	do {
		check = inb( synth_port_tts + UART_LSR );
		if ( --tmout == 0 ) {
			pr_warn( "TXPRT:  timed out\n" );
			timeouts++;
			return 0;
		}
	} while ( ( check & BOTH_EMPTY ) != BOTH_EMPTY );
	tmout = SPK_XMITR_TIMEOUT;
	do {
		check = inb( synth_port_tts + UART_MSR );
				if ( --tmout == 0 ) {
					timeouts++;
					return 0;
				}
	} while ( ( check & UART_MSR_CTS ) != UART_MSR_CTS );
	timeouts = 0;
	return 1;
}

static int spk_serial_out( const char ch )
{
	if ( synth_alive && wait_for_xmitr( ) ) {
		outb( ch, synth_port_tts );
		return 1;
	}
	return 0;
}

static unsigned char spk_serial_in( void )
{
	int c, lsr, tmout = SPK_SERIAL_TIMEOUT;
	do {
		lsr = inb( synth_port_tts + UART_LSR );
		if ( --tmout == 0 ) return 0xff;
	} while ( !( lsr & UART_LSR_DR ) );
	c = inb( synth_port_tts + UART_RX );
	return ( unsigned char ) c;
}

static void do_catch_up( unsigned long data )
{
	unsigned long jiff_max = jiffies+synth_jiffy_delta;
	u_char ch;
	
	synth_stop_timer( );
	while ( synth_buff_out < synth_buff_in ) {
		ch = *synth_buff_out;
		if ( ch == '\n' ) ch = PROCSPEECH;
		if ( !spk_serial_out( ch ) ) {
			synth_delay( synth_full_time );
			return;
		}
		synth_buff_out++;
		if ( jiffies >= jiff_max && ch == ' ' ) { 
			spk_serial_out( PROCSPEECH );
			synth_delay( synth_delay_time );
			return; 
		}
	}
	spk_serial_out( PROCSPEECH );
	synth_done( );
}

static const char *synth_immediate ( const char *buf )
{
	u_char ch;
	while ( ( ch = *buf ) ) {
	if ( ch == 0x0a ) ch = PROCSPEECH;
        if ( wait_for_xmitr( ) )
	  outb( ch, synth_port_tts );
	else return buf;
	buf++;
	}
	return 0;
}

static void synth_flush ( void )
{
	spk_serial_out ( SYNTH_CLEAR );
}

static int serprobe( int index )
{
	u_char test=0;
	struct serial_state *ser = spk_serial_init( index );
	if ( ser == NULL ) return -1;
	if ( synth_port_forced ) return 0;
	/* check for txprt now... */
	if (synth_immediate( "\x05$" ))
	  pr_warn("synth_immediate could not unload\n");
	if (synth_immediate( "\x05Ik" ))
	  pr_warn("synth_immediate could not unload again\n");
	if (synth_immediate( "\x05Q\r" ))
	  pr_warn("synth_immediate could not unload a third time\n");
	if ( ( test = spk_serial_in( ) ) == 'k' ) return 0;
	else pr_warn( "synth returned %x on port %03lx\n", test, ser->port );
	synth_release_region( ser->port,8 );
	timeouts = synth_alive = 0;
			  return -1;
}

static int synth_probe( void )
{
	int i, failed=0;
	pr_info( "Probing for %s.\n", synth->long_name );
	for ( i=SPK_LO_TTY; i <= SPK_HI_TTY; i++ ) {
		if (( failed = serprobe( i )) == 0 ) break; /* found it */
	}
	if ( failed ) {
		pr_info( "%s:  not found\n", synth->long_name );
		return -ENODEV;
	}
	pr_info( "%s: %03x-%03x..\n", synth->long_name, (int) synth_port_tts, (int) synth_port_tts+7 );
	pr_info( "%s: driver version %s.\n",  synth->long_name, synth->version);
	return 0;
}

static int synth_is_alive( void )
{
	if ( synth_alive ) return 1;
	if ( wait_for_xmitr( ) > 0 ) { /* restart */
		synth_alive = 1;
		synth_write_string( synth->init );
		return 2;
	}
	pr_warn( "%s: can't restart synth\n", synth->long_name );
	return 0;
}

static const char init_string[] = "\x05N1";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "\x05P8" },
	{ CAPS_STOP, "\x05P5" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "\x05R%d", 5, 0, 9, 0, 0, 0 },
	{ PITCH, "\x05P%d", 5, 0, 9, 0, 0, 0 },
	{ VOL, "\x05V%d", 5, 0, 9, 0, 0, 0 },
	{ TONE, "\x05T%c", 12, 0, 25, 61, 0, 0 },
	V_LAST_NUM
	 };

struct spk_synth synth_txprt = {"txprt", DRV_VERSION, "Transport",
	init_string, 500, 50, 50, 5000, 0, 0, SYNTH_CHECK,
	stringvars, numvars, synth_probe, spk_serial_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, NULL, NULL,
	{NULL,0,0,0} };

static int __init txprt_init(void)
{
	int status = do_synth_init(&MY_SYNTH);
	if (status != 0)
		return status;
	synth_add(&MY_SYNTH);
	return 0;
}

static void __exit txprt_exit(void)
{
	if (synth == &MY_SYNTH)
		synth_release();
	synth_remove(&MY_SYNTH);
}

module_init(txprt_init);
module_exit(txprt_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Transport synthesizers");
MODULE_LICENSE("GPL");

