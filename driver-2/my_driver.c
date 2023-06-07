// SPDX-License-Identifier: GPL-2.0+
/*
 * USB FTDI SIO driver
 *
 *	Copyright (C) 2009 - 2013
 *	    Johan Hovold (jhovold@gmail.com)
 *	Copyright (C) 1999 - 2001
 *	    Greg Kroah-Hartman (greg@kroah.com)
 *          Bill Ryder (bryder@sgi.com)
 *	Copyright (C) 2002
 *	    Kuba Ober (kuba@mareimbrium.org)
 *
 * See Documentation/usb/usb-serial.rst for more information on using this
 * driver
 *
 * See http://ftdi-usb-sio.sourceforge.net for up to date testing info
 *	and extra documentation
 *
 * Change entries from 2004 and earlier can be found in versions of this
 * file in kernel versions prior to the 2.6.24 release.
 *
 */

/* Bill Ryder - bryder@sgi.com - wrote the FTDI_SIO implementation */
/* Thanx to FTDI for so kindly providing details of the protocol required */
/*   to talk to the device */
/* Thanx to gkh and the rest of the usb dev group for all code I have
   assimilated :-) */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/serial.h>
#include <linux/gpio/driver.h>
#include <linux/usb/serial.h>
#include "my_driver.h"
#include "my_driver_id.h"

#define DRIVER_AUTHOR "Greg Kroah-Hartman <greg@kroah.com>, Bill Ryder <bryder@sgi.com>, Kuba Ober <kuba@mareimbrium.org>, Andreas Mohr, Johan Hovold <jhovold@gmail.com>"
#define DRIVER_DESC "USB FTDI Serial Converters Driver"

#define VENDOR_ID 0x0403
#define PRODUCT_ID 0x6001

struct ftdi_private {
	enum ftdi_chip_type chip_type;
				/* type of device, either SIO or FT8U232AM */
	int baud_base;		/* baud base clock for divisor setting */
	int custom_divisor;	/* custom_divisor kludge, this is for
				   baud_base (different from what goes to the
				   chip!) */
	u16 last_set_data_value; /* the last data state set - needed for doing
				  * a break
				  */
	int flags;		/* some ASYNC_xxxx flags are supported */
	unsigned long last_dtr_rts;	/* saved modem control outputs */
	char prev_status;        /* Used for TIOCMIWAIT */
	char transmit_empty;	/* If transmitter is empty or not */
	u16 interface;		/* FT2232C, FT2232H or FT4232H port interface
				   (0 for FT232/245) */

	speed_t force_baud;	/* if non-zero, force the baud rate to
				   this value */
	int force_rtscts;	/* if non-zero, force RTS-CTS to always
				   be enabled */

	unsigned int latency;		/* latency setting in use */
	unsigned short max_packet_size;
	struct mutex cfg_lock; /* Avoid mess by parallel calls of config ioctl() and change_speed() */
#ifdef CONFIG_GPIOLIB
	struct gpio_chip gc;
	struct mutex gpio_lock;	/* protects GPIO state */
	bool gpio_registered;	/* is the gpiochip in kernel registered */
	bool gpio_used;		/* true if the user requested a gpio */
	u8 gpio_altfunc;	/* which pins are in gpio mode */
	u8 gpio_output;		/* pin directions cache */
	u8 gpio_value;		/* pin value for outputs */
#endif
};

/* struct ftdi_sio_quirk is used by devices requiring special attention. */
struct ftdi_sio_quirk {
	int (*probe)(struct usb_serial *);
	/* Special settings for probed ports. */
	void (*port_probe)(struct ftdi_private *);
};

static int   ftdi_jtag_probe(struct usb_serial *serial);
static int   ftdi_NDI_device_setup(struct usb_serial *serial);
static int   ftdi_stmclite_probe(struct usb_serial *serial);
static int   ftdi_8u2232c_probe(struct usb_serial *serial);
static void  ftdi_USB_UIRT_setup(struct ftdi_private *priv);
static void  ftdi_HE_TIRA1_setup(struct ftdi_private *priv);

static const struct ftdi_sio_quirk ftdi_jtag_quirk = {
	.probe	= ftdi_jtag_probe,
};

static const struct ftdi_sio_quirk ftdi_NDI_device_quirk = {
	.probe	= ftdi_NDI_device_setup,
};

static const struct ftdi_sio_quirk ftdi_USB_UIRT_quirk = {
	.port_probe = ftdi_USB_UIRT_setup,
};

static const struct ftdi_sio_quirk ftdi_HE_TIRA1_quirk = {
	.port_probe = ftdi_HE_TIRA1_setup,
};

static const struct ftdi_sio_quirk ftdi_stmclite_quirk = {
	.probe	= ftdi_stmclite_probe,
};

static const struct ftdi_sio_quirk ftdi_8u2232c_quirk = {
	.probe	= ftdi_8u2232c_probe,
};

/*
 * The 8U232AM has the same API as the sio except for:
 * - it can support MUCH higher baudrates; up to:
 *   o 921600 for RS232 and 2000000 for RS422/485 at 48MHz
 *   o 230400 at 12MHz
 *   so .. 8U232AM's baudrate setting codes are different
 * - it has a two byte status code.
 * - it returns characters every 16ms (the FTDI does it every 40ms)
 *
 * the bcdDevice value is used to differentiate FT232BM and FT245BM from
 * the earlier FT8U232AM and FT8U232BM.  For now, include all known VID/PID
 * combinations in both tables.
 * FIXME: perhaps bcdDevice can also identify 12MHz FT8U232AM devices,
 * but I don't know if those ever went into mass production. [Ian Abbott]
 */



/*
 * Device ID not listed? Test it using
 * /sys/bus/usb-serial/drivers/ftdi_sio/new_id and send a patch or report.
 */
static const struct usb_device_id id_table_combined[] = {
	{USB_DEVICE(VENDOR_ID, PRODUCT_ID)},
	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, id_table_combined);

static const char *ftdi_chip_name[] = {
	[SIO] = "SIO",	/* the serial part of FT8U100AX */
	[FT8U232AM] = "FT8U232AM",
	[FT232BM] = "FT232BM",
	[FT2232C] = "FT2232C",
	[FT232RL] = "FT232RL",
	[FT2232H] = "FT2232H",
	[FT4232H] = "FT4232H",
	[FT232H]  = "FT232H",
	[FTX]     = "FT-X"
};


/* Used for TIOCMIWAIT */
#define FTDI_STATUS_B0_MASK	(FTDI_RS0_CTS | FTDI_RS0_DSR | FTDI_RS0_RI | FTDI_RS0_RLSD)
#define FTDI_STATUS_B1_MASK	(FTDI_RS_BI)
/* End TIOCMIWAIT */

/* function prototypes for a FTDI serial converter */
static int  ftdi_sio_probe(struct usb_serial *serial,
					const struct usb_device_id *id);
static int  ftdi_sio_port_probe(struct usb_serial_port *port);
static int  ftdi_sio_port_remove(struct usb_serial_port *port);
static int  ftdi_open(struct tty_struct *tty, struct usb_serial_port *port);
static void ftdi_dtr_rts(struct usb_serial_port *port, int on);
static void ftdi_process_read_urb(struct urb *urb);
static int ftdi_prepare_write_buffer(struct usb_serial_port *port,
						void *dest, size_t size);
static void ftdi_set_termios(struct tty_struct *tty,
			struct usb_serial_port *port, struct ktermios *old);
static int  ftdi_tiocmget(struct tty_struct *tty);
static int  ftdi_tiocmset(struct tty_struct *tty,
			unsigned int set, unsigned int clear);
static int  ftdi_ioctl(struct tty_struct *tty,
			unsigned int cmd, unsigned long arg);
static int get_serial_info(struct tty_struct *tty,
				struct serial_struct *ss);
static int set_serial_info(struct tty_struct *tty,
				struct serial_struct *ss);
static void ftdi_break_ctl(struct tty_struct *tty, int break_state);
static bool ftdi_tx_empty(struct usb_serial_port *port);
static int ftdi_get_modem_status(struct usb_serial_port *port,
						unsigned char status[2]);

static unsigned short int ftdi_232am_baud_base_to_divisor(int baud, int base);
static unsigned short int ftdi_232am_baud_to_divisor(int baud);
static u32 ftdi_232bm_baud_base_to_divisor(int baud, int base);
static u32 ftdi_232bm_baud_to_divisor(int baud);
static u32 ftdi_2232h_baud_base_to_divisor(int baud, int base);
static u32 ftdi_2232h_baud_to_divisor(int baud);

static struct usb_serial_driver ftdi_sio_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"ftdi_sio",
	},
	.description =		"FTDI USB Serial Device",
	.id_table =		id_table_combined,
	.num_ports =		1,
	.bulk_in_size =		512,
	.bulk_out_size =	256,
	.probe =		ftdi_sio_probe,
	.port_probe =		ftdi_sio_port_probe,
	.port_remove =		ftdi_sio_port_remove,
	.open =			ftdi_open,
	.dtr_rts =		ftdi_dtr_rts,
	.throttle =		usb_serial_generic_throttle,
	.unthrottle =		usb_serial_generic_unthrottle,
	.process_read_urb =	ftdi_process_read_urb,
	.prepare_write_buffer =	ftdi_prepare_write_buffer,
	.tiocmget =		ftdi_tiocmget,
	.tiocmset =		ftdi_tiocmset,
	.tiocmiwait =		usb_serial_generic_tiocmiwait,
	.get_icount =           usb_serial_generic_get_icount,
	.ioctl =		ftdi_ioctl,
	.get_serial =		get_serial_info,
	.set_serial =		set_serial_info,
	.set_termios =		ftdi_set_termios,
	.break_ctl =		ftdi_break_ctl,
	.tx_empty =		ftdi_tx_empty,
};

static struct usb_serial_driver * const serial_drivers[] = {
	&ftdi_sio_device, NULL
};


#define WDR_TIMEOUT 5000 /* default urb timeout */
#define WDR_SHORT_TIMEOUT 1000	/* shorter urb timeout */

/*
 * ***************************************************************************
 * Utility functions
 * ***************************************************************************
 */

static unsigned short int ftdi_232am_baud_base_to_divisor(int baud, int base)
{
	unsigned short int divisor;
	/* divisor shifted 3 bits to the left */
	int divisor3 = DIV_ROUND_CLOSEST(base, 2 * baud);
	if ((divisor3 & 0x7) == 7)
		divisor3++; /* round x.7/8 up to x+1 */
	divisor = divisor3 >> 3;
	divisor3 &= 0x7;
	if (divisor3 == 1)
		divisor |= 0xc000;
	else if (divisor3 >= 4)
		divisor |= 0x4000;
	else if (divisor3 != 0)
		divisor |= 0x8000;
	else if (divisor == 1)
		divisor = 0;	/* special case for maximum baud rate */
	return divisor;
}

static unsigned short int ftdi_232am_baud_to_divisor(int baud)
{
	 return ftdi_232am_baud_base_to_divisor(baud, 48000000);
}

static u32 ftdi_232bm_baud_base_to_divisor(int baud, int base)
{
	static const unsigned char divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
	u32 divisor;
	/* divisor shifted 3 bits to the left */
	int divisor3 = DIV_ROUND_CLOSEST(base, 2 * baud);
	divisor = divisor3 >> 3;
	divisor |= (u32)divfrac[divisor3 & 0x7] << 14;
	/* Deal with special cases for highest baud rates. */
	if (divisor == 1)
		divisor = 0;
	else if (divisor == 0x4001)
		divisor = 1;
	return divisor;
}

static u32 ftdi_232bm_baud_to_divisor(int baud)
{
	 return ftdi_232bm_baud_base_to_divisor(baud, 48000000);
}

static u32 ftdi_2232h_baud_base_to_divisor(int baud, int base)
{
	static const unsigned char divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
	u32 divisor;
	int divisor3;

	/* hi-speed baud rate is 10-bit sampling instead of 16-bit */
	divisor3 = DIV_ROUND_CLOSEST(8 * base, 10 * baud);

	divisor = divisor3 >> 3;
	divisor |= (u32)divfrac[divisor3 & 0x7] << 14;
	/* Deal with special cases for highest baud rates. */
	if (divisor == 1)
		divisor = 0;
	else if (divisor == 0x4001)
		divisor = 1;
	/*
	 * Set this bit to turn off a divide by 2.5 on baud rate generator
	 * This enables baud rates up to 12Mbaud but cannot reach below 1200
	 * baud with this bit set
	 */
	divisor |= 0x00020000;
	return divisor;
}

static u32 ftdi_2232h_baud_to_divisor(int baud)
{
	 return ftdi_2232h_baud_base_to_divisor(baud, 120000000);
}

#define set_mctrl(port, set)		update_mctrl((port), (set), 0)
#define clear_mctrl(port, clear)	update_mctrl((port), 0, (clear))

static int update_mctrl(struct usb_serial_port *port, unsigned int set,
							unsigned int clear)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct device *dev = &port->dev;
	unsigned value;
	int rv;

	if (((set | clear) & (TIOCM_DTR | TIOCM_RTS)) == 0) {
		dev_dbg(dev, "%s - DTR|RTS not being set|cleared\n", __func__);
		return 0;	/* no change */
	}

	clear &= ~set;	/* 'set' takes precedence over 'clear' */
	value = 0;
	if (clear & TIOCM_DTR)
		value |= FTDI_SIO_SET_DTR_LOW;
	if (clear & TIOCM_RTS)
		value |= FTDI_SIO_SET_RTS_LOW;
	if (set & TIOCM_DTR)
		value |= FTDI_SIO_SET_DTR_HIGH;
	if (set & TIOCM_RTS)
		value |= FTDI_SIO_SET_RTS_HIGH;
	rv = usb_control_msg(port->serial->dev,
			       usb_sndctrlpipe(port->serial->dev, 0),
			       FTDI_SIO_SET_MODEM_CTRL_REQUEST,
			       FTDI_SIO_SET_MODEM_CTRL_REQUEST_TYPE,
			       value, priv->interface,
			       NULL, 0, WDR_TIMEOUT);
	if (rv < 0) {
		dev_dbg(dev, "%s Error from MODEM_CTRL urb: DTR %s, RTS %s\n",
			__func__,
			(set & TIOCM_DTR) ? "HIGH" : (clear & TIOCM_DTR) ? "LOW" : "unchanged",
			(set & TIOCM_RTS) ? "HIGH" : (clear & TIOCM_RTS) ? "LOW" : "unchanged");
		rv = usb_translate_errors(rv);
	} else {
		dev_dbg(dev, "%s - DTR %s, RTS %s\n", __func__,
			(set & TIOCM_DTR) ? "HIGH" : (clear & TIOCM_DTR) ? "LOW" : "unchanged",
			(set & TIOCM_RTS) ? "HIGH" : (clear & TIOCM_RTS) ? "LOW" : "unchanged");
		/* FIXME: locking on last_dtr_rts */
		priv->last_dtr_rts = (priv->last_dtr_rts & ~clear) | set;
	}
	return rv;
}


static u32 get_ftdi_divisor(struct tty_struct *tty,
						struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct device *dev = &port->dev;
	u32 div_value = 0;
	int div_okay = 1;
	int baud;

	baud = tty_get_baud_rate(tty);
	dev_dbg(dev, "%s - tty_get_baud_rate reports speed %d\n", __func__, baud);

	/*
	 * Observe deprecated async-compatible custom_divisor hack, update
	 * baudrate if needed.
	 */
	if (baud == 38400 &&
	    ((priv->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST) &&
	     (priv->custom_divisor)) {
		baud = priv->baud_base / priv->custom_divisor;
		dev_dbg(dev, "%s - custom divisor %d sets baud rate to %d\n",
			__func__, priv->custom_divisor, baud);
	}

	if (!baud)
		baud = 9600;
	switch (priv->chip_type) {
	case SIO: /* SIO chip */
		switch (baud) {
		case 300: div_value = ftdi_sio_b300; break;
		case 600: div_value = ftdi_sio_b600; break;
		case 1200: div_value = ftdi_sio_b1200; break;
		case 2400: div_value = ftdi_sio_b2400; break;
		case 4800: div_value = ftdi_sio_b4800; break;
		case 9600: div_value = ftdi_sio_b9600; break;
		case 19200: div_value = ftdi_sio_b19200; break;
		case 38400: div_value = ftdi_sio_b38400; break;
		case 57600: div_value = ftdi_sio_b57600;  break;
		case 115200: div_value = ftdi_sio_b115200; break;
		} /* baud */
		if (div_value == 0) {
			dev_dbg(dev, "%s - Baudrate (%d) requested is not supported\n",
				__func__,  baud);
			div_value = ftdi_sio_b9600;
			baud = 9600;
			div_okay = 0;
		}
		break;
	case FT8U232AM: /* 8U232AM chip */
		if (baud <= 3000000) {
			div_value = ftdi_232am_baud_to_divisor(baud);
		} else {
			dev_dbg(dev, "%s - Baud rate too high!\n", __func__);
			baud = 9600;
			div_value = ftdi_232am_baud_to_divisor(9600);
			div_okay = 0;
		}
		break;
	case FT232BM: /* FT232BM chip */
	case FT2232C: /* FT2232C chip */
	case FT232RL: /* FT232RL chip */
	case FTX:     /* FT-X series */
		if (baud <= 3000000) {
			u16 product_id = le16_to_cpu(
				port->serial->dev->descriptor.idProduct);
			if (((product_id == FTDI_NDI_HUC_PID)		||
			     (product_id == FTDI_NDI_SPECTRA_SCU_PID)	||
			     (product_id == FTDI_NDI_FUTURE_2_PID)	||
			     (product_id == FTDI_NDI_FUTURE_3_PID)	||
			     (product_id == FTDI_NDI_AURORA_SCU_PID))	&&
			    (baud == 19200)) {
				baud = 1200000;
			}
			div_value = ftdi_232bm_baud_to_divisor(baud);
		} else {
			dev_dbg(dev, "%s - Baud rate too high!\n", __func__);
			div_value = ftdi_232bm_baud_to_divisor(9600);
			div_okay = 0;
			baud = 9600;
		}
		break;
	case FT2232H: /* FT2232H chip */
	case FT4232H: /* FT4232H chip */
	case FT232H:  /* FT232H chip */
		if ((baud <= 12000000) && (baud >= 1200)) {
			div_value = ftdi_2232h_baud_to_divisor(baud);
		} else if (baud < 1200) {
			div_value = ftdi_232bm_baud_to_divisor(baud);
		} else {
			dev_dbg(dev, "%s - Baud rate too high!\n", __func__);
			div_value = ftdi_232bm_baud_to_divisor(9600);
			div_okay = 0;
			baud = 9600;
		}
		break;
	} /* priv->chip_type */

	if (div_okay) {
		dev_dbg(dev, "%s - Baud rate set to %d (divisor 0x%lX) on chip %s\n",
			__func__, baud, (unsigned long)div_value,
			ftdi_chip_name[priv->chip_type]);
	}

	tty_encode_baud_rate(tty, baud, baud);
	return div_value;
}

static int change_speed(struct tty_struct *tty, struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	u16 value;
	u16 index;
	u32 index_value;
	int rv;

	index_value = get_ftdi_divisor(tty, port);
	value = (u16)index_value;
	index = (u16)(index_value >> 16);
	if ((priv->chip_type == FT2232C) || (priv->chip_type == FT2232H) ||
		(priv->chip_type == FT4232H) || (priv->chip_type == FT232H)) {
		/* Probably the BM type needs the MSB of the encoded fractional
		 * divider also moved like for the chips above. Any infos? */
		index = (u16)((index << 8) | priv->interface);
	}

	rv = usb_control_msg(port->serial->dev,
			    usb_sndctrlpipe(port->serial->dev, 0),
			    FTDI_SIO_SET_BAUDRATE_REQUEST,
			    FTDI_SIO_SET_BAUDRATE_REQUEST_TYPE,
			    value, index,
			    NULL, 0, WDR_SHORT_TIMEOUT);
	return rv;
}

static int write_latency_timer(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct usb_device *udev = port->serial->dev;
	int rv;
	int l = priv->latency;

	if (priv->chip_type == SIO || priv->chip_type == FT8U232AM)
		return -EINVAL;

	if (priv->flags & ASYNC_LOW_LATENCY)
		l = 1;

	dev_dbg(&port->dev, "%s: setting latency timer = %i\n", __func__, l);

	rv = usb_control_msg(udev,
			     usb_sndctrlpipe(udev, 0),
			     FTDI_SIO_SET_LATENCY_TIMER_REQUEST,
			     FTDI_SIO_SET_LATENCY_TIMER_REQUEST_TYPE,
			     l, priv->interface,
			     NULL, 0, WDR_TIMEOUT);
	if (rv < 0)
		dev_err(&port->dev, "Unable to write latency timer: %i\n", rv);
	return rv;
}

static int _read_latency_timer(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct usb_device *udev = port->serial->dev;
	unsigned char *buf;
	int rv;

	buf = kmalloc(1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	rv = usb_control_msg(udev,
			     usb_rcvctrlpipe(udev, 0),
			     FTDI_SIO_GET_LATENCY_TIMER_REQUEST,
			     FTDI_SIO_GET_LATENCY_TIMER_REQUEST_TYPE,
			     0, priv->interface,
			     buf, 1, WDR_TIMEOUT);
	if (rv < 1) {
		if (rv >= 0)
			rv = -EIO;
	} else {
		rv = buf[0];
	}

	kfree(buf);

	return rv;
}

static int read_latency_timer(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	int rv;

	if (priv->chip_type == SIO || priv->chip_type == FT8U232AM)
		return -EINVAL;

	rv = _read_latency_timer(port);
	if (rv < 0) {
		dev_err(&port->dev, "Unable to read latency timer: %i\n", rv);
		return rv;
	}

	priv->latency = rv;

	return 0;
}

static int get_serial_info(struct tty_struct *tty,
				struct serial_struct *ss)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	ss->flags = priv->flags;
	ss->baud_base = priv->baud_base;
	ss->custom_divisor = priv->custom_divisor;
	return 0;
}

static int set_serial_info(struct tty_struct *tty,
	struct serial_struct *ss)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct ftdi_private old_priv;

	mutex_lock(&priv->cfg_lock);
	old_priv = *priv;

	/* Do error checking and permission checking */

	if (!capable(CAP_SYS_ADMIN)) {
		if ((ss->flags ^ priv->flags) & ~ASYNC_USR_MASK) {
			mutex_unlock(&priv->cfg_lock);
			return -EPERM;
		}
		priv->flags = ((priv->flags & ~ASYNC_USR_MASK) |
			       (ss->flags & ASYNC_USR_MASK));
		priv->custom_divisor = ss->custom_divisor;
		goto check_and_exit;
	}

	if (ss->baud_base != priv->baud_base) {
		mutex_unlock(&priv->cfg_lock);
		return -EINVAL;
	}

	/* Make the changes - these are privileged changes! */

	priv->flags = ((priv->flags & ~ASYNC_FLAGS) |
					(ss->flags & ASYNC_FLAGS));
	priv->custom_divisor = ss->custom_divisor;

check_and_exit:
	write_latency_timer(port);

	if ((priv->flags ^ old_priv.flags) & ASYNC_SPD_MASK ||
			((priv->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST &&
			 priv->custom_divisor != old_priv.custom_divisor)) {

		/* warn about deprecation unless clearing */
		if (priv->flags & ASYNC_SPD_MASK)
			dev_warn_ratelimited(&port->dev, "use of SPD flags is deprecated\n");

		change_speed(tty, port);
	}
	mutex_unlock(&priv->cfg_lock);
	return 0;
}

static int get_lsr_info(struct usb_serial_port *port,
			unsigned int __user *retinfo)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	unsigned int result = 0;

	if (priv->transmit_empty)
		result = TIOCSER_TEMT;

	if (copy_to_user(retinfo, &result, sizeof(unsigned int)))
		return -EFAULT;
	return 0;
}


/* Determine type of FTDI chip based on USB config and descriptor. */
static void ftdi_determine_type(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct usb_serial *serial = port->serial;
	struct usb_device *udev = serial->dev;
	unsigned version;
	unsigned interfaces;

	/* Assume it is not the original SIO device for now. */
	priv->baud_base = 48000000 / 2;

	version = le16_to_cpu(udev->descriptor.bcdDevice);
	interfaces = udev->actconfig->desc.bNumInterfaces;
	dev_dbg(&port->dev, "%s: bcdDevice = 0x%x, bNumInterfaces = %u\n", __func__,
		version, interfaces);
	if (interfaces > 1) {
		int inter;

		/* Multiple interfaces.*/
		if (version == 0x0800) {
			priv->chip_type = FT4232H;
			/* Hi-speed - baud clock runs at 120MHz */
			priv->baud_base = 120000000 / 2;
		} else if (version == 0x0700) {
			priv->chip_type = FT2232H;
			/* Hi-speed - baud clock runs at 120MHz */
			priv->baud_base = 120000000 / 2;
		} else
			priv->chip_type = FT2232C;

		/* Determine interface code. */
		inter = serial->interface->altsetting->desc.bInterfaceNumber;
		if (inter == 0) {
			priv->interface = INTERFACE_A;
		} else  if (inter == 1) {
			priv->interface = INTERFACE_B;
		} else  if (inter == 2) {
			priv->interface = INTERFACE_C;
		} else  if (inter == 3) {
			priv->interface = INTERFACE_D;
		}
		/* BM-type devices have a bug where bcdDevice gets set
		 * to 0x200 when iSerialNumber is 0.  */
		if (version < 0x500) {
			dev_dbg(&port->dev,
				"%s: something fishy - bcdDevice too low for multi-interface device\n",
				__func__);
		}
	} else if (version < 0x200) {
		/* Old device.  Assume it's the original SIO. */
		priv->chip_type = SIO;
		priv->baud_base = 12000000 / 16;
	} else if (version < 0x400) {
		/* Assume it's an FT8U232AM (or FT8U245AM) */
		priv->chip_type = FT8U232AM;
		/*
		 * It might be a BM type because of the iSerialNumber bug.
		 * If iSerialNumber==0 and the latency timer is readable,
		 * assume it is BM type.
		 */
		if (udev->descriptor.iSerialNumber == 0 &&
				_read_latency_timer(port) >= 0) {
			dev_dbg(&port->dev,
				"%s: has latency timer so not an AM type\n",
				__func__);
			priv->chip_type = FT232BM;
		}
	} else if (version < 0x600) {
		/* Assume it's an FT232BM (or FT245BM) */
		priv->chip_type = FT232BM;
	} else if (version < 0x900) {
		/* Assume it's an FT232RL */
		priv->chip_type = FT232RL;
	} else if (version < 0x1000) {
		/* Assume it's an FT232H */
		priv->chip_type = FT232H;
	} else {
		/* Assume it's an FT-X series device */
		priv->chip_type = FTX;
	}

	dev_info(&udev->dev, "Detected %s\n", ftdi_chip_name[priv->chip_type]);
}


/*
 * Determine the maximum packet size for the device. This depends on the chip
 * type and the USB host capabilities. The value should be obtained from the
 * device descriptor as the chip will use the appropriate values for the host.
 */
static void ftdi_set_max_packet_size(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct usb_interface *interface = port->serial->interface;
	struct usb_endpoint_descriptor *ep_desc;
	unsigned num_endpoints;
	unsigned i;

	num_endpoints = interface->cur_altsetting->desc.bNumEndpoints;
	if (!num_endpoints)
		return;

	/*
	 * NOTE: Some customers have programmed FT232R/FT245R devices
	 * with an endpoint size of 0 - not good. In this case, we
	 * want to override the endpoint descriptor setting and use a
	 * value of 64 for wMaxPacketSize.
	 */
	for (i = 0; i < num_endpoints; i++) {
		ep_desc = &interface->cur_altsetting->endpoint[i].desc;
		if (!ep_desc->wMaxPacketSize) {
			ep_desc->wMaxPacketSize = cpu_to_le16(0x40);
			dev_warn(&port->dev, "Overriding wMaxPacketSize on endpoint %d\n",
					usb_endpoint_num(ep_desc));
		}
	}

	/* Set max packet size based on last descriptor. */
	priv->max_packet_size = usb_endpoint_maxp(ep_desc);
}


/*
 * ***************************************************************************
 * Sysfs Attribute
 * ***************************************************************************
 */

static ssize_t latency_timer_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct usb_serial_port *port = to_usb_serial_port(dev);
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	if (priv->flags & ASYNC_LOW_LATENCY)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "%i\n", priv->latency);
}

/* Write a new value of the latency timer, in units of milliseconds. */
static ssize_t latency_timer_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *valbuf, size_t count)
{
	struct usb_serial_port *port = to_usb_serial_port(dev);
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	u8 v;
	int rv;

	if (kstrtou8(valbuf, 10, &v))
		return -EINVAL;

	priv->latency = v;
	rv = write_latency_timer(port);
	if (rv < 0)
		return -EIO;
	return count;
}
static DEVICE_ATTR_RW(latency_timer);

/* Write an event character directly to the FTDI register.  The ASCII
   value is in the low 8 bits, with the enable bit in the 9th bit. */
static ssize_t event_char_store(struct device *dev,
	struct device_attribute *attr, const char *valbuf, size_t count)
{
	struct usb_serial_port *port = to_usb_serial_port(dev);
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct usb_device *udev = port->serial->dev;
	unsigned int v;
	int rv;

	if (kstrtouint(valbuf, 0, &v) || v >= 0x200)
		return -EINVAL;

	dev_dbg(&port->dev, "%s: setting event char = 0x%03x\n", __func__, v);

	rv = usb_control_msg(udev,
			     usb_sndctrlpipe(udev, 0),
			     FTDI_SIO_SET_EVENT_CHAR_REQUEST,
			     FTDI_SIO_SET_EVENT_CHAR_REQUEST_TYPE,
			     v, priv->interface,
			     NULL, 0, WDR_TIMEOUT);
	if (rv < 0) {
		dev_dbg(&port->dev, "Unable to write event character: %i\n", rv);
		return -EIO;
	}

	return count;
}
static DEVICE_ATTR_WO(event_char);

static int create_sysfs_attrs(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	int retval = 0;

	/* XXX I've no idea if the original SIO supports the event_char
	 * sysfs parameter, so I'm playing it safe.  */
	if (priv->chip_type != SIO) {
		dev_dbg(&port->dev, "sysfs attributes for %s\n", ftdi_chip_name[priv->chip_type]);
		retval = device_create_file(&port->dev, &dev_attr_event_char);
		if ((!retval) &&
		    (priv->chip_type == FT232BM ||
		     priv->chip_type == FT2232C ||
		     priv->chip_type == FT232RL ||
		     priv->chip_type == FT2232H ||
		     priv->chip_type == FT4232H ||
		     priv->chip_type == FT232H ||
		     priv->chip_type == FTX)) {
			retval = device_create_file(&port->dev,
						    &dev_attr_latency_timer);
		}
	}
	return retval;
}

static void remove_sysfs_attrs(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	/* XXX see create_sysfs_attrs */
	if (priv->chip_type != SIO) {
		device_remove_file(&port->dev, &dev_attr_event_char);
		if (priv->chip_type == FT232BM ||
		    priv->chip_type == FT2232C ||
		    priv->chip_type == FT232RL ||
		    priv->chip_type == FT2232H ||
		    priv->chip_type == FT4232H ||
		    priv->chip_type == FT232H ||
		    priv->chip_type == FTX) {
			device_remove_file(&port->dev, &dev_attr_latency_timer);
		}
	}

}

#ifdef CONFIG_GPIOLIB

static int ftdi_set_bitmode(struct usb_serial_port *port, u8 mode)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct usb_serial *serial = port->serial;
	int result;
	u16 val;

	result = usb_autopm_get_interface(serial->interface);
	if (result)
		return result;

	val = (mode << 8) | (priv->gpio_output << 4) | priv->gpio_value;
	result = usb_control_msg(serial->dev,
				 usb_sndctrlpipe(serial->dev, 0),
				 FTDI_SIO_SET_BITMODE_REQUEST,
				 FTDI_SIO_SET_BITMODE_REQUEST_TYPE, val,
				 priv->interface, NULL, 0, WDR_TIMEOUT);
	if (result < 0) {
		dev_err(&serial->interface->dev,
			"bitmode request failed for value 0x%04x: %d\n",
			val, result);
	}

	usb_autopm_put_interface(serial->interface);

	return result;
}

static int ftdi_set_cbus_pins(struct usb_serial_port *port)
{
	return ftdi_set_bitmode(port, FTDI_SIO_BITMODE_CBUS);
}

static int ftdi_exit_cbus_mode(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	priv->gpio_output = 0;
	priv->gpio_value = 0;
	return ftdi_set_bitmode(port, FTDI_SIO_BITMODE_RESET);
}

static int ftdi_gpio_request(struct gpio_chip *gc, unsigned int offset)
{
	struct usb_serial_port *port = gpiochip_get_data(gc);
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	int result;

	if (priv->gpio_altfunc & BIT(offset))
		return -ENODEV;

	mutex_lock(&priv->gpio_lock);
	if (!priv->gpio_used) {
		/* Set default pin states, as we cannot get them from device */
		priv->gpio_output = 0x00;
		priv->gpio_value = 0x00;
		result = ftdi_set_cbus_pins(port);
		if (result) {
			mutex_unlock(&priv->gpio_lock);
			return result;
		}

		priv->gpio_used = true;
	}
	mutex_unlock(&priv->gpio_lock);

	return 0;
}

static int ftdi_read_cbus_pins(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct usb_serial *serial = port->serial;
	unsigned char *buf;
	int result;

	result = usb_autopm_get_interface(serial->interface);
	if (result)
		return result;

	buf = kmalloc(1, GFP_KERNEL);
	if (!buf) {
		usb_autopm_put_interface(serial->interface);
		return -ENOMEM;
	}

	result = usb_control_msg(serial->dev,
				 usb_rcvctrlpipe(serial->dev, 0),
				 FTDI_SIO_READ_PINS_REQUEST,
				 FTDI_SIO_READ_PINS_REQUEST_TYPE, 0,
				 priv->interface, buf, 1, WDR_TIMEOUT);
	if (result < 1) {
		if (result >= 0)
			result = -EIO;
	} else {
		result = buf[0];
	}

	kfree(buf);
	usb_autopm_put_interface(serial->interface);

	return result;
}

static int ftdi_gpio_get(struct gpio_chip *gc, unsigned int gpio)
{
	struct usb_serial_port *port = gpiochip_get_data(gc);
	int result;

	result = ftdi_read_cbus_pins(port);
	if (result < 0)
		return result;

	return !!(result & BIT(gpio));
}

static void ftdi_gpio_set(struct gpio_chip *gc, unsigned int gpio, int value)
{
	struct usb_serial_port *port = gpiochip_get_data(gc);
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	mutex_lock(&priv->gpio_lock);

	if (value)
		priv->gpio_value |= BIT(gpio);
	else
		priv->gpio_value &= ~BIT(gpio);

	ftdi_set_cbus_pins(port);

	mutex_unlock(&priv->gpio_lock);
}

static int ftdi_gpio_get_multiple(struct gpio_chip *gc, unsigned long *mask,
					unsigned long *bits)
{
	struct usb_serial_port *port = gpiochip_get_data(gc);
	int result;

	result = ftdi_read_cbus_pins(port);
	if (result < 0)
		return result;

	*bits = result & *mask;

	return 0;
}

static void ftdi_gpio_set_multiple(struct gpio_chip *gc, unsigned long *mask,
					unsigned long *bits)
{
	struct usb_serial_port *port = gpiochip_get_data(gc);
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	mutex_lock(&priv->gpio_lock);

	priv->gpio_value &= ~(*mask);
	priv->gpio_value |= *bits & *mask;
	ftdi_set_cbus_pins(port);

	mutex_unlock(&priv->gpio_lock);
}

static int ftdi_gpio_direction_get(struct gpio_chip *gc, unsigned int gpio)
{
	struct usb_serial_port *port = gpiochip_get_data(gc);
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	return !(priv->gpio_output & BIT(gpio));
}

static int ftdi_gpio_direction_input(struct gpio_chip *gc, unsigned int gpio)
{
	struct usb_serial_port *port = gpiochip_get_data(gc);
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	int result;

	mutex_lock(&priv->gpio_lock);

	priv->gpio_output &= ~BIT(gpio);
	result = ftdi_set_cbus_pins(port);

	mutex_unlock(&priv->gpio_lock);

	return result;
}

static int ftdi_gpio_direction_output(struct gpio_chip *gc, unsigned int gpio,
					int value)
{
	struct usb_serial_port *port = gpiochip_get_data(gc);
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	int result;

	mutex_lock(&priv->gpio_lock);

	priv->gpio_output |= BIT(gpio);
	if (value)
		priv->gpio_value |= BIT(gpio);
	else
		priv->gpio_value &= ~BIT(gpio);

	result = ftdi_set_cbus_pins(port);

	mutex_unlock(&priv->gpio_lock);

	return result;
}

static int ftdi_read_eeprom(struct usb_serial *serial, void *dst, u16 addr,
				u16 nbytes)
{
	int read = 0;

	if (addr % 2 != 0)
		return -EINVAL;
	if (nbytes % 2 != 0)
		return -EINVAL;

	/* Read EEPROM two bytes at a time */
	while (read < nbytes) {
		int rv;

		rv = usb_control_msg(serial->dev,
				     usb_rcvctrlpipe(serial->dev, 0),
				     FTDI_SIO_READ_EEPROM_REQUEST,
				     FTDI_SIO_READ_EEPROM_REQUEST_TYPE,
				     0, (addr + read) / 2, dst + read, 2,
				     WDR_TIMEOUT);
		if (rv < 2) {
			if (rv >= 0)
				return -EIO;
			else
				return rv;
		}

		read += rv;
	}

	return 0;
}

static int ftdi_gpio_init_ft232h(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	u16 cbus_config;
	u8 *buf;
	int ret;
	int i;

	buf = kmalloc(4, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = ftdi_read_eeprom(port->serial, buf, 0x1a, 4);
	if (ret < 0)
		goto out_free;

	/*
	 * FT232H CBUS Memory Map
	 *
	 * 0x1a: X- (upper nibble -> AC5)
	 * 0x1b: -X (lower nibble -> AC6)
	 * 0x1c: XX (upper nibble -> AC9 | lower nibble -> AC8)
	 */
	cbus_config = buf[2] << 8 | (buf[1] & 0xf) << 4 | (buf[0] & 0xf0) >> 4;

	priv->gc.ngpio = 4;
	priv->gpio_altfunc = 0xff;

	for (i = 0; i < priv->gc.ngpio; ++i) {
		if ((cbus_config & 0xf) == FTDI_FTX_CBUS_MUX_GPIO)
			priv->gpio_altfunc &= ~BIT(i);
		cbus_config >>= 4;
	}

out_free:
	kfree(buf);

	return ret;
}

static int ftdi_gpio_init_ft232r(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	u16 cbus_config;
	u8 *buf;
	int ret;
	int i;

	buf = kmalloc(2, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = ftdi_read_eeprom(port->serial, buf, 0x14, 2);
	if (ret < 0)
		goto out_free;

	cbus_config = le16_to_cpup((__le16 *)buf);
	dev_dbg(&port->dev, "cbus_config = 0x%04x\n", cbus_config);

	priv->gc.ngpio = 4;

	priv->gpio_altfunc = 0xff;
	for (i = 0; i < priv->gc.ngpio; ++i) {
		if ((cbus_config & 0xf) == FTDI_FT232R_CBUS_MUX_GPIO)
			priv->gpio_altfunc &= ~BIT(i);
		cbus_config >>= 4;
	}
out_free:
	kfree(buf);

	return ret;
}

static int ftdi_gpio_init_ftx(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct usb_serial *serial = port->serial;
	const u16 cbus_cfg_addr = 0x1a;
	const u16 cbus_cfg_size = 4;
	u8 *cbus_cfg_buf;
	int result;
	u8 i;

	cbus_cfg_buf = kmalloc(cbus_cfg_size, GFP_KERNEL);
	if (!cbus_cfg_buf)
		return -ENOMEM;

	result = ftdi_read_eeprom(serial, cbus_cfg_buf,
				  cbus_cfg_addr, cbus_cfg_size);
	if (result < 0)
		goto out_free;

	/* FIXME: FT234XD alone has 1 GPIO, but how to recognize this IC? */
	priv->gc.ngpio = 4;

	/* Determine which pins are configured for CBUS bitbanging */
	priv->gpio_altfunc = 0xff;
	for (i = 0; i < priv->gc.ngpio; ++i) {
		if (cbus_cfg_buf[i] == FTDI_FTX_CBUS_MUX_GPIO)
			priv->gpio_altfunc &= ~BIT(i);
	}

out_free:
	kfree(cbus_cfg_buf);

	return result;
}

static int ftdi_gpio_init(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct usb_serial *serial = port->serial;
	int result;

	switch (priv->chip_type) {
	case FT232H:
		result = ftdi_gpio_init_ft232h(port);
		break;
	case FT232RL:
		result = ftdi_gpio_init_ft232r(port);
		break;
	case FTX:
		result = ftdi_gpio_init_ftx(port);
		break;
	default:
		return 0;
	}

	if (result < 0)
		return result;

	mutex_init(&priv->gpio_lock);

	priv->gc.label = "ftdi-cbus";
	priv->gc.request = ftdi_gpio_request;
	priv->gc.get_direction = ftdi_gpio_direction_get;
	priv->gc.direction_input = ftdi_gpio_direction_input;
	priv->gc.direction_output = ftdi_gpio_direction_output;
	priv->gc.get = ftdi_gpio_get;
	priv->gc.set = ftdi_gpio_set;
	priv->gc.get_multiple = ftdi_gpio_get_multiple;
	priv->gc.set_multiple = ftdi_gpio_set_multiple;
	priv->gc.owner = THIS_MODULE;
	priv->gc.parent = &serial->interface->dev;
	priv->gc.base = -1;
	priv->gc.can_sleep = true;

	result = gpiochip_add_data(&priv->gc, port);
	if (!result)
		priv->gpio_registered = true;

	return result;
}

static void ftdi_gpio_remove(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	if (priv->gpio_registered) {
		gpiochip_remove(&priv->gc);
		priv->gpio_registered = false;
	}

	if (priv->gpio_used) {
		/* Exiting CBUS-mode does not reset pin states. */
		ftdi_exit_cbus_mode(port);
		priv->gpio_used = false;
	}
}

#else

static int ftdi_gpio_init(struct usb_serial_port *port)
{
	return 0;
}

static void ftdi_gpio_remove(struct usb_serial_port *port) { }

#endif	/* CONFIG_GPIOLIB */

/*
 * ***************************************************************************
 * FTDI driver specific functions
 * ***************************************************************************
 */

/* Probe function to check for special devices */
static int ftdi_sio_probe(struct usb_serial *serial,
					const struct usb_device_id *id)
{
	const struct ftdi_sio_quirk *quirk =
				(struct ftdi_sio_quirk *)id->driver_info;

	if (quirk && quirk->probe) {
		int ret = quirk->probe(serial);
		if (ret != 0)
			return ret;
	}

	usb_set_serial_data(serial, (void *)id->driver_info);

	return 0;
}

static int ftdi_sio_port_probe(struct usb_serial_port *port)
{
	struct ftdi_private *priv;
	const struct ftdi_sio_quirk *quirk = usb_get_serial_data(port->serial);
	int result;

	priv = kzalloc(sizeof(struct ftdi_private), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	mutex_init(&priv->cfg_lock);

	if (quirk && quirk->port_probe)
		quirk->port_probe(priv);

	usb_set_serial_port_data(port, priv);

	ftdi_determine_type(port);
	ftdi_set_max_packet_size(port);
	if (read_latency_timer(port) < 0)
		priv->latency = 16;
	write_latency_timer(port);
	create_sysfs_attrs(port);

	result = ftdi_gpio_init(port);
	if (result < 0) {
		dev_err(&port->serial->interface->dev,
			"GPIO initialisation failed: %d\n",
			result);
	}

	return 0;
}

/* Setup for the USB-UIRT device, which requires hardwired
 * baudrate (38400 gets mapped to 312500) */
/* Called from usbserial:serial_probe */
static void ftdi_USB_UIRT_setup(struct ftdi_private *priv)
{
	priv->flags |= ASYNC_SPD_CUST;
	priv->custom_divisor = 77;
	priv->force_baud = 38400;
}

/* Setup for the HE-TIRA1 device, which requires hardwired
 * baudrate (38400 gets mapped to 100000) and RTS-CTS enabled.  */

static void ftdi_HE_TIRA1_setup(struct ftdi_private *priv)
{
	priv->flags |= ASYNC_SPD_CUST;
	priv->custom_divisor = 240;
	priv->force_baud = 38400;
	priv->force_rtscts = 1;
}

/*
 * Module parameter to control latency timer for NDI FTDI-based USB devices.
 * If this value is not set in /etc/modprobe.d/ its value will be set
 * to 1ms.
 */
static int ndi_latency_timer = 1;

/* Setup for the NDI FTDI-based USB devices, which requires hardwired
 * baudrate (19200 gets mapped to 1200000).
 *
 * Called from usbserial:serial_probe.
 */
static int ftdi_NDI_device_setup(struct usb_serial *serial)
{
	struct usb_device *udev = serial->dev;
	int latency = ndi_latency_timer;

	if (latency == 0)
		latency = 1;
	if (latency > 99)
		latency = 99;

	dev_dbg(&udev->dev, "%s setting NDI device latency to %d\n", __func__, latency);
	dev_info(&udev->dev, "NDI device with a latency value of %d\n", latency);

	/* FIXME: errors are not returned */
	usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
				FTDI_SIO_SET_LATENCY_TIMER_REQUEST,
				FTDI_SIO_SET_LATENCY_TIMER_REQUEST_TYPE,
				latency, 0, NULL, 0, WDR_TIMEOUT);
	return 0;
}

/*
 * First port on JTAG adaptors such as Olimex arm-usb-ocd or the FIC/OpenMoko
 * Neo1973 Debug Board is reserved for JTAG interface and can be accessed from
 * userspace using openocd.
 */
static int ftdi_jtag_probe(struct usb_serial *serial)
{
	struct usb_device *udev = serial->dev;
	struct usb_interface *interface = serial->interface;

	if (interface == udev->actconfig->interface[0]) {
		dev_info(&udev->dev,
			 "Ignoring serial port reserved for JTAG\n");
		return -ENODEV;
	}

	return 0;
}

static int ftdi_8u2232c_probe(struct usb_serial *serial)
{
	struct usb_device *udev = serial->dev;

	if (udev->manufacturer && !strcmp(udev->manufacturer, "CALAO Systems"))
		return ftdi_jtag_probe(serial);

	if (udev->product &&
		(!strcmp(udev->product, "Arrow USB Blaster") ||
		 !strcmp(udev->product, "BeagleBone/XDS100V2") ||
		 !strcmp(udev->product, "SNAP Connect E10")))
		return ftdi_jtag_probe(serial);

	return 0;
}

/*
 * First two ports on JTAG adaptors using an FT4232 such as STMicroelectronics's
 * ST Micro Connect Lite are reserved for JTAG or other non-UART interfaces and
 * can be accessed from userspace.
 * The next two ports are enabled as UARTs by default, where port 2 is
 * a conventional RS-232 UART.
 */
static int ftdi_stmclite_probe(struct usb_serial *serial)
{
	struct usb_device *udev = serial->dev;
	struct usb_interface *interface = serial->interface;

	if (interface == udev->actconfig->interface[0] ||
	    interface == udev->actconfig->interface[1]) {
		dev_info(&udev->dev, "Ignoring serial port reserved for JTAG\n");
		return -ENODEV;
	}

	return 0;
}

static int ftdi_sio_port_remove(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	ftdi_gpio_remove(port);

	remove_sysfs_attrs(port);

	kfree(priv);

	return 0;
}

static int ftdi_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	struct usb_device *dev = port->serial->dev;
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	/* No error checking for this (will get errors later anyway) */
	/* See ftdi_sio.h for description of what is reset */
	usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
			FTDI_SIO_RESET_REQUEST, FTDI_SIO_RESET_REQUEST_TYPE,
			FTDI_SIO_RESET_SIO,
			priv->interface, NULL, 0, WDR_TIMEOUT);

	/* Termios defaults are set by usb_serial_init. We don't change
	   port->tty->termios - this would lose speed settings, etc.
	   This is same behaviour as serial.c/rs_open() - Kuba */

	/* ftdi_set_termios  will send usb control messages */
	if (tty)
		ftdi_set_termios(tty, port, NULL);

	return usb_serial_generic_open(tty, port);
}

static void ftdi_dtr_rts(struct usb_serial_port *port, int on)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	/* Disable flow control */
	if (!on) {
		if (usb_control_msg(port->serial->dev,
			    usb_sndctrlpipe(port->serial->dev, 0),
			    FTDI_SIO_SET_FLOW_CTRL_REQUEST,
			    FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE,
			    0, priv->interface, NULL, 0,
			    WDR_TIMEOUT) < 0) {
			dev_err(&port->dev, "error from flowcontrol urb\n");
		}
	}
	/* drop RTS and DTR */
	if (on)
		set_mctrl(port, TIOCM_DTR | TIOCM_RTS);
	else
		clear_mctrl(port, TIOCM_DTR | TIOCM_RTS);
}

/* The SIO requires the first byte to have:
 *  B0 1
 *  B1 0
 *  B2..7 length of message excluding byte 0
 *
 * The new devices do not require this byte
 */
static int ftdi_prepare_write_buffer(struct usb_serial_port *port,
						void *dest, size_t size)
{
	struct ftdi_private *priv;
	int count;
	unsigned long flags;

	priv = usb_get_serial_port_data(port);

	if (priv->chip_type == SIO) {
		unsigned char *buffer = dest;
		int i, len, c;

		count = 0;
		spin_lock_irqsave(&port->lock, flags);
		for (i = 0; i < size - 1; i += priv->max_packet_size) {
			len = min_t(int, size - i, priv->max_packet_size) - 1;
			c = kfifo_out(&port->write_fifo, &buffer[i + 1], len);
			if (!c)
				break;
			port->icount.tx += c;
			buffer[i] = (c << 2) + 1;
			count += c + 1;
		}
		spin_unlock_irqrestore(&port->lock, flags);
	} else {
		count = kfifo_out_locked(&port->write_fifo, dest, size,
								&port->lock);
		port->icount.tx += count;
	}

	return count;
}

#define FTDI_RS_ERR_MASK (FTDI_RS_BI | FTDI_RS_PE | FTDI_RS_FE | FTDI_RS_OE)

static int ftdi_process_packet(struct usb_serial_port *port,
		struct ftdi_private *priv, char *packet, int len)
{
	int i;
	char status;
	char flag;
	char *ch;

	if (len < 2) {
		dev_dbg(&port->dev, "malformed packet\n");
		return 0;
	}

	/* Compare new line status to the old one, signal if different/
	   N.B. packet may be processed more than once, but differences
	   are only processed once.  */
	status = packet[0] & FTDI_STATUS_B0_MASK;
	if (status != priv->prev_status) {
		char diff_status = status ^ priv->prev_status;

		if (diff_status & FTDI_RS0_CTS)
			port->icount.cts++;
		if (diff_status & FTDI_RS0_DSR)
			port->icount.dsr++;
		if (diff_status & FTDI_RS0_RI)
			port->icount.rng++;
		if (diff_status & FTDI_RS0_RLSD) {
			struct tty_struct *tty;

			port->icount.dcd++;
			tty = tty_port_tty_get(&port->port);
			if (tty)
				usb_serial_handle_dcd_change(port, tty,
						status & FTDI_RS0_RLSD);
			tty_kref_put(tty);
		}

		wake_up_interruptible(&port->port.delta_msr_wait);
		priv->prev_status = status;
	}

	/* save if the transmitter is empty or not */
	if (packet[1] & FTDI_RS_TEMT)
		priv->transmit_empty = 1;
	else
		priv->transmit_empty = 0;

	len -= 2;
	if (!len)
		return 0;	/* status only */

	/*
	 * Break and error status must only be processed for packets with
	 * data payload to avoid over-reporting.
	 */
	flag = TTY_NORMAL;
	if (packet[1] & FTDI_RS_ERR_MASK) {
		/* Break takes precedence over parity, which takes precedence
		 * over framing errors */
		if (packet[1] & FTDI_RS_BI) {
			flag = TTY_BREAK;
			port->icount.brk++;
			usb_serial_handle_break(port);
		} else if (packet[1] & FTDI_RS_PE) {
			flag = TTY_PARITY;
			port->icount.parity++;
		} else if (packet[1] & FTDI_RS_FE) {
			flag = TTY_FRAME;
			port->icount.frame++;
		}
		/* Overrun is special, not associated with a char */
		if (packet[1] & FTDI_RS_OE) {
			port->icount.overrun++;
			tty_insert_flip_char(&port->port, 0, TTY_OVERRUN);
		}
	}

	port->icount.rx += len;
	ch = packet + 2;

	if (port->port.console && port->sysrq) {
		for (i = 0; i < len; i++, ch++) {
			if (!usb_serial_handle_sysrq_char(port, *ch))
				tty_insert_flip_char(&port->port, *ch, flag);
		}
	} else {
		tty_insert_flip_string_fixed_flag(&port->port, ch, flag, len);
	}

	return len;
}

static void ftdi_process_read_urb(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	char *data = (char *)urb->transfer_buffer;
	int i;
	int len;
	int count = 0;

	for (i = 0; i < urb->actual_length; i += priv->max_packet_size) {
		len = min_t(int, urb->actual_length - i, priv->max_packet_size);
		count += ftdi_process_packet(port, priv, &data[i], len);
	}

	if (count)
		tty_flip_buffer_push(&port->port);
}

static void ftdi_break_ctl(struct tty_struct *tty, int break_state)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	u16 value;

	/* break_state = -1 to turn on break, and 0 to turn off break */
	/* see drivers/char/tty_io.c to see it used */
	/* last_set_data_value NEVER has the break bit set in it */

	if (break_state)
		value = priv->last_set_data_value | FTDI_SIO_SET_BREAK;
	else
		value = priv->last_set_data_value;

	if (usb_control_msg(port->serial->dev,
			usb_sndctrlpipe(port->serial->dev, 0),
			FTDI_SIO_SET_DATA_REQUEST,
			FTDI_SIO_SET_DATA_REQUEST_TYPE,
			value , priv->interface,
			NULL, 0, WDR_TIMEOUT) < 0) {
		dev_err(&port->dev, "%s FAILED to enable/disable break state (state was %d)\n",
			__func__, break_state);
	}

	dev_dbg(&port->dev, "%s break state is %d - urb is %d\n", __func__,
		break_state, value);

}

static bool ftdi_tx_empty(struct usb_serial_port *port)
{
	unsigned char buf[2];
	int ret;

	ret = ftdi_get_modem_status(port, buf);
	if (ret == 2) {
		if (!(buf[1] & FTDI_RS_TEMT))
			return false;
	}

	return true;
}

/* old_termios contains the original termios settings and tty->termios contains
 * the new setting to be used
 * WARNING: set_termios calls this with old_termios in kernel space
 */
static void ftdi_set_termios(struct tty_struct *tty,
		struct usb_serial_port *port, struct ktermios *old_termios)
{
	struct usb_device *dev = port->serial->dev;
	struct device *ddev = &port->dev;
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct ktermios *termios = &tty->termios;
	unsigned int cflag = termios->c_cflag;
	u16 value, index;
	int ret;

	/* Force baud rate if this device requires it, unless it is set to
	   B0. */
	if (priv->force_baud && ((termios->c_cflag & CBAUD) != B0)) {
		dev_dbg(ddev, "%s: forcing baud rate for this device\n", __func__);
		tty_encode_baud_rate(tty, priv->force_baud,
					priv->force_baud);
	}

	/* Force RTS-CTS if this device requires it. */
	if (priv->force_rtscts) {
		dev_dbg(ddev, "%s: forcing rtscts for this device\n", __func__);
		termios->c_cflag |= CRTSCTS;
	}

	/*
	 * All FTDI UART chips are limited to CS7/8. We shouldn't pretend to
	 * support CS5/6 and revert the CSIZE setting instead.
	 *
	 * CS5 however is used to control some smartcard readers which abuse
	 * this limitation to switch modes. Original FTDI chips fall back to
	 * eight data bits.
	 *
	 * TODO: Implement a quirk to only allow this with mentioned
	 *       readers. One I know of (Argolis Smartreader V1)
	 *       returns "USB smartcard server" as iInterface string.
	 *       The vendor didn't bother with a custom VID/PID of
	 *       course.
	 */
	if (C_CSIZE(tty) == CS6) {
		dev_warn(ddev, "requested CSIZE setting not supported\n");

		termios->c_cflag &= ~CSIZE;
		if (old_termios)
			termios->c_cflag |= old_termios->c_cflag & CSIZE;
		else
			termios->c_cflag |= CS8;
	}

	cflag = termios->c_cflag;

	if (!old_termios)
		goto no_skip;

	if (old_termios->c_cflag == termios->c_cflag
	    && old_termios->c_ispeed == termios->c_ispeed
	    && old_termios->c_ospeed == termios->c_ospeed)
		goto no_c_cflag_changes;

	/* NOTE These routines can get interrupted by
	   ftdi_sio_read_bulk_callback  - need to examine what this means -
	   don't see any problems yet */

	if ((old_termios->c_cflag & (CSIZE|PARODD|PARENB|CMSPAR|CSTOPB)) ==
	    (termios->c_cflag & (CSIZE|PARODD|PARENB|CMSPAR|CSTOPB)))
		goto no_data_parity_stop_changes;

no_skip:
	/* Set number of data bits, parity, stop bits */

	value = 0;
	value |= (cflag & CSTOPB ? FTDI_SIO_SET_DATA_STOP_BITS_2 :
			FTDI_SIO_SET_DATA_STOP_BITS_1);
	if (cflag & PARENB) {
		if (cflag & CMSPAR)
			value |= cflag & PARODD ?
					FTDI_SIO_SET_DATA_PARITY_MARK :
					FTDI_SIO_SET_DATA_PARITY_SPACE;
		else
			value |= cflag & PARODD ?
					FTDI_SIO_SET_DATA_PARITY_ODD :
					FTDI_SIO_SET_DATA_PARITY_EVEN;
	} else {
		value |= FTDI_SIO_SET_DATA_PARITY_NONE;
	}
	switch (cflag & CSIZE) {
	case CS5:
		dev_dbg(ddev, "Setting CS5 quirk\n");
		break;
	case CS7:
		value |= 7;
		dev_dbg(ddev, "Setting CS7\n");
		break;
	default:
	case CS8:
		value |= 8;
		dev_dbg(ddev, "Setting CS8\n");
		break;
	}

	/* This is needed by the break command since it uses the same command
	   - but is or'ed with this value  */
	priv->last_set_data_value = value;

	if (usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
			    FTDI_SIO_SET_DATA_REQUEST,
			    FTDI_SIO_SET_DATA_REQUEST_TYPE,
			    value , priv->interface,
			    NULL, 0, WDR_SHORT_TIMEOUT) < 0) {
		dev_err(ddev, "%s FAILED to set databits/stopbits/parity\n",
			__func__);
	}

	/* Now do the baudrate */
no_data_parity_stop_changes:
	if ((cflag & CBAUD) == B0) {
		/* Disable flow control */
		if (usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
				    FTDI_SIO_SET_FLOW_CTRL_REQUEST,
				    FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE,
				    0, priv->interface,
				    NULL, 0, WDR_TIMEOUT) < 0) {
			dev_err(ddev, "%s error from disable flowcontrol urb\n",
				__func__);
		}
		/* Drop RTS and DTR */
		clear_mctrl(port, TIOCM_DTR | TIOCM_RTS);
	} else {
		/* set the baudrate determined before */
		mutex_lock(&priv->cfg_lock);
		if (change_speed(tty, port))
			dev_err(ddev, "%s urb failed to set baudrate\n", __func__);
		mutex_unlock(&priv->cfg_lock);
		/* Ensure RTS and DTR are raised when baudrate changed from 0 */
		if (old_termios && (old_termios->c_cflag & CBAUD) == B0)
			set_mctrl(port, TIOCM_DTR | TIOCM_RTS);
	}

no_c_cflag_changes:
	/* Set hardware-assisted flow control */
	value = 0;

	if (C_CRTSCTS(tty)) {
		dev_dbg(&port->dev, "enabling rts/cts flow control\n");
		index = FTDI_SIO_RTS_CTS_HS;
	} else if (I_IXON(tty)) {
		dev_dbg(&port->dev, "enabling xon/xoff flow control\n");
		index = FTDI_SIO_XON_XOFF_HS;
		value = STOP_CHAR(tty) << 8 | START_CHAR(tty);
	} else {
		dev_dbg(&port->dev, "disabling flow control\n");
		index = FTDI_SIO_DISABLE_FLOW_CTRL;
	}

	index |= priv->interface;

	ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
			FTDI_SIO_SET_FLOW_CTRL_REQUEST,
			FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE,
			value, index, NULL, 0, WDR_TIMEOUT);
	if (ret < 0)
		dev_err(&port->dev, "failed to set flow control: %d\n", ret);
}

/*
 * Get modem-control status.
 *
 * Returns the number of status bytes retrieved (device dependant), or
 * negative error code.
 */
static int ftdi_get_modem_status(struct usb_serial_port *port,
						unsigned char status[2])
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	unsigned char *buf;
	int len;
	int ret;

	buf = kmalloc(2, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	/*
	 * The 8U232AM returns a two byte value (the SIO a 1 byte value) in
	 * the same format as the data returned from the in point.
	 */
	switch (priv->chip_type) {
	case SIO:
		len = 1;
		break;
	case FT8U232AM:
	case FT232BM:
	case FT2232C:
	case FT232RL:
	case FT2232H:
	case FT4232H:
	case FT232H:
	case FTX:
		len = 2;
		break;
	default:
		ret = -EFAULT;
		goto out;
	}

	ret = usb_control_msg(port->serial->dev,
			usb_rcvctrlpipe(port->serial->dev, 0),
			FTDI_SIO_GET_MODEM_STATUS_REQUEST,
			FTDI_SIO_GET_MODEM_STATUS_REQUEST_TYPE,
			0, priv->interface,
			buf, len, WDR_TIMEOUT);

	/* NOTE: We allow short responses and handle that below. */
	if (ret < 1) {
		dev_err(&port->dev, "failed to get modem status: %d\n", ret);
		if (ret >= 0)
			ret = -EIO;
		ret = usb_translate_errors(ret);
		goto out;
	}

	status[0] = buf[0];
	if (ret > 1)
		status[1] = buf[1];
	else
		status[1] = 0;

	dev_dbg(&port->dev, "%s - 0x%02x%02x\n", __func__, status[0],
								status[1]);
out:
	kfree(buf);

	return ret;
}

static int ftdi_tiocmget(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	unsigned char buf[2];
	int ret;

	ret = ftdi_get_modem_status(port, buf);
	if (ret < 0)
		return ret;

	ret =	(buf[0] & FTDI_SIO_DSR_MASK  ? TIOCM_DSR : 0) |
		(buf[0] & FTDI_SIO_CTS_MASK  ? TIOCM_CTS : 0) |
		(buf[0] & FTDI_SIO_RI_MASK   ? TIOCM_RI  : 0) |
		(buf[0] & FTDI_SIO_RLSD_MASK ? TIOCM_CD  : 0) |
		priv->last_dtr_rts;

	return ret;
}

static int ftdi_tiocmset(struct tty_struct *tty,
			unsigned int set, unsigned int clear)
{
	struct usb_serial_port *port = tty->driver_data;

	return update_mctrl(port, set, clear);
}

static int ftdi_ioctl(struct tty_struct *tty,
					unsigned int cmd, unsigned long arg)
{
	struct usb_serial_port *port = tty->driver_data;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case TIOCSERGETLSR:
		return get_lsr_info(port, argp);
	default:
		break;
	}

	return -ENOIOCTLCMD;
}

module_usb_serial_driver(serial_drivers, id_table_combined);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

module_param(ndi_latency_timer, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ndi_latency_timer, "NDI device latency timer override");