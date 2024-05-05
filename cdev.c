#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/kernel.h>

#define DRIVER_NAME "bmp280"
#define DRIVER_CLASS "bmp280Class"

static struct i2c_adapter * bmp_i2c_adapter = NULL;
static struct i2c_client * bmp280_i2c_client = NULL;

MODULE_AUTHOR("PTIT");
MODULE_LICENSE("GPL");

#define I2C_BUS_AVAILABLE	1	
#define SLAVE_DEVICE_NAME	"BMP280"	
#define BMP280_SLAVE_ADDRESS	0x76	

static const struct i2c_device_id bmp_id[] = {
		{ SLAVE_DEVICE_NAME, 0 }, 
		{ }
};

static struct i2c_driver bmp_driver = {
	.driver = {
		.name = SLAVE_DEVICE_NAME,
		.owner = THIS_MODULE
	}
};

static struct i2c_board_info bmp_i2c_board_info = {
	I2C_BOARD_INFO(SLAVE_DEVICE_NAME, BMP280_SLAVE_ADDRESS)
};

static dev_t myDeviceNr;
static struct class *myClass;
static struct cdev myDevice;
s32 dig_T1, dig_T2, dig_T3;

s32 read_temperature(void) {
	int var1, var2;
	s32 raw_temp;
	s32 d1, d2, d3;
	d1 = i2c_smbus_read_byte_data(bmp280_i2c_client, 0xFA);
	d2 = i2c_smbus_read_byte_data(bmp280_i2c_client, 0xFB);
	d3 = i2c_smbus_read_byte_data(bmp280_i2c_client, 0xFC);
	raw_temp = ((d1<<16) | (d2<<8) | d3) >> 4;
	var1 = ((((raw_temp >> 3) - (dig_T1 << 1))) * (dig_T2)) >> 11;
	var2 = (((((raw_temp >> 4) - (dig_T1)) * ((raw_temp >> 4) - (dig_T1))) >> 12) * (dig_T3)) >> 14;
	return ((var1 + var2) *5 +128) >> 8;
}

static ssize_t driver_read(struct file *File, char *user_buffer, size_t count, loff_t *offs) {
	int to_copy, not_copied, delta;
	char out_string[20];
	int temperature;
	to_copy = min(sizeof(out_string), count);
	temperature = read_temperature();
	snprintf(out_string, sizeof(out_string), "%d.%d\n", temperature/100, temperature%100);
	not_copied = copy_to_user(user_buffer, out_string, to_copy);
	delta = to_copy - not_copied;
	return delta;
}

static int driver_open(struct inode *deviceFile, struct file *instance) {
	printk("MyDeviceDriver -  Open was called\n");
	return 0;
}

static int driver_close(struct inode *deviceFile, struct file *instance) {
	printk("MyDeviceDriver -  Close was called\n");
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = driver_open,
	.release = driver_close,
	.read = driver_read,
};

static int __init ModuleInit(void) {
	int ret = -1;
	u8 id;
	alloc_chrdev_region(&myDeviceNr, 0, 1, DRIVER_NAME);
	printk("MyDeviceDriver - Device Nr %d was registered\n", myDeviceNr);
	if ((myClass = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
		printk("Device Class can not be created!\n");
		goto ClassError;
	}

	if (device_create(myClass, NULL, myDeviceNr, NULL, DRIVER_NAME) == NULL) {
		printk("Can not create device file!\n");
		goto FileError;
	}

	cdev_init(&myDevice, &fops);

	if (cdev_add(&myDevice, myDeviceNr, 1) == -1) {
		printk("Registering of device to kernel failed!\n");
		goto KernelError;
	}
	
	bmp_i2c_adapter = i2c_get_adapter(I2C_BUS_AVAILABLE);

	if(bmp_i2c_adapter != NULL) {
		bmp280_i2c_client = i2c_new_device(bmp_i2c_adapter, &bmp_i2c_board_info);
		if(bmp280_i2c_client != NULL) {
			if(i2c_add_driver(&bmp_driver) != -1) {
				ret = 0;
			}
			else
				printk("Can't add driver\n");
		}
		i2c_put_adapter(bmp_i2c_adapter);
	}
	printk("BMP280 Driver added!\n");

	id = i2c_smbus_read_byte_data(bmp280_i2c_client, 0xD0);
	printk("ID: 0x%x\n", id);
	dig_T1 = i2c_smbus_read_word_data(bmp280_i2c_client, 0x88);
	dig_T2 = i2c_smbus_read_word_data(bmp280_i2c_client, 0x8a);
	dig_T3 = i2c_smbus_read_word_data(bmp280_i2c_client, 0x8c);

	if(dig_T2 > 32767)
		dig_T2 -= 65536;

	if(dig_T3 > 32767)
		dig_T3 -= 65536;

	i2c_smbus_write_byte_data(bmp280_i2c_client, 0xf5, 5<<5);
	i2c_smbus_write_byte_data(bmp280_i2c_client, 0xf4, ((5<<5) | (5<<2) | (3<<0)));
	return ret;

KernelError:
	device_destroy(myClass, myDeviceNr);
FileError:
	class_destroy(myClass);
ClassError:
	unregister_chrdev(myDeviceNr, DRIVER_NAME);
	return (-1);
}

static void __exit ModuleExit(void) {
	printk("MyDeviceDriver - Goodbye, Kernel!\n");
	i2c_unregister_device(bmp280_i2c_client);
	i2c_del_driver(&bmp_driver);
	cdev_del(&myDevice);
   	device_destroy(myClass, myDeviceNr);
    	class_destroy(myClass);
    	unregister_chrdev_region(myDeviceNr, 1);
}

module_init(ModuleInit);
module_exit(ModuleExit);
