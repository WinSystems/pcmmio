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
//	Name	 : getall.c
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
//	11/14/18	  4.0		Changes due to driver enhancements
//
//****************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include "mio_io.h" // Our IOCTL definitions and all function prototypes    

// This program demonstrates usage of the adc_convert_all_chs function call.
// This allows for all 16 chs to be quickly converted and the results passed
// back in an 16 element array. Note that this function does not return voltage values,
// it returns raw 16-bit data directly from the converter.

// This array will receive the result values for all 16 chs
unsigned short values[16];

int main(int argc, char *argv[])
{
    int dev, ch;
    unsigned short result;
    float current;

    if (argc != 2)
    {
        printf("\nUsage: getall <devnum>\n");
        printf("  getall 1\n");
        exit(1);
    }

    dev = atoi(argv[1]);

    // We set the mode on all 16 chs to single-ended bipolar +/- 10V scale.
    // This allows for any device legal input voltages
    for (ch=0; ch < 16; ch++)
    {			
        adc_set_channel_mode(dev, ch, ADC_SINGLE_ENDED, ADC_BIPOLAR, ADC_TOP_10V);
            
        // Check for an error by loooking at mio_error_code
        if (mio_error_code)
        {
            // If an error occurs, print out the string and exit
            printf("%s - Aborting\n", mio_error_string);
            exit(1);
        }
    }
        
    // This is it! When this function returns the values from all 8 chs
    // will be present in the values array
    adc_convert_all_channels(dev,values);
            
    // Check for possible errors
    if (mio_error_code)
    {
        printf("%s - Aborting\n", mio_error_string);
        exit(1);
    }	
            
    // Now we'll extract the data, convert it to volts, and display the results
    for (ch = 0; ch < 16; ch++)
    {
        // This is for print formatting
        if (ch == 4 || ch == 8 || ch == 12)
            printf("\n");
            
        // Get a result from the array
        result = values[ch];
            
        // Convert the raw value to voltage
        current = adc_convert_to_volts(dev, ch, result);

        // Display the result
        printf("CH%2d%8.4f | ", ch, current);
    }
        
    printf("\n\n");
}
