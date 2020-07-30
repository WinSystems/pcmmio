//****************************************************************************
//	
//	Copyright 2010-20 by WinSystems Inc.
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
//****************************************************************************
//
//	Name	 : pcmmio_ws.c
//
//	Project	 : PCMMIO Linux Device Driver
//
//	Author	 : Paul DeMetrotion
//
//****************************************************************************
//
//	  Date		Revision	                Description
//	--------	--------	---------------------------------------------
//	11/11/10	  1.0		Original Release	
//	08/30/11	  2.1		Fixed bug in write_dio_byte function	
//	10/09/12	  3.0		Added unlocked_ioctl to address deprecation
//	10/09/12	  3.1		Renamed file to pcmmio_ws
//	11/07/18	  4.0		Upgraded to support Linux 4.x kernels
//                                      Improved ISR performance		
//      07/30/20          4.1           Added Suspend/Resume capability
//
//****************************************************************************

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

//******************************************************************************
//
// Local preprocessor macros
//

#define NUMBER_OF_DIO_PORTS         6

#define IO_PORT_ADDR_COUNT          0x20

//
// macro to turn a struct device's driver_data field into a void * suitable for
// casting to a pcmmio structure pointer...
//
#define to_pcmmio_dev( p )    dev_get_drvdata( p )


//******************************************************************************
//
// Local structures, typedefs and enums...
//

//
// This is it - the software representation of the pcmmio device. It'll be
// instantiated as part of a system "struct device", and will have it's own
// unique system "struct class".
//
struct pcmmio_device {
    char                name[ 32 ];
    unsigned short      irq;
    struct cdev         cdev;
    unsigned            base_port;
    unsigned char       int_buffer[ MAX_INTS ];
    int                 inptr;
    int                 outptr;
    wait_queue_head_t   wq;
    int                 ready_adc_1, 
                        ready_adc_2;
    int                 ready_dac_1, 
                        ready_dac_2;
    int                 ready_dio;
//    unsigned char       port_images[ NUMBER_OF_DIO_PORTS ];
    struct mutex        mtx;
    spinlock_t          spnlck;
    
    struct device      *pDev;       // added so we can self-reference from the
                                    // power management functions
};

typedef struct pcmmio_device    *p_pcmmio_device;



//******************************************************************************
//
// local (static) variables
//

//
// Driver major number
//
static int          pcmmio_ws_major;

//
// Our modprobe command line arguments
//
static unsigned short       io[ MAX_DEV ];
static unsigned short       irq[ MAX_DEV ];

module_param_array( io, ushort, NULL, S_IRUGO );
module_param_array( irq, ushort, NULL, S_IRUGO );

//
// Array of pcmmio device structs, one for each PCM-MIO card
// on the PC104 stack
//
static struct pcmmio_device     pcmmio_devs[ MAX_DEV ];

static struct class            *p_pcmmio_class;

static dev_t                    pcmmio_devno;

static unsigned char            BoardCount = 0;     // incremented each time a board is added
                                                    // used as control so that we don't uninstall
                                                    // non-existant boards


//******************************************************************************
// global (exported) variables                        



//******************************************************************************
//
// local (static) functions
//                        

static int      get_buffered_int( p_pcmmio_device pmdev );
static void     init_io( p_pcmmio_device pmdev, unsigned io_address );
static void     clr_int( p_pcmmio_device pmdev, int bit_number );
static int      get_int( p_pcmmio_device pmdev );


//******************************************************************************
//
// global (exported) functions
//                        







// ******************* Device Declarations *****************************



/* Interrupt Service Routine */
static irqreturn_t irq_handler(int __irq, void *dev_id)
{
    struct pcmmio_device *pmdev = dev_id;
    unsigned char status, int_num;
    int i;

    /* Read the interrupt ID register from ADC2. */
    status = inb(pmdev->base_port + DAC2_IRQ_REG);

    //pr_devel("interrupt register %02x\n", status);

    /* Check the interrupts */
    for (i = 0; i < 5; i++) {
        if (!(status & (1 << i)))
            continue;

        switch (i) {
            case 0: /* ADC 1 */
                inb(pmdev->base_port + ADC1_DATA_HI);
                pmdev->ready_adc_1 = 1;
                break;

            case 1: /* ADC 2 */
                inb(pmdev->base_port + ADC2_DATA_HI);
                pmdev->ready_adc_2 = 1;
                break;

            case 2: /* DAC 1 */
                inb(pmdev->base_port + DAC1_DATA_HI);
                pmdev->ready_dac_1 = 1;
                break;

            case 3: /* DIO */
                int_num = get_int(pmdev);

                if (int_num) {
                    //pr_devel("Buffering DIO interrupt on bit %d\n", int_num);
                    pmdev->int_buffer[pmdev->inptr++] = int_num;

                    if (pmdev->inptr == MAX_INTS)
                        pmdev->inptr = 0;

                    clr_int(pmdev, int_num);
                }

                pmdev->ready_dio = 1;
                break;

            case 4: /* DAC 2 */
                inb(pmdev->base_port + DAC2_DATA_HI);
                pmdev->ready_dac_2 = 1;
                break;
            }
    }

    /* Notify waiters that an event may be of interest to them. */
    wake_up_all(&pmdev->wq);

    if ((status & 0x1F) == 0)
        pr_devel("unknown interrupt\n");

    return IRQ_HANDLED;
}


//****************************************************************************
//
//! \fucntion  static InitializeIntRegs( p_pcmmio_device pDev )
//
//! \brief  Initializes the HW's interrupt related registers to their runtime
//!         values. Called by the device's initialization code and by their
//!         power management "resume" routine when the system is coming out
//!         of suspend or hibernate
//
//! @param[in]      pDev        Pointer to struct pcmmio_device
//
//! \return     void
//
//****************************************************************************
static void InitializeIntRegs( p_pcmmio_device pDev )
{
   // 
   // configure dio/adc1 for selected irq
   //
    
   outb( 0x08, pDev->base_port + ADC1_RSRC_ENBL );
   outb( pDev->irq, pDev->base_port + ADC1_RESOURCE );
   outb( 0x10, pDev->base_port + ADC1_RSRC_ENBL );
   outb( pDev->irq, pDev->base_port + DIO_RESOURCE );
   outb( 0x01, pDev->base_port + ADC1_RSRC_ENBL );	// Enable the interrupt

   //
   // configure adc2 for selected irq
   //
   
   outb( 0x08, pDev->base_port + ADC2_RSRC_ENBL );
   outb( pDev->irq, pDev->base_port + ADC2_RESOURCE );
   outb( 0x01, pDev->base_port + ADC2_RSRC_ENBL );	// Enable the interrupt

   //
   // configure dac1 for selected irq
   //
   
   outb( 0x18, pDev->base_port + DAC1_RSRC_ENBL );
   outb( pDev->irq, pDev->base_port + DAC1_RESOURCE );
   outb( 0x11, pDev->base_port + DAC1_RSRC_ENBL );	// Enable the interrupt
   
   //
   // configure dac2 for selected irq
   //
   
   outb( 0x38, pDev->base_port + DAC2_RSRC_ENBL );
   outb( pDev->irq, pDev->base_port + DAC2_RESOURCE );
   outb( 0x31, pDev->base_port + DAC2_RSRC_ENBL );	// Enable the interrupt
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

    pr_devel("[%s] IOCTL CODE %04X\n", pmdev->name, ioctl_num);

    /* Switch according to the ioctl called */
    switch (ioctl_num) {
        case ADC_WRITE_COMMAND:
            mutex_lock_interruptible(&pmdev->mtx);

            /* This is the data value. */
            offset_val = (ioctl_param & 0xff) ? 4 : 0;
            byte_val = ioctl_param >> 8;
            outb(byte_val, base_port + ADC1_COMMAND + offset_val);

            mutex_unlock(&pmdev->mtx);

            return 0;

        case ADC_READ_DATA:
            offset_val = (ioctl_param & 0xff) ? 4 : 0;
            return inw(base_port + offset_val);

        case ADC_READ_STATUS:
            offset_val = (ioctl_param & 0xff) ? 4 : 0;
            return inb(base_port + ADC1_STATUS + offset_val);

        case ADC1_WAIT_INT:
            PCMMIO_WAIT_READY(pmdev, adc_1);
            return 0;

        case ADC2_WAIT_INT:
            PCMMIO_WAIT_READY(pmdev, adc_2);
            return 0;

        case DAC_WRITE_DATA:
            mutex_lock_interruptible(&pmdev->mtx);

            /* This is the data value. */
            offset_val = (ioctl_param & 0xff) ? 4 : 0;
            word_val = (ioctl_param >> 8) & 0xffff;
            outw(word_val, base_port + DAC1_DATA_LO + offset_val);

            mutex_unlock(&pmdev->mtx);

            return 0;

        case DAC_READ_STATUS:
            offset_val = (ioctl_param & 0xff) ? 4 : 0;
            return inb(base_port + DAC1_STATUS + offset_val);

        case DAC_WRITE_COMMAND:
            mutex_lock_interruptible(&pmdev->mtx);

            /* This is the data value. */
            offset_val = (ioctl_param & 0xff) ? 4 : 0;
            byte_val = ioctl_param >> 8;
            outb(byte_val, base_port + DAC1_COMMAND + offset_val);

            mutex_unlock(&pmdev->mtx);

            return 0;

        case DAC1_WAIT_INT:
            PCMMIO_WAIT_READY(pmdev, dac_1);
            return 0;

        case DAC2_WAIT_INT:
            PCMMIO_WAIT_READY(pmdev, dac_2);
            return 0;

        case DIO_WRITE_BYTE:
            mutex_lock_interruptible(&pmdev->mtx);

            offset_val = ioctl_param & 0xff;
            byte_val = ioctl_param >> 8;
            outb(byte_val, base_port + DIO_PORT0 + offset_val);

            mutex_unlock(&pmdev->mtx);

            return 0;

        case DIO_READ_BYTE:
            offset_val = ioctl_param & 0xff;
            return inb(base_port + DIO_PORT0 + offset_val);

        case DIO_WAIT_INT:
            if ((i = get_buffered_int(pmdev)))
                return i;

            PCMMIO_WAIT_READY(pmdev, dio);

            return get_buffered_int(pmdev);

        case DIO_GET_INT:
            return get_buffered_int(pmdev) & 0xff;

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

        default:
            return -EINVAL;
    }
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// power management device operations & structure
//
//****************************************************************************
//
//! \fucntion   static int pcmmio_suspend( struct device *pDev )
//
//! \brief  Called by the power management framework when the system is entering
//!         a "suspend" state.
//
//! @param[in]  pDev    Pointer to "parent" device for this pcmmio_device
//
//! \return     Always returns 0
//
//! \note       This call is from the power management capability of this
//!             device's class
//
//****************************************************************************
static int pcmmio_suspend( struct device *pDev )
{
   p_pcmmio_device pMioDev = ( p_pcmmio_device ) to_pcmmio_dev( pDev );   // pointer to the 
                                                                          // pcmmmio device
   mutex_lock( &pMioDev->mtx );

   pr_info( "%s - /dev/%s\n", __func__, pMioDev->name );

   mutex_unlock( &pMioDev->mtx );

   return 0;
}

//****************************************************************************
//
//! \fucntion   static int pcmmio_resume( struct device *pDev )
//
//! \brief  Called by the power management framework when the system is entering
//!         a "suspend" state.
//
//! @param[in]  pDev    Pointer to "parent" device for this pcmmio_device
//
//! \return     Always returns 0
//
//! \note       This call is from the power management capability of this
//!             device's class
//
//****************************************************************************
static int pcmmio_resume( struct device *pDev )
{
   p_pcmmio_device pMioDev = ( p_pcmmio_device ) to_pcmmio_dev( pDev );   // pointer to the 
                                                                          // pcmmmio device
   mutex_lock( &pMioDev->mtx );
   
   pr_info( "%s - /dev/%s\n", __func__, pMioDev->name );
   
   InitializeIntRegs( pMioDev );

   mutex_unlock( &pMioDev->mtx );

   return 0;
}

//****************************************************************************
//
//! \fucntion   static int pcmmio_idle( struct device *pDev )
//
//! \brief  Called by the power management framework when the system is entering
//!         a "suspend" state.
//
//! @param[in]  pDev    Pointer to "parent" device for this pcmmio_device
//
//! \return     Always returns 0
//
//! \note       This call is from the power management capability of this
//!             device's class
//
//****************************************************************************
static int pcmmio_idle( struct device *pDev )
{
   p_pcmmio_device pMioDev = ( p_pcmmio_device ) to_pcmmio_dev( pDev );   // pointer to the 
                                                                          // pcmmmio device
   mutex_lock( &pMioDev->mtx );
   
   pr_info( "%s - /dev/%s\n", __func__, pMioDev->name );
                                                                  
   mutex_unlock( &pMioDev->mtx );

   return 0;
}


static UNIVERSAL_DEV_PM_OPS( pcmmio_class_dev_pm_ops, pcmmio_suspend, pcmmio_resume, pcmmio_idle );


////////////////////////////////////////////////////////////////////////////

//***********************************************************************
//			Module Declarations
// This structure will hold the functions to be called
// when a process does something to the our device
//***********************************************************************
static struct file_operations pcmmio_ws_fops = {
    owner:                  THIS_MODULE,
    unlocked_ioctl:         device_ioctl,
    open:                   device_open,
    release:                device_release,
};

/* Module entry point */
int init_module()
{
    int     ret_val, 
            i, 
            io_num;
    dev_t   dev;

    pr_info(MOD_DESC " loading\n");

    p_pcmmio_class = class_create( THIS_MODULE, KBUILD_MODNAME );
    if (IS_ERR(p_pcmmio_class)) {
        pr_err("Could not create module class\n");
        return PTR_ERR(p_pcmmio_class);
    }
    
    p_pcmmio_class->pm = &pcmmio_class_dev_pm_ops;

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

    for (i = io_num = 0; i < MAX_DEV; i++) {
        struct pcmmio_device *pmdev = &pcmmio_devs[i];
        
        if ( io[ i ] == 0 )
            continue;

        /* Initialize device context */
        mutex_init(&pmdev->mtx);
        spin_lock_init(&pmdev->spnlck);
        init_waitqueue_head(&pmdev->wq);
        
        sprintf(pmdev->name, KBUILD_MODNAME "%c", 'a' + i);

        dev = pcmmio_devno + i;

        /* Initialize character device */
        cdev_init(&pmdev->cdev, &pcmmio_ws_fops);
        ret_val = cdev_add(&pmdev->cdev, dev, 1);

        if (ret_val) {
            pr_err("Error adding character device for node %d\n", i);
            return ret_val;
        }

        /* Check and map our I/O region requests. */
        if ( request_region( io[ i ], IO_PORT_ADDR_COUNT, KBUILD_MODNAME ) == NULL ) {
            pr_err("Unable to use I/O Address %04X\n", io[i]);
            cdev_del(&pmdev->cdev);
            continue;
        }

        init_io( pmdev, io[i] );

        /* Check and map any interrupts */
        if ( irq[ i ] ) 
        {
            pmdev->irq = irq[ i ];

            if ( request_irq( pmdev->irq, irq_handler, IRQF_SHARED, KBUILD_MODNAME, pmdev ) ) {
                pr_err("Unable to register IRQ %d\n", irq[i]);
                release_region( io[i], IO_PORT_ADDR_COUNT );
                cdev_del(&pmdev->cdev);
                continue;
            }

            InitializeIntRegs( pmdev );
        }

        io_num++;
        BoardCount++;

        pr_info("[%s] Added new device\n", pmdev->name );

        pmdev->pDev = device_create( p_pcmmio_class, 
                                     NULL,                  // parent 
                                     dev,                   // dev_t
                                     NULL,                  // void ptr to drvdata 
                                     "%s", 
                                     pmdev->name );
        
        pmdev->pDev->driver_data = ( void *)( pmdev );      // pmdev->pDev is a pointer to the system device 
                                                            // returned by device_create. We're setting that
                                                            // device's driver_data to be a pointer to the
                                                            // current pcmmio_device
    }

    if (io_num)
        return 0;

    pr_warning("No resources available, driver terminating\n");

    class_destroy(p_pcmmio_class);
    unregister_chrdev_region(pcmmio_devno, MAX_DEV);

    return -ENODEV;
}

//***********************************************************************
//
// Module cleanup
//
void cleanup_module()
{
    int i;
    
    pr_devel( "Entered %s\n", __func__ );

    for (i = 0; i < MAX_DEV; i++) {
        
        struct pcmmio_device *pmdev = &pcmmio_devs[ i ];
        
        if ( i < BoardCount )
            {
               if ( pmdev->base_port )
               {
                  release_region( pmdev->base_port, IO_PORT_ADDR_COUNT );
               }

               if ( pmdev->irq )
               {
                 free_irq(pmdev->irq, pmdev);
               }

               device_destroy( p_pcmmio_class, pcmmio_devno + i );
               cdev_del(&pmdev->cdev);
               
               pr_info("[%s] Removed existing device\n", pmdev->name );
            }
        else
            {
               break;
            }
    }

    class_destroy( p_pcmmio_class );
    unregister_chrdev_region( pcmmio_devno, MAX_DEV );
    
    pr_devel("Exiting %s\n", __func__ );
}

// ********************** Device Subroutines **********************

static void init_io( p_pcmmio_device pmdev, unsigned io_address)
{
    int i;

    // obtain lock
    mutex_lock_interruptible(&pmdev->mtx);

    // save the address for later use
    pmdev->base_port = io_address;

    // Clear all of the I/O ports. This also makes them inputs
    for (i = 0; i < NUMBER_OF_DIO_PORTS; i++)
        outb(0, io_address + DIO_PORT0 + i);

    // Clear the image values as well
    //for (i = 0; i < NUMBER_OF_DIO_PORTS; i++)
    //    pmdev->port_images[i] = 0;

    // Set page 2 access, for interrupt enables
    outb(PAGE2, io_address + DIO_PAGE_LOCK);

    // Clear all interrupt enables
    outb(0, io_address + DIO_ENABLE0);
    outb(0, io_address + DIO_ENABLE1);
    outb(0, io_address + DIO_ENABLE2);

    // Restore page 3 register access
    outb(PAGE3, io_address + DIO_PAGE_LOCK);

    //release lock
    mutex_unlock(&pmdev->mtx);
}

//***********************************************************************

static void clr_int( p_pcmmio_device pmdev, int bit_number)
{
    unsigned short port;
    unsigned short temp;
    unsigned short mask;

    // Also adjust bit number
    --bit_number;

    // obtain lock
    spin_lock(&pmdev->spnlck);

    // Calculate the I/O address based upon bit number
    port = pmdev->base_port + DIO_ENABLE0 + (bit_number / 8);

    // Calculate a bit mask based upon the specified bit number
    mask = (1 << (bit_number % 8));

    // Set page 2 access, for interrupt enables
    outb(PAGE2, pmdev->base_port + DIO_PAGE_LOCK);

    // Get the current state of the interrupt enable register
    temp = inb(port);

    // Temporarily clear only our enable. This clears the interrupt
    temp= temp & ~mask; // Clear the enable for this bit

    // Now update the interrupt enable register
    outb(temp, port);

    // Re-enable our interrupt bit
    temp = temp | mask;

    outb(temp, port);

    // Restore page 3 register access
    outb(PAGE3, pmdev->base_port + DIO_PAGE_LOCK);

    //release lock
    spin_unlock(&pmdev->spnlck);
}

//***********************************************************************

static int get_int( p_pcmmio_device pmdev)
{
    int temp;
    int i, j, ret = 0;

    // obtain lock
    spin_lock(&pmdev->spnlck);

    // Read the master interrupt pending register,
    // mask off undefined bits
    temp = inb(pmdev->base_port + DIO_INT_PENDING) & 0x07;

    // If there are no pending interrupts, return 0
    if ((temp & 0x07) == 0) {
        spin_unlock(&pmdev->spnlck);
        return 0;
    }

    // There is something pending, now we need to identify it
    /* Check all three ports */
    for (j = 0; j < 3; j++) {
        // Read the interrupt ID register for port
        temp = inb(pmdev->base_port + DIO_INT_ID0 + j);

        if (temp == 0)
            continue;

        // See if any bit set, if so return the bit number
        for (i = 0; i <= 7; i++) {
            if (!(temp & (1 << i)))
                continue;

            ret = i + 1 + (8 * j);
            goto isr_out;
        }
    }

    /* We should never get here unless the hardware is seriously
     * misbehaving. */
    WARN_ONCE(1, KBUILD_MODNAME ": Encountered superflous interrupt");

isr_out:
    spin_unlock(&pmdev->spnlck);

    return ret;
}

//***********************************************************************

static int get_buffered_int( p_pcmmio_device pmdev)
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
