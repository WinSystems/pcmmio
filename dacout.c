//****************************************************************************
//	
//	Copyright 2010-18 by WinSystems Inc.
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
//	Name	 : dacout.c
//
//	Project	 : PCMMIO Sample Application Program
//
//	Author	 : Paul DeMetrotion
//
//****************************************************************************
//
//	  Date		Revision	                Description
//	--------	--------	---------------------------------------------
//	11/11/10	  1.0		Original Release	
//	10/09/12	  3.0		Cleaned up	
//	11/07/18	  4.0		Updated mio_io function names that changed
//
//****************************************************************************

#include "mio_io.h" // Our IOCTL definitions and all function prototypes    
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int dev = 0;
    int channel = 0;
    float voltage;

    // We must have arguments for device, channel and voltage
    if (argc !=4)
    {
        printf("\nUsage: dacout <devnum> <channel> <voltage>\n");
        printf("  dacout 0 2 5.5\n");
        exit(1);
    }

    // Device is the first argument. Channel is the second argument. 
    // We'll let the driver check for valid channel numbers just to 
    // show how the mio_error_string works
    dev = atoi(argv[1]);
    channel = atoi(argv[2]);

    // The same goes for the voltage argument. The driver will tell us
    // if the input is out of range
    voltage = atof(argv[3]);

    printf("Setting DAC device %d - channel %d to %9.5f Volts\n", dev, channel, voltage);

    dac_set_voltage(dev, channel, voltage);

    // Here's where any problems with the input parameters will be determined.
    // by checking mio_error_code for a non-zero value we can detect error
    // conditions.
    if(mio_error_code)
    {
        // We'll print out the error and exit
        printf("%s\n",mio_error_string);
        exit(1);
    }
}
