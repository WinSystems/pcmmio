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
//	Name	 : dacbuff.c
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

// This array will store the results of 2000 conversions
unsigned char commands[16385];
unsigned short values[16384];

// Keyboard support function prototypes  
void init_keyboard(void);
void close_keyboard(void);
int kbhit(void);
int readch(void);

int main(int argc, char *argv[])
{
    int dev;
    unsigned x;

    if (argc !=2)
    {
        printf("\nUsage: dacbuff <devnum>\n");
        printf("  dacbuff 1\n");
        exit(1);
    }

    dev = atoi(argv[1]);

    // Set all 8 DAC outputs to a known span +/- 10V
    for(x=0; x<8; x++)
    {
        dac_set_span(dev, x, DAC_SPAN_BI10);
        if(mio_error_code)
        {
            printf("\n%s\n",mio_error_string);
            exit(1);
        }
    }

    // For this program we are going to use only channel 0
    // with 16384 updates.
    // The data will step up from -10V to +10V in 4/65536 increments
    for(x = 0; x < 16384; x++)
    {
        commands[x] = 0;
        values[x] = x * 4;
    }

    // We need to terminate the command so that it knows when its done
    commands[16384] = 0xff;
 
    // This program runs until a key is pressed. It prints nothing on
    // the screen to keep from slowing it down. To see the results it
    // would be necessary to attach an oscilloscope to DAC channel 0.
    init_keyboard();

    printf("DACBUFF running - press any key to exit\n");

    while(!kbhit())
    {
        // This command returns when all 16,384 samples have been
        // sent to the DAC as fast as possible
        dac_buffered_output(dev,commands,values);
        if(mio_error_code)
        {
            printf("\n%s\n",mio_error_string);
            exit(2);
        }
    }

    readch();
    close_keyboard();
    return 0;
}
