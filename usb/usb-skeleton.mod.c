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
	{ 0x867847b8, "usb_deregister" },
	{ 0x785cf894, "usb_register_driver" },
	{ 0x7b262fbe, "usb_deregister_dev" },
	{ 0xaa2a8b37, "usb_find_interface" },
	{ 0x1e33f55, "_dev_info" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x296695f, "refcount_warn_saturate" },
	{ 0x853bbff4, "usb_register_dev" },
	{ 0xcc89dd22, "usb_get_dev" },
	{ 0xca7a3159, "kmem_cache_alloc_trace" },
	{ 0x428db41d, "kmalloc_caches" },
	{ 0xb44ad4b3, "_copy_to_user" },
	{ 0x4ae457fa, "usb_bulk_msg" },
	{ 0xc5850110, "printk" },
	{ 0x72e2c5fd, "usb_submit_urb" },
	{ 0xbdcfd186, "usb_free_urb" },
	{ 0x362ef408, "_copy_from_user" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x5b8856f3, "usb_alloc_coherent" },
	{ 0x102ff065, "usb_alloc_urb" },
	{ 0x3cc3f5dc, "__dynamic_dev_dbg" },
	{ 0x39fd0f21, "usb_free_coherent" },
	{ 0x37a0cba, "kfree" },
	{ 0xd3ffbaac, "usb_put_dev" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("usb:v0403p6001d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "C459941BB1D116E58AAEA2D");
