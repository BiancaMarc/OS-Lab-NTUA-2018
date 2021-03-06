/*
 * lunix-chrdev.c
 *
 * Implementation of character devices
 * for Lunix:TNG
 *
 * < Your name here >
 *
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include "lunix.h"
#include "lunix-chrdev.h"
#include "lunix-lookup.h"

/*
 * Global data
 */
struct cdev lunix_chrdev_cdev;

/*
 * Just a quick [unlocked] check to see if the cached
 * chrdev state needs to be updated from sensor measurements.
 */
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	
	WARN_ON (!(sensor = state->sensor));
	/* ? */
	if (sensor->msr_data[state->type]->last_update != state->timestamp){
		debug("thelei refresh\n");
		return 1;
	}
	else {
		debug("den thelei refresh\n");
		return 0;
	}
	/* The following return is bogus, just for the stub to compile */
	return 0; /* ? */
}

/*
 * Updates the cached state of a character device
 * based on sensor data. Must be called with the
 * character device state lock held.
 */
static int lunix_chrdev_state_update(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	
	debug("leaving\n");
	sensor = state->sensor;

	/*
	 * Grab the raw data quickly, hold the
	 * spinlock for as little as possible.
	 */
	unsigned long f;
	uint32_t val1,val2,now,val3;
	spin_lock_irq(sensor->lock);
	val1 = sensor->msr_data[state->type]->values[0];	//pairnw to value apo ton sensora kai ananeono to timestamp
	now = sensor->msr_data[state->type]->last_update;	//state->buf_timestamp=sensor->msr_data[state->type]->last_update;
	spin_unlock_irq(sensor->lock);
	
	/* Why use spinlocks? See LDD3, p. 119 */
	//lunix.h

		
	/*
	 * Any new data available?
	 */
	down(&state->lock);
	if(lunix_chrdev_state_needs_refresh(state)){
		if (state->type == BATT){
			val2 = lookup_voltage[val1];
		}
		else if (state->type == TEMP){
			val2 = lookup_temperature[val1];
		}
		else if (state->type == light){	
			val2  = lookup_light[val1];
		}
		else{
			up(&state->lock);
			return -1;
		}
	}

	if ((val3 = sprintf(state->buf_data, "%ld.%3ld", modified/1000, modified%1000))<0){
		debug("error updating\n");
		up(&state->lock);
		return val3;
	}
	else state->buf_lim = val3;
	wake_up_interruptible(&(sensor->wq));
	up(&state->lock);
	debug("updating gone well\n");
	/*
	 * Now we can take our time to format them,
	 * holding only the private state semaphore
	 */

	/* ? */

	debug("leaving\n");
	return 0;
}

/*************************************
 * Implementation of file operations
 * for the Lunix character device
 *************************************/

static int lunix_chrdev_open(struct inode *inode, struct file *filp)
{
	/* Declarations */
	/* ? */
	int ret,minor_num,sensor,measurement;

	debug("entering\n");
	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto out;
    struct lunix_chrdev_state_struct *state;
    state = kmalloc(sizeof(struct lunix_chrdev_state_struct), GFP_KERNEL);
    if (!state){
    	debug("kmalloc failed\n");
    	/* να δω τι τιμη παιρνει το ret για αποτυχια kmalloc*/
    	ret = -1;
    	goto out;
    }
    minor_num = iminor(inode);
    /* minor = αισθητήρας * 8 + μέτρηση*/
    sensor = minor_num/8;
    measurement = minor_num%8;
    state->type = measurement;
    sema_init(&state->lock,1);
    /* δεν ξερω τι να βαλω στα υπολοιπα πεδια(μαλλον λαθος*/
	state->buf_lim=0;
	state->buf_timestamp=0;
	state->sensor = &lunix_sensors[sensor];
  	

    filp->private_data = state;
  	ret = 0;
  	debug("open succeeded");

	/*
	 * Associate this open file with the relevant sensor based on
	 * the minor number of the device node [/dev/sensor<NO>-<TYPE>]
	 */
	
	/* Allocate a new Lunix character device private state structure */
	/* ? */
out:
	debug("leaving, with ret = %d\n", ret);
	return ret;
}

static int lunix_chrdev_release(struct inode *inode, struct file *filp)
{
	debug("released memory\n");
	kfree(filp->private_data);
	/* ? */
	return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* Why? */

	return -EINVAL;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos)
{
	ssize_t ret;

	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;

	state = filp->private_data;
	WARN_ON(!state);

	sensor = state->sensor;
	WARN_ON(!sensor);

	/* Lock? */
	/*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement, do so
	 */
	if (*f_pos == 0) {
		while (lunix_chrdev_state_update(state) == -EAGAIN) {
			/* ? */
			/* The process needs to sleep */
			/* See LDD3, page 153 for a hint */
		}
	}

	/* End of file */
	/* ? */
	
	/* Determine the number of cached bytes to copy to userspace */
	/* ? */

	/* Auto-rewind on EOF mode? */
	/* ? */
out:
	/* Unlock? */
	return ret;
}

static int lunix_chrdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return -EINVAL;
}

static struct file_operations lunix_chrdev_fops = 
{
        .owner          = THIS_MODULE,
	.open           = lunix_chrdev_open,
	.release        = lunix_chrdev_release,
	.read           = lunix_chrdev_read,
	.unlocked_ioctl = lunix_chrdev_ioctl,
	.mmap           = lunix_chrdev_mmap
};

int lunix_chrdev_init(void)
{
	/*
	 * Register the character device with the kernel, asking for
	 * a range of minor numbers (number of sensors * 8 measurements / sensor)
	 * beginning with LINUX_CHRDEV_MAJOR:0
	 */
	int ret;
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
	
	debug("initializing character device\n");
	cdev_init(&lunix_chrdev_cdev, &lunix_chrdev_fops);
	lunix_chrdev_cdev.owner = THIS_MODULE;
	
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	/* ? */
	/* register_chrdev_region?  DONE*/
	ret = register_chrdev_region(dev_no,lunix_minor_cnt,"lunix");
	if (ret < 0) {
		debug("failed to register region, ret = %d\n", ret);
		goto out;
	}	
	/* ? */
	/* cdev_add? */
	ret = cdev_add(lunix_chrdev_cdev,dev_no,lunix_minor_cnt);
	if (ret < 0) {
		debug("failed to add character device\n");
		goto out_with_chrdev_region;
	}
	debug("completed successfully\n");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
out:
	return ret;
}

void lunix_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
		
	debug("entering\n");
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	cdev_del(&lunix_chrdev_cdev);
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
	debug("leaving\n");
}
