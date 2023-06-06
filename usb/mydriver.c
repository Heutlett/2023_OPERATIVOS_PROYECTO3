#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jachm");
MODULE_DESCRIPTION("A driver for my YP-05 USB device");

#define VENDOR_ID 0x0403
#define PRODUCT_ID 0x6001
#define DEVICE_NAME "mydriver"

static dev_t dev_num;
static struct cdev cdev;
static struct class *dev_class;
static struct device *dev;

// Implement the file operations for the character device:
static int my_open(struct inode *inode, struct file *file)
{
	// Called when the device file is opened
	printk("mydriver - Device file opened\n");
	return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
	// Called when the device file is closed
	printk("mydriver - Device file closed\n");
	return 0;
}

static ssize_t my_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	// Called when data is read from the device file
	// Implement your read logic here
	return 0;
}

static ssize_t my_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	// Called when data is written to the device file
	// Implement your write logic here
	return count;
}

static struct file_operations fops = {
	.open = my_open,
	.release = my_release,
	.read = my_read,
	.write = my_write,
};

static struct usb_device_id usb_dev_table[] = {
	{USB_DEVICE(VENDOR_ID, PRODUCT_ID)},
	{},
};
MODULE_DEVICE_TABLE(usb, usb_dev_table);

static int my_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{

	struct usb_device *usb_dev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i;

	printk("mydriver - Probe Function\n");

	usb_dev = interface_to_usbdev(intf);
	if (usb_dev == NULL)
	{
		printk("mydriver - Error getting device from interface\n");
		return -ENODEV;
	}

	// Print some information about the connected USB device
	printk("mydriver - USB device connected: VendorID=0x%04X, ProductID=0x%04X\n",
		   usb_dev->descriptor.idVendor, usb_dev->descriptor.idProduct);

	// Access the interface descriptor
	iface_desc = intf->cur_altsetting;

	// Find the bulk-in and bulk-out endpoints
	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++)
	{
		endpoint = &iface_desc->endpoint[i].desc;

		if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK)
		{
			if (endpoint->bEndpointAddress & USB_DIR_IN)
			{
				// Bulk-in endpoint
				printk("mydriver - Found bulk-in endpoint: Address=0x%02X, Size=%u\n",
					   endpoint->bEndpointAddress, endpoint->wMaxPacketSize);
				// Implement handling for bulk-in endpoint
				// ...
			}
			else
			{
				// Bulk-out endpoint
				printk("mydriver - Found bulk-out endpoint: Address=0x%02X, Size=%u\n",
					   endpoint->bEndpointAddress, endpoint->wMaxPacketSize);
				// Implement handling for bulk-out endpoint
				// ...
			}
		}
	}

	// Create character device, class, and device
	// Allocate a major number for the device
	if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0)
	{
		printk("mydriver - Failed to allocate major number\n");
		return -1;
	}

	// Create a class for the device
	dev_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(dev_class))
	{
		unregister_chrdev_region(dev_num, 1);
		printk("mydriver - Failed to create device class\n");
		return -1;
	}

	// Initialize the character device
	cdev_init(&cdev, &fops);
	if (cdev_add(&cdev, dev_num, 1) < 0)
	{
		device_destroy(dev_class, dev_num);
		class_destroy(dev_class);
		unregister_chrdev_region(dev_num, 1);
		printk("mydriver - Failed to add character device\n");
		return -1;
	}

	// Create the device file
	dev = device_create(dev_class, NULL, dev_num, NULL, DEVICE_NAME);
	if (IS_ERR(dev))
	{
		cdev_del(&cdev);
		class_destroy(dev_class);
		unregister_chrdev_region(dev_num, 1);
		printk("mydriver - Failed to create device file\n");
		return -1;
	}

	printk("mydriver - Character device created\n");

	return 0;
}

static void my_usb_disconnect(struct usb_interface *intf)
{
	printk("mydriver - Disconnect Function\n");
}

static struct usb_driver my_usb_driver = {
	.name = "mydriver",
	.id_table = usb_dev_table,
	.probe = my_usb_probe,
	.disconnect = my_usb_disconnect,
};

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init my_init(void)
{
	int result;
	printk("mydriver - Init Function\n");
	result = usb_register(&my_usb_driver);
	if (result)
	{
		printk("mydriver - Error during register!\n");
		return -result;
	}

	return 0;
}

/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit my_exit(void)
{
	// Destroy the device file
	device_destroy(dev_class, dev_num);

	// Remove the character device
	cdev_del(&cdev);

	// Destroy the device class
	class_destroy(dev_class);

	// Unregister the major number
	unregister_chrdev_region(dev_num, 1);

	printk("mydriver - Character device removed\n");
	printk("mydriver - Exit Function\n");
	usb_deregister(&my_usb_driver);
}

module_init(my_init);
module_exit(my_exit);