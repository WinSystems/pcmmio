///**************************************************************************
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
///**************************************************************************
//
//	Name	 : repeat.c
//
//	Project	 : PCMMIO Sample Application Program
//
//	Author	 : Paul DeMetrotion
//
///**************************************************************************
//
//	  Date		Revision	                Description
//	--------	--------	---------------------------------------------
//	11/11/10	  1.0		Original Release	
//	10/09/12	  3.0		Fixed bugs	
//	11/14/18	  4.0		Changes due to driver enhancements
//
///**************************************************************************

#include "mio_io.h"		// Our IOCTL definitions and all function prototypes    
#include <stdio.h>
#include <fcntl.h>      // open 
#include <unistd.h>     // exit
#include <sys/ioctl.h>  // ioctl
#include <stdlib.h>
#include <pthread.h>

// This array will store the results of 2000 conversions
unsigned short values[2000];
volatile unsigned long count = 0;

void *thread_function(void *arg);
void *thread_function2(void *arg);

// Event count, counts the number of events we've handled
volatile int exit_flag = 0;
volatile int dev = 0;

// Keyboard support function prototypes  
void init_keyboard(void);
void close_keyboard(void);
int kbhit(void);
int readch(void);

int main(int argc, char *argv[])
{
    int ch = 0;
    unsigned short result;
    float min, max, current;
    int x, c;
    int res, res2;
    pthread_t a_thread;
    pthread_t b_thread;

    // We'll keep track of the minimum and maximum voltage values
    // we see on a channel as well as the count of conversions
    // completed.
    max = -10.0;
    min	= 10.0;
    count = 0;

    // We must have arguments for device, channel and voltage  
    if (argc != 3)
    {
        printf("\nUsage: repeat <devnum> <channel>\n");
        printf("  repeat 0 2\n");
        exit(1);
    }

    // Device is the first argument. Channel is the second argument. 
    // We'll let the driver check for valid channel numbers just to 
    // show how the mio_error_string works  
    dev = atoi(argv[1]);
    ch = atoi(argv[2]);

    // Check for a valid channel number. Abort if bad
    if (ch < 0 || ch > 15)
    {
        printf("Channel numbers must be between 0 and 15 - Aborting\n");
        exit(0);
    }

    // This call sets the mode for the specified channel. We are going to
    // set up for single-ended +/- 10 V range. That way any legal input 
    // can be accomodated.
    adc_set_channel_mode(dev, ch, ADC_SINGLE_ENDED, ADC_BIPOLAR, ADC_TOP_10V);

    if (mio_error_code)
    {
        printf("\nError occured - %s\n", mio_error_string);
        exit(1); 
    }

    // Enable interrupts on both controllers  
    adc_enable_interrupt(dev, 0);
    adc_enable_interrupt(dev, 1);

    if (mio_error_code)
    {
        printf("\nError occured - %s\n", mio_error_string);
        exit(1);
    }

    res = pthread_create(&a_thread, NULL, thread_function, NULL);

    if (res != 0)
    {
        perror("Thread 1 creation failed\n");
        exit(EXIT_FAILURE);
    }

    res2 = pthread_create(&b_thread, NULL, thread_function2, NULL);

    if (res2 != 0)
    {
        perror("Thread 2 creation failed\n");
        exit(EXIT_FAILURE);
    }

    init_keyboard();

    while(1)
    {
        // We'll keep running until a recognized key is pressed 
        if (kbhit())
        {
            c = readch();

            // The 'C' key clears the min/max and count values
            if (c == 'c' || c == 'C')
            {
                count = 0;
                min = 10.0;
                max = -10.0;
            }
            // The 'N' key moves to the next channel, wrapping from 15
            // back to 0 when appropriate.
            else if (c == 'n' || c == 'N')
            {
                printf("\n");
                ch++;
            
                if (ch > 15)
                    ch = 0;

                // When we change channels we need to make sure we set
                // the channel's mode to a valid range.
                adc_set_channel_mode(dev, ch, ADC_SINGLE_ENDED,ADC_BIPOLAR, ADC_TOP_10V);

                if (mio_error_code)
                {
                    printf("\nError occured - %s\n",mio_error_string);
                    exit(1);
                }

                // A new channel also clears the count and min/max values.
                count = 0;
                min = 10.0;
                max = -10.0;
            }
            else
            {
                exit_flag = 1;

                close_keyboard();

                pthread_cancel(a_thread);
                pthread_cancel(b_thread);

                printf("\n\n");

                adc_convert_single_repeated(dev, 0, 2000, values);
                adc_convert_single_repeated(dev, 8, 2000, values);

                adc_disable_interrupt(dev, 0);
                adc_disable_interrupt(dev, 1);

                exit(0);
            }
        }

        // Finally the real thing. This function-call results in 2000
        // conversions on the specified channel with the results going into
        // a buffer called "values".
        adc_convert_single_repeated(dev, ch, 2000, values);

        if (mio_error_code)
        {
            printf("\nError occured - %s\n", mio_error_string);
            exit(1);
        }

        // Bump up the count
        count += 2000;

        // Now we'll read out the 2000 conversion values. Convert them to 
        // floating point voltages and set the min and max values as appropriate
        for (x = 0; x < 2000; x++)
        {
            result = values[x];
            
            current = adc_convert_to_volts(dev, ch, result);

            // Check and load the min/max values as needed
            if (current < min)
                min = current;

            if (current > max)
                max = current;

            // Print the values
            printf("DEV %01d CH %02d %09ld %9.5f Min =%9.5f Max =%9.5f\r", dev, ch, count, current, min, max);
        }
    }

    return 0;
}

void *thread_function(void *arg)
{
    int c;

    while(1)
    {
        pthread_testcancel();

        if (exit_flag)
            break;

        // This call will put THIS process to sleep until either an
        // interrupt occurs or a terminating signal is sent by the 
        // parent or the system.  
        adc_wait_int(dev, 0);
        ++count;
        usleep(1000);
    }
}

void *thread_function2(void *arg)
{
    int c;

    while(1)
    {
        pthread_testcancel();

        if (exit_flag)
            break;

        // This call will put THIS process to sleep until either an
        // interrupt occurs or a terminating signal is sent by the 
        // parent or the system.  
        adc_wait_int(dev, 1);
        ++count;
        usleep(1000);
    }
}
