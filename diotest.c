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
//	Name	 : diotest.c
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
    int x;
    int dev, reg;

    if (argc !=3)
    {
        printf("\nUsage: diotest <devnum> <regnum>\n");
        printf("  diotest 1 5\n");
        exit(1);
    }

    dev = atoi(argv[1]);
    reg = atoi(argv[2]);

    x = dio_read_bit(dev, 1);	// Just a test for availability
    if(mio_error_code)
    {
        // Print the error and exit, if one occurs
        printf("\n%s\n",mio_error_string);
        exit(1);
    }

    printf("DIO Test - Verify bit and byte manipulation.\n");

    printf("Port %d = 0x%02X\n", reg, dio_read_byte(dev, reg));

    printf("Write 0xA5 to Port %d...", reg);
    dio_write_byte(dev, reg, 0xa5);
    printf("Port %d = 0x%02X\n", reg, dio_read_byte(dev, reg));

    printf("Clear bit 0 of Port %d...", reg);
    dio_write_bit(dev, reg * 8 + 1, 0);
    printf("Port %d = 0x%02X\n", reg, dio_read_byte(dev, reg));

    printf("Set bit 6 of Port %d...", reg);
    dio_write_bit(dev, reg * 8 + 7, 1);
    printf("Port %d = 0x%02X\n", reg, dio_read_byte(dev, reg));

    printf("Write 0xC3 to Port %d...", reg);
    dio_write_byte(dev, reg, 0xc3);
    printf("Port %d = 0x%02X\n", reg, dio_read_byte(dev, reg));

    printf("\n");
}
