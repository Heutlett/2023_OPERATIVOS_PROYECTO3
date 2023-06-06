#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes 4 GNU/Linux");
MODULE_DESCRIPTION("A driver for my Atmega32U4 USB device");

#define VENDOR_ID 0x0403
#define PRODUCT_ID 0x6001

static dev_t dev_num;
static struct cdev my_cdev;
static struct usb_device *usb_dev;

static int my_open(struct inode *inode, struct file *filp) {
    printk("my_usb_devdrv - Open Function\n");
    return 0;
}

static int my_release(struct inode *inode, struct file *filp) {
    printk("my_usb_devdrv - Release Function\n");
    return 0;
}

static ssize_t my_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
	char text[32];
	int to_copy, not_copied, delta, status;
	u8 val;

	/* Get amount of data to copy */
	to_copy = min(count, sizeof(text));

	/* Read from USB Device */
	status = usb_control_msg_recv(usb_dev, usb_rcvctrlpipe(usb_dev, 0), 0x2, 0xC0, 0, 0, (unsigned char *)&val, 1, 100, GFP_KERNEL);
	if (status < 0) {
		printk("my_usb_devdrv - Error during control message\n");
		return -1;
	}
	sprintf(text, "0x%x\n", val);

	/* Copy data to user */
	not_copied = copy_to_user(buf, text, to_copy);

	/* Calculate data */
	delta = to_copy - not_copied;

	return delta;
}

static ssize_t my_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
	char text[255];
	int to_copy, not_copied, delta, status;
	long val;
	u16 seg_val;

	/* Clear text */
	memset(text, 0, sizeof(text));

	/* Get amount of data to copy */
	to_copy = min(count, sizeof(text));

	/* Copy data from user */
	not_copied = copy_from_user(text, buf, to_copy);
	if (kstrtol(text, 0, &val) != 0) {
		printk("my_usb_devdrv - Error converting input\n");
		return -1;
	}

	seg_val = (u16)val;
	status = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), 0x1, 0x40, seg_val, 0, NULL, 0, 100);
	if (status < 0) {
		printk("my_usb_devdrv - Error during control message\n");
		return -1;
	}

	/* Calculate data */
	delta = to_copy - not_copied;

	return delta;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_release,
	.read = my_read,
	.write = my_write,
};

static struct usb_device_id usb_dev_table[] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{},
};
MODULE_DEVICE_TABLE(usb, usb_dev_table);

static int my_usb_probe(struct usb_interface *intf, const struct usb_device_id *id) {
	
	static struct cdev my_cdev;
	static struct class *my_class;
	int result;
	
	printk("my_usb_devdrv - Probe Function\n");

	usb_dev = interface_to_usbdev(intf);
	if (usb_dev == NULL) {
		printk("my_usb_devdrv - Error getting device from interface\n");
		return -1;
	}

	/* Create character device */
	result = alloc_chrdev_region(&dev_num, 0, 1, "my_usb_dev");
	if (result < 0) {
		printk("my_usb_devdrv - Error allocating device number: %d\n", result);
		return result;
	}

	cdev_init(&my_cdev, &fops);
	my_cdev.owner = THIS_MODULE;
	result = cdev_add(&my_cdev, dev_num, 1);
	if (result < 0) {
		printk("my_usb_devdrv - Error adding character device: %d\n", result);
		unregister_chrdev_region(dev_num, 1);
		return result;
	}

	my_class = class_create(THIS_MODULE, "my_usb_dev");
	device_create(my_class, NULL, dev_num, NULL, "my_usb_dev");

	result = cdev_add(&my_cdev, dev_num, 1);
	if (result < 0) {
		printk("my_usb_devdrv - Error adding character device\n");
		unregister_chrdev_region(dev_num, 1);
		return result;
	}

	return 0;
}

static void my_usb_disconnect(struct usb_interface *intf) {
	cdev_del(&my_cdev);
	unregister_chrdev_region(dev_num, 1);
	printk("my_usb_devdrv - Disconnect Function\n");
}

static struct usb_driver my_usb_driver = {
	.name = "my_usb_devdrv",
	.id_table = usb_dev_table,
	.probe = my_usb_probe,
	.disconnect = my_usb_disconnect,
};

static int __init my_init(void) {
	int result;
	printk("my_usb_devdrv - Init Function\n");
	result = usb_register(&my_usb_driver);
	if (result) {
		printk("my_usb_devdrv - Error during register!\n");
		return -result;
	}

	return 0;
}

static void __exit my_exit(void) {
	printk("my_usb_devdrv - Exit Function\n");
	usb_deregister(&my_usb_driver);
}

module_init(my_init);
module_exit(my_exit);
