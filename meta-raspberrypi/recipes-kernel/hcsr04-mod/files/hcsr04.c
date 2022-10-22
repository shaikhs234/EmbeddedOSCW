#include <linux/uaccess.h> 
#include <asm/uaccess.h> 
#include <linux/module.h> 
#include <linux/init.h> 
#include <linux/kernel.h> 
#include <linux/types.h> 
#include <linux/kdev_t.h> 
#include <linux/fs.h>
#include <linux/cdev.h> 
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <asm/delay.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/timekeeping.h>
#include <linux/ktime.h>
#define GPIO_OUT 20 // GPIO20
#define GPIO_IN 21 // GPIO21

static dev_t hcsr04_dev; // contains major and first minor number of kernel module
struct cdev hcsr04_cdev; // Data structure to describe properties of our character device

static int hcsr04_lock = 0; // variable to check if sensor is being used 

static struct kobject *hcsr04_kobject; // kernel object that will create hcsr04 directory under sysfs and in this directory the files that store the latest values will be saved

static ktime_t rising, falling; // variables to calculate pulse from sensor

int pulse; // Stores the pulse value 
char dt[30]; // stores the current date
char final[80]; // Stores line with date, pulse duration and distance calculated which will be then stored in allocated memory.

struct rtc_time tm; // variable for datetime calculation

static char *pulseptr[5]; // array pointer to memory allocated using kmalloc which will store the values to return for the sysfs functions
int pulsecount = 0; // variable that stores the number of iterations that the user application has performed.


static ssize_t hcsr04_show(struct kobject *kobj, // function called when the /sys/kernel/hcsr04/hcsr04 file is read
struct kobj_attribute *attr,
char *buf)

{
	return sprintf(buf, "%s %s", pulseptr[pulsecount-1]); // returns the last value stored in the allocated memory (or the last pulse duration read and distance read along with timestamp)
}

static ssize_t hcsr04_show_vals(struct kobject *kobj, // function called when the /sys/kernel/hcsr04/hcsr04vals file is read
struct kobj_attribute *attr,
char *buf)

{
	switch(pulsecount-1) // this switch case checks the number of values passed by the user app. For eg. if the program runs only for 3 iterations first time,
			     // then only those 3 values will be printed. Switch case is implemented instead of a loop as sprintf() will only display the last value
			     // and does not append values to a particular string.
	{
	case 0:
		return sprintf(buf, "%s", pulseptr[0]);
		break;
	case 1:
		return sprintf(buf, "%s%s", pulseptr[0],pulseptr[1]);
		break;
	case 2:
		return sprintf(buf, "%s%s%s", pulseptr[0],pulseptr[1],pulseptr[2]);
		break;
	case 3:
		return sprintf(buf, "%s%s%s%s", pulseptr[0],pulseptr[1],pulseptr[2],pulseptr[3]);
		break;
	case 4:
		return sprintf(buf, "%s%s%s%s%s", pulseptr[0],pulseptr[1],pulseptr[2],pulseptr[3],pulseptr[4]);
		break;	
	}
}

static ssize_t hcsr04_store(struct kobject *kobj, // function called when both the /sys/kernel/hcsr04/hcsr04vals and hcsr04 files are written into
struct kobj_attribute *attr,
const char *buf, 
size_t count)
{
	return 1;
}

static struct kobj_attribute hcsr04_attribute = __ATTR(hcsr04, 0660, hcsr04_show, hcsr04_store); // kernel object attribute for /sys/kernel/hcsr04/hcsr04
static struct kobj_attribute hcsr04_attribute_vals = __ATTR(hcsr04vals, 0660, hcsr04_show_vals, hcsr04_store); // kernel object attribute for /sys/kernel/hcsr04/hcsr04vals


int hcsr04_open(struct inode *inode, struct file *file) // implementation of vfs function to open a device file
{
	int ret = 0;
	if( hcsr04_lock > 0 ) // checks if sensor is already in use
		ret = -EBUSY; 
	else
		hcsr04_lock++; // increments the lock value so that the sensor status is now busy
	return( ret );
}

int hcsr04_close(struct inode *inode, struct file *file) // implementation of vfs function to close a device file
{
	hcsr04_lock = 0;
	return( 0 );
}

ssize_t hcsr04_write(struct file *filp, const char *buffer, size_t length, loff_t * offset) // implementation of vfs function to write data to a device file
{
	if (length == 1){ // this section of the code is executed during the first instance of the write function call in the user application
		gpio_set_value( GPIO_OUT, 0 ); 
		gpio_set_value( GPIO_OUT, 1 ); 
		udelay( 10 );
		gpio_set_value( GPIO_OUT, 0 ); // these 4 lines generate a pulse on the output pin (pin 21) connected to the trigger pin that lasts 10 us
					       // so that we can 'trigger' the sensor
		while( gpio_get_value( GPIO_IN ) == 0 );
		rising = ktime_get(); // this while loop executes as long as the echo pin (connected to gpio input pin 20) is equal to 0 and records the kernel time
				      // as soon as the rising edge occured
		while( gpio_get_value( GPIO_IN ) == 1 );
		falling = ktime_get(); // this while loop executes as long as the echo pin is equal to 1 and records the kernel time as soon as the falling edge
				       // occured
		return( 1 );
	}
	else{ // this section of the code is executed during the second instance of the write function call in the user application
		sprintf(final, "%s Pulse duration : %d us Distance: %s cm\n", dt , pulse , buffer); // stores the date, pulse duration and distance in one single buffer
		if (pulsecount < (5)){ // checks if the number of iteration is less than 5 and then allocates memory for each iteration
			pulseptr[pulsecount] = kmalloc(sizeof(final),GFP_ATOMIC); 
			sprintf (pulseptr[pulsecount],"%s",final); // stores the buffer in the memory location
			pulsecount++; //increments the number of iterations
		}
		else{ // if the number of iterations is equal to 5, this else statement removes the oldest data, pushes each element in the array to the left and then appends new data
		      // towards the end.
			int j = 0;
			while (j<4){
				sprintf (pulseptr[j], "%s", (pulseptr[j+1]) ); // [5 , 20 , 30 , 70 , 50] ===> [20 , 30 , 70 , 50 , 50]
				j++;
			}
			sprintf (pulseptr[4],"%s",final); // stores the new data that is recieved at the last memory location that was allocated.
		}		
	}
	
}

ssize_t hcsr04_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) // implementation of vfs function to read data from a device file
{
	if (count == 4){ // this section of the code is executed during the first instance of the read function call in the user application
		int ret;
		pulse = (int)ktime_to_us( ktime_sub( falling, rising ) ); // subtracts the rising time and falling time to get pulse duration in microseconds
		ret = copy_to_user( buf, &pulse, 4 ); // sends the pulse value to the user application
		return 4;
	}
	else{ // this section of the code is executed during the second instance of the read function call in the user application
		char dtret;
		struct timespec time;
		unsigned long local_time;
		getnstimeofday(&time);
		local_time = (u32)(time.tv_sec - (sys_tz.tz_minuteswest * 60));
		rtc_time_to_tm(local_time, &tm); // variables to initialize the current kernel datetime
	   	sprintf(dt,"%d-%02d-%02d %02d:%02d:%02d", tm.tm_mon + 1, tm.tm_mday,tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec); // stores the datetime into a buffer
	   	dtret = copy_to_user( buf, &dt, 30); // sends the datetime to the user application
	   	return 30;
	}
}


struct file_operations hcsr04_fops = { // File operations for the kernel module corresponding to the vfs api
.owner = THIS_MODULE,
.read = hcsr04_read,
.write = hcsr04_write,
.open = hcsr04_open,
.release = hcsr04_close,
};

static int __init hcsr04_module_init(void) // Executed as soon as the insmod command is used in the command line of linux
{
	char buffer[64]; 
	alloc_chrdev_region(&hcsr04_dev, 0, 1, "hcsr04_dev"); 
	printk(KERN_INFO "%s\n", format_dev_t(buffer, hcsr04_dev)); 
	cdev_init(&hcsr04_cdev, &hcsr04_fops);
	hcsr04_cdev.owner = THIS_MODULE;
	cdev_add(&hcsr04_cdev, hcsr04_dev, 1); // The above 5 lines Initialize the character device and adds it in the linux kernel
	gpio_request( GPIO_OUT, "hcsr04_dev" );
	gpio_request( GPIO_IN, "hcsr04_dev" );
	gpio_direction_output( GPIO_OUT, 0 );
	gpio_direction_input( GPIO_IN ); // The above 4 lines Reserve two GPIO ports (20 and 21) for the character device and set them as output and input respectively
	hcsr04_kobject = kobject_create_and_add("hcsr04", kernel_kobj); // Adds the hcsr04 directory to /sys/kernel
	sysfs_create_file(hcsr04_kobject, &hcsr04_attribute.attr); // Adds the hcsr04 file to /sys/kernel/hcsr04
	sysfs_create_file(hcsr04_kobject, &hcsr04_attribute_vals.attr); // Adds the hcsr04vals file to /sys/kernel/hcsr04vals
	return 0;
}

static void __exit hcsr04_module_cleanup(void) // Executed as soon as the rmmod command is used in the command line of linux
{
	int i =0;
	while (i<5){ // this while loop frees the memory that was allocated through kmalloc
		kfree(pulseptr[i]);
		i++;
	}
	gpio_free( GPIO_OUT );
	gpio_free( GPIO_IN ); // frees the gpio ports 
	hcsr04_lock = 0; // sets the device status back to not busy
	cdev_del(&hcsr04_cdev);
	unregister_chrdev_region( hcsr04_dev, 1 ); // removes the character device from the kernel
	kobject_put( hcsr04_kobject ); // deletes the /sys/kernel/hcsr04 directory and everything under it
	printk(KERN_INFO "Cleanup successful\n");
}

module_init(hcsr04_module_init); 
module_exit(hcsr04_module_cleanup); // sets the initialization and cleanup functions for the module
MODULE_AUTHOR("Mohammed Maaz Shaikh");
MODULE_LICENSE("GPL"); // sets the module author and license.
