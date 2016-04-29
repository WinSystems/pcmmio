///****************************************************************************
//
//	Copyright 2010-12 by WinSystems Inc.
//
//	Permission is hereby granted to the purchaser of WinSystems GPIO cards
//	and CPU products incorporating a GPIO device, to distribute any binary
//	file or files compiled using this source code directly or in any work
//	derived by the user from this file. In no case may the source code,
//	original or derived from this file, be distributed to any third party
//	except by explicit permission of WinSystems. This file is distributed
//	on an "As-is" basis and no warranty as to performance or fitness of pur-
//	poses is expressed or implied. In no case shall WinSystems be liable for
//	any direct or indirect loss or damage, real or consequential resulting
//	from the usage of this source code. It is the user's sole responsibility
//	to determine fitness for any considered purpose.
//
///****************************************************************************
//
//	Name	 : pcmmio.c
//
//	Project	 : PCMMIO Linux Device Driver
//
//	Author	 : Paul DeMetrotion
//
///****************************************************************************
//
//	  Date		Revision	                Description
//	--------	--------	---------------------------------------------
//	11/11/10	  1.0		Original Release
//	08/30/11	  2.1		Fixed bug in write_dio_byte function
//	10/09/12	  3.0		Changes:
//								Function ioctl deprecated for unlocked_ioctl
//								Added mutex/spinlock support
//								Removed pre-2.6.18 interrupt support
//								Added debug messages
//
///****************************************************************************

static char *RCSInfo = "$Id: pcmmio_ws.ko 3.0 2012-10-09 1:26 pdemet Exp $";

// Portions of original code Copyright (C) 1998-99 by Ori Pomerantz

// #define DEBUG 1

/* Helper to format our pr_* functions */
#define pr_fmt(__fmt) KBUILD_MODNAME ": " __fmt

#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "mio_io.h"

#define MOD_DESC "WinSystems,Inc. PCM-MIO-G Driver"
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(MOD_DESC);
MODULE_AUTHOR("Paul DeMetrotion");

#define MAX_INTS 1024

struct pcmmio_device {
	int id;
	unsigned short irq;
	struct cdev pcmmio_ws_cdev;
	unsigned base_port;
	unsigned char int_buffer[MAX_INTS];
	int inptr;
	int outptr;
	wait_queue_head_t	wq_adc_1,
				wq_adc_2,
				wq_dac_1,
				wq_dac_2,
				wq_dio;
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
static int pcmmio_ws_init_major;	// 0 = allocate dynamically
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


/* Interrupt Service Routine */
static irqreturn_t irq_handler(int __irq, void *dev_id)
{
	struct pcmmio_device *pmdev = dev_id;
	unsigned char status, int_num;

	// Read the interrupt ID register from ADC2
	pmdev->dac2_port_image |= 0x20;
	outb(pmdev->dac2_port_image, pmdev->base_port + 0x0f);

	while(1) {
		status = inb(pmdev->base_port + 0x0f);

		// Clear ADC1 interrupt
		if (status & 1) {
			pr_devel("adc1 interrupt\n");

			inb(pmdev->base_port + 1);

			wake_up_interruptible(&pmdev->wq_adc_1);
		}

		// Clear ADC2 interrupt
		if (status & 2) {
			pr_devel("adc2 interrupt\n");

			inb(pmdev->base_port + 5);

			wake_up_interruptible(&pmdev->wq_adc_2);
		}

		// Clear DAC1 interrupt
		if (status & 4) {
			pr_devel("dac1 interrupt\n");

			inb(pmdev->base_port + 9);

			wake_up_interruptible(&pmdev->wq_dac_1);
		}

		// DIO interrupt - Find out which bit
		if (status & 8) {
			pr_devel("dio interrupt\n");

			int_num = get_int(pmdev);

			if (int_num) {
				pr_devel("Buffering DIO interrupt on bit %d\n",int_num);

				// Buffer the interrupt
				pmdev->int_buffer[pmdev->inptr++] = int_num;

				// Pointer wrap
				if (pmdev->inptr == MAX_INTS)
					pmdev->inptr = 0;

				// Clear the interrupt
				clr_int(pmdev, int_num);
			}

			// Wake up anybody waiting for a DIO interrupt
			wake_up_interruptible(&pmdev->wq_dio);
		}

		// Clear DAC2 Interrupt
		if (status & 0x10) {
			pr_devel("dac2 interrupt\n");

			inb(pmdev->base_port + 0x0d);

			wake_up_interruptible(&pmdev->wq_dac_2);
		}

		if ((status & 0x1F) == 0)
			pr_devel("unknown interrupt\n");

		break;
	}

	// Reset the access to the interrupt ID register
	pmdev->dac2_port_image &= 0xdf;
	outb(pmdev->dac2_port_image, pmdev->base_port + 0x0f);

	return IRQ_HANDLED;
}

//***********************************************************************
//			DEVICE OPEN
//***********************************************************************
static int device_open(struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR(inode->i_rdev);
	struct pcmmio_device *pmdev = &pcmmio_devs[minor];

	if (pmdev->base_port == 0) {
		pr_warning("**** OPEN ATTEMPT on uninitialized port *****\n");
		return -1;
	}

	file->private_data = pmdev;

	pr_devel("device_open(%p)\n", file);

	return MIO_SUCCESS;
}

//***********************************************************************
//			DEVICE CLOSE
//***********************************************************************
static int device_release(struct inode *inode, struct file *file)
{
	pr_devel("device_release(%p,%p)\n", inode, file);

	file->private_data = NULL;

	return MIO_SUCCESS;
}

//***********************************************************************
//			DEVICE IOCTL
//***********************************************************************
static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
	unsigned short word_val;
	unsigned char byte_val, offset_val;
	struct pcmmio_device *pmdev = file->private_data;
	unsigned base_port = pmdev->base_port;
	int i;

	// Switch according to the ioctl called
	switch (ioctl_num)
	{
		case WRITE_DAC_DATA:
			pr_devel("IOCTL call WRITE_DAC_DATA\n");

			// obtain lock before writing
			mutex_lock_interruptible(&pmdev->mtx);

			byte_val = ioctl_param & 0xff;	// This is the DAC number
			word_val = (ioctl_param >> 8) & 0xffff;	// This is the data value

			if (byte_val)		// DAC 2
				outw(word_val, base_port + 0x0c);
			else
				outw(word_val, base_port + 8);

			//release lock
			mutex_unlock(&pmdev->mtx);

			return MIO_SUCCESS;

		case READ_DAC_STATUS:
			pr_devel("IOCTL call READ_DAC_STATUS\n");

			byte_val = ioctl_param & 0xff;	// This is the DAC number

			if (byte_val)		// DAC 2
				i = inb(base_port + 0x0f);
			else
				i = inb(base_port + 0x0b);

			return i;

		case WRITE_DAC_COMMAND:
			pr_devel("IOCTL call WRITE_DAC_COMMAND\n");

			// obtain lock before writing
			mutex_lock_interruptible(&pmdev->mtx);

			byte_val = ioctl_param & 0xff;	// This is the DAC number
			offset_val = ioctl_param >> 8;	// This is the data value

			if (byte_val)		// DAC 2
				outb(offset_val, base_port + 0x0e);
			else
				outb(offset_val, base_port + 0x0a);

			//release lock
			mutex_unlock(&pmdev->mtx);

			return MIO_SUCCESS;

		case WRITE_ADC_COMMAND:
			pr_devel("IOCTL call WRITE_ADC_COMMAND\n");

			// obtain lock before writing
			mutex_lock_interruptible(&pmdev->mtx);

			byte_val = ioctl_param & 0xff;	// This is the ADC number
			offset_val = ioctl_param >> 8;	// This is the data value

			if (byte_val)		// ADC 2
				outb(offset_val, base_port + 0x06);
			else
				outb(offset_val, base_port + 0x02);

			//release lock
			mutex_unlock(&pmdev->mtx);

			return MIO_SUCCESS;

		case READ_ADC_DATA:
			pr_devel("IOCTL call READ_ADC_DATA\n");

			byte_val = ioctl_param & 0xff;	// This is the ADC number

			if (byte_val)		// ADC 2
				word_val = inw(base_port + 4);
			else
				word_val = inw(base_port);

			return word_val;

		case READ_ADC_STATUS:
			pr_devel("IOCTL call READ_ADC_STATUS\n");

			byte_val = ioctl_param & 0xff;		// This is the ADC number

			if (byte_val)		// ADC 2
				i = inb(base_port + 7);
			else
				i = inb(base_port + 3);

			return i;

		case WRITE_DIO_BYTE:
			pr_devel("IOCTL call WRITE_DIO_BYTE\n");

			// obtain lock before writing
			mutex_lock_interruptible(&pmdev->mtx);

			offset_val = ioctl_param & 0xff;
			byte_val = ioctl_param >> 8;
			outb(byte_val, base_port + 0x10 + offset_val);

			//release lock
			mutex_unlock(&pmdev->mtx);

			return MIO_SUCCESS;

		case READ_DIO_BYTE:
			pr_devel("IOCTL call READ_DIO_BYTE\n");

			offset_val = ioctl_param & 0xff;
			byte_val = inb(base_port + 0x10 + offset_val);
			return (byte_val & 0xff);

		case MIO_WRITE_REG:
			pr_devel("IOCTL call MIO_WRITE_REG\n");

			// obtain lock before writing
			mutex_lock_interruptible(&pmdev->mtx);

			offset_val = ioctl_param & 0xff;
			byte_val = ioctl_param >> 8;
			outb(byte_val, base_port + offset_val);

			//release lock
			mutex_unlock(&pmdev->mtx);

			return MIO_SUCCESS;

		case MIO_READ_REG:
			pr_devel("IOCTL call MIO_READ_REG\n");

			offset_val = ioctl_param & 0xff;
			byte_val = inb(base_port + offset_val);
			return MIO_SUCCESS;

		case WAIT_ADC_INT_1:
			pr_devel("IOCTL call WAIT_ADC_INT_1\n");
			pr_devel("current process %i (%s) going to sleep\n", current->pid,current->comm);

			wait_event_interruptible(pmdev->wq_adc_1, 1);

			pr_devel("awoken by adc1 %i (%s)\n", current->pid, current->comm);

			return 0;

		case WAIT_ADC_INT_2:
			pr_devel("IOCTL call WAIT_ADC_INT_2\n");
			pr_devel("current process %i (%s) going to sleep\n", current->pid,current->comm);

			wait_event_interruptible(pmdev->wq_adc_2, 1);

			pr_devel("awoken by adc2 %i (%s)\n", current->pid, current->comm);

			return 0;

		case WAIT_DAC_INT_1:
			pr_devel("IOCTL call WAIT_DAC_INT_1\n");
			pr_devel("current process %i (%s) going to sleep\n", current->pid,current->comm);

			wait_event_interruptible(pmdev->wq_dac_1, 1);

			pr_devel("awoken by dac1 %i (%s)\n", current->pid, current->comm);

			return 0;

		case WAIT_DAC_INT_2:
			pr_devel("IOCTL call WAIT_DAC_INT_2\n");
			pr_devel("current process %i (%s) going to sleep\n", current->pid,current->comm);

			wait_event_interruptible(pmdev->wq_dac_2, 1);

			pr_devel("awoken by dac2 %i (%s)\n", current->pid, current->comm);

			return 0;

		case WAIT_DIO_INT:
			if ((i = get_buffered_int(pmdev)))
				return i;

			pr_devel("IOCTL call WAIT_DIO_INT\n");
			pr_devel("current process %i (%s) going to sleep\n", current->pid,current->comm);

			wait_event_interruptible(pmdev->wq_dio, 1);

			pr_devel("awoken by dio %i (%s)\n", current->pid, current->comm);

			i = get_buffered_int(pmdev);

			return i;

		case READ_IRQ_ASSIGNED:
			pr_devel("IOCTL call READ_IRQ_ASSIGNED\n");

			return (pmdev->irq & 0xff);

		case DIO_GET_INT:
			pr_devel("IOCTL call DIO_GET_INT\n");

			i = get_buffered_int(pmdev);

			return (i & 0xff);

		// Catch all return
		default:
			pr_devel("IOCTL call Undefined\n");

			return(-EINVAL);
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

//***********************************************************************
//			INIT MODULE
//***********************************************************************
int init_module()
{
	int ret_val, x, io_num;
	dev_t devno;

	// Sign-on
	pr_info(MOD_DESC "\n");
	pr_info("Copyright 2010-2012, All rights reserved\n");
	pr_info("%s\n", RCSInfo);

	// register the character device
	if (pcmmio_ws_init_major) {
		pcmmio_ws_major = pcmmio_ws_init_major;
		devno = MKDEV(pcmmio_ws_major, 0);
		ret_val = register_chrdev_region(devno, 1, KBUILD_MODNAME);
	} else {
		ret_val = alloc_chrdev_region(&devno, 0, 1, KBUILD_MODNAME);
		pcmmio_ws_major = MAJOR(devno);
	}

	if (ret_val < 0) {
		pr_err("Cannot obtain major number %d\n", pcmmio_ws_major);
		return -ENODEV;
	} else {
		pr_info("Major number %d assigned\n", pcmmio_ws_major);
	}

	for (x = io_num = 0; x < MAX_DEV; x++) {
		struct pcmmio_device *pmdev = &pcmmio_devs[x];

		if (io[x] == 00)
			continue;

		// initialize mutex array
		mutex_init(&pmdev->mtx);

		// initialize spinlock array
		spin_lock_init(&pmdev->spnlck);

		init_waitqueue_head(&pmdev->wq_adc_1);
		init_waitqueue_head(&pmdev->wq_adc_2);
		init_waitqueue_head(&pmdev->wq_dac_1);
		init_waitqueue_head(&pmdev->wq_dac_2);
		init_waitqueue_head(&pmdev->wq_dio);

		// add character device
		cdev_init(&pmdev->pcmmio_ws_cdev, &pcmmio_ws_fops);
		pmdev->pcmmio_ws_cdev.owner = THIS_MODULE;
		pmdev->pcmmio_ws_cdev.ops = &pcmmio_ws_fops;
		ret_val = cdev_add(&pmdev->pcmmio_ws_cdev, MKDEV(pcmmio_ws_major, x), MAX_DEV);

		if (!ret_val) {
			pr_info("Added character device %s node %d\n", KBUILD_MODNAME, x);
		} else {
			pr_err("Error %d adding character device %s node %d\n", ret_val, KBUILD_MODNAME, x);
			goto exit_cdev_delete;
		}

		// check and map our I/O region requests
		if (request_region(io[x], 0x20, KBUILD_MODNAME) == NULL) {
			pr_err("Unable to use I/O Address %04X\n", io[x]);
			io[x] = 0;
			continue;
		} else {
			pr_info("Base I/O Address = %04X\n", io[x]);
			init_io(pmdev, io[x]);
			io_num++;
		}

		// check and map any interrupts
		if (irq[x] == 0)
			continue;

		pmdev->irq = irq[x];

		if (request_irq(irq[x], irq_handler, IRQF_SHARED, KBUILD_MODNAME, pmdev)) {
			pr_err("Unable to register IRQ %d on chip %d\n", irq[x], x);
		} else {
			pmdev->irq = irq[x];
			pr_info("IRQ %d registered to Chip %d\n", irq[x], x + 1);
		}
	}

	if (io_num)
		return MIO_SUCCESS;

	pr_warning("No resources available, driver terminating\n");

exit_cdev_delete:
	unregister_chrdev_region(devno, 1);
	pcmmio_ws_major = 0;

	return -ENODEV;
}

//**************************************************************************
//			CLEANUP_MODULE
//****************************************************************************/
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

	unregister_chrdev_region(MKDEV(pcmmio_ws_major, 0), 1);
	pcmmio_ws_major = 0;
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
	if ((temp & 7) == 0)
		return 0;

	// There is something pending, now we need to identify it

	// Set access to page 3 for interrupt id register
	outb(PAGE3, dio_port + 7);

	/* Check all three ports */
	for (t = 0; t < 3; t++) {
		// Read the interrupt ID register for port
		temp = inb(dio_port + 8 + t);

		// See if any bit set, if so return the bit number
		if (temp != 0) {
			for (x = 0; x <= 7; x++) {
				if (!(temp & (1 << x)))
					continue;

				ret = x + 1 + (8 * t);
				goto isr_out;
			}
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
