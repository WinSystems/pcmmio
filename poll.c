//***************************************************************************
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
//***************************************************************************
//
//	Name	 : poll.c
//
//	Project	 : PCMMIO Sample Application Program
//
//	Author	 : Paul DeMetrotion
//
//***************************************************************************
//
//	  Date		Revision	                Description
//	--------	--------	---------------------------------------------
//	11/11/10	  1.0		Original Release	
//	10/09/12	  3.0		Cleaned up	
//	11/07/18	  4.0		Updated mio_io function names that changed
//
//***************************************************************************

#include "mio_io.h"		// Our IOCTL definitions and all function prototypes    
#include <stdio.h>
#include <fcntl.h>      // open 
#include <unistd.h>     // exit
#include <sys/ioctl.h>  // ioctl
#include <stdlib.h>
#include <pthread.h>

// This function will be a sub-processe using the Posix threads 
// capability of Linux. This thread will simulate a type of 
// Interrupt service routine in that it will start up and then suspend 
// until an interrupt occurs and the driver awakens it.

void *thread_function(void *arg);

// Event count, counts the number of events we've handled
volatile int event_count;
volatile int exit_flag = 0;
volatile int dev;

char line[80];

int main(int argc, char *argv[])
{
	int res, res1;
	pthread_t a_thread;
	int c, x;

	if (argc !=2)
	{
		printf("\nUsage: poll <devnum>\n");
		printf("  poll 1\n");
		exit(1);
	}

	dev = atoi(argv[1]);

	// Do a read_bit to test for port/driver availability  
	c = dio_read_bit(dev, 1);

	if(mio_error_code)
	{
		printf("%s\n",mio_error_string);
		exit(1);
	}

	// Here, we'll enable all 24 bits for falling edge interrupts on both 
    // chips. We'll also make sure that they're ready and armed by 
    // explicitly calling the clr_int() function.
    for(x=1; x < 25; x++)
    {
        dio_enab_bit_int(dev,x,FALLING);
		dio_clr_int(dev,x);
    }

    // We'll also clear out any events that are queued up within the 
    // driver and clear any pending interrupts
	dio_enable_interrupt(dev);
	
	if(mio_error_code)
	{
		printf("%s\n",mio_error_string);
		exit(1);
	}

    while((x= dio_get_int(dev)))
    {
		printf("Clearing interrupt on Chip 1 bit %d\n",x);
		dio_clr_int(dev,x);
    }

    // Now the sub-thread will be started  
    printf("Splitting off polling process\n");

    res = pthread_create(&a_thread,NULL,thread_function,NULL);

    if(res != 0)
    {
		perror("Thread creation failed\n");
		exit(EXIT_FAILURE);
    }

    // The thread is now running in the background. It will execute up
    // to the point were there are no interrupts and suspend. We as its
    // parent continue on. The nice thing about POSIX threads is that we're 
    // all in the same data space the parent and the children so we can 
    // share data directly. In this program we share the event_count 
    // variable.

    // We'll continue on in this loop until we're terminated  
    while(1)
    {
		// Print Something so we know the foreground is alive  
		printf("**");

		// The foreground will now wait for an input from the console
		// We could actually go on and do anything we wanted to at this 
		// point.
		fgets(line,75,stdin);

		if(line[0] == 'q' || line[0] == 'Q')
			break;

		// Here's the actual exit. If we hit 'Q' and Enter. The program
		// terminates.
    }

    // This flag is a shared variable that the children can look at to
	// know we're finished and they can exit too.
	exit_flag = 1;

	dio_disable_interrupt(dev);

    // Display our event count total  
    printf("Event count = %05d\r",event_count);

	printf("\n\nAttempting to cancel subthread\n");
    
    // If out children are not in a position to see the exit_flag, we
    // will use a more forceful technique to make sure they terminate with
    // us. If we leave them hanging and we try to re-run the program or
    // if another program wants to talk to the device they may be locked
    // out. This way everything cleans up much nicer.
    pthread_cancel(a_thread);
    printf("\nExiting Now\n");

    fflush(NULL);
}

// This is the the sub-process. For the purpose of this
// example, it does nothing but wait for an interrupt to be active on
// chip 1 and then reports that fact via the console. It also
// increments the shared data variable event_count.
void *thread_function(void *arg)
{
	int c;

	while(1)
	{
		// Test for a thread cancellation signal  
	    pthread_testcancel();

		// Test the exit_flag also for exit  
	    if(exit_flag)
			break;

	    // This call will put THIS process to sleep until either an
	    // interrupt occurs or a terminating signal is sent by the 
	    // parent or the system.
	    c = dio_wait_int(dev);

	    // We check to see if it was a real interrupt instead of a
	    // termination request.
		if(c > 0)
	    {
		    printf("Event sense occured on bit %d\n",c);
		    ++event_count;
	    }
	    else
			break;
	}
}
