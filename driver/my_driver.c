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

enum ftdi_chip_type {
	SIO,
	FT232A,
	FT232B,
	FT2232C,
	FT232R,
	FT232H,
	FT2232H,
	FT4232H,
	FT4232HA,
	FT232HP,
	FT233HP,
	FT2232HP,
	FT2233HP,
	FT4232HP,
	FT4233HP,
	FTX,
};

struct ftdi_private {
	enum ftdi_chip_type chip_type;
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
	u16 channel;		/* channel index, or 0 for legacy types */

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

struct ftdi_quirk {
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

static const struct ftdi_quirk ftdi_jtag_quirk = {
	.probe	= ftdi_jtag_probe,
};

static const struct ftdi_quirk ftdi_NDI_device_quirk = {
	.probe	= ftdi_NDI_device_setup,
};

static const struct ftdi_quirk ftdi_USB_UIRT_quirk = {
	.port_probe = ftdi_USB_UIRT_setup,
};

static const struct ftdi_quirk ftdi_HE_TIRA1_quirk = {
	.port_probe = ftdi_HE_TIRA1_setup,
};

static const struct ftdi_quirk ftdi_stmclite_quirk = {
	.probe	= ftdi_stmclite_probe,
};

static const struct ftdi_quirk ftdi_8u2232c_quirk = {
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
	[SIO]		= "SIO",	/* the serial part of FT8U100AX */
	[FT232A]	= "FT232A",
	[FT232B]	= "FT232B",
	[FT2232C]	= "FT2232C/D",
	[FT232R]	= "FT232R",
	[FT232H]	= "FT232H",
	[FT2232H]	= "FT2232H",
	[FT4232H]	= "FT4232H",
	[FT4232HA]	= "FT4232HA",
	[FT232HP]	= "FT232HP",
	[FT233HP]	= "FT233HP",
	[FT2232HP]	= "FT2232HP",
	[FT2233HP]	= "FT2233HP",
	[FT4232HP]	= "FT4232HP",
	[FT4233HP]	= "FT4233HP",
	[FTX]		= "FT-X",
};


/* Used for TIOCMIWAIT */
#define FTDI_STATUS_B0_MASK	(FTDI_RS0_CTS | FTDI_RS0_DSR | FTDI_RS0_RI | FTDI_RS0_RLSD)
#define FTDI_STATUS_B1_MASK	(FTDI_RS_BI)
/* End TIOCMIWAIT */

static void ftdi_set_termios(struct tty_struct *tty,
			     struct usb_serial_port *port,
			     const struct ktermios *old_termios);
static int ftdi_get_modem_status(struct usb_serial_port *port,
						unsigned char status[2]);

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
		divisor |= 0xc000;	/* +0.125 */
	else if (divisor3 >= 4)
		divisor |= 0x4000;	/* +0.5 */
	else if (divisor3 != 0)
		divisor |= 0x8000;	/* +0.25 */
	else if (divisor == 1)
		divisor = 0;		/* special case for maximum baud rate */
	return divisor;
}

/**
 * ftdi_232am_baud_to_divisor - Calcula el divisor para la velocidad de baudios en un dispositivo FTDI 232AM.
 * @baud: La velocidad de baudios deseada.
 *
 * Esta función calcula el divisor necesario para establecer la velocidad de baudios deseada en un dispositivo FTDI 232AM.
 * El divisor se utiliza para configurar el reloj interno del dispositivo y determinar la velocidad de comunicación.
 *
 * Devuelve: El divisor calculado.
 */
static unsigned short int ftdi_232am_baud_to_divisor(int baud)
{
	 return ftdi_232am_baud_base_to_divisor(baud, 48000000);
}

/**
 * ftdi_232bm_baud_base_to_divisor - Calcula el divisor base para la velocidad de baudios en un dispositivo FTDI 232BM.
 * @baud: La velocidad de baudios deseada.
 * @base: La frecuencia base del reloj en el dispositivo.
 *
 * Esta función calcula el divisor base necesario para establecer la velocidad de baudios deseada en un dispositivo FTDI 232BM.
 * El divisor base se utiliza para configurar el reloj interno del dispositivo y determinar la velocidad de comunicación.
 *
 * Devuelve: El divisor base calculado.
 */
static u32 ftdi_232bm_baud_base_to_divisor(int baud, int base)
{
	static const unsigned char divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
	u32 divisor;
	/* divisor shifted 3 bits to the left */
	int divisor3 = DIV_ROUND_CLOSEST(base, 2 * baud);
	divisor = divisor3 >> 3;
	divisor |= (u32)divfrac[divisor3 & 0x7] << 14;
	/* Deal with special cases for highest baud rates. */
	if (divisor == 1)		/* 1.0 */
		divisor = 0;
	else if (divisor == 0x4001)	/* 1.5 */
		divisor = 1;
	return divisor;
}

/**
 * ftdi_232bm_baud_to_divisor - Calcula el divisor para la velocidad de baudios en un dispositivo FTDI 232BM.
 * @baud: La velocidad de baudios deseada.
 *
 * Esta función calcula el divisor necesario para establecer la velocidad de baudios deseada en un dispositivo FTDI 232BM.
 * Utiliza una frecuencia base de reloj de 48.000.000 Hz (48 MHz) para el cálculo.
 *
 * Devuelve: El divisor calculado.
 */
static u32 ftdi_232bm_baud_to_divisor(int baud)
{
	 return ftdi_232bm_baud_base_to_divisor(baud, 48000000);
}

/**
 * ftdi_2232h_baud_base_to_divisor - Calcula el divisor base para la velocidad de baudios en un dispositivo FTDI 2232H.
 * @baud: La velocidad de baudios deseada.
 * @base: La frecuencia base del reloj en el dispositivo.
 *
 * Esta función calcula el divisor base necesario para establecer la velocidad de baudios deseada en un dispositivo FTDI 2232H.
 * El divisor base se utiliza para configurar el reloj interno del dispositivo y determinar la velocidad de comunicación.
 *
 * Devuelve: El divisor base calculado.
 */
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
	if (divisor == 1)		/* 1.0 */
		divisor = 0;
	else if (divisor == 0x4001)	/* 1.5 */
		divisor = 1;
	/*
	 * Set this bit to turn off a divide by 2.5 on baud rate generator
	 * This enables baud rates up to 12Mbaud but cannot reach below 1200
	 * baud with this bit set
	 */
	divisor |= 0x00020000;
	return divisor;
}

/**
 * ftdi_2232h_baud_to_divisor - Calcula el divisor para la velocidad de baudios en un dispositivo FTDI 2232H.
 * @baud: La velocidad de baudios deseada.
 *
 * Esta función calcula el divisor necesario para establecer la velocidad de baudios deseada en un dispositivo FTDI 2232H.
 * Utiliza una frecuencia base de reloj de 120.000.000 Hz (120 MHz) para el cálculo.
 *
 * Devuelve: El divisor calculado.
 */
static u32 ftdi_2232h_baud_to_divisor(int baud)
{
	 return ftdi_2232h_baud_base_to_divisor(baud, 120000000);
}

/**
 * set_mctrl - Establece los bits de control del modem en un puerto serie USB.
 * @port: Puntero al puerto serie USB.
 * @set: Bits a establecer.
 *
 * Esta macro establece los bits de control del modem especificados en el puerto serie USB.
 * Utiliza la función update_mctrl para realizar la actualización.
 */
#define set_mctrl(port, set)		update_mctrl((port), (set), 0)

/**
 * clear_mctrl - Borra los bits de control del modem en un puerto serie USB.
 * @port: Puntero al puerto serie USB.
 * @clear: Bits a borrar.
 *
 * Esta macro borra los bits de control del modem especificados en el puerto serie USB.
 * Utiliza la función update_mctrl para realizar la actualización.
 */
#define clear_mctrl(port, clear)	update_mctrl((port), 0, (clear))

/**
 * update_mctrl - Actualiza los bits de control del modem en un puerto serie USB.
 * @port: Puntero al puerto serie USB.
 * @set: Bits a establecer.
 * @clear: Bits a borrar.
 *
 * Esta función actualiza los bits de control del modem en un puerto serie USB.
 * Toma como parámetros el puerto serie USB, los bits a establecer y los bits a borrar.
 * Utiliza una llamada de control USB para enviar los cambios al dispositivo FTDI.
 * También actualiza la variable priv->last_dtr_rts para realizar un seguimiento del estado anterior de DTR y RTS.
 *
 * Devuelve: 0 en caso de éxito, un valor negativo en caso de error.
 */
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
			       value, priv->channel,
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
	case SIO:
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
		default:
			dev_dbg(dev, "%s - Baudrate (%d) requested is not supported\n",
				__func__,  baud);
			div_value = ftdi_sio_b9600;
			baud = 9600;
			div_okay = 0;
		}
		break;
	case FT232A:
		if (baud <= 3000000) {
			div_value = ftdi_232am_baud_to_divisor(baud);
		} else {
			dev_dbg(dev, "%s - Baud rate too high!\n", __func__);
			baud = 9600;
			div_value = ftdi_232am_baud_to_divisor(9600);
			div_okay = 0;
		}
		break;
	case FT232B:
	case FT2232C:
	case FT232R:
	case FTX:
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
	default:
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
	}

	if (div_okay) {
		dev_dbg(dev, "%s - Baud rate set to %d (divisor 0x%lX) on chip %s\n",
			__func__, baud, (unsigned long)div_value,
			ftdi_chip_name[priv->chip_type]);
	}

	tty_encode_baud_rate(tty, baud, baud);
	return div_value;
}


/**
 * get_ftdi_divisor - Obtiene el divisor FTDI para una velocidad de baudios dada.
 * @tty: Puntero a la estructura tty_struct que representa el dispositivo TTY.
 * @port: Puntero al puerto serie USB.
 *
 * Esta función calcula el divisor FTDI necesario para una velocidad de baudios dada en el dispositivo TTY.
 * Utiliza diferentes funciones auxiliares para calcular el divisor en función del tipo de chip FTDI y la velocidad de baudios.
 *
 * Devuelve: El divisor FTDI correspondiente a la velocidad de baudios dada.
 */
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
	if (priv->channel)
		index = (u16)((index << 8) | priv->channel);

	rv = usb_control_msg(port->serial->dev,
			    usb_sndctrlpipe(port->serial->dev, 0),
			    FTDI_SIO_SET_BAUDRATE_REQUEST,
			    FTDI_SIO_SET_BAUDRATE_REQUEST_TYPE,
			    value, index,
			    NULL, 0, WDR_SHORT_TIMEOUT);
	return rv;
}

/**
 * write_latency_timer - Escribe el temporizador de latencia en un puerto serie USB.
 * @port: Puntero al puerto serie USB.
 *
 * Esta función escribe el valor del temporizador de latencia en un puerto serie USB.
 * Toma como parámetro el puerto serie USB y utiliza una llamada de control USB para enviar el valor al dispositivo FTDI.
 * El valor del temporizador de latencia se recupera de la estructura priv->latency en la variable 'l'.
 *
 * Devuelve: 0 en caso de éxito, un valor negativo en caso de error.
 */
static int write_latency_timer(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct usb_device *udev = port->serial->dev;
	int rv;
	int l = priv->latency;

	if (priv->chip_type == SIO || priv->chip_type == FT232A)
		return -EINVAL;

	if (priv->flags & ASYNC_LOW_LATENCY)
		l = 1;

	dev_dbg(&port->dev, "%s: setting latency timer = %i\n", __func__, l);

	rv = usb_control_msg(udev,
			     usb_sndctrlpipe(udev, 0),
			     FTDI_SIO_SET_LATENCY_TIMER_REQUEST,
			     FTDI_SIO_SET_LATENCY_TIMER_REQUEST_TYPE,
			     l, priv->channel,
			     NULL, 0, WDR_TIMEOUT);
	if (rv < 0)
		dev_err(&port->dev, "Unable to write latency timer: %i\n", rv);
	return rv;
}

/**
 * _read_latency_timer - Lee el temporizador de latencia de un puerto serie USB.
 * @port: Puntero al puerto serie USB.
 *
 * Esta función lee el valor del temporizador de latencia de un puerto serie USB.
 * Toma como parámetro el puerto serie USB y utiliza una llamada de control USB para recibir el valor del dispositivo FTDI.
 * El valor del temporizador de latencia se almacena en el búfer 'buf'.
 *
 * Devuelve: El valor del temporizador de latencia leído, o un valor negativo en caso de error.
 */
static int _read_latency_timer(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct usb_device *udev = port->serial->dev;
	u8 buf;
	int rv;

	rv = usb_control_msg_recv(udev, 0, FTDI_SIO_GET_LATENCY_TIMER_REQUEST,
				  FTDI_SIO_GET_LATENCY_TIMER_REQUEST_TYPE, 0,
				  priv->channel, &buf, 1, WDR_TIMEOUT,
				  GFP_KERNEL);
	if (rv == 0)
		rv = buf;

	return rv;
}

/**
 * read_latency_timer - Lee el temporizador de latencia de un puerto serie USB.
 * @port: Puntero al puerto serie USB.
 *
 * Esta función lee el valor del temporizador de latencia de un puerto serie USB.
 * Toma como parámetro el puerto serie USB y utiliza la función interna _read_latency_timer para realizar la lectura.
 * Si la lectura es exitosa, el valor del temporizador de latencia se almacena en 'priv->latency'.
 *
 * Devuelve: 0 si la lectura del temporizador de latencia es exitosa, o un valor negativo en caso de error.
 */
static int read_latency_timer(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	int rv;

	if (priv->chip_type == SIO || priv->chip_type == FT232A)
		return -EINVAL;

	rv = _read_latency_timer(port);
	if (rv < 0) {
		dev_err(&port->dev, "Unable to read latency timer: %i\n", rv);
		return rv;
	}

	priv->latency = rv;

	return 0;
}

/**
 * get_serial_info - Obtiene información sobre la configuración del puerto serie.
 * @tty: Puntero a la estructura tty_struct que representa el terminal.
 * @ss: Puntero a la estructura serial_struct donde se almacenará la información.
 *
 * Esta función obtiene información sobre la configuración del puerto serie asociado al terminal.
 * Toma como parámetros el puntero a la estructura tty_struct que representa el terminal y el puntero a la estructura serial_struct donde se almacenará la información.
 * Utiliza el campo driver_data de tty para obtener el puerto serie USB correspondiente.
 * A continuación, copia la información relevante de la estructura ftdi_private asociada al puerto serie en la estructura serial_struct proporcionada.
 */
static void get_serial_info(struct tty_struct *tty, struct serial_struct *ss)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	ss->flags = priv->flags;
	ss->baud_base = priv->baud_base;
	ss->custom_divisor = priv->custom_divisor;
}

/**
 * set_serial_info - Establece la información de configuración del puerto serie.
 * @tty: Puntero a la estructura tty_struct que representa el terminal.
 * @ss: Puntero a la estructura serial_struct que contiene la información de configuración.
 *
 * Esta función establece la información de configuración del puerto serie asociado al terminal.
 * Toma como parámetros el puntero a la estructura tty_struct que representa el terminal y el puntero a la estructura serial_struct que contiene la información de configuración.
 * Utiliza el campo driver_data de tty para obtener el puerto serie USB correspondiente.
 * Bloquea el acceso a la configuración del puerto serie con un mutex para evitar condiciones de carrera.
 * Si el usuario no tiene los privilegios de administrador necesarios, verifica si se están modificando las banderas de configuración (flags) que están más allá de la máscara ASYNC_USR_MASK.
 * Si se están modificando estas banderas y el usuario no tiene los privilegios necesarios, se desbloquea el mutex y se devuelve un error de permiso (-EPERM).
 * Almacena los valores antiguos de las banderas y del divisor personalizado para compararlos más adelante.
 * Actualiza las banderas (flags) y el divisor personalizado del puerto serie con los valores proporcionados en la estructura serial_struct.
 * Llama a la función write_latency_timer para escribir el temporizador de latencia.
 * Verifica si las banderas de velocidad (ASYNC_SPD_MASK) o el divisor personalizado (custom_divisor) han cambiado.
 * Si han cambiado, muestra una advertencia de deprecación si se están utilizando las banderas de velocidad y llama a la función change_speed para cambiar la velocidad del puerto serie.
 * Desbloquea el mutex y devuelve 0 para indicar un éxito.
 */
static int set_serial_info(struct tty_struct *tty, struct serial_struct *ss)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	int old_flags, old_divisor;

	mutex_lock(&priv->cfg_lock);

	if (!capable(CAP_SYS_ADMIN)) {
		if ((ss->flags ^ priv->flags) & ~ASYNC_USR_MASK) {
			mutex_unlock(&priv->cfg_lock);
			return -EPERM;
		}
	}

	old_flags = priv->flags;
	old_divisor = priv->custom_divisor;

	priv->flags = ss->flags & ASYNC_FLAGS;
	priv->custom_divisor = ss->custom_divisor;

	write_latency_timer(port);

	if ((priv->flags ^ old_flags) & ASYNC_SPD_MASK ||
			((priv->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST &&
			 priv->custom_divisor != old_divisor)) {

		/* warn about deprecation unless clearing */
		if (priv->flags & ASYNC_SPD_MASK)
			dev_warn_ratelimited(&port->dev, "use of SPD flags is deprecated\n");

		change_speed(tty, port);
	}
	mutex_unlock(&priv->cfg_lock);
	return 0;
}

/**
 * get_lsr_info - Obtiene información del registro Line Status del puerto serie.
 * @port: Puntero al puerto serie USB.
 * @retinfo: Puntero a la variable de usuario donde se almacenará la información del registro Line Status.
 *
 * Esta función obtiene información del registro Line Status del puerto serie USB.
 * Toma como parámetros un puntero al puerto serie USB y un puntero a la variable de usuario donde se almacenará la información del registro Line Status.
 * Obtiene el puntero a la estructura ftdi_private asociada al puerto serie USB.
 * Inicializa la variable result a 0.
 * Verifica si el indicador transmit_empty está activado en la estructura ftdi_private.
 * Si está activado, establece el indicador TIOCSER_TEMT en la variable result.
 * Copia el valor de result a la variable de usuario utilizando la función copy_to_user.
 * Si hay un error al copiar los datos, devuelve un error de falta de acceso (-EFAULT).
 * Devuelve 0 para indicar un éxito.
 */
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

static int ftdi_determine_type(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	struct usb_serial *serial = port->serial;
	struct usb_device *udev = serial->dev;
	unsigned int version, ifnum;

	version = le16_to_cpu(udev->descriptor.bcdDevice);
	ifnum = serial->interface->cur_altsetting->desc.bInterfaceNumber;

	/* Assume Hi-Speed type */
	priv->baud_base = 120000000 / 2;
	priv->channel = CHANNEL_A + ifnum;

	switch (version) {
	case 0x200:
		priv->chip_type = FT232A;
		priv->baud_base = 48000000 / 2;
		priv->channel = 0;
		/*
		 * FT232B devices have a bug where bcdDevice gets set to 0x200
		 * when iSerialNumber is 0. Assume it is an FT232B in case the
		 * latency timer is readable.
		 */
		if (udev->descriptor.iSerialNumber == 0 &&
				_read_latency_timer(port) >= 0) {
			priv->chip_type = FT232B;
		}
		break;
	case 0x400:
		priv->chip_type = FT232B;
		priv->baud_base = 48000000 / 2;
		priv->channel = 0;
		break;
	case 0x500:
		priv->chip_type = FT2232C;
		priv->baud_base = 48000000 / 2;
		break;
	case 0x600:
		priv->chip_type = FT232R;
		priv->baud_base = 48000000 / 2;
		priv->channel = 0;
		break;
	case 0x700:
		priv->chip_type = FT2232H;
		break;
	case 0x800:
		priv->chip_type = FT4232H;
		break;
	case 0x900:
		priv->chip_type = FT232H;
		break;
	case 0x1000:
		priv->chip_type = FTX;
		priv->baud_base = 48000000 / 2;
		break;
	case 0x2800:
		priv->chip_type = FT2233HP;
		break;
	case 0x2900:
		priv->chip_type = FT4233HP;
		break;
	case 0x3000:
		priv->chip_type = FT2232HP;
		break;
	case 0x3100:
		priv->chip_type = FT4232HP;
		break;
	case 0x3200:
		priv->chip_type = FT233HP;
		break;
	case 0x3300:
		priv->chip_type = FT232HP;
		break;
	case 0x3600:
		priv->chip_type = FT4232HA;
		break;
	default:
		if (version < 0x200) {
			priv->chip_type = SIO;
			priv->baud_base = 12000000 / 16;
			priv->channel = 0;
		} else {
			dev_err(&port->dev, "unknown device type: 0x%02x\n", version);
			return -ENODEV;
		}
	}

	dev_info(&udev->dev, "Detected %s\n", ftdi_chip_name[priv->chip_type]);

	return 0;
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
		return sprintf(buf, "%u\n", priv->latency);
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
			     v, priv->channel,
			     NULL, 0, WDR_TIMEOUT);
	if (rv < 0) {
		dev_dbg(&port->dev, "Unable to write event character: %i\n", rv);
		return -EIO;
	}

	return count;
}
/**
 * DEVICE_ATTR_WO - Macro para definir un atributo de dispositivo de solo escritura.
 *
 * Esta macro se utiliza para definir un atributo de dispositivo de solo escritura.
 * Específicamente, se utiliza para definir el atributo "event_char".
 */

static DEVICE_ATTR_WO(event_char);

/* Declaración del arreglo de atributos del dispositivo FTDI */
static struct attribute *ftdi_attrs[] = {
	&dev_attr_event_char.attr,
	&dev_attr_latency_timer.attr,
	NULL
};

/*
 * Función que determina si un atributo es visible en el objeto kobject del dispositivo FTDI.
 * Retorna los permisos (modo) del atributo.
 */
static umode_t ftdi_is_visible(struct kobject *kobj, struct attribute *attr, int idx)
{
	struct device *dev = kobj_to_dev(kobj);
	struct usb_serial_port *port = to_usb_serial_port(dev);
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	enum ftdi_chip_type type = priv->chip_type;

	if (attr == &dev_attr_event_char.attr) {
		if (type == SIO)
			return 0;
	}

	if (attr == &dev_attr_latency_timer.attr) {
		if (type == SIO || type == FT232A)
			return 0;
	}

	return attr->mode;
}

static const struct attribute_group ftdi_group = {
	.attrs		= ftdi_attrs,
	.is_visible	= ftdi_is_visible,
};

/*
 * Definición de un grupo de atributos para el dispositivo FTDI.
 */
static const struct attribute_group *ftdi_groups[] = {
	&ftdi_group,
	NULL
};

/*
* El código "#ifdef CONFIG_GPIOLIB" se utiliza para condicionalmente 
* incluir o excluir cierto código en función de si la opción de configuración 
* CONFIG_GPIOLIB está habilitada o no en el kernel.
* 
* Si la opción CONFIG_GPIOLIB está habilitada, el código entre "#ifdef CONFIG_GPIOLIB" 
* y "#endif" se compilará y formará parte del programa final. Si la opción CONFIG_GPIOLIB 
* está deshabilitada, ese código se omitirá durante la compilación.
*/
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
				 priv->channel, NULL, 0, WDR_TIMEOUT);
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
	u8 buf;
	int result;

	result = usb_autopm_get_interface(serial->interface);
	if (result)
		return result;

	result = usb_control_msg_recv(serial->dev, 0,
				      FTDI_SIO_READ_PINS_REQUEST,
				      FTDI_SIO_READ_PINS_REQUEST_TYPE, 0,
				      priv->channel, &buf, 1, WDR_TIMEOUT,
				      GFP_KERNEL);
	if (result == 0)
		result = buf;

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

static int ftdi_gpio_init_valid_mask(struct gpio_chip *gc,
				     unsigned long *valid_mask,
				     unsigned int ngpios)
{
	struct usb_serial_port *port = gpiochip_get_data(gc);
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	unsigned long map = priv->gpio_altfunc;

	bitmap_complement(valid_mask, &map, ngpios);

	if (bitmap_empty(valid_mask, ngpios))
		dev_dbg(&port->dev, "no CBUS pin configured for GPIO\n");
	else
		dev_dbg(&port->dev, "CBUS%*pbl configured for GPIO\n", ngpios,
			valid_mask);

	return 0;
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
	case FT232R:
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
	priv->gc.init_valid_mask = ftdi_gpio_init_valid_mask;
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

/**
 * ftdi_gpio_init - Inicializa los GPIO para el puerto serie USB
 * @port: Estructura del puerto serie USB
 *
 * Esta función inicializa los GPIO para el puerto serie USB. Actúa como un marcador de posición
 * y actualmente no realiza ninguna acción específica.
 *
 * Retorno: 0 en caso de éxito, código de error en caso de fallo
 */
static int ftdi_gpio_init(struct usb_serial_port *port)
{
	return 0;
}

/**
 * ftdi_gpio_remove - Elimina la configuración GPIO del puerto serie USB
 * @port: Estructura del puerto serie USB
 *
 * Esta función elimina la configuración GPIO previamente establecida para el puerto serie USB.
 * Actúa como un marcador de posición y no realiza ninguna acción específica en la implementación actual.
 * Se puede utilizar para limpiar y liberar recursos relacionados con la configuración GPIO, si es necesario.
 */
static void ftdi_gpio_remove(struct usb_serial_port *port) { }

#endif	/* CONFIG_GPIOLIB */

/*
 * ***************************************************************************
 * FTDI driver specific functions
 * ***************************************************************************
 */

/**
 * ftdi_probe - Función de sondeo para el dispositivo FTDI
 * @serial: Estructura del dispositivo de serie USB
 * @id: Identificación del dispositivo USB
 *
 * Esta función se llama cuando se está realizando el sondeo del dispositivo FTDI.
 * Comprueba si hay alguna peculiaridad (quirk) asociada con el dispositivo y, en caso afirmativo,
 * llama a la función de sondeo correspondiente. A continuación, establece los datos de serie USB
 * asociados con el dispositivo y devuelve un valor de éxito.
 *
 * @serial: Estructura del dispositivo de serie USB
 * @id: Identificación del dispositivo USB
 * @return: 0 en caso de éxito, un código de error en caso de fallo.
 */
static int ftdi_probe(struct usb_serial *serial, const struct usb_device_id *id)
{
	const struct ftdi_quirk *quirk = (struct ftdi_quirk *)id->driver_info;

	if (quirk && quirk->probe) {
		int ret = quirk->probe(serial);
		if (ret != 0)
			return ret;
	}

	usb_set_serial_data(serial, (void *)id->driver_info);

	return 0;
}

/**
 * ftdi_port_probe - Función de sondeo de un puerto FTDI
 * @port: Puerto serie USB
 *
 * Esta función se utiliza durante el sondeo de un puerto FTDI. Comprueba si hay alguna peculiaridad
 * (quirk) asociada con el puerto y, en caso afirmativo, llama a la función de sondeo correspondiente.
 * A continuación, inicializa los datos y configuraciones necesarios para el puerto FTDI, determina el tipo
 * de chip FTDI, configura el tamaño máximo del paquete y realiza la inicialización de GPIO.
 *
 * @port: Puerto serie USB
 * @return: 0 en caso de éxito, un código de error en caso de fallo.
 */
static int ftdi_port_probe(struct usb_serial_port *port)
{
	const struct ftdi_quirk *quirk = usb_get_serial_data(port->serial);
	struct ftdi_private *priv;
	int result;

	priv = kzalloc(sizeof(struct ftdi_private), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	mutex_init(&priv->cfg_lock);

	if (quirk && quirk->port_probe)
		quirk->port_probe(priv);

	usb_set_serial_port_data(port, priv);

	result = ftdi_determine_type(port);
	if (result)
		goto err_free;

	ftdi_set_max_packet_size(port);
	if (read_latency_timer(port) < 0)
		priv->latency = 16;
	write_latency_timer(port);

	result = ftdi_gpio_init(port);
	if (result < 0) {
		dev_err(&port->serial->interface->dev,
			"GPIO initialisation failed: %d\n",
			result);
	}

	return 0;

err_free:
	kfree(priv);

	return result;
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
	struct usb_interface *intf = serial->interface;
	int ifnum = intf->cur_altsetting->desc.bInterfaceNumber;

	if (ifnum == 0) {
		dev_info(&intf->dev, "Ignoring interface reserved for JTAG\n");
		return -ENODEV;
	}

	return 0;
}

/**
 * ftdi_8u2232c_probe - Función de sondeo para el chip FTDI 8U2232C
 * @serial: Estructura usb_serial
 *
 * Esta función se utiliza para el sondeo del chip FTDI 8U2232C. Comprueba el fabricante y el producto
 * del dispositivo USB asociado para determinar si es compatible con JTAG. En caso afirmativo, llama a la
 * función de sondeo ftdi_jtag_probe.
 *
 * @serial: Estructura usb_serial
 * @return: 0 en caso de éxito, un código de error en caso de fallo.
 */
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
	struct usb_interface *intf = serial->interface;
	int ifnum = intf->cur_altsetting->desc.bInterfaceNumber;

	if (ifnum < 2) {
		dev_info(&intf->dev, "Ignoring interface reserved for JTAG\n");
		return -ENODEV;
	}

	return 0;
}

/**
 * ftdi_port_remove - Función de eliminación del puerto FTDI
 * @port: Puntero al puerto USB serial
 *
 * Esta función se utiliza para eliminar y liberar los recursos asociados a un puerto FTDI.
 * Se encarga de eliminar las configuraciones GPIO y liberar la memoria utilizada por la estructura privada.
 */
static void ftdi_port_remove(struct usb_serial_port *port)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	ftdi_gpio_remove(port);

	kfree(priv);
}

/**
 * ftdi_open - Función de apertura de puerto FTDI
 * @tty: Puntero a la estructura tty_struct
 * @port: Puntero al puerto USB serial
 * 
 * Esta función se utiliza para abrir un puerto FTDI. Realiza una solicitud de reinicio al dispositivo USB,
 * establece la configuración de termios utilizando ftdi_set_termios y luego llama a la función usb_serial_generic_open
 * para realizar la apertura genérica del puerto.
 *
 * Retorna 0 en caso de éxito, un código de error en caso contrario.
 */
static int ftdi_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	struct usb_device *dev = port->serial->dev;
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	/* No error checking for this (will get errors later anyway) */
	/* See ftdi_sio.h for description of what is reset */
	usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
			FTDI_SIO_RESET_REQUEST, FTDI_SIO_RESET_REQUEST_TYPE,
			FTDI_SIO_RESET_SIO,
			priv->channel, NULL, 0, WDR_TIMEOUT);

	/* Termios defaults are set by usb_serial_init. We don't change
	   port->tty->termios - this would lose speed settings, etc.
	   This is same behaviour as serial.c/rs_open() - Kuba */

	/* ftdi_set_termios  will send usb control messages */
	if (tty)
		ftdi_set_termios(tty, port, NULL);

	return usb_serial_generic_open(tty, port);
}

/**
 * ftdi_dtr_rts - Configuración de DTR y RTS en un puerto FTDI
 * @port: Puntero al puerto USB serial
 * @on: Valor booleano que indica si se deben activar o desactivar DTR y RTS
 * 
 * Esta función se utiliza para configurar los pines DTR (Data Terminal Ready) y RTS (Request To Send) en un puerto FTDI.
 * Si @on es verdadero, se activarán los pines DTR y RTS llamando a la función set_mctrl.
 * Si @on es falso, se desactivarán los pines DTR y RTS llamando a la función clear_mctrl.
 * Además, si @on es falso, se desactivará el control de flujo enviando un mensaje de control USB al dispositivo para
 * deshabilitar el flujo de control.
 */
static void ftdi_dtr_rts(struct usb_serial_port *port, int on)
{
	struct ftdi_private *priv = usb_get_serial_port_data(port);

	/* Disable flow control */
	if (!on) {
		if (usb_control_msg(port->serial->dev,
			    usb_sndctrlpipe(port->serial->dev, 0),
			    FTDI_SIO_SET_FLOW_CTRL_REQUEST,
			    FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE,
			    0, priv->channel, NULL, 0,
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
		struct ftdi_private *priv, unsigned char *buf, int len)
{
	unsigned char status;
	bool brkint = false;
	int i;
	char flag;

	if (len < 2) {
		dev_dbg(&port->dev, "malformed packet\n");
		return 0;
	}

	/* Compare new line status to the old one, signal if different/
	   N.B. packet may be processed more than once, but differences
	   are only processed once.  */
	status = buf[0] & FTDI_STATUS_B0_MASK;
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
	if (buf[1] & FTDI_RS_TEMT)
		priv->transmit_empty = 1;
	else
		priv->transmit_empty = 0;

	if (len == 2)
		return 0;	/* status only */

	/*
	 * Break and error status must only be processed for packets with
	 * data payload to avoid over-reporting.
	 */
	flag = TTY_NORMAL;
	if (buf[1] & FTDI_RS_ERR_MASK) {
		/*
		 * Break takes precedence over parity, which takes precedence
		 * over framing errors. Note that break is only associated
		 * with the last character in the buffer and only when it's a
		 * NUL.
		 */
		if (buf[1] & FTDI_RS_BI && buf[len - 1] == '\0') {
			port->icount.brk++;
			brkint = true;
		}
		if (buf[1] & FTDI_RS_PE) {
			flag = TTY_PARITY;
			port->icount.parity++;
		} else if (buf[1] & FTDI_RS_FE) {
			flag = TTY_FRAME;
			port->icount.frame++;
		}
		/* Overrun is special, not associated with a char */
		if (buf[1] & FTDI_RS_OE) {
			port->icount.overrun++;
			tty_insert_flip_char(&port->port, 0, TTY_OVERRUN);
		}
	}

	port->icount.rx += len - 2;

	if (brkint || port->sysrq) {
		for (i = 2; i < len; i++) {
			if (brkint && i == len - 1) {
				if (usb_serial_handle_break(port))
					return len - 3;
				flag = TTY_BREAK;
			}
			if (usb_serial_handle_sysrq_char(port, buf[i]))
				continue;
			tty_insert_flip_char(&port->port, buf[i], flag);
		}
	} else {
		tty_insert_flip_string_fixed_flag(&port->port, buf + 2, flag,
				len - 2);
	}

	return len - 2;
}

/**
 * ftdi_process_packet - Procesamiento de paquetes recibidos en un puerto FTDI
 * @port: Puntero al puerto USB serial
 * @priv: Puntero a la estructura de datos privados del puerto FTDI
 * @buf: Puntero al búfer de datos recibidos
 * @len: Longitud del búfer de datos recibidos
 * 
 * Esta función se utiliza para procesar los paquetes de datos recibidos en un puerto FTDI.
 * Analiza el estado de la línea de transmisión (CTS, DSR, RI, RLSD) y realiza las acciones correspondientes.
 * También verifica si el transmisor está vacío y guarda esa información en la estructura privada del puerto.
 * A continuación, procesa los caracteres recibidos, identifica las condiciones de error (break, paridad, framing, overrun)
 * y notifica al controlador TTY correspondiente.
 * 
 * Retorna el número de caracteres procesados.
 */
static void ftdi_process_read_urb(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	struct ftdi_private *priv = usb_get_serial_port_data(port);
	char *data = urb->transfer_buffer;
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

/**
 * ftdi_break_ctl - Control de la señal de break en un puerto FTDI
 * @tty: Puntero a la estructura de datos de la terminal TTY
 * @break_state: Estado de la señal de break (-1 para activar el break, 0 para desactivar el break)
 * 
 * Esta función se utiliza para controlar la señal de break en un puerto FTDI.
 * Si break_state es -1, se activa el break enviando un mensaje de control USB al dispositivo.
 * Si break_state es 0, se desactiva el break restaurando el valor de datos anterior.
 * 
 * Retorna void.
 */
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
			value, priv->channel,
			NULL, 0, WDR_TIMEOUT) < 0) {
		dev_err(&port->dev, "%s FAILED to enable/disable break state (state was %d)\n",
			__func__, break_state);
	}

	dev_dbg(&port->dev, "%s break state is %d - urb is %d\n", __func__,
		break_state, value);

}

/**
 * ftdi_tx_empty - Verifica si el búfer de transmisión está vacío en un puerto FTDI
 * @port: Puntero al puerto serie USB
 * 
 * Esta función verifica si el búfer de transmisión está vacío en un puerto FTDI.
 * Realiza una lectura del estado del modem y comprueba el indicador TEMT (Transmitter Empty).
 * Si el indicador TEMT está activo, significa que el búfer de transmisión está vacío.
 * 
 * Retorna true si el búfer de transmisión está vacío, false en caso contrario.
 */
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
		             struct usb_serial_port *port,
		             const struct ktermios *old_termios)
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
			    value, priv->channel,
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
				    0, priv->channel,
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

	index |= priv->channel;

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
	 * The device returns a two byte value (the SIO a 1 byte value) in the
	 * same format as the data returned from the IN endpoint.
	 */
	if (priv->chip_type == SIO)
		len = 1;
	else
		len = 2;

	ret = usb_control_msg(port->serial->dev,
			usb_rcvctrlpipe(port->serial->dev, 0),
			FTDI_SIO_GET_MODEM_STATUS_REQUEST,
			FTDI_SIO_GET_MODEM_STATUS_REQUEST_TYPE,
			0, priv->channel,
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

/**
 * ftdi_tiocmget - Obtiene el estado de las señales del modem en un puerto FTDI
 * @tty: Puntero a la estructura tty_struct
 * 
 * Esta función obtiene el estado de las señales del modem en un puerto FTDI.
 * Realiza una lectura del estado del modem y convierte los bits correspondientes
 * a las señales DSR, CTS, RI y CD en los valores de las constantes TIOCM_DSR,
 * TIOCM_CTS, TIOCM_RI y TIOCM_CD, respectivamente. Además, se añade el estado
 * previo de las señales DTR y RTS.
 * 
 * Retorna un entero que representa el estado de las señales del modem.
 */
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

/**
 * ftdi_tiocmset - Configura el estado de las señales del modem en un puerto FTDI
 * @tty: Puntero a la estructura tty_struct
 * @set: Máscara de bits para establecer las señales del modem
 * @clear: Máscara de bits para desactivar las señales del modem
 * 
 * Esta función configura el estado de las señales del modem en un puerto FTDI.
 * Recibe una máscara de bits 'set' que indica qué señales deben ser activadas
 * y una máscara de bits 'clear' que indica qué señales deben ser desactivadas.
 * Luego, invoca la función 'update_mctrl' para actualizar el estado de las señales
 * en el puerto.
 * 
 * Retorna el resultado de la función 'update_mctrl'.
 */
static int ftdi_tiocmset(struct tty_struct *tty,
			unsigned int set, unsigned int clear)
{
	struct usb_serial_port *port = tty->driver_data;

	return update_mctrl(port, set, clear);
}
/**
 * ftdi_ioctl - Implementación de la operación ioctl para el puerto FTDI
 * @tty: Puntero a la estructura tty_struct
 * @cmd: Comando ioctl
 * @arg: Argumento asociado al comando
 * 
 * Esta función implementa la operación ioctl para el puerto FTDI. Recibe el
 * comando ioctl y el argumento asociado al comando. En esta implementación,
 * se maneja el comando TIOCSERGETLSR para obtener información del estado
 * de la línea de estado del enlace (Line Status Register). Se invoca la función
 * 'get_lsr_info' para obtener esta información.
 * 
 * Retorna el resultado de la función 'get_lsr_info' si el comando es TIOCSERGETLSR,
 * de lo contrario, retorna -ENOIOCTLCMD para indicar que el comando ioctl no es compatible.
 */
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

static struct usb_serial_driver ftdi_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"ftdi_sio",
		.dev_groups =	ftdi_groups,
	},
	.description =		"FTDI USB Serial Device",
	.id_table =		id_table_combined,
	.num_ports =		1,
	.bulk_in_size =		512,
	.bulk_out_size =	256,
	.probe =		ftdi_probe,
	.port_probe =		ftdi_port_probe,
	.port_remove =		ftdi_port_remove,
	.open =			ftdi_open,
	.dtr_rts =		ftdi_dtr_rts,
	.throttle =		usb_serial_generic_throttle,
	.unthrottle =		usb_serial_generic_unthrottle,
	.process_read_urb =	ftdi_process_read_urb,
	.prepare_write_buffer =	ftdi_prepare_write_buffer,
	.tiocmget =		ftdi_tiocmget,
	.tiocmset =		ftdi_tiocmset,
	.tiocmiwait =		usb_serial_generic_tiocmiwait,
	.get_icount =		usb_serial_generic_get_icount,
	.ioctl =		ftdi_ioctl,
	.get_serial =		get_serial_info,
	.set_serial =		set_serial_info,
	.set_termios =		ftdi_set_termios,
	.break_ctl =		ftdi_break_ctl,
	.tx_empty =		ftdi_tx_empty,
};

static struct usb_serial_driver * const serial_drivers[] = {
	&ftdi_device, NULL
};
module_usb_serial_driver(serial_drivers, id_table_combined);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

module_param(ndi_latency_timer, int, 0644);
MODULE_PARM_DESC(ndi_latency_timer, "NDI device latency timer override");