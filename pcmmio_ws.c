/*
 * pcmmio_ws.c: PCM-MIO-G Driver
 *
 * (C) Copyright 2010-2012, 2016 by WinSystems, Inc.
 * Author: Paul DeMetrotion <pdemetrotion@winsystems.com>
 *
 * Portions of original code Copyright (C) 1998-99 by Ori Pomerantz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

/* Helper to format our pr_* functions */
#define pr_fmt(__fmt) KBUILD_MODNAME ": " __fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/fs.h>

#include "mio_io.h"

#define MOD_DESC "WinSystems, Inc. PCM-MIO-G Driver"
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(MOD_DESC);
MODULE_AUTHOR("Paul DeMetrotion");

#define MAX_INTS 1024

struct pcmmio_device {
	char name[32];
	unsigned short irq;
	struct cdev cdev;
	unsigned base_port;
	unsigned char int_buffer[MAX_INTS];
	int inptr;
	int outptr;
	wait_queue_head_t wq;
	int ready_adc_1, ready_adc_2;
	int ready_dac_1, ready_dac_2;
	int ready_dio;
	unsigned char dac2_port_image;
	unsigned char port_images[6];
	struct mutex mtx;
	spinlock_t spnlck;
};

// Function prototypes for local functions
static int get_buffered_int(struct pcmmio_device *pmdev);
static void init_io(struct pcmmio_device *pmdev, unsigned io_address);
static void clr_int(struct pcmmio_device *pmdev, int bit_number);
static int get_int(struct pcmmio_device *pmdev);


// ******************* Device Declarations *****************************

// Driver major number
static int pcmmio_ws_major;

// Page defintions
#define PAGE0		0x0
#define PAGE1		0x40
#define PAGE2		0x80
#define PAGE3		0xc0

// Our modprobe command line arguments
static unsigned short io[MAX_DEV];
static unsigned short irq[MAX_DEV];

module_param_array(io, ushort, NULL, S_IRUGO);
module_param_array(irq, ushort, NULL, S_IRUGO);

/* Device structs */
struct pcmmio_device pcmmio_devs[MAX_DEV];

static struct class *pcmmio_class;
static dev_t pcmmio_devno;


/* Interrupt Service Routine */
static irqreturn_t irq_handler(int __irq, void *dev_id)
{
	struct pcmmio_device *pmdev = dev_id;
	unsigned char status, int_num;
	int i;

	/* Read the interrupt ID register from ADC2. */
	pmdev->dac2_port_image |= 0x20;
	outb(pmdev->dac2_port_image, pmdev->base_port + 0x0f);
	status = inb(pmdev->base_port + 0x0f);

	pr_devel("interrupt register %02x\n", status);

	/* Check the interrupts */
	for (i = 0; i < 5; i++) {
		if (!(status & (1 << i)))
			continue;

		switch (i) {
		case 0: /* ADC 1 */
			inb(pmdev->base_port + 1);
			pmdev->ready_adc_1 = 1;
			break;

		case 1: /* ADC 2 */
			inb(pmdev->base_port + 5);
			pmdev->ready_adc_2 = 1;
			break;

		case 2: /* DAC 1 */
			inb(pmdev->base_port + 9);
			pmdev->ready_dac_1 = 1;
			break;

		case 3: /* DIO */
			int_num = get_int(pmdev);

			if (int_num) {
				pr_devel("Buffering DIO interrupt on bit %d\n", int_num);
				pmdev->int_buffer[pmdev->inptr++] = int_num;

				if (pmdev->inptr == MAX_INTS)
					pmdev->inptr = 0;

				clr_int(pmdev, int_num);
			}

			pmdev->ready_dio = 1;
			break;

		case 4: /* DAC 2 */
			inb(pmdev->base_port + 0xd);
			pmdev->ready_dac_2 = 1;
			break;
		}
	}

	/* Notify waiters that an event may be of interest to them. */
	wake_up_all(&pmdev->wq);

	if ((status & 0x1F) == 0)
		pr_devel("unknown interrupt\n");

	/* Reset the access to the interrupt ID register */
	pmdev->dac2_port_image &= 0xdf;
	outb(pmdev->dac2_port_image, pmdev->base_port + 0x0f);

	return IRQ_HANDLED;
}

/* Device open */
static int device_open(struct inode *inode, struct file *file)
{
	struct pcmmio_device *pmdev;

	pmdev = container_of(inode->i_cdev, struct pcmmio_device, cdev);

	file->private_data = pmdev;

	pr_devel("[%s] device_open\n", pmdev->name);

	return 0;
}

/* Device close */
static int device_release(struct inode *inode, struct file *file)
{
	struct pcmmio_device *pmdev;

	pmdev = container_of(inode->i_cdev, struct pcmmio_device, cdev);

	pr_devel("[%s] device_release\n", pmdev->name);

	return 0;
}

#define PCMMIO_WAIT_READY(__d, __t) do {		\
	__d->ready_##__t = 0;				\
	wait_event(__d->wq, __d->ready_##__t);		\
} while(0)

/* Device ioctl */
static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
	unsigned short word_val;
	unsigned char byte_val, offset_val;
	struct pcmmio_device *pmdev = file->private_data;
	unsigned base_port = pmdev->base_port;
	int i;

	byte_val = (ioctl_param & 0xff) ? 4 : 0;

	pr_devel("[%s] IOCTL CODE %04X\n", pmdev->name, ioctl_num);

	/* Switch according to the ioctl called */
	switch (ioctl_num) {
	case WRITE_DAC_DATA:
		mutex_lock_interruptible(&pmdev->mtx);

		/* This is the data value. */
		word_val = (ioctl_param >> 8) & 0xffff;
		outw(word_val, base_port + 0x0c + byte_val);

		mutex_unlock(&pmdev->mtx);

		return 0;

	case READ_DAC_STATUS:
		return inb(base_port + 0x0f + byte_val);

	case WRITE_DAC_COMMAND:
		mutex_lock_interruptible(&pmdev->mtx);

		/* This is the data value. */
		offset_val = ioctl_param >> 8;
		outb(offset_val, base_port + 0x0e + byte_val);

		mutex_unlock(&pmdev->mtx);

		return 0;

	case WRITE_ADC_COMMAND:
		mutex_lock_interruptible(&pmdev->mtx);

		/* This is the data value. */
		offset_val = ioctl_param >> 8;
		outb(offset_val, base_port + 0x02 + byte_val);

		mutex_unlock(&pmdev->mtx);

		return 0;

	case READ_ADC_DATA:
		return inw(base_port + byte_val);

	case READ_ADC_STATUS:
		return inb(base_port + 0x03 + byte_val);

	case WRITE_DIO_BYTE:
		mutex_lock_interruptible(&pmdev->mtx);

		offset_val = ioctl_param & 0xff;
		byte_val = ioctl_param >> 8;
		outb(byte_val, base_port + 0x10 + offset_val);

		mutex_unlock(&pmdev->mtx);

		return 0;

	case READ_DIO_BYTE:
		offset_val = ioctl_param & 0xff;
		return inb(base_port + 0x10 + offset_val);

	case MIO_WRITE_REG:
		mutex_lock_interruptible(&pmdev->mtx);

		offset_val = ioctl_param & 0xff;
		byte_val = ioctl_param >> 8;
		outb(byte_val, base_port + offset_val);

		mutex_unlock(&pmdev->mtx);

		return 0;

	case MIO_READ_REG:
		offset_val = ioctl_param & 0xff;
		return inb(base_port + offset_val);

	case WAIT_ADC_INT_1:
		PCMMIO_WAIT_READY(pmdev, adc_1);
		return 0;

	case WAIT_ADC_INT_2:
		PCMMIO_WAIT_READY(pmdev, adc_2);
		return 0;

	case WAIT_DAC_INT_1:
		PCMMIO_WAIT_READY(pmdev, dac_1);
		return 0;

	case WAIT_DAC_INT_2:
		PCMMIO_WAIT_READY(pmdev, dac_2);
		return 0;

	case WAIT_DIO_INT:
		if ((i = get_buffered_int(pmdev)))
			return i;

		PCMMIO_WAIT_READY(pmdev, dio);

		return get_buffered_int(pmdev);

	case READ_IRQ_ASSIGNED:
		return (pmdev->irq & 0xff);

	case DIO_GET_INT:
		return get_buffered_int(pmdev) & 0xff;

	default:
		return -EINVAL;
	}
}

//***********************************************************************
//			Module Declarations
// This structure will hold the functions to be called
// when a process does something to the our device
//***********************************************************************
static struct file_operations pcmmio_ws_fops = {
	owner:			THIS_MODULE,
	unlocked_ioctl:		device_ioctl,
	open:			device_open,
	release:		device_release,
};

/* Module entry point */
int init_module()
{
	int ret_val, x, io_num;
	dev_t dev;

	pr_info(MOD_DESC " loading\n");

	pcmmio_class = class_create(THIS_MODULE, KBUILD_MODNAME);
	if (IS_ERR(pcmmio_class)) {
		pr_err("Could not create module class\n");
		return PTR_ERR(pcmmio_class);
	}

	/* Register the character device. */
	if (pcmmio_ws_major) {
		pcmmio_devno = MKDEV(pcmmio_ws_major, 0);
		ret_val = register_chrdev_region(pcmmio_devno, MAX_DEV, KBUILD_MODNAME);
	} else {
		ret_val = alloc_chrdev_region(&pcmmio_devno, 0, MAX_DEV, KBUILD_MODNAME);
		pcmmio_ws_major = MAJOR(pcmmio_devno);
	}

	if (ret_val < 0) {
		pr_err("Cannot obtain major number %d\n", pcmmio_ws_major);
		return ret_val;
	}

	for (x = io_num = 0; x < MAX_DEV; x++) {
		struct pcmmio_device *pmdev = &pcmmio_devs[x];

		if (io[x] == 0)
			continue;

		/* Initialize device context */
		mutex_init(&pmdev->mtx);
		spin_lock_init(&pmdev->spnlck);
		init_waitqueue_head(&pmdev->wq);

		sprintf(pmdev->name, KBUILD_MODNAME "%c", 'a' + x);

		dev = pcmmio_devno + x;

		/* Initialize character device */
		cdev_init(&pmdev->cdev, &pcmmio_ws_fops);
		ret_val = cdev_add(&pmdev->cdev, dev, 1);

		if (ret_val) {
			pr_err("Error adding character device for node %d\n", x);
			return ret_val;
		}

		/* Check and map our I/O region requests. */
		if (request_region(io[x], 0x20, KBUILD_MODNAME) == NULL) {
			pr_err("Unable to use I/O Address %04X\n", io[x]);
			cdev_del(&pmdev->cdev);
			continue;
		}

		init_io(pmdev, io[x]);

		/* Check and map any interrupts */
		if (irq[x]) {
			pmdev->irq = irq[x];

			if (request_irq(irq[x], irq_handler, IRQF_SHARED, KBUILD_MODNAME, pmdev)) {
				pr_err("Unable to register IRQ %d\n", irq[x]);
				release_region(io[x], 0x20);
				cdev_del(&pmdev->cdev);
				continue;
			}
		}

		io_num++;

		pr_info("[%s] Added new device\n", pmdev->name);

		device_create(pcmmio_class, NULL, dev, NULL, "%s", pmdev->name);
	}

	if (io_num)
		return 0;

	pr_warning("No resources available, driver terminating\n");

	class_destroy(pcmmio_class);
	unregister_chrdev_region(pcmmio_devno, MAX_DEV);

	return -ENODEV;
}

/* Module cleanup */
void cleanup_module()
{
	int i;

	for (i = 0; i < MAX_DEV; i++) {
		struct pcmmio_device *pmdev = &pcmmio_devs[i];

		if (pmdev->base_port)
			release_region(pmdev->base_port, 0x20);

		if (pmdev->irq)
			free_irq(pmdev->irq, pmdev);
	}

	device_destroy(pcmmio_class, pcmmio_devno);
	class_destroy(pcmmio_class);
	unregister_chrdev_region(pcmmio_devno, MAX_DEV);
}

// ********************** Device Subroutines **********************

static void init_io(struct pcmmio_device *pmdev, unsigned io_address)
{
	unsigned short port;
	int x;

	// save the address for later use
	port = io_address + 0X10;

	// obtain lock
	mutex_lock_interruptible(&pmdev->mtx);

	// save the address for later use
	pmdev->base_port = io_address;

	// Clear all of the I/O ports. This also makes them inputs
	for (x = 0; x < 7; x++)
		outb(0, port + x);

	// Clear the image values as well
	for (x = 0; x < 6; x++)
		pmdev->port_images[x] = 0;

	// Set page 2 access, for interrupt enables
	outb(PAGE2, port + 7);

	// Clear all interrupt enables
	outb(0, port+8);
	outb(0, port+9);
	outb(0, port+0x0a);

	// Restore page 0 register access
	outb(PAGE0, port + 7);

	//release lock
	mutex_unlock(&pmdev->mtx);
}

static void clr_int(struct pcmmio_device *pmdev, int bit_number)
{
	unsigned short port;
	unsigned short temp;
	unsigned short mask;
	unsigned short dio_port;

	dio_port = pmdev->base_port + 0x10;

	// Also adjust bit number
	--bit_number;

	// obtain lock
	spin_lock(&pmdev->spnlck);

	// Calculate the I/O address based upon bit number
	port = (bit_number / 8) + dio_port + 8;

	// Calculate a bit mask based upon the specified bit number
	mask = (1 << (bit_number % 8));

	// Turn on page 2 access
	outb(PAGE2, dio_port+7);

	// Get the current state of the interrupt enable register
	temp = inb(port);

	// Temporarily clear only our enable. This clears the interrupt
	temp= temp & ~mask; // Clear the enable for this bit

	// Now update the interrupt enable register
	outb(temp, port);

	// Re-enable our interrupt bit
	temp = temp | mask;

	outb(temp, port);

	// Set access back to page 0
	outb(PAGE0, dio_port+7);

	//release lock
	spin_unlock(&pmdev->spnlck);
}

static int get_int(struct pcmmio_device *pmdev)
{
	int temp;
	int x, t, ret = 0;
	unsigned short dio_port;

	dio_port = pmdev->base_port + 0x10;

	// obtain lock
	spin_lock(&pmdev->spnlck);

	// Read the master interrupt pending register,
	// mask off undefined bits
	temp = inb(dio_port + 6) & 0x07;

	// If there are no pending interrupts, return 0
	if ((temp & 7) == 0) {
		spin_unlock(&pmdev->spnlck);
		return 0;
	}

	// There is something pending, now we need to identify it

	// Set access to page 3 for interrupt id register
	outb(PAGE3, dio_port + 7);

	/* Check all three ports */
	for (t = 0; t < 3; t++) {
		// Read the interrupt ID register for port
		temp = inb(dio_port + 8 + t);

		if (temp == 0)
			continue;

		// See if any bit set, if so return the bit number
		for (x = 0; x <= 7; x++) {
			if (!(temp & (1 << x)))
				continue;

			ret = x + 1 + (8 * t);
			goto isr_out;
		}
	}

	/* We should never get here unless the hardware is seriously
	 * misbehaving. */
	WARN_ONCE(1, KBUILD_MODNAME ": Encountered superflous interrupt");

isr_out:
	outb(PAGE0, dio_port + 7);

	spin_unlock(&pmdev->spnlck);

	return ret;
}

static int get_buffered_int(struct pcmmio_device *pmdev)
{
	int temp;

	if (pmdev->irq == 0) {
		temp = get_int(pmdev);
		if (temp)
			clr_int(pmdev, temp);
		return temp;
	}

	if (pmdev->outptr != pmdev->inptr) {
		temp = pmdev->int_buffer[pmdev->outptr++];
		if (pmdev->outptr == MAX_INTS)
			pmdev->outptr = 0;
		return temp;
	}

	return 0;
}
