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
//	Name	 : flash.c
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
#include <unistd.h>

// Keyboard support function prototypes  
void init_keyboard(void);
void close_keyboard(void);
int kbhit(void);
int readch(void);

int main(int argc, char *argv[])
{
	int x, dev;

	if (argc !=2)
	{
		printf("\nUsage: flash <devnum>\n");
		printf("  flash 1\n");
		exit(1);
	}

	dev = atoi(argv[1]);

	x = dio_read_bit(dev, 1);	// Just a test for availability
	if(mio_error_code)
	{
		// Print the error and exit, if one occurs
		printf("\n%s\n",mio_error_string);
		exit(1);
	}

	printf("Flashing - Press any key to exit\n");

	init_keyboard();

	while(!kbhit())
	{
		for(x=1; x<=48; x++)
		{
			dio_set_bit(dev, x);	// Turn on the LED

			// Ideally, we should check mio_error_code after all calls. Practically, there's little to 
			// go wrong once we've validated the driver presence.
			// Got to sleep for 250ms
			usleep(25000);

			dio_clr_bit(dev, x);	// Turn off the LED
		}
	}

	readch();
	close_keyboard();
	printf("\n");
}
