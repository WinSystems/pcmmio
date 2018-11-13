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
#include "mio_io.h" // Our IOCTL definitions and all function prototypes    
#include "jenkins.h"

#define DEVICE 0
#define MAJOR_VER 1
#define MINOR_VER 0

int main(int argc, char *argv[])
{
    int t;
    char ch;
    unsigned int bitValue;
    unsigned int irqValue;
    unsigned long irqArray = 0;
    unsigned char portInit[] = { 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6 };

    // list of tests
    TEST test[] = { 
        {1, "dio_reset_device", PASS},
        {2, "dio_read_bit + dio_write_bit", PASS},
        {3, "dio_set_bit + dio_clr_bit", PASS},
        {4, "dio_read_byte + dio_write_byte", PASS},
        {5, "dio_enab_bit_int + dio_disab_bit_int + dio_get_int", PASS},
        {6, "dio_wait_int", PASS}
    };
    
    printf("PCM-MIO Application : dioTest\n");
    printf("Version %d.%d\n\n", MAJOR_VER, MINOR_VER);

    if (argc > 1)
    {
        printf("Usage error:\n");
        printf("  dioTest\n");
        exit(1);
    }

    // run all tests
    for (t = 1; t <= sizeof(test) / sizeof(TEST); t++)
    {
        printf("Test %d: %s ... ", test[t - 1].number, test[t - 1].name);
        
        switch(t) {
            case 1:
                // reset device to known state
                dio_reset_device(DEVICE);

                if (mio_error_code)
                    TEST_FAIL;

                // check error conditions
                dio_reset_device(4);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                break;
                
            case 2:
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
                
            case 3:
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

            case 4:
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
                
            case 5:
                // interrupt test
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

                // intialize array
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

                // intialize array
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

                // intialize array
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

                // intialize array
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

            case 6:
                // wait for interrupt test
                // reset board to known state
                dio_reset_device(DEVICE);

                break;
                
            default:
                break;
        }        

        if (test[t - 1].pass_fail == FAIL) PRINT_FAIL;
        else PRINT_PASS;
    }
        
    return 0;
}
