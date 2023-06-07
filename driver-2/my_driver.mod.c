#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section(__versions) = {
	{ 0xdd8f8694, "module_layout" },
	{ 0x74975366, "usb_serial_generic_get_icount" },
	{ 0xd9c2de9b, "usb_serial_generic_tiocmiwait" },
	{ 0x55060252, "usb_serial_generic_unthrottle" },
	{ 0xd8f1d83f, "usb_serial_generic_throttle" },
	{ 0xf1e60ce1, "param_ops_int" },
	{ 0xcfc4760f, "usb_serial_deregister_drivers" },
	{ 0xe41b3e8b, "usb_serial_register_drivers" },
	{ 0xcca485c4, "usb_serial_handle_break" },
	{ 0x228eecdc, "tty_flip_buffer_push" },
	{ 0xf3a3506a, "tty_kref_put" },
	{ 0x1cb576e2, "usb_serial_handle_dcd_change" },
	{ 0x8fee4a78, "tty_port_tty_get" },
	{ 0x12d01816, "tty_insert_flip_string_fixed_flag" },
	{ 0xbc47cbcd, "__tty_insert_flip_char" },
	{ 0x91d1d1dc, "usb_serial_handle_sysrq_char" },
	{ 0x3eeb2322, "__wake_up" },
	{ 0xe5a6929f, "gpiochip_add_data_with_key" },
	{ 0x66380fa9, "device_create_file" },
	{ 0x1e33f55, "_dev_info" },
	{ 0x2ea2c95c, "__x86_indirect_thunk_rax" },
	{ 0x977f511b, "__mutex_init" },
	{ 0x1d24c881, "___ratelimit" },
	{ 0xc6cbbc89, "capable" },
	{ 0x52a871c4, "usb_serial_generic_open" },
	{ 0xc98c1e2e, "_dev_warn" },
	{ 0x72f258fd, "tty_encode_baud_rate" },
	{ 0x409873e3, "tty_termios_baud_rate" },
	{ 0x3c3ff9fd, "sprintf" },
	{ 0x409bcb62, "mutex_unlock" },
	{ 0x2ab7989d, "mutex_lock" },
	{ 0xf9c9b744, "gpiochip_get_data" },
	{ 0x69ad2f20, "kstrtouint" },
	{ 0x46045dd7, "kstrtou8" },
	{ 0xb1479e3c, "gpiochip_remove" },
	{ 0xbe52cb84, "device_remove_file" },
	{ 0xd8983494, "usb_autopm_put_interface" },
	{ 0xb18a7645, "usb_autopm_get_interface" },
	{ 0xb44ad4b3, "_copy_to_user" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0x37a0cba, "kfree" },
	{ 0xca7a3159, "kmem_cache_alloc_trace" },
	{ 0x428db41d, "kmalloc_caches" },
	{ 0xdfe5547c, "_dev_err" },
	{ 0x3cc3f5dc, "__dynamic_dev_dbg" },
	{ 0x5b42218f, "usb_control_msg" },
	{ 0x3812050a, "_raw_spin_unlock_irqrestore" },
	{ 0x13d0adf7, "__kfifo_out" },
	{ 0x51760917, "_raw_spin_lock_irqsave" },
	{ 0xb601be4c, "__x86_indirect_thunk_rdx" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "usbserial");

MODULE_ALIAS("usb:v0403p6001d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "A90C2182CAB8D1DD6FC36DB");
