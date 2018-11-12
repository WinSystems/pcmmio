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

#include "mio_io.h" // Our IOCTL definitions and all function prototypes    
#include <stdio.h>
#include <stdlib.h>

#define DEVICE 0
#define MAJOR_VER 1
#define MINOR_VER 0

void displayPorts(void);
void displayIrq(void);
void writeAllPorts(unsigned char Value[6]);

int main(int argc, char *argv[])
{
    bool pass_fail = true;
    char ch;
    unsigned int bitValue;
    unsigned int bitTest[] = { 0, 5, 10, 15, 20, 25, 30, 35, 40, 45 };
    unsigned int readValue[6];
    unsigned int readAllValues[6];
    unsigned int irqArray[3];
    unsigned char portInit[] = { 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6 };
    unsigned char allZero[] = { 0, 0, 0, 0, 0, 0 };
    unsigned char allFF[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    int dllReturn = SUCCESS;
    
    printf("PCM-MIO Application : dioTest\n");
    printf("Version %d.%d\n\n", MAJOR_VER, MINOR_VER);

    if (argc > 1)
    {
        printf("Usage error:\n");
        printf("  dioTest\n");
        exit(1);
    }

    //*****************************************************************
    // reset device to known state
    printf("\nTest 1: dio_reset_device\n");

    dio_reset_device(DEVICE);

    if (mio_error_code)
    {
        printf("\n%s\n", mio_error_string);
        exit(1);
    }
    else
        printf("Reset device ... PASS");

    //*****************************************************************
    // manipulate bits
    printf("\nTest 2: dio_read_bit + dio_write_bit\n");
        
    // read and write bits
    for (int i = 0; i < sizeof(bitTest) / sizeof(unsigned int); i++)
    {
        printf("Test bit %2d ... ", bitTest[i]);

        bitValue = dio_read_bit(DEVICE, bitTest[i]);

        if (mio_error_code != MIO_SUCCESS || bitValue != 0)
        {
            printf("FAIL\n");
            continue;
        }

        dio_write_bit(DEVICE, bitTest[i], 1);
    
        if (mio_error_code != MIO_SUCCESS)
        {
            printf("FAIL\n");
            continue;
        }

        bitValue = dio_read_bit(DEVICE, bitTest[i]);

        if (mio_error_code != MIO_SUCCESS || bitValue != 1)
        {
            printf("FAIL\n");
            continue;
        }

        dio_write_bit(DEVICE, bitTest[i], 0);

        if (mio_error_code != MIO_SUCCESS)
        {
            printf("FAIL\n");
            continue;
        }

        bitValue = dio_read_bit(DEVICE, bitTest[i]);

        if (mio_error_code != MIO_SUCCESS || bitValue != 0)
        {
            printf("FAIL\n");
            continue;
        }

        printf("PASS\n");
    }

    // check error conditions
    printf("Attempted to write illegal bit ... ");

    dio_write_bit(DEVICE, 0, 0);

    if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to write illegal bit ... ");

    dio_write_bit(DEVICE, 49, 1);

    if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to write illegal value ... ");

    dio_write_bit(DEVICE, 1, 2);

    if (mio_error_code != MIO_BAD_VALUE)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to read illegal bit ... ");

    dio_read_bit(DEVICE, 0);

    if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to read illegal bit ... ");

    dio_read_bit(DEVICE, 49);

    if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    //*****************************************************************
    // manipulate bits
    printf("\nTest 3: dio_set_bit + dio_clr_bit\n");
    printf("Toggle bit test ... ");

    // set and clear every bit
    for (int i = 1; i <= 48; i++)
    {
        dio_set_bit(DEVICE, i);

        if (mio_error_code != MIO_SUCCESS)
        {
            printf("FAIL\n");
            continue;
        }

        bitValue = dio_read_bit(DEVICE, bitTest[i]);

        if (mio_error_code != MIO_SUCCESS || bitValue != 1)
        {
            printf("FAIL\n");
            continue;
        }

        dio_clr_bit(DEVICE, i);

        if (mio_error_code != MIO_SUCCESS)
        {
            printf("FAIL\n");
            continue;
        }

        bitValue = dio_read_bit(DEVICE, bitTest[i]);

        if (mio_error_code != MIO_SUCCESS || bitValue != 0)
        {
            printf("FAIL\n");
            continue;
        }
    }

    printf("PASS\n");

    // check error conditions
    printf("Attempted to set illegal bit ... ");

    dllReturn = dio_set_bit(DEVICE, 0);

    if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to set illegal bit ... ");

    dllReturn = dio_set_bit(DEVICE, 49);

    if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to clear illegal bit ... ");

    dllReturn = dio_clr_bit(DEVICE, 0);

    if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to clear illegal bit ... ");

    dllReturn = dio_clr_bit(DEVICE, 49);

    if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
        printf("FAIL\n");
    else
        printf("PASS\n");

#if 0
    //*****************************************************************
    // port tests
    printf("\nTest 4: dio_read_byte + dio_write_byte\n");

    writeAllPorts(portInit);
        
    for (int i = 0; i < 6; i++)
    {
        printf("Read port %d test ... ", i);

        dllReturn = DioReadPort(DEVICE, i, &readValue[i]);

        if (dllReturn)
        {
            printf("Error reading port %d\n", i);
            exit(dllReturn);
        }
        else
        {
            if (readValue[i] != portInit[i])
                printf("FAIL\n");
            else
                printf("PASS\n");
        }
    }

    // check error conditions
    printf("Attempted to read illegal port ... ");

    dllReturn = DioReadPort(DEVICE, 6, &readValue[0]);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to read port with null pointer ... ");

    dllReturn = DioReadPort(DEVICE, 3, NULL);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    //*****************************************************************
    // interrupt test
    printf("\nTest 5: dio_enab_bit_int + dio_disab_bit_int + dio_get_int\n");
    printf("Connect the two DIO ports on the PCM-MIO with the ribbon cable ... ");
    printf("hit any key to continue\n");
    ch = _getch();

    // ports 0 + 1 + 2 inputs, ports 3 + 4 + 5 outputs
    dllReturn = DioSetIoMask(DEVICE, maskIrqTest);

    if (dllReturn)
        printf("Error setting I/O mask\n");

    // clear ports 3 + 4 + 5
    for (int i = 3; i < 6; i++)
    {
        dllReturn = DioWritePort(DEVICE, i, 0);

        if (dllReturn)
            printf("Error writing port %d\n", i);
    }

    // rising edge interrupts for even bits, falling edge interrupts for odd bits
    for (int i = 0; i < 24; i++)
    {
        if (i % 2) // odd bit
        {
            dllReturn = DioEnableInterrupt(DEVICE, i, FALLING_EDGE);

            if (dllReturn)
                printf("Error enabling interrupts for bit %d\n", i);
        }
        else // even bit
        {
            dllReturn = DioEnableInterrupt(DEVICE, i, RISING_EDGE);

            if (dllReturn)
                printf("Error enabling interrupts for bit %d\n", i);
        }
    }

    printf("Rising edge interrupt test ... ");

    // reset pass/fail
    pass_fail = true;

    // set each output bit
    for (int i = 24; i < 48; i++)
    {
        dllReturn = DioSetBit(DEVICE, i);

        if (dllReturn)
        {
            printf("Error setting bit %d\n", i);
            break;
        }
    }

    dllReturn = DioGetInterrupt(DEVICE, irqArray);

    if (dllReturn)
        printf("Error getting interrupts\n");
    else
    {
        for (int i = 0; i < 3; i++)
        {
            if (irqArray[i] != 0x55)
            {
                printf("FAIL\n");
                pass_fail = false;
                break;
            }
        }
    }

    if (pass_fail == true)
        printf("PASS\n");

    printf("Falling edge interrupt test ... ");

    // reset pass/fail
    pass_fail = true;

    // clear each output bit
    for (int i = 24; i < 48; i++)
    {
        dllReturn = DioClearBit(DEVICE, i);

        if (dllReturn)
        {
            printf("Error clearing bit %d\n", i);
            break;
        }
    }

    dllReturn = DioGetInterrupt(DEVICE, irqArray);

    if (dllReturn)
        printf("Error getting interrupts\n");
    else
    {
        for (int i = 0; i < 3; i++)
        {
            if (irqArray[i] != 0xAA)
            {
                printf("FAIL\n");
                pass_fail = false;
                break;
            }
        }
    }

    if (pass_fail == true)
        printf("PASS\n");

    printf("Disable interrupt test ... ");

    // disable interrupts for every 3 bits
    for (int i = 0; i < 24; i += 4)
    {
        dllReturn = DioDisableInterrupt(DEVICE, i);

        if (dllReturn)
            printf("Error disabling interrupts for bit %d\n", i);
    }

    // reset pass/fail
    pass_fail = true;

    // set each output bit
    for (int i = 24; i < 48; i++)
    {
        dllReturn = DioSetBit(DEVICE, i);

        if (dllReturn)
        {
            printf("Error setting bit %d\n", i);
            break;
        }
    }

    dllReturn = DioGetInterrupt(DEVICE, irqArray);

    if (dllReturn)
        printf("Error getting interrupts\n");
    else
    {
        for (int i = 0; i < 3; i++)
        {
            if (irqArray[i] != 0x44)
            {
                printf("FAIL\n");
                pass_fail = false;
                break;
            }
        }
    }

    if (pass_fail == true)
        printf("PASS\n");

    // check error conditions
    printf("Attempted to enable interrupts for illegal bit ... ");

    dllReturn = DioEnableInterrupt(DEVICE, 24, RISING_EDGE);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to enable interrupts for illegal edge ... ");

    dllReturn = DioEnableInterrupt(DEVICE, 2, RISING_EDGE + 1);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to get interrupts with null pointer ... ");

    dllReturn = DioGetInterrupt(DEVICE, NULL);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to disable interrupts for illegal bit ... ");

    dllReturn = DioDisableInterrupt(DEVICE, 24);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");
#endif

	return dllReturn;
}

void displayPorts(void)
{
    unsigned int readValueArray[6];
    int dllReturn = SUCCESS;

    printf("PCM-UIO48 Ports Display\n");
    printf("Offset    Value\n");

    dllReturn = DioReadAllPorts(DEVICE, readValueArray);

    if (dllReturn)
    {
        printf("Error reading ports\n");
        exit(dllReturn);
    }

    printf(" 00h       %02Xh\n", readValueArray[0]);
    printf(" 01h       %02Xh\n", readValueArray[1]);
    printf(" 02h       %02Xh\n", readValueArray[2]);
    printf(" 03h       %02Xh\n", readValueArray[3]);
    printf(" 04h       %02Xh\n", readValueArray[4]);
    printf(" 05h       %02Xh\n", readValueArray[5]);
    printf("\n");
}

void displayIrq(void)
{
    unsigned int irqArray[3];
    int dllReturn = SUCCESS;

    printf("PCM-UIO48 Interrupt Display\n");
    printf("Offset    Value\n");

    dllReturn = DioGetInterrupt(DEVICE, irqArray);

    if (dllReturn)
    {
        printf("Error reading ports\n");
        exit(dllReturn);
    }

    printf(" 00h       %02Xh\n", irqArray[0]);
    printf(" 01h       %02Xh\n", irqArray[1]);
    printf(" 02h       %02Xh\n", irqArray[2]);
    printf("\n");
}

void writeAllPorts(BYTE Value[6])
{
    int dllReturn = SUCCESS;

    for (int i = 0; i < 6; i++) 
    {
        dllReturn = DioWritePort(DEVICE, i, Value[i]);

        if (dllReturn)
        {
            if (dllReturn != ACCESS_ERROR)
            {
                printf("Error writing port %d\n", i);
                exit(dllReturn);
            }
        }
    }
}
