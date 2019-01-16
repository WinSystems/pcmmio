//****************************************************************************
//	
//	Copyright 2018 by WinSystems Inc.
//
//****************************************************************************
//
//	Name	 : dioTest.c
//
//	Project	 : PCM-MIO Test Code - Jenkins Suite
//
//	Author	 : Paul DeMetrotion
//
//****************************************************************************
//
//	  Date		  Rev	                Description
//	--------    -------	   ---------------------------------------------
//	11/09/18	  1.0		Original Release
//
//****************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "mio_io.h" // Our IOCTL definitions and all function prototypes    
#include "jenkins.h"

#define DEVICE        0
#define MAJOR_VER     1
#define MINOR_VER     0

// list of tests
TEST test[] = { 
    {1, "dio_reset_device", PASS},
    {2, "dio_read_bit + dio_write_bit", PASS},
    {3, "dio_set_bit + dio_clr_bit", PASS},
    {4, "dio_read_byte + dio_write_byte", PASS},
    {5, "dio_enab_bit_int + dio_disab_bit_int + dio_get_int", PASS},
    {6, "dio_wait_int", PASS},
    {7, "dio_clr_int", PASS}
};
    
void *thread_function(void *arguments);

struct arg_struct {
    int test;
    int bit;
};

int main(int argc, char *argv[])
{
    char *app_name = "diotest";
    int t, bit;
    unsigned char portInit[] = { 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6 };
    unsigned char irq, temp;
    unsigned int bitValue;
    unsigned int irqValue;
    unsigned long irqArray;
    pthread_t a_thread;
    struct arg_struct args;
    
    srand(time(0));

    printf("PCM-MIO Application : %s\n", app_name);
    printf("Version %d.%d\n\n", MAJOR_VER, MINOR_VER);

    if (argc > 1)
    {
        printf("Usage error:\n");
        printf("  %s\n", app_name);
        exit(1);
    }

    // run all tests
    for (t = 1; t <= sizeof(test) / sizeof(TEST); t++)
    {
        printf("\nTest %d: %s ... ", test[t - 1].number, test[t - 1].name);
        
        switch(t) {
            case 1:  // reset test
                // reset device to known state
                dio_reset_device(DEVICE);

                if (mio_error_code)
                    TEST_FAIL;

                // check error conditions
                dio_reset_device(4);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                break;
                
            case 2: // read/write bit test
                // read and write all bits
                for (int i = 1; i <= 48; i++)
                {
                    bitValue = dio_read_bit(DEVICE, i);

                    if (mio_error_code != MIO_SUCCESS || bitValue != 0)
                    {
                        TEST_FAIL;
                        continue;
                    }

                    dio_write_bit(DEVICE, i, 1);
    
                    if (mio_error_code != MIO_SUCCESS)
                    {
                        TEST_FAIL;
                        continue;
                    }

                    bitValue = dio_read_bit(DEVICE, i);

                    if (mio_error_code != MIO_SUCCESS || bitValue != 1)
                    {
                        TEST_FAIL;
                        continue;
                    }

                    dio_write_bit(DEVICE, i, 0);

                    if (mio_error_code != MIO_SUCCESS)
                    {
                        TEST_FAIL;
                        continue;
                    }

                    bitValue = dio_read_bit(DEVICE, i);

                    if (mio_error_code != MIO_SUCCESS || bitValue != 0)
                    {
                        TEST_FAIL;
                        continue;
                    }
                }

                // check error conditions
                dio_write_bit(4, 1, 0);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dio_write_bit(DEVICE, 0, 0);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dio_write_bit(DEVICE, 49, 1);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dio_write_bit(DEVICE, 1, 2);

                if (mio_error_code != MIO_BAD_VALUE)
                    TEST_FAIL;

                dio_read_bit(4, 1);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dio_read_bit(DEVICE, 0);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dio_read_bit(DEVICE, 49);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                break;
                
            case 3: // set/clear bit test
                // set and clear every bit
                for (int i = 1; i <= 48; i++)
                {
                    dio_set_bit(DEVICE, i);

                    if (mio_error_code != MIO_SUCCESS)
                    {
                        TEST_FAIL;
                        continue;
                    }

                    bitValue = dio_read_bit(DEVICE, i);

                    if (mio_error_code != MIO_SUCCESS || bitValue != 1)
                    {
                        TEST_FAIL;
                        continue;
                    }

                    dio_clr_bit(DEVICE, i);

                    if (mio_error_code != MIO_SUCCESS)
                    {
                        TEST_FAIL;
                        continue;
                    }

                    bitValue = dio_read_bit(DEVICE, i);
    
                    if (mio_error_code != MIO_SUCCESS || bitValue != 0)
                    {
                        TEST_FAIL;
                        continue;
                    }
                }

                // check error conditions
                dio_set_bit(4, 1);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dio_set_bit(DEVICE, 0);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dio_set_bit(DEVICE, 49);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dio_clr_bit(4, 1);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dio_clr_bit(DEVICE, 0);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dio_clr_bit(DEVICE, 49);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;
                
                break;

            case 4: // read/write byte test
                // initilaize all ports to known state
                for (int i = 0; i < 6; i++) 
                {
                    dio_write_byte(DEVICE, i, portInit[i]);

                    if (mio_error_code != MIO_SUCCESS)
                        TEST_FAIL;
                }
                
                // verify writes
                for (int i = 0; i < 6; i++)
                {
                    if (dio_read_byte(DEVICE, i) != portInit[i] || mio_error_code != MIO_SUCCESS)
                        TEST_FAIL;
                }

                // check error conditions
                dio_read_byte(4, 0);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dio_read_byte(DEVICE, 6);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                // check error conditions
                dio_write_byte(4, 0, 0);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dio_write_byte(DEVICE, 6, 0);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                break;
                
            case 5:  // interrupt test
                // reset board to known state
                dio_reset_device(DEVICE);

                if (mio_error_code)
                    TEST_FAIL;
                
                // rising edge interrupts for even bits, falling edge interrupts for odd bits
                for (int i = 1; i <= 24; i++)
                {
                    if (i % 2) // odd bit
                    {
                        dio_enab_bit_int(DEVICE, i, FALLING);

                        if (mio_error_code != MIO_SUCCESS)
                            TEST_FAIL;
                    }
                    else // even bit
                    {
                        dio_enab_bit_int(DEVICE, i, RISING);

                        if (mio_error_code != MIO_SUCCESS)
                            TEST_FAIL;
                    }
                }

                // rising edge on each bit
                for (int i = 1; i <= 24; i++)
                {
                    dio_set_bit(DEVICE, i);

                    if (mio_error_code != MIO_SUCCESS)
                        TEST_FAIL;

                    usleep(200);
                }

                // retrieve interrupts received
                irqArray = 0;

                while (irqValue = dio_get_int(DEVICE))
                    irqArray |= 1 << (irqValue - 1);

                if (irqArray != 0x00555555)
                    TEST_FAIL;
                    
                // falling edge on each bit
                for (int i = 1; i <= 24; i++)
                {
                    dio_clr_bit(DEVICE, i);

                    if (mio_error_code != MIO_SUCCESS)
                        TEST_FAIL;

                    usleep(200);
                }

                // retrieve interrupts received
                irqArray = 0;

                while (irqValue = dio_get_int(DEVICE))
                    irqArray |= 1 << (irqValue - 1);

                if (irqArray != 0x00AAAAAA)
                    TEST_FAIL;

                // disable interrupts for port 1
                for (int i = 9; i <= 16; i++)
                {
                    dio_disab_bit_int(DEVICE, i);

                    if (mio_error_code != MIO_SUCCESS)
                        TEST_FAIL;
                }

                // rising edge on each bit
                for (int i = 1; i <= 24; i++)
                {
                    dio_set_bit(DEVICE, i);

                    if (mio_error_code != MIO_SUCCESS)
                        TEST_FAIL;

                    usleep(200);
                }

                // retrieve interrupts received
                irqArray = 0;

                while (irqValue = dio_get_int(DEVICE))
                    irqArray |= 1 << (irqValue - 1);

                if (irqArray != 0x00550055)
                    TEST_FAIL;
                    
                // falling edge on each bit
                for (int i = 1; i <= 24; i++)
                {
                    dio_clr_bit(DEVICE, i);

                    if (mio_error_code != MIO_SUCCESS)
                        TEST_FAIL;

                    usleep(200);
                }

                // retrieve interrupts received
                irqArray = 0;

                while (irqValue = dio_get_int(DEVICE))
                    irqArray |= 1 << (irqValue - 1);

                if (irqArray != 0x00AA00AA)
                    TEST_FAIL;

                // check error conditions
                dio_enab_bit_int(4, 1, FALLING);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dio_enab_bit_int(DEVICE, 0, FALLING);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dio_enab_bit_int(DEVICE, 25, FALLING);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dio_enab_bit_int(DEVICE, 1, 2);

                if (mio_error_code != MIO_BAD_POLARITY)
                    TEST_FAIL;

                dio_disab_bit_int(4, 1);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dio_disab_bit_int(DEVICE, 0);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dio_disab_bit_int(DEVICE, 25);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dio_get_int(4);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                break;

            case 6:  // wait for interrupt test
                // reset board to known state
                dio_reset_device(DEVICE);

                bit = (rand() % 24) + 1;  // integer from 1 to 24

                // enable single bit for rising edge interrupts
                dio_enab_bit_int(DEVICE, bit, RISING);

                if (mio_error_code != MIO_SUCCESS)
                    TEST_FAIL;

                // start thread function and wait 2 seconds before generating interrupt
                args.test = t;
                args.bit = bit;
                
                if (pthread_create(&a_thread, NULL, thread_function, (void *)&args))
                    TEST_FAIL;
                
                usleep(2000000);

                // rising edge on bit to generate interrupt
                dio_set_bit(DEVICE, bit);

                if (mio_error_code != MIO_SUCCESS)
                    TEST_FAIL;

                usleep(200);

                dio_clr_bit(DEVICE, bit);

                if (mio_error_code != MIO_SUCCESS)
                    TEST_FAIL;

                usleep(200);

                // disable irq
                dio_disab_bit_int(DEVICE, bit);

                if (mio_error_code != MIO_SUCCESS)
                    TEST_FAIL;

                // clean up
                pthread_cancel(a_thread);

                // check error conditions
                dio_wait_int(4);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                break;
                
            case 7:  // polling test
                // reset board to known state
                dio_reset_device(DEVICE);

                bit = (rand() % 24) + 1;  // integer from 1 to 24

                // disable irq for dio bits to enable polling
                temp = mio_read_reg(DEVICE, ADC1_RSRC_ENBL);
                mio_write_reg(DEVICE, ADC1_RSRC_ENBL, temp | 0x10);
                irq = mio_read_reg(DEVICE, DIO_RESOURCE);
                mio_write_reg(DEVICE, DIO_RESOURCE, 0);

                // enable single bit for rising edge interrupts
                dio_enab_bit_int(DEVICE, bit, RISING);

                if (mio_error_code != MIO_SUCCESS)
                    TEST_FAIL;
                
                // rising edge on bit to generate interrupt
                dio_set_bit(DEVICE, bit);

                if (mio_error_code != MIO_SUCCESS)
                    TEST_FAIL;

                usleep(200);

                dio_clr_bit(DEVICE, bit);

                if (mio_error_code != MIO_SUCCESS)
                    TEST_FAIL;

                usleep(200);

                // retrieve interrupt 2x, interrupt should remain until cleared
                for (int j = 0; j < 2; j++)
                {
                    for (int i = 0; i < 3; i++)
                    {
                        temp = mio_read_reg(DEVICE, DIO_INT_ID0 + i);
 
                        if (!temp)
                            continue;

                        int x = 0;
                        while (((temp >> x) & 1) == 0) x++; 
                        irqValue = (i * 8) + x + 1;
                
                        if (irqValue != bit)
                            TEST_FAIL;
                    }
                }

                // now clear interrupt and verify it is gone
                dio_clr_int(DEVICE, bit);
                
                if (mio_error_code != MIO_SUCCESS)
                    TEST_FAIL;

                for (int i = 0; i < 3; i++)
                {
                    temp = mio_read_reg(DEVICE, DIO_INT_ID0 + i);

                    if (temp)
                        TEST_FAIL;
                }
                
                // disable irq
                dio_disab_bit_int(DEVICE, bit);

                if (mio_error_code != MIO_SUCCESS)
                    TEST_FAIL;

                // restore irq for dio bits
                temp = mio_read_reg(DEVICE, ADC1_RSRC_ENBL);
                mio_write_reg(DEVICE, ADC1_RSRC_ENBL, temp | 0x10);
                mio_write_reg(DEVICE, DIO_RESOURCE, irq);
                mio_write_reg(DEVICE, ADC1_RSRC_ENBL, temp);

                // check error conditions
                dio_clr_int(4, 1);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dio_clr_int(DEVICE, 0);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dio_clr_int(DEVICE, 25);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                break;
                
            default:
                break;
        }        

        if (test[t - 1].pass_fail == FAIL) 
        {
            PRINT_FAIL;
            global_pass_fail = FAIL;
        }            
        else 
            PRINT_PASS;
    }
        
    printf("\nTest %s ... %s!\n\n", app_name, global_pass_fail ? "Failed" : "Passed");
    
    return 0;
}

void *thread_function(void *arguments)
{
    int irq;
    struct arg_struct *args = (struct arg_struct *)arguments;

    // wait for interrupt here ...
    irq = dio_wait_int(DEVICE);

    if (mio_error_code != MIO_SUCCESS || irq != args->bit)
        test[args->test - 1].pass_fail = FAIL;
}
