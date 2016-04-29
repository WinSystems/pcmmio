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

#ifndef __KERNEL__
#  define __KERNEL__
#endif

#ifndef MODULE
#  define MODULE
#endif

#include <linux/module.h>
#include <linux/moduleparam.h>
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
	#include <linux/autoconf.h>
# else
	#include <generated/autoconf.h>
#endif

#include "mio_io.h"

#define DRVR_NAME		"pcmmio_ws"
#define DRVR_VERSION	"3.0"
#define DRVR_RELDATE	"09Oct2012"

// Function prototypes for local functions 
int get_buffered_int(int dev_num);
void init_io(int chip_number, unsigned io_address);
void clr_int(int dev_num, int bit_number);
int get_int(int dev_num);

// Interrupt handlers 
irqreturn_t mio_handler_1(int, void *);
irqreturn_t mio_handler_2(int, void *);
irqreturn_t mio_handler_3(int, void *);
irqreturn_t mio_handler_4(int, void *);

void common_handler(int dev_num);

// ******************* Device Declarations *****************************

#define MAX_INTS 1024

// Driver major number
static int pcmmio_ws_init_major = 0;	// 0 = allocate dynamically
static int pcmmio_ws_major;

// pcmmio char device structure
static struct cdev pcmmio_ws_cdev[MAX_DEV];
static int cdev_num;

// This holds the base addresses of the IO chips
unsigned base_port[MAX_DEV] = {0,0,0,0};

// Page defintions
#define PAGE0		0x0
#define PAGE1		0x40
#define PAGE2		0x80
#define PAGE3		0xc0

// Our modprobe command line arguments
static int arr_argc_io = 0;
static int arr_argc_irq = 0;
static unsigned short io[MAX_DEV] = {0, 0, 0, 0};
static unsigned short irq[MAX_DEV] = {0, 0, 0, 0};

module_param_array(io, ushort, &arr_argc_io, S_IRUGO);
module_param_array(irq, ushort, &arr_argc_irq, S_IRUGO);

unsigned int_count[MAX_DEV] = {0,0,0,0};

// We will buffer up the transition interrupts and will pass them on
// to waiting applications
unsigned char int_buffer[MAX_DEV][MAX_INTS];
int inptr[MAX_DEV] = {0, 0, 0, 0};
int outptr[MAX_DEV] = {0, 0, 0, 0};

// These declarations create the wait queues. One for each supported device
DECLARE_WAIT_QUEUE_HEAD(wq_adc_1_1);
DECLARE_WAIT_QUEUE_HEAD(wq_adc_1_2);
DECLARE_WAIT_QUEUE_HEAD(wq_dac_1_1);
DECLARE_WAIT_QUEUE_HEAD(wq_dac_1_2);
DECLARE_WAIT_QUEUE_HEAD(wq_dio_1);
DECLARE_WAIT_QUEUE_HEAD(wq_adc_2_1);
DECLARE_WAIT_QUEUE_HEAD(wq_adc_2_2);
DECLARE_WAIT_QUEUE_HEAD(wq_dac_2_1);
DECLARE_WAIT_QUEUE_HEAD(wq_dac_2_2);
DECLARE_WAIT_QUEUE_HEAD(wq_dio_2);
DECLARE_WAIT_QUEUE_HEAD(wq_adc_3_1);
DECLARE_WAIT_QUEUE_HEAD(wq_adc_3_2);
DECLARE_WAIT_QUEUE_HEAD(wq_dac_3_1);
DECLARE_WAIT_QUEUE_HEAD(wq_dac_3_2);
DECLARE_WAIT_QUEUE_HEAD(wq_dio_3);
DECLARE_WAIT_QUEUE_HEAD(wq_adc_4_1);
DECLARE_WAIT_QUEUE_HEAD(wq_adc_4_2);
DECLARE_WAIT_QUEUE_HEAD(wq_dac_4_1);
DECLARE_WAIT_QUEUE_HEAD(wq_dac_4_2);
DECLARE_WAIT_QUEUE_HEAD(wq_dio_4);

unsigned char dac2_port_image[MAX_DEV] = {0, 0, 0, 0};

// mutex & spinlock
static struct mutex mtx[MAX_DEV];
static spinlock_t spnlck[MAX_DEV];

// This is the common interrupt handler. It is called by the chip specific
// handlers with the device number as an argument
void common_handler(int dev_num)
{
	unsigned char status, int_num;

	// Read the interrupt ID register from ADC2 
	dac2_port_image[dev_num] = dac2_port_image[dev_num] | 0x20;
	outb(dac2_port_image[dev_num], base_port[dev_num] + 0x0f);	
	
	while(1) {
		status = inb(base_port[dev_num] + 0x0f);
			
		// Clear ADC1 interrupt 
		if(status & 1) {
			#ifdef DEBUG
				printk("<1>PCMMIO : adc1 interrupt\n");
			#endif

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
			#ifdef DEBUG
				printk("<1>PCMMIO : adc2 interrupt\n");
			#endif

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
			#ifdef DEBUG
				printk("<1>PCMMIO : dac1 interrupt\n");
			#endif

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
			#ifdef DEBUG
				printk("<1>PCMMIO : dio interrupt\n");
			#endif

			int_num = get_int(dev_num);

			if(int_num)	{
				#ifdef DEBUG
					printk("<1>Buffering DIO interrupt on bit %d\n",int_num);
				#endif

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
			#ifdef DEBUG
				printk("<1>PCMMIO : dac2 interrupt\n");
			#endif

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
			#ifdef DEBUG
				printk("<1>PCMMIO : unknown interrupt\n");
			#endif

			break;
	}

	// Reset the access to the interrupt ID register 
	dac2_port_image[dev_num] = dac2_port_image[dev_num] & 0xdf;
	outb(dac2_port_image[dev_num], base_port[dev_num] + 0x0f);
}

// Handler 1
irqreturn_t mio_handler_1(int irq, void *dev_id)
{
	#ifdef DEBUG
		printk("<1>PCMMIO : Interrupt received on Device 1\n");
	#endif

	common_handler(0);
	return IRQ_HANDLED;
}

// Handler 2
irqreturn_t mio_handler_2(int irq, void *dev_id)
{
	#ifdef DEBUG
		printk("<1>PCMMIO : Interrupt received on Device 2\n");
	#endif

	common_handler(1);
	return IRQ_HANDLED;
}

// Handler 3
irqreturn_t mio_handler_3(int irq, void *dev_id)
{
	#ifdef DEBUG
		printk("<1>PCMMIO : Interrupt received on Device 3\n");
	#endif

	common_handler(2);
	return IRQ_HANDLED;
}

// Handler 4
irqreturn_t mio_handler_4(int irq, void *dev_id)
{
	#ifdef DEBUG
		printk("<1>PCMMIO : Interrupt received on Device 4\n");
	#endif

	common_handler(3);
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
		printk("<1>PCMMIO **** OPEN ATTEMPT on uninitialized port *****\n");
		return -1;
	}

	#ifdef DEBUG
		printk ("<1>PCMMIO : device_open(%p)\n", file);
	#endif

	return MIO_SUCCESS;
}

//***********************************************************************
//			DEVICE CLOSE
//***********************************************************************
int device_release(struct inode *inode, struct file *file)
{
	#ifdef DEBUG
		printk ("<1>PCMMIO : device_release(%p,%p)\n", inode, file);
	#endif 

	return MIO_SUCCESS;
}

//***********************************************************************
//			DEVICE IOCTL
//***********************************************************************
long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
	unsigned short word_val;
	unsigned char byte_val, offset_val;
	unsigned int minor = MINOR(file->f_dentry->d_inode->i_rdev);
	int	i;

	// Switch according to the ioctl called 
	switch (ioctl_num)
	{
		case WRITE_DAC_DATA:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call WRITE_DAC_DATA\n");
			#endif

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
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call READ_DAC_STATUS\n");
			#endif

			byte_val = ioctl_param & 0xff;	// This is the DAC number

			if (byte_val)		// DAC 2
				i = inb(base_port[minor] + 0x0f);
			else
				i = inb(base_port[minor] + 0x0b);

			return i;

		case WRITE_DAC_COMMAND:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call WRITE_DAC_COMMAND\n");
			#endif

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
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call WRITE_ADC_COMMAND\n");
			#endif

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
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call READ_ADC_DATA\n");
			#endif

			byte_val = ioctl_param & 0xff;	// This is the ADC number

			if (byte_val)		// ADC 2
				word_val = inw(base_port[minor] + 4);
			else
				word_val = inw(base_port[minor]);

			return word_val;

		case READ_ADC_STATUS:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call READ_ADC_STATUS\n");
			#endif

			byte_val = ioctl_param & 0xff;		// This is the ADC number 
        
			if (byte_val)		// ADC 2 
				i = inb(base_port[minor] + 7);
			else
				i = inb(base_port[minor] + 3);

			return i;

		case WRITE_DIO_BYTE:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call WRITE_DIO_BYTE\n");
			#endif

			// obtain lock before writing
			mutex_lock_interruptible(&mtx[minor]);

			offset_val = ioctl_param & 0xff;
			byte_val = ioctl_param >> 8;
			outb(byte_val, base_port[minor] + 0x10 + offset_val);

			//release lock
			mutex_unlock(&mtx[minor]);

			return MIO_SUCCESS;

		case READ_DIO_BYTE:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call READ_DIO_BYTE\n");
			#endif

			offset_val = ioctl_param & 0xff;
			byte_val = inb(base_port[minor] + 0x10 + offset_val);
			return (byte_val & 0xff);

		case MIO_WRITE_REG:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call MIO_WRITE_REG\n");
			#endif

			// obtain lock before writing
			mutex_lock_interruptible(&mtx[minor]);

			offset_val = ioctl_param & 0xff;
			byte_val = ioctl_param >> 8;
			outb(byte_val, base_port[minor] + offset_val);

			//release lock
			mutex_unlock(&mtx[minor]);

			return MIO_SUCCESS;

		case MIO_READ_REG:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call MIO_READ_REG\n");
			#endif

			offset_val = ioctl_param & 0xff;
			byte_val = inb(base_port[minor] + offset_val);
			return MIO_SUCCESS;

		case WAIT_ADC_INT_1:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call WAIT_ADC_INT_1\n");
				printk("<1>PCMMIO : current process %i (%s) going to sleep\n", current->pid,current->comm);
			#endif

			switch(minor) {
				case 0:
					interruptible_sleep_on(&wq_adc_1_1);
					break;

				case 1:
					interruptible_sleep_on(&wq_adc_2_1);
					break;

				case 2:
					interruptible_sleep_on(&wq_adc_3_1);
					break;

				case 3:
					interruptible_sleep_on(&wq_adc_4_1);
					break;
				
				default:
					break;
			}

			#ifdef DEBUG
				printk("<1>PCMMIO : awoken by adc1 %i (%s)\n", current->pid, current->comm);
			#endif

			return 0;

		case WAIT_ADC_INT_2:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call WAIT_ADC_INT_2\n");
				printk("<1>PCMMIO : current process %i (%s) going to sleep\n", current->pid,current->comm);
			#endif

			switch(minor) {
				case 0:
					interruptible_sleep_on(&wq_adc_1_2);
					break;

				case 1:
					interruptible_sleep_on(&wq_adc_2_2);
					break;

				case 2:
					interruptible_sleep_on(&wq_adc_3_2);
					break;

				case 3:
					interruptible_sleep_on(&wq_adc_4_2);
					break;
				
				default:
					break;
			}

			#ifdef DEBUG
				printk("<1>PCMMIO : awoken by adc2 %i (%s)\n", current->pid, current->comm);
			#endif

			return 0;

		case WAIT_DAC_INT_1:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call WAIT_DAC_INT_1\n");
				printk("<1>PCMMIO : current process %i (%s) going to sleep\n", current->pid,current->comm);
			#endif

			switch(minor) {
				case 0:
					interruptible_sleep_on(&wq_dac_1_1);
					break;

				case 1:
					interruptible_sleep_on(&wq_dac_2_1);
					break;

				case 2:
					interruptible_sleep_on(&wq_dac_3_1);
					break;

				case 3:
					interruptible_sleep_on(&wq_dac_4_1);
					break;
				
				default:
					break;
			}

			#ifdef DEBUG
				printk("<1>PCMMIO : awoken by dac1 %i (%s)\n", current->pid, current->comm);
			#endif

			return 0;

		case WAIT_DAC_INT_2:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call WAIT_DAC_INT_2\n");
				printk("<1>PCMMIO : current process %i (%s) going to sleep\n", current->pid,current->comm);
			#endif

			switch(minor) {
				case 0:
					interruptible_sleep_on(&wq_dac_1_2);
					break;

				case 1:
					interruptible_sleep_on(&wq_dac_2_2);
					break;

				case 2:
					interruptible_sleep_on(&wq_dac_3_2);
					break;

				case 3:
					interruptible_sleep_on(&wq_dac_4_2);
					break;
				
				default:
					break;
			}

			#ifdef DEBUG
				printk("<1>PCMMIO : awoken by dac2 %i (%s)\n", current->pid, current->comm);
			#endif

			return 0;

		case WAIT_DIO_INT:
			if((i = get_buffered_int(minor)))
				return i;

			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call WAIT_DIO_INT\n");
				printk("<1>PCMMIO : current process %i (%s) going to sleep\n", current->pid,current->comm);
			#endif

			switch(minor) {
				case 0:
					interruptible_sleep_on(&wq_dio_1);
					break;

				case 1:
					interruptible_sleep_on(&wq_dio_2);
					break;

				case 2:
					interruptible_sleep_on(&wq_dio_3);
					break;

				case 3:
					interruptible_sleep_on(&wq_dio_4);
					break;
				
				default:
					break;
			}

			#ifdef DEBUG
				printk("<1>PCMMIO : awoken by dio %i (%s)\n", current->pid, current->comm);
			#endif

			i = get_buffered_int(minor);

			return i;

		case READ_IRQ_ASSIGNED:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call READ_IRQ_ASSIGNED\n");
			#endif

			return (irq[minor] & 0xff);

		case DIO_GET_INT:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call DIO_GET_INT\n");
			#endif

			i = get_buffered_int(minor);

			return (i & 0xff);

		// Catch all return
		default:
			#ifdef DEBUG
				printk("<1>PCMMIO : IOCTL call Undefined\n");
			#endif

			return(-EINVAL);
	 }
}

//***********************************************************************
//			Module Declarations
// This structure will hold the functions to be called 
// when a process does something to the our device
//***********************************************************************
struct file_operations pcmmio_ws_fops = {
	owner:				THIS_MODULE,
	unlocked_ioctl:		device_ioctl,
	open:				device_open,
	release:			device_release,
};

//***********************************************************************
//			INIT MODULE
//***********************************************************************
int init_module()
{
	int ret_val, x, io_num;
	dev_t devno;

	// Sign-on
	printk("<1>WinSystems, Inc. PCM-MIO-G Linux Device Driver\n");
	printk("<1>Copyright 2010-2012, All rights reserved\n");
	printk("<1>%s\n", RCSInfo);

	// register the character device
	if(pcmmio_ws_init_major)
	{
		pcmmio_ws_major = pcmmio_ws_init_major;
		devno = MKDEV(pcmmio_ws_major, 0);
		ret_val = register_chrdev_region(devno, 1, DRVR_NAME);
	}
	else
	{
		ret_val = alloc_chrdev_region(&devno, 0, 1, DRVR_NAME);
		pcmmio_ws_major = MAJOR(devno);
	}

	if(ret_val < 0)
	{
		printk("<1>PCMMIO : Cannot obtain major number %d\n", pcmmio_ws_major);
		return -ENODEV;
	}
	else
		printk("<1>PCMMIO : Major number %d assigned\n", pcmmio_ws_major);

	// initialize character devices
	for(x = 0, cdev_num = 0; x < MAX_DEV; x++)
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
				printk("<1>PCMMIO : Added character device %s node %d\n", DRVR_NAME, x);
				cdev_num++;
			}
			else
			{
				printk("<1>PCMMIO : Error %d adding character device %s node %d\n", ret_val, DRVR_NAME, x);
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
			if(request_region(io[x], 0x20, DRVR_NAME) == NULL)
			{
				printk("<1>PCMMIO : Unable to use I/O Address %04X\n", io[x]);
				io[x] = 0;
				continue;
			}
			else
			{
				printk("<1>PCMMIO : Base I/O Address = %04X\n", io[x]);
				init_io(x, io[x]);
				io_num++;
			}
		
			// check and map any interrupts
			if(irq[x])
	        {
			    switch(x)
			    {
					case 0:
						if(request_irq(irq[x], mio_handler_1, IRQF_SHARED, DRVR_NAME, RCSInfo))
							printk("<1>PCMMIO : Unable to register IRQ %d\n", irq[x]);
						else
							printk("<1>PCMMIO : IRQ %d registered to Chip 1\n", irq[x]);
						break;
		
					case 1:
						if(request_irq(irq[x], mio_handler_2, IRQF_SHARED, DRVR_NAME, RCSInfo))
							printk("<1>PCMMIO : Unable to register IRQ %d\n", irq[x]);
						else
							printk("<1>PCMMIO : IRQ %d registered to Chip 2\n", irq[x]);
						break;
		
					case 2:
						if(request_irq(irq[x], mio_handler_3, IRQF_SHARED, DRVR_NAME, RCSInfo))
							printk("<1>PCMMIO : Unable to register IRQ %d\n", irq[x]);
						else
							printk("<1>PCMMIO : IRQ %d registered to Chip 3\n", irq[x]);
						break;
		
					case 3:
						if(request_irq(irq[x], mio_handler_4, IRQF_SHARED, DRVR_NAME, RCSInfo))
							printk("<1>PCMMIO : Unable to register IRQ %d\n", irq[x]);
						else
							printk("<1>PCMMIO : IRQ %d registered to Chip 4\n", irq[x]);
						break;
				}
			}
		}
	}

	if (!io_num)	// no resources allocated
	{
		printk("<1>PCMMIO : No resources available, driver terminating\n");
		goto exit_cdev_delete;
	}

	return MIO_SUCCESS;

exit_cdev_delete:
	while (cdev_num) 
		cdev_del(&pcmmio_ws_cdev[--cdev_num]);

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

	for(i=0; i < cdev_num; i++)
	{
		// remove and unregister the device
		cdev_del(&pcmmio_ws_cdev[i]);
	}

	// Unregister the device 
	unregister_chrdev_region(MKDEV(pcmmio_ws_major, 0), 1);
	pcmmio_ws_major = 0;
}  

// ********************** Device Subroutines **********************
// This array holds the image values of the last write to each I/O
// port. This allows bit manipulation routines to work without having 
// to actually do a read-modify-write to the I/O port.
unsigned char port_images[MAX_DEV][6];

void init_io(int dev_num, unsigned io_address)
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

void clr_int(int dev_num, int bit_number)
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

int get_int(int dev_num)
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

int get_buffered_int(int dev_num)
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

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WinSystems,Inc. PCM-MIO-G Driver");
MODULE_AUTHOR("Paul DeMetrotion");
