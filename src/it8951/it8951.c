#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/gpio/consumer.h>
#include <linux/spi/spi.h>

#include <linux/printk.h>

#include "it8951.h"

#define MAX_DEVICES 64
#define FIFO_SIZE 2048

static struct class device_class = {
	.name = "it8951",
};

struct device_data {
	struct mutex lock;
	struct gpio_desc* pin_hrdy;
	struct spi_device* dev;
	__u8* buf;
	struct gpio_desc* pin_rset;
	dev_t devnum;
	struct list_head node;
	int refs;
};

static DEFINE_MUTEX(devices_lock);
static LIST_HEAD(devices);

static struct device_data* device_acquire(struct file* file)
{
	struct device_data* data = file->private_data;

	mutex_lock(&data->lock);

	if (!data->dev) {
		mutex_unlock(&data->lock);
		return ERR_PTR(-ESHUTDOWN);
	}

	return data;
}

static void device_release(struct device_data* data)
{
	mutex_unlock(&data->lock);
}

static void it8951_reset(struct device_data* data)
{
	gpiod_set_value_cansleep(data->pin_rset, 1);
	udelay(100);
	gpiod_set_value_cansleep(data->pin_rset, 0);
}

static void it8951_wait_ready(struct device_data* data)
{
	while (!gpiod_get_value_cansleep(data->pin_hrdy));
}

static int it8951_send_preamble(struct device_data* data, unsigned short prmbl)
{
	__u8 bytes[] = { (__u8)(prmbl >> 8), (__u8)(prmbl) };

	struct spi_transfer transfer = {
		.tx_buf = bytes,
		.len = sizeof(bytes),
		.cs_change = 1
	};

	struct spi_message msg;

	spi_message_init_with_transfers(&msg, &transfer, 1);

	it8951_wait_ready(data);
	return spi_sync_locked(data->dev, &msg);
}

static int it8951_send_command(struct device_data* data, unsigned short cmd)
{
	int result;
	__u8 bytes[] = { (__u8)(cmd >> 8), (__u8)(cmd) };

	struct spi_transfer transfer = {
		.tx_buf = bytes,
		.len = sizeof(bytes)
	};

	struct spi_message msg;

	spi_message_init_with_transfers(&msg, &transfer, 1);

	spi_bus_lock(data->dev->controller);

	if ((result = it8951_send_preamble(data, 0x6000)) < 0)
		goto fail;

	it8951_wait_ready(data);
	result = spi_sync_locked(data->dev, &msg);

fail:
	spi_bus_unlock(data->dev->controller);
	return result;
}

static ssize_t it8951_read(struct file* file, char __user* dst,
                           size_t size, loff_t* offset)
{
	struct device_data* data = device_acquire(file);

	if (IS_ERR(data))
		return PTR_ERR(data);

	device_release(data);

	return 0;
}

static ssize_t it8951_write(struct file* file, const char __user* src,
                            size_t size, loff_t* offset)
{
	struct device_data* data = device_acquire(file);

	if (IS_ERR(data))
		return PTR_ERR(data);

	device_release(data);

	return 0;
}

static long it8951_ioctl(struct file* file, unsigned int cmd,
                         unsigned long arg)
{
	int result = 0;
	struct device_data* data = device_acquire(file);

	if (IS_ERR(data))
		return PTR_ERR(data);

	if (_IOC_TYPE(cmd) != IT8951_IOC_MAGIC) {
		result = -ENOTTY;
		goto fail;
	}

	switch (cmd) {
	case IT8951_IOCTL_RESET:
		it8951_reset(data);
		break;

	case IT8951_IOCTL_COMMAND:
		result = it8951_send_command(data, (__u16)arg);
		break;

	default:
		result = -ENOTTY;
		goto fail;
	}

fail:
	device_release(data);
	return result;
}

static int it8951_open(struct inode* inode, struct file* file)
{
	int result = 0;
	struct device_data* node, * data = NULL;

	mutex_lock(&devices_lock);

	list_for_each_entry(node, &devices, node) {
		if (node->devnum == inode->i_rdev) {
			data = node;
			break;
		}
	}

	if (!data) {
		result = -ENXIO;
		goto failed_find_device;
	}

	mutex_lock(&data->lock);

	dev_info(&data->dev->dev, "attempting to open device\n");

	if (data->buf != 0) {
		result = -EBUSY;
		goto error_device_in_use;
	}

	if (!(data->buf = kmalloc(2 * FIFO_SIZE, GFP_KERNEL))) {
		dev_err(&data->dev->dev, "failed to allocate buffer memory\n");
		result = -ENOMEM;
		goto failed_alloc_buffer;
	}

	file->private_data = data;
	stream_open(inode, file);
	++data->refs;

error_device_in_use:
failed_alloc_buffer:
	mutex_unlock(&data->lock);
failed_find_device:
	mutex_unlock(&devices_lock);
	return result;
}

static int it8951_release(struct inode* inode, struct file* file)
{
	struct device_data* data = file->private_data;

	dev_info(&data->dev->dev, "releasing device\n");

	mutex_lock(&data->lock);

	kfree(data->buf);
	data->buf = NULL;

	if (--data->refs == 0)
		kfree(data);

	mutex_unlock(&data->lock);

	return 0;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = &it8951_read,
	.write = &it8951_write,
	.unlocked_ioctl = &it8951_ioctl,
	.open = &it8951_open,
	.release = &it8951_release
};

static_assert(MAX_DEVICES > 0);

static struct cdev cdev;
static dev_t devnum_start;
static DECLARE_BITMAP(minors, MAX_DEVICES);

static int probe_device(struct spi_device* dev)
{
	int result;
	struct device_data* data;
	int minor;
	struct device* device;

	dev_info(&dev->dev, "probing\n");

	dev->max_speed_hz = 12000000;
	dev->bits_per_word = 8;
	dev->mode = 0;
	dev->cs_setup.value = 10;
	dev->cs_setup.unit = SPI_DELAY_UNIT_NSECS;
	dev->cs_hold.value = 10;
	dev->cs_hold.unit = SPI_DELAY_UNIT_NSECS;

	result = spi_setup(dev);

	if (result < 0) {
		dev_err(&dev->dev, "failed to configure SPI device\n");
		goto failed_spi_setup;
	}

	if (!(data = kmalloc(sizeof(*data), GFP_KERNEL))) {
		dev_err(&dev->dev, "failed to allocate per-device data\n");
		result = -ENOMEM;
		goto failed_alloc_data;
	}

	mutex_init(&data->lock);
	data->dev = dev;
	data->buf = NULL;
	data->refs = 1;

	if (IS_ERR(data->pin_rset = gpiod_get(&dev->dev, "RESET_N",
	                                      GPIOD_OUT_LOW))) {
		dev_err(&dev->dev, "failed to allocate reset pin\n");
		result = PTR_ERR(data->pin_rset);
		goto failed_alloc_reset;
	}

	if (IS_ERR(data->pin_hrdy = gpiod_get(&dev->dev, "HOST_HRDY",
	                                      GPIOD_IN))) {
		dev_err(&dev->dev, "failed to allocate hrdy pin\n");
		result = PTR_ERR(data->pin_hrdy);
		goto failed_alloc_hrdy;
	}

	INIT_LIST_HEAD(&data->node);

	mutex_lock(&devices_lock);

	if ((minor = find_first_zero_bit(minors, MAX_DEVICES))
	    >= MAX_DEVICES) {
		dev_err(&dev->dev, "failed to allocate device minor number\n");
		result = -ENODEV;
		goto failed_alloc_dev_num;
	}

	set_bit(minor, minors);
	data->devnum = MKDEV(MAJOR(devnum_start), MINOR(minor));

	device = device_create(&device_class, &dev->dev,
	                       data->devnum, data,
	                       "it8951-spi%d.%d",
	                       dev->master->bus_num,
	                       dev->chip_select);

	if (IS_ERR(device)) {
		dev_err(&dev->dev, "failed to create device\n");
		result = PTR_ERR(device);
		goto failed_create_device;
	}

	spi_set_drvdata(dev, data);

	list_add(&data->node, &devices);

	dev_info(&dev->dev, "created device %d:%d\n",
	         MAJOR(data->devnum), MINOR(data->devnum));

	mutex_unlock(&devices_lock);

	return 0;

	device_destroy(&device_class, data->devnum);
failed_create_device:
	clear_bit(MINOR(data->devnum), minors);
failed_alloc_dev_num:
	mutex_unlock(&devices_lock);
	gpiod_put(data->pin_hrdy);
failed_alloc_hrdy:
	gpiod_put(data->pin_rset);
failed_alloc_reset:
	kfree(data);
failed_alloc_data:
failed_spi_setup:
	return result;
}

static void remove_device(struct spi_device* dev)
{
	struct device_data* data = spi_get_drvdata(dev);

	dev_info(&dev->dev, "removing device %d:%d\n",
	         MAJOR(data->devnum), MINOR(data->devnum));

	mutex_lock(&devices_lock);
	list_del(&data->node);
	mutex_unlock(&devices_lock);

	mutex_lock(&data->lock);
	data->dev = NULL;
	mutex_unlock(&data->lock);

	device_destroy(&device_class, data->devnum);
	clear_bit(MINOR(data->devnum), minors);
	gpiod_put(data->pin_hrdy);
	gpiod_put(data->pin_rset);

	mutex_lock(&data->lock);
	if (--data->refs == 0)
		kfree(data);
	mutex_unlock(&data->lock);
}

static const struct of_device_id of_match_table[] = {
	{ .compatible = "waveshare,it8951" },
	{}
};
MODULE_DEVICE_TABLE(of, of_match_table);

static const struct spi_device_id spi_device_ids[] = {
	{ "it8951", 0 },
	{}
};
MODULE_DEVICE_TABLE(spi, spi_device_ids);

static struct spi_driver driver = {
	.driver = {
		.name = "it8951",
		.of_match_table = of_match_ptr(of_match_table)
	},
	.id_table = spi_device_ids,
	.probe = probe_device,
	.remove = remove_device
};

int __init init_driver(void)
{
	int result;

	if ((result = alloc_chrdev_region(&devnum_start, 0, MAX_DEVICES,
	                                  "it8951")) < 0) {
		pr_err("failed to allocate chrdev region\n");
		goto failed_alloc_chrdev_region;
	}

	pr_info("using chrdev region %d:%d through %d:%d\n",
	        MAJOR(devnum_start), 0,
	        MAJOR(devnum_start), MAX_DEVICES - 1);

	cdev_init(&cdev, &fops);
	cdev.owner = THIS_MODULE;

	if ((result = cdev_add(&cdev, devnum_start, MAX_DEVICES)) < 0) {
		pr_err("failed to register character device\n");
		goto failed_register_cdev;
	}

	if ((result = class_register(&device_class)) < 0) {
		pr_err("failed to register class\n");
		goto failed_register_class;
	}

	if ((result = spi_register_driver(&driver)) < 0) {
		pr_err("failed to register driver\n");
		goto failed_register_driver;
	}

	return 0;

	spi_unregister_driver(&driver);
failed_register_driver:
	class_unregister(&device_class);
failed_register_class:
	cdev_del(&cdev);
failed_register_cdev:
	unregister_chrdev_region(devnum_start, MAX_DEVICES);
failed_alloc_chrdev_region:
	return result;
}

void __exit cleanup_driver(void)
{
	spi_unregister_driver(&driver);
	class_unregister(&device_class);
	cdev_del(&cdev);
	unregister_chrdev_region(devnum_start, MAX_DEVICES);
}

module_init(init_driver)
module_exit(cleanup_driver)

MODULE_DESCRIPTION("SPI protocol driver for IT8951-based Waveshare e-Paper HAT");
MODULE_AUTHOR("Michael Kenzel");
MODULE_LICENSE("GPL");
