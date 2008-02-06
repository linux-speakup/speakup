#ifndef _SPEAKUP_SERIAL_H
#define _SPEAKUP_SERIAL_H

#include <linux/serial.h>	/* for rs_table, serial constants &
				   serial_uart_config */
#include <linux/serial_reg.h>	/* for more serial constants */
#include <linux/serialP.h>	/* for struct serial_state */
#include <asm/serial.h>

/* countdown values for serial timeouts */
#define SPK_SERIAL_TIMEOUT 1000000
/* countdown values transmitter/dsr timeouts */
#define SPK_XMITR_TIMEOUT 1000000
/* check ttyS0 ... ttyS3 */
#define SPK_LO_TTY 0
#define SPK_HI_TTY 3
/* # of timeouts permitted before disable */
#define NUM_DISABLE_TIMEOUTS 3
/* buffer timeout in ms */
#define SPK_TIMEOUT 100
#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

#endif
