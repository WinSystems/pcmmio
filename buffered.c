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
//	Name	 : buffered.c
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

// This array will hold 2000 channel numbers plus a terminating 0xff charcter
unsigned char to_do_channels[2001];

// This array will store the results of 2000 conversions
unsigned short values[2000];

// Keyboard support function prototypes  
void init_keyboard(void);
void close_keyboard(void);
int kbhit(void);
int readch(void);

int main(int argc, char *argv[])

{
	int dev = 0;
	int channel = 0;
	unsigned short result;
	float current;
	unsigned long count = 0;
	int x;

	if (argc !=2)
	{
		printf("\nUsage: buffered <devnum>\n");
		printf("  buffered 1\n");
		exit(1);
	}

	dev = atoi(argv[1]);

	// Set up all 16 channels for the +/- 10 Volt range  
	for(channel =0; channel < 16; channel ++)
	{
		adc_set_channel_mode(dev,channel,ADC_SINGLE_ENDED,ADC_BIPOLAR,ADC_TOP_10V);
		if(mio_error_code)
		{
			printf("\nError occured - %s\n",mio_error_string);
			exit(1);
		}
	}

	// We'll fill the to_do list with the four different channels 500
	// each successively.  

	for(x=0; x < 500; x++)
	{
		to_do_channels[x] = 0;
		to_do_channels[x+500] = 1;
		to_do_channels[x+1000] = 2;
		to_do_channels[x+1500] = 3;
	}

	// Load the "terminator" into the last position   

	to_do_channels[2000] = 0xff;

	//  We'll keep going until a key is pressed  

	init_keyboard();

	while(!kbhit())
	{
		// Start up the conversions. This function returns when all 2000 of
	    // our conversions are complete.  

		adc_buffered_channel_conversions(dev,to_do_channels,values);
		
		count += 2000;

		if(mio_error_code)
		{
			printf("\nError occured - %s\n",mio_error_string);
			exit(1);
		}

		// We'll extract our data from the "values" array. In order to make the
		// display more readable, we'll take a value from each channel display them,
		// and move to the next result.  

		for(x=0; x < 500; x++)
		{
			printf("%08ld  ",count);

			// Get the raw data  
			result = values[x];

			// Convert to voltage  
			current = adc_convert_to_volts(dev, 0, result);

			// Display the value  
			printf("DEV%d CH0 %9.5f ",dev, current);

			// Repeat for channels 1 - 3   
			result = values[x+500];
			current = adc_convert_to_volts(dev, 1, result);
			printf("DEV%d CH1 %9.5f ",dev, current);

			result = values[x+1000];
			current = adc_convert_to_volts(dev, 2, result);
			printf("DEV%d CH2 %9.5f ",dev, current);
			
			result = values[x+1500];
			current = adc_convert_to_volts(dev, 3, result);
			printf("DEV%d CH3 %9.5f ",dev, current);
			printf("\r");
		}
	}
	readch();
	printf("\n\n");
	close_keyboard();
	return 0;
}
