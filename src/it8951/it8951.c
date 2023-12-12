#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/gpio/consumer.h>
#include <linux/spi/spi.h>

#include <linux/printk.h>

#define MAX_DEVICES 64

static struct class device_class = {
	.name = "it8951",
};

struct device_data {
	struct gpio_desc* pin_rset;
	struct gpio_desc* pin_hrdy;
	struct cdev cdev;
	dev_t devnum;
};

static const struct file_operations fops = {
	.owner = THIS_MODULE,
};

static_assert(MAX_DEVICES > 0);

static dev_t dev_range_first;
static DECLARE_BITMAP(minors, MAX_DEVICES);

static int probe_device(struct spi_device* dev)
{
	int result;
	struct device_data* data;
	int minor;
	struct device* device;

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

	data = kmalloc(sizeof(*data), GFP_KERNEL);

	if (!data) {
		dev_err(&dev->dev, "failed to allocate per-device data\n");
		result = -ENOMEM;
		goto failed_alloc_data;
	}

	spi_set_drvdata(dev, data);

	if (IS_ERR(data->pin_rset = gpiod_get(&dev->dev, "RESET_N",
	                                      GPIOD_OUT_HIGH))) {
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

	if ((minor = find_first_zero_bit(minors, MAX_DEVICES))
	    >= MAX_DEVICES) {
		dev_err(&dev->dev, "failed to allocate minor number\n");
		result = -ENODEV;
		goto failed_alloc_dev_num;
	}

	set_bit(minor, minors);
	data->devnum = MKDEV(MAJOR(dev_range_first), MINOR(minor));

	cdev_init(&data->cdev, &fops);
	data->cdev.owner = THIS_MODULE;

	if ((result = cdev_add(&data->cdev, data->devnum, 1)) < 0) {
		pr_err("failed to register character device\n");
		goto failed_register_cdev;
	}

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

	dev_info(&dev->dev, "created device %d:%d -> it8951-spi%d.%d\n",
	         MAJOR(data->devnum), MINOR(data->devnum),
	         dev->master->bus_num, dev->chip_select);

	return 0;

	device_destroy(&device_class, data->devnum);
failed_create_device:
	cdev_del(&data->cdev);
failed_register_cdev:
	clear_bit(MINOR(data->devnum), minors);
failed_alloc_dev_num:
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

	device_destroy(&device_class, data->devnum);
	cdev_del(&data->cdev);
	clear_bit(MINOR(data->devnum), minors);
	gpiod_put(data->pin_hrdy);
	gpiod_put(data->pin_rset);
	kfree(data);
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

	if ((result = alloc_chrdev_region(&dev_range_first, 0,
	                                  MAX_DEVICES, "it8951")) < 0) {
		pr_err("failed to allocate chrdev region\n");
		goto failed_alloc_chrdev_region;
	}

	pr_info("using chrdev region %d:%d through %d:%d\n",
	        MAJOR(dev_range_first), 0,
	        MAJOR(dev_range_first), MAX_DEVICES - 1);

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
	unregister_chrdev_region(dev_range_first, MAX_DEVICES);
failed_alloc_chrdev_region:
	return result;
}

void __exit cleanup_driver(void)
{
	spi_unregister_driver(&driver);
	class_unregister(&device_class);
	unregister_chrdev_region(dev_range_first, MAX_DEVICES);
}

module_init(init_driver)
module_exit(cleanup_driver)

MODULE_DESCRIPTION("SPI protocol driver for IT8951-based Waveshare e-Paper HAT");
MODULE_AUTHOR("Michael Kenzel");
MODULE_LICENSE("GPL");
