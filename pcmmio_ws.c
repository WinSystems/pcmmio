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

struct pcmmio_device {
	unsigned short irq;
};

// Function prototypes for local functions 
static int get_buffered_int(int dev_num);
static void init_io(int chip_number, unsigned io_address);
static void clr_int(int dev_num, int bit_number);
static int get_int(int dev_num);


// ******************* Device Declarations *****************************

#define MAX_INTS 1024

// Driver major number
static int pcmmio_ws_init_major = 0;	// 0 = allocate dynamically
static int pcmmio_ws_major;

// pcmmio char device structure
static struct cdev pcmmio_ws_cdev[MAX_DEV];

// This holds the base addresses of the IO chips
static unsigned base_port[MAX_DEV];

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

// We will buffer up the transition interrupts and will pass them on
// to waiting applications
static unsigned char int_buffer[MAX_DEV][MAX_INTS];
static int inptr[MAX_DEV];
static int outptr[MAX_DEV];

// These declarations create the wait queues. One for each supported device
static DECLARE_WAIT_QUEUE_HEAD(wq_adc_1_1);
static DECLARE_WAIT_QUEUE_HEAD(wq_adc_1_2);
static DECLARE_WAIT_QUEUE_HEAD(wq_dac_1_1);
static DECLARE_WAIT_QUEUE_HEAD(wq_dac_1_2);
static DECLARE_WAIT_QUEUE_HEAD(wq_dio_1);
static DECLARE_WAIT_QUEUE_HEAD(wq_adc_2_1);
static DECLARE_WAIT_QUEUE_HEAD(wq_adc_2_2);
static DECLARE_WAIT_QUEUE_HEAD(wq_dac_2_1);
static DECLARE_WAIT_QUEUE_HEAD(wq_dac_2_2);
static DECLARE_WAIT_QUEUE_HEAD(wq_dio_2);
static DECLARE_WAIT_QUEUE_HEAD(wq_adc_3_1);
static DECLARE_WAIT_QUEUE_HEAD(wq_adc_3_2);
static DECLARE_WAIT_QUEUE_HEAD(wq_dac_3_1);
static DECLARE_WAIT_QUEUE_HEAD(wq_dac_3_2);
static DECLARE_WAIT_QUEUE_HEAD(wq_dio_3);
static DECLARE_WAIT_QUEUE_HEAD(wq_adc_4_1);
static DECLARE_WAIT_QUEUE_HEAD(wq_adc_4_2);
static DECLARE_WAIT_QUEUE_HEAD(wq_dac_4_1);
static DECLARE_WAIT_QUEUE_HEAD(wq_dac_4_2);
static DECLARE_WAIT_QUEUE_HEAD(wq_dio_4);

static unsigned char dac2_port_image[MAX_DEV];

// mutex & spinlock
static struct mutex mtx[MAX_DEV];
static spinlock_t spnlck[MAX_DEV];

/* Interrupt Service Routine */
static irqreturn_t irq_handler(int __irq, void *dev_id)
{
	int dev_num = (unsigned long)dev_id;
	unsigned char status, int_num;

	// Read the interrupt ID register from ADC2 
	dac2_port_image[dev_num] = dac2_port_image[dev_num] | 0x20;
	outb(dac2_port_image[dev_num], base_port[dev_num] + 0x0f);	
	
	while(1) {
		status = inb(base_port[dev_num] + 0x0f);
			
		// Clear ADC1 interrupt 
		if(status & 1) {
			pr_devel("adc1 interrupt\n");

			inb(base_port[dev_num]+1);	// Clear interrupt

			// Wake up anybody waiting for ADC1
			switch(dev_num)
			{
				case 0:
					wake_up_interruptible(&wq_adc_1_1);
					break;

				case 1:
					wake_up_interruptible(&wq_adc_2_1);
					break;

				case 2:
					wake_up_interruptible(&wq_adc_3_1);
					break;

				case 3:
					wake_up_interruptible(&wq_adc_4_1);
					break;
			}
		}
	
		// Clear ADC2 interrupt
		if(status & 2) {			
			pr_devel("adc2 interrupt\n");

			inb(base_port[dev_num]+5);	// Clear interrupt

			// Wake up anybody waiting for ADC2
			switch(dev_num)
			{
				case 0:
					wake_up_interruptible(&wq_adc_1_2);
					break;

				case 1:
					wake_up_interruptible(&wq_adc_2_2);
					break;

				case 2:
					wake_up_interruptible(&wq_adc_3_2);
					break;

				case 3:
					wake_up_interruptible(&wq_adc_4_2);
					break;
			}
		}

		// Clear DAC1 interrupt
		if(status & 4) {
			pr_devel("dac1 interrupt\n");

			inb(base_port[dev_num]+9);	// Clear interrupt

			// Wake up anybody waiting for DAC1
			switch(dev_num)
			{
				case 0:
					wake_up_interruptible(&wq_dac_1_1);
					break;

				case 1:
					wake_up_interruptible(&wq_dac_2_1);
					break;

				case 2:
					wake_up_interruptible(&wq_dac_3_1);
					break;

				case 3:
					wake_up_interruptible(&wq_dac_4_1);
					break;
			}
		}
	
		// DIO interrupt - Find out which bit
		if(status & 8) {
			pr_devel("dio interrupt\n");

			int_num = get_int(dev_num);

			if(int_num)	{
				pr_devel("Buffering DIO interrupt on bit %d\n",int_num);

				// Buffer the interrupt
				int_buffer[dev_num][inptr[dev_num]++] = int_num;
				
				// Pointer wrap
				if(inptr[dev_num] == MAX_INTS)
					inptr[dev_num] = 0;
				
				// Clear the interrupt
				clr_int(dev_num, int_num);
			}

			// Wake up anybody waiting for a DIO interrupt
			switch(dev_num)
			{
				case 0:
					wake_up_interruptible(&wq_dio_1);
					break;

				case 1:
					wake_up_interruptible(&wq_dio_2);
					break;

				case 2:
					wake_up_interruptible(&wq_dio_3);
					break;

				case 3:
					wake_up_interruptible(&wq_dio_4);
					break;
			}
		}

		// Clear DAC2 Interrupt
		if(status & 0x10) {
			pr_devel("dac2 interrupt\n");

			inb(base_port[dev_num]+0x0d);	// Clear interrupt

			// Wake up DAC2 holding processes
			switch(dev_num)
			{
				case 0:
					wake_up_interruptible(&wq_dac_1_2);
					break;

				case 1:
					wake_up_interruptible(&wq_dac_2_2);
					break;

				case 2:
					wake_up_interruptible(&wq_dac_3_2);
					break;

				case 3:
					wake_up_interruptible(&wq_dac_4_2);
					break;
			}
		}

		if((status & 0x1F) == 0)
			pr_devel("unknown interrupt\n");

			break;
	}

	// Reset the access to the interrupt ID register 
	dac2_port_image[dev_num] = dac2_port_image[dev_num] & 0xdf;
	outb(dac2_port_image[dev_num], base_port[dev_num] + 0x0f);

	return IRQ_HANDLED;
}

//***********************************************************************
//			DEVICE OPEN
//***********************************************************************
static int device_open(struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR(inode->i_rdev);

	if(base_port[minor] == 0)
	{
		pr_warning("**** OPEN ATTEMPT on uninitialized port *****\n");
		return -1;
	}

	pr_devel("device_open(%p)\n", file);

	return MIO_SUCCESS;
}

//***********************************************************************
//			DEVICE CLOSE
//***********************************************************************
static int device_release(struct inode *inode, struct file *file)
{
	pr_devel("device_release(%p,%p)\n", inode, file);

	return MIO_SUCCESS;
}

//***********************************************************************
//			DEVICE IOCTL
//***********************************************************************
static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
	unsigned short word_val;
	unsigned char byte_val, offset_val;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	unsigned int minor = MINOR(file->f_dentry->d_inode->i_rdev);
#else
	unsigned int minor = MINOR(file_inode(file)->i_rdev);
#endif
	int	i;

	// Switch according to the ioctl called 
	switch (ioctl_num)
	{
		case WRITE_DAC_DATA:
			pr_devel("IOCTL call WRITE_DAC_DATA\n");

			// obtain lock before writing
			mutex_lock_interruptible(&mtx[minor]);

			byte_val = ioctl_param & 0xff;	// This is the DAC number 
			word_val = (ioctl_param >> 8) & 0xffff;	// This is the data value 

			if (byte_val)		// DAC 2
				outw(word_val, base_port[minor] + 0x0c);
			else
				outw(word_val, base_port[minor] + 8);

			//release lock
			mutex_unlock(&mtx[minor]);

			return MIO_SUCCESS;

		case READ_DAC_STATUS:
			pr_devel("IOCTL call READ_DAC_STATUS\n");

			byte_val = ioctl_param & 0xff;	// This is the DAC number

			if (byte_val)		// DAC 2
				i = inb(base_port[minor] + 0x0f);
			else
				i = inb(base_port[minor] + 0x0b);

			return i;

		case WRITE_DAC_COMMAND:
			pr_devel("IOCTL call WRITE_DAC_COMMAND\n");

			// obtain lock before writing
			mutex_lock_interruptible(&mtx[minor]);

			byte_val = ioctl_param & 0xff;	// This is the DAC number
			offset_val = ioctl_param >> 8;	// This is the data value 

			if (byte_val)		// DAC 2
				outb(offset_val, base_port[minor] + 0x0e);
			else
				outb(offset_val, base_port[minor] + 0x0a);

			//release lock
			mutex_unlock(&mtx[minor]);

			return MIO_SUCCESS;

		case WRITE_ADC_COMMAND:
			pr_devel("IOCTL call WRITE_ADC_COMMAND\n");

			// obtain lock before writing
			mutex_lock_interruptible(&mtx[minor]);

			byte_val = ioctl_param & 0xff;	// This is the ADC number 
			offset_val = ioctl_param >> 8;	// This is the data value 

			if (byte_val)		// ADC 2 
				outb(offset_val, base_port[minor] + 0x06);
			else
				outb(offset_val, base_port[minor] + 0x02);

			//release lock
			mutex_unlock(&mtx[minor]);

			return MIO_SUCCESS;

		case READ_ADC_DATA:
			pr_devel("IOCTL call READ_ADC_DATA\n");

			byte_val = ioctl_param & 0xff;	// This is the ADC number

			if (byte_val)		// ADC 2
				word_val = inw(base_port[minor] + 4);
			else
				word_val = inw(base_port[minor]);

			return word_val;

		case READ_ADC_STATUS:
			pr_devel("IOCTL call READ_ADC_STATUS\n");

			byte_val = ioctl_param & 0xff;		// This is the ADC number 
        
			if (byte_val)		// ADC 2 
				i = inb(base_port[minor] + 7);
			else
				i = inb(base_port[minor] + 3);

			return i;

		case WRITE_DIO_BYTE:
			pr_devel("IOCTL call WRITE_DIO_BYTE\n");

			// obtain lock before writing
			mutex_lock_interruptible(&mtx[minor]);

			offset_val = ioctl_param & 0xff;
			byte_val = ioctl_param >> 8;
			outb(byte_val, base_port[minor] + 0x10 + offset_val);

			//release lock
			mutex_unlock(&mtx[minor]);

			return MIO_SUCCESS;

		case READ_DIO_BYTE:
			pr_devel("IOCTL call READ_DIO_BYTE\n");

			offset_val = ioctl_param & 0xff;
			byte_val = inb(base_port[minor] + 0x10 + offset_val);
			return (byte_val & 0xff);

		case MIO_WRITE_REG:
			pr_devel("IOCTL call MIO_WRITE_REG\n");

			// obtain lock before writing
			mutex_lock_interruptible(&mtx[minor]);

			offset_val = ioctl_param & 0xff;
			byte_val = ioctl_param >> 8;
			outb(byte_val, base_port[minor] + offset_val);

			//release lock
			mutex_unlock(&mtx[minor]);

			return MIO_SUCCESS;

		case MIO_READ_REG:
			pr_devel("IOCTL call MIO_READ_REG\n");

			offset_val = ioctl_param & 0xff;
			byte_val = inb(base_port[minor] + offset_val);
			return MIO_SUCCESS;

		case WAIT_ADC_INT_1:
			pr_devel("IOCTL call WAIT_ADC_INT_1\n");
			pr_devel("current process %i (%s) going to sleep\n", current->pid,current->comm);

			switch(minor) {
				case 0:
					wait_event_interruptible(wq_adc_1_1, 1);
					break;

				case 1:
					wait_event_interruptible(wq_adc_2_1, 1);
					break;

				case 2:
					wait_event_interruptible(wq_adc_3_1, 1);
					break;

				case 3:
					wait_event_interruptible(wq_adc_4_1, 1);
					break;
				
				default:
					break;
			}

			pr_devel("awoken by adc1 %i (%s)\n", current->pid, current->comm);

			return 0;

		case WAIT_ADC_INT_2:
			pr_devel("IOCTL call WAIT_ADC_INT_2\n");
			pr_devel("current process %i (%s) going to sleep\n", current->pid,current->comm);

			switch(minor) {
				case 0:
					wait_event_interruptible(wq_adc_1_2, 1);
					break;

				case 1:
					wait_event_interruptible(wq_adc_2_2, 1);
					break;

				case 2:
					wait_event_interruptible(wq_adc_3_2, 1);
					break;

				case 3:
					wait_event_interruptible(wq_adc_4_2, 1);
					break;
				
				default:
					break;
			}

			pr_devel("awoken by adc2 %i (%s)\n", current->pid, current->comm);

			return 0;

		case WAIT_DAC_INT_1:
			pr_devel("IOCTL call WAIT_DAC_INT_1\n");
			pr_devel("current process %i (%s) going to sleep\n", current->pid,current->comm);

			switch(minor) {
				case 0:
					wait_event_interruptible(wq_dac_1_1, 1);
					break;

				case 1:
					wait_event_interruptible(wq_dac_2_1, 1);
					break;

				case 2:
					wait_event_interruptible(wq_dac_3_1, 1);
					break;

				case 3:
					wait_event_interruptible(wq_dac_4_1, 1);
					break;
				
				default:
					break;
			}

			pr_devel("awoken by dac1 %i (%s)\n", current->pid, current->comm);

			return 0;

		case WAIT_DAC_INT_2:
			pr_devel("IOCTL call WAIT_DAC_INT_2\n");
			pr_devel("current process %i (%s) going to sleep\n", current->pid,current->comm);

			switch(minor) {
				case 0:
					wait_event_interruptible(wq_dac_1_2, 1);
					break;

				case 1:
					wait_event_interruptible(wq_dac_2_2, 1);
					break;

				case 2:
					wait_event_interruptible(wq_dac_3_2, 1);
					break;

				case 3:
					wait_event_interruptible(wq_dac_4_2, 1);
					break;
				
				default:
					break;
			}

			pr_devel("awoken by dac2 %i (%s)\n", current->pid, current->comm);

			return 0;

		case WAIT_DIO_INT:
			if((i = get_buffered_int(minor)))
				return i;

			pr_devel("IOCTL call WAIT_DIO_INT\n");
			pr_devel("current process %i (%s) going to sleep\n", current->pid,current->comm);

			switch(minor) {
				case 0:
					wait_event_interruptible(wq_dio_1, 1);
					break;

				case 1:
					wait_event_interruptible(wq_dio_2, 1);
					break;

				case 2:
					wait_event_interruptible(wq_dio_3, 1);
					break;

				case 3:
					wait_event_interruptible(wq_dio_4, 1);
					break;
				
				default:
					break;
			}

			pr_devel("awoken by dio %i (%s)\n", current->pid, current->comm);

			i = get_buffered_int(minor);

			return i;

		case READ_IRQ_ASSIGNED:
			pr_devel("IOCTL call READ_IRQ_ASSIGNED\n");

			return (irq[minor] & 0xff);

		case DIO_GET_INT:
			pr_devel("IOCTL call DIO_GET_INT\n");

			i = get_buffered_int(minor);

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
	if(pcmmio_ws_init_major)
	{
		pcmmio_ws_major = pcmmio_ws_init_major;
		devno = MKDEV(pcmmio_ws_major, 0);
		ret_val = register_chrdev_region(devno, 1, KBUILD_MODNAME);
	}
	else
	{
		ret_val = alloc_chrdev_region(&devno, 0, 1, KBUILD_MODNAME);
		pcmmio_ws_major = MAJOR(devno);
	}

	if(ret_val < 0)
	{
		pr_err("Cannot obtain major number %d\n", pcmmio_ws_major);
		return -ENODEV;
	}
	else
		pr_info("Major number %d assigned\n", pcmmio_ws_major);

	// initialize character devices
	for(x = 0; x < MAX_DEV; x++)
	{
		if(io[x])	// is device required?
		{
			// add character device
			cdev_init(&pcmmio_ws_cdev[x], &pcmmio_ws_fops);
			pcmmio_ws_cdev[x].owner = THIS_MODULE;
			pcmmio_ws_cdev[x].ops = &pcmmio_ws_fops;
			ret_val = cdev_add(&pcmmio_ws_cdev[x], MKDEV(pcmmio_ws_major, x), MAX_DEV);

			if(!ret_val)
			{
				pr_info("Added character device %s node %d\n", KBUILD_MODNAME, x);
			}
			else
			{
				pr_err("Error %d adding character device %s node %d\n", ret_val, KBUILD_MODNAME, x);
				goto exit_cdev_delete;
			}
		}
	}

	for(x = 0, io_num = 0; x < MAX_DEV; x++)
	{
		if(io[x])	// is device required?
		{
			// initialize mutex array
			mutex_init(&mtx[x]);

			// initialize spinlock array
			spin_lock_init(&spnlck[x]);

			// check and map our I/O region requests
			if(request_region(io[x], 0x20, KBUILD_MODNAME) == NULL)
			{
				pr_err("Unable to use I/O Address %04X\n", io[x]);
				io[x] = 0;
				continue;
			}
			else
			{
				pr_info("Base I/O Address = %04X\n", io[x]);
				init_io(x, io[x]);
				io_num++;
			}
		
			// check and map any interrupts
			if (irq[x]) {
				if(request_irq(irq[x], irq_handler, IRQF_SHARED, KBUILD_MODNAME, (void *)((unsigned long)x)))
					pr_err("Unable to register IRQ %d on chip %d\n", irq[x], x);
				else
					pr_info("IRQ %d registered to Chip %d\n", irq[x], x + 1);
			}
		}
	}

	if (!io_num)	// no resources allocated
	{
		pr_warning("No resources available, driver terminating\n");
		goto exit_cdev_delete;
	}

	return MIO_SUCCESS;

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

	for (i=0; i<MAX_DEV; i++)
	{
		// Unregister I/O Port usage 
		if(io[i])
		{
			release_region(io[i], 0x20);

			if(irq[i]) free_irq(irq[i], RCSInfo);
		}
	}

	// Unregister the device 
	unregister_chrdev_region(MKDEV(pcmmio_ws_major, 0), 1);
	pcmmio_ws_major = 0;
}  

// ********************** Device Subroutines **********************
// This array holds the image values of the last write to each I/O
// port. This allows bit manipulation routines to work without having 
// to actually do a read-modify-write to the I/O port.
static unsigned char port_images[MAX_DEV][6];

static void init_io(int dev_num, unsigned io_address)
{
	int x;
	unsigned short port;

	// save the address for later use
	port = io_address + 0X10;

	// obtain lock
	mutex_lock_interruptible(&mtx[dev_num]);

	// save the address for later use 
	base_port[dev_num] = io_address;

	// Clear all of the I/O ports. This also makes them inputs
	for(x=0; x < 7; x++)
		outb(0, port+x);

	// Clear the image values as well 
	for(x=0; x < 6; x++)
		port_images[dev_num][x] = 0;

	// Set page 2 access, for interrupt enables 
	outb(PAGE2, port+7);

	// Clear all interrupt enables 
	outb(0, port+8);
	outb(0, port+9);
	outb(0, port+0x0a);

	// Restore page 0 register access 
	outb(PAGE0, port+7);

	//release lock
	mutex_unlock(&mtx[dev_num]);
}

static void clr_int(int dev_num, int bit_number)
{
	unsigned short port;
	unsigned short temp;
	unsigned short mask;
	unsigned short dio_port;

	dio_port = base_port[dev_num] + 0x10;

	// Also adjust bit number
	--bit_number;

	// obtain lock
	spin_lock(&spnlck[dev_num]);

	// Calculate the I/O address based upon bit number
	port = (bit_number / 8) + dio_port + 8;

	// Calculate a bit mask based upon the specified bit number
	mask = (1 << (bit_number % 8));

	// Turn on page 2 access
	outb(PAGE2, dio_port+7);

	// Get the current state of the interrupt enable register
	temp = inb(port);

	// Temporarily clear only our enable. This clears the interrupt
	temp= temp & ~mask;    // Clear the enable for this bit

	// Now update the interrupt enable register
	outb(temp, port);

	// Re-enable our interrupt bit
	temp = temp | mask;

	outb(temp, port);

	// Set access back to page 0
	outb(PAGE0, dio_port+7);

	//release lock
	spin_unlock(&spnlck[dev_num]);
}

static int get_int(int dev_num)
{
	int temp;
	int x;
	unsigned short dio_port;

	dio_port = base_port[dev_num] + 0x10;

	// obtain lock
	spin_lock(&spnlck[dev_num]);

	// Read the master interrupt pending register,
    // mask off undefined bits
	temp = inb(dio_port+6) & 0x07;

	// If there are no pending interrupts, return 0
	if((temp & 7) == 0)
	    return 0;

	// There is something pending, now we need to identify it

	// Set access to page 3 for interrupt id register
	outb(PAGE3, dio_port + 7);

	// Read the interrupt ID register for port 0
	temp = inb(dio_port+8);

	// See if any bit set, if so return the bit number
	if(temp != 0)
	{
	    for(x=0; x<=7; x++)
	    {
            if(temp & (1 << x))
	     	{
			    outb(PAGE0, dio_port+7);
				spin_unlock(&spnlck[dev_num]);
				return(x+1);
            }
         }
     }

	// None in port 0, read port 1 interrupt ID register
	temp = inb(dio_port+9);

	// See if any bit set, if so return the bit number
	if(temp != 0)
	{
	    for(x=0; x<=7; x++)
	    {
			if(temp & (1 << x))
			{
			    outb(PAGE0, dio_port+7);
				spin_unlock(&spnlck[dev_num]);
			    return(x+9);
			}
	    }
	}

	// Lastly, read the status of port 2 interrupt ID register
	temp = inb(dio_port+0x0a);

	// If any pending, return the appropriate bit number
	if(temp != 0)
	{
	    for(x=0; x<=7; x++)
	    {
			if(temp & (1 << x))
			{
			   outb(PAGE0, dio_port+7);
				spin_unlock(&spnlck[dev_num]);
			   return(x+17);
			}
	    }
	}

	// We should never get here unless the hardware is seriously
	// misbehaving, but just to be sure, we'll turn the page access
	// back to 0 and return a 0 for no interrupt found
	outb(PAGE0, dio_port+7);

	spin_unlock(&spnlck[dev_num]);

	return 0;
}

static int get_buffered_int(int dev_num)
{
	int temp;

	if(irq[dev_num] == 0)
	{
	    temp = get_int(dev_num);
	    if(temp)
			clr_int(dev_num, temp);
	    return temp;
	}

	if(outptr[dev_num] != inptr[dev_num])
	{
	    temp = int_buffer[dev_num][outptr[dev_num]++];
	    if(outptr[dev_num] == MAX_INTS)
			outptr[dev_num] = 0;
	    return temp;
	}

	return 0;
}
