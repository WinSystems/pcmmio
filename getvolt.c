//****************************************************************************
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
//****************************************************************************
//
//	Name	 : getvolt.c
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
//
//****************************************************************************

#include "mio_io.h" // Our IOCTL definitions and all function prototypes    
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int dev = 0;
    int channel = 0;
    float result;

    if (argc !=3)
    {
        printf("\nUsage: getvolt <devnum> <channel>\n");
        printf("  getvolt 0 2\n");
        exit(1);
    }

    dev = atoi(argv[1]);
    channel = atoi(argv[2]);

    // We'll let the driver validate the channel number
    result = adc_auto_get_channel_voltage(dev,channel);

    // Check for an error
    if(mio_error_code)
    {
        // If an error occured. Display the error and exit
        printf("%s\n",mio_error_string);
        exit(1);
    }

    // Print the results
    printf(" Device %d : Channel %d  =  %9.4f\n",dev,channel,result);
}
