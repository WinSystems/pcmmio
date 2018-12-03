//****************************************************************************
//	
//	Copyright 2018 by WinSystems Inc.
//
//****************************************************************************
//
//	Name	 : adcTest.c
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
//
// Following connections must be made for the test to pass
//
// DAC channel <> ADC channel
//           0 <> 8
//           1 <> 10
//           2 <> 12
//           3 <> 14
//           4 <> 1
//           5 <> 3
//           6 <> 5
//           7 <> 7
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
#define ERROR_MARGIN  0.1

// list of tests
TEST test[] = { 
    {1, "adc_set_channel_mode + adc_get_channel_voltage", PASS},
    {2, "adc_convert_all_channels + adc_convert_to_volts", PASS},
    {3, "adc_auto_get_channel_voltage", PASS},
    {4, "adc_convert_single_repeated", PASS},
    {5, "adc_buffered_channel_conversions", PASS},
    {6, "adc_start_conversion + adc_write_command + adc_wait_ready + adc_read_conversion_data", PASS},
    {7, "adc_enable_interrupt + adc_disable_interrupt + adc_wait_int", PASS}
};
   
void *thread_function(void *arguments);
float random_float(float min, float max);

struct arg_struct {
    int test;
};

// adc channel map
static int chanMap[] = { 8, 10, 12, 14, 1, 3, 5, 7 };
static int flag = 0;

int main(int argc, char *argv[])
{
    unsigned char chanBuf[] = { 14, 12, 10, 8, 7, 5, 3, 1, 1, 5, 3, 8, 7, 12, 10, 10, 0xff };
    unsigned char map[] = { 3, 2, 1, 0, 7, 6, 5, 4, 4, 6, 5, 0, 7, 2, 1, 1 };
    char *app_name = "adctest";
    unsigned short adcVoltageArray[16];
    float adcVoltage;
    float voltBuf[8] = { 0 };
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

    // use the DAC to create input voltages
    for (int v = 0; v < 8; v++) {
        voltBuf[v] = random_float(-10.0, 10.0);
        dac_set_voltage(DEVICE, v, voltBuf[v]);
        printf("voltBuf[%d] = %.3f\n", v, voltBuf[v]);
    }
    
    // run all tests
    for (int t = 1; t <= sizeof(test) / sizeof(TEST); t++)
    {
        printf("\nTest %d: %s ... ", test[t - 1].number, test[t - 1].name);
        
        switch(t) {
            case 1:  // configure and get voltage test  
                // configure adc channels
                for (int ch = 0; ch < 8; ch++)
                {
                    adc_set_channel_mode(DEVICE, chanMap[ch], ADC_SINGLE_ENDED, ADC_BIPOLAR, ADC_TOP_10V);

                    if (mio_error_code)
                        TEST_FAIL;

                    adcVoltage = adc_get_channel_voltage(DEVICE, chanMap[ch]);

                    if ((mio_error_code) ||
                        (adcVoltage < voltBuf[ch] - ERROR_MARGIN || adcVoltage > voltBuf[ch] + ERROR_MARGIN))
                        TEST_FAIL;
                }
    
                // check error conditions
                adc_set_channel_mode(4, 1, ADC_SINGLE_ENDED, ADC_BIPOLAR, ADC_TOP_10V);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_set_channel_mode(DEVICE, 16, ADC_SINGLE_ENDED, ADC_BIPOLAR, ADC_TOP_10V);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                adc_set_channel_mode(DEVICE, -1, ADC_SINGLE_ENDED, ADC_BIPOLAR, ADC_TOP_10V);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                adc_set_channel_mode(DEVICE, 1, 0x40, ADC_BIPOLAR, ADC_TOP_10V);

                if (mio_error_code != MIO_BAD_MODE_NUMBER)
                    TEST_FAIL;

                adc_set_channel_mode(DEVICE, 1, ADC_SINGLE_ENDED, 0x04, ADC_TOP_10V);

                if (mio_error_code != MIO_BAD_MODE_NUMBER)
                    TEST_FAIL;

                adc_set_channel_mode(DEVICE, 1, ADC_SINGLE_ENDED, ADC_BIPOLAR, 0x02);

                if (mio_error_code != MIO_BAD_RANGE)
                    TEST_FAIL;

                adc_get_channel_voltage(4, 1);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_get_channel_voltage(DEVICE, 16);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                adc_get_channel_voltage(DEVICE, -1);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                break;
                
            case 2: 
                // read all channels
                adc_convert_all_channels(DEVICE, adcVoltageArray);

                if (mio_error_code)
                    TEST_FAIL;
                else
                {
                    for (int ch = 0; ch < 8; ch++)
                    {
                        adcVoltage = adc_convert_to_volts(DEVICE, chanMap[ch], adcVoltageArray[chanMap[ch]]);

                        if (adcVoltage < voltBuf[ch] - ERROR_MARGIN || adcVoltage > voltBuf[ch] + ERROR_MARGIN)
                            TEST_FAIL;
                    }
                }

                // check error conditions
                adc_convert_all_channels(4, adcVoltageArray);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_convert_all_channels(DEVICE, NULL);

                if (mio_error_code != MIO_NULL_POINTER)
                    TEST_FAIL;

                adc_convert_to_volts(4, 1, adcVoltageArray[0]);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_convert_to_volts(DEVICE, 16, adcVoltageArray[0]);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                adc_convert_to_volts(DEVICE, -1, adcVoltageArray[0]);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                break;
                
            case 3: 
                for (int ch = 0; ch < 8; ch++)
                {
                    adcVoltage = adc_auto_get_channel_voltage(DEVICE, chanMap[ch]);

                    if (adcVoltage < (voltBuf[ch] - ERROR_MARGIN) || adcVoltage > (voltBuf[ch] + ERROR_MARGIN))
                        TEST_FAIL;
                }

                // check error conditions
                adc_auto_get_channel_voltage(4, 0);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_auto_get_channel_voltage(DEVICE, 16);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                adc_auto_get_channel_voltage(DEVICE, -1);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                break;

            case 4: 
                // sixteen conversions for channel five
                adc_convert_single_repeated(DEVICE, 5, 16, adcVoltageArray);

                if (mio_error_code)
                    TEST_FAIL;
                else
                {
                    for (int v = 0; v < 16; v++)
                    {
                        adcVoltage = adc_convert_to_volts(DEVICE, 5, adcVoltageArray[v]);

                        if (adcVoltage < (voltBuf[6] - ERROR_MARGIN) || adcVoltage > (voltBuf[6] + ERROR_MARGIN))
                            TEST_FAIL;
                    }
                }

                // check error conditions
                adc_convert_single_repeated(4, 1, 3, adcVoltageArray);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_convert_single_repeated(DEVICE, 16, 3, adcVoltageArray);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                adc_convert_single_repeated(DEVICE, -1, 3, adcVoltageArray);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                adc_convert_single_repeated(DEVICE, 1, 3, NULL);

                if (mio_error_code != MIO_NULL_POINTER)
                    TEST_FAIL;

                break;
                
            case 5: 
                // convert preset sequence of channels across both ADC devices
                adc_buffered_channel_conversions(DEVICE, chanBuf, adcVoltageArray);

                if (mio_error_code)
                    TEST_FAIL;
                else
                {
                    for (int v = 0; v < 16; v++)
                    {
                        adcVoltage = adc_convert_to_volts(DEVICE, chanBuf[v], adcVoltageArray[v]);

                        if (adcVoltage < (voltBuf[map[v]] - ERROR_MARGIN) || adcVoltage > (voltBuf[map[v]] + ERROR_MARGIN))
                            TEST_FAIL;
                    }
                }

                // check error conditions
                adc_buffered_channel_conversions(4, chanBuf, adcVoltageArray);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_buffered_channel_conversions(DEVICE, NULL, adcVoltageArray);

                if (mio_error_code != MIO_NULL_POINTER)
                    TEST_FAIL;

                adc_buffered_channel_conversions(DEVICE, chanBuf, NULL);

                if (mio_error_code != MIO_NULL_POINTER)
                    TEST_FAIL;

                break;

            case 6: 
                // perform conversion on channel 8 using discrete commands
                // adc1, ch 0, single ended, unipolar, -10V to +10V
                adc_write_command(DEVICE, 1, ADC_CH0_SELECT | ADC_SINGLE_ENDED | ADC_BIPOLAR | ADC_TOP_10V);

                if (mio_error_code)
                    TEST_FAIL;

                // dummy conversion
                adc_start_conversion(DEVICE, 8);

                if (mio_error_code)
                    TEST_FAIL;

                adc_wait_ready(DEVICE, 8);

                if (mio_error_code)
                    TEST_FAIL;

                // real conversion
                adc_start_conversion(DEVICE, 8);

                if (mio_error_code)
                    TEST_FAIL;

                adc_wait_ready(DEVICE, 8);

                if (mio_error_code)
                    TEST_FAIL;

                adcVoltageArray[0] = adc_read_conversion_data(DEVICE, 8);

                if (mio_error_code)
                    TEST_FAIL;

                adcVoltage = adc_convert_to_volts(DEVICE, 8, adcVoltageArray[0]);

                if (mio_error_code)
                    TEST_FAIL;
                else if (adcVoltage < (voltBuf[0] - ERROR_MARGIN) || adcVoltage > (voltBuf[0] + ERROR_MARGIN))
                    TEST_FAIL;

                // check error conditions
                adc_start_conversion(4, 8);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_start_conversion(DEVICE, 16);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                adc_start_conversion(DEVICE, -1);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                adc_wait_ready(4, 8);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_wait_ready(DEVICE, 16);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                adc_wait_ready(DEVICE, -1);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                adc_write_command(4, 0, 0x84);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_write_command(DEVICE, 2, 0x84);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                adc_write_command(DEVICE, -1, 0x84);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                adc_read_conversion_data(4, 8);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_read_conversion_data(DEVICE, 16);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                adc_read_conversion_data(DEVICE, -1);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                break;
                
            case 7:
                // test interrupts on adc 0 channel 2
                adc_enable_interrupt(DEVICE, 0);
                
                if (mio_error_code)
                    TEST_FAIL;

                // start thread function and wait 2 seconds before starting conversion
                args.test = t;
                
                if (pthread_create(&a_thread, NULL, thread_function, (void *)&args))
                    TEST_FAIL;
                
                usleep(2000000);

                // rising edge on bit to generate interrupt
                adc_start_conversion(DEVICE, 2);

                if (mio_error_code)
                    TEST_FAIL;

                while (!flag) ;

                // disable irq
                adc_disable_interrupt(DEVICE, 0);

                if (mio_error_code != MIO_SUCCESS)
                    TEST_FAIL;

                // clean up
                pthread_cancel(a_thread);

                // check error conditions
                adc_enable_interrupt(4, 0);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_enable_interrupt(DEVICE, -1);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                adc_enable_interrupt(DEVICE, 2);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                adc_disable_interrupt(4, 0);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_disable_interrupt(DEVICE, -1);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                adc_disable_interrupt(DEVICE, 2);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                adc_wait_int(4, 2);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                adc_wait_int(DEVICE, 2);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                adc_wait_int(DEVICE, -1);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
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

float random_float(float min, float max)
{
    if (max == min) 
        return min;
    else if (min < max) 
        return (max - min) * ((float)rand() / RAND_MAX) + min;

    // return 0 if min > max
    return 0;
}

void *thread_function(void *arguments)
{
    struct arg_struct *args = (struct arg_struct *)arguments;

    // wait for interrupt here ...
    adc_wait_int(DEVICE, 0);

    if (mio_error_code != MIO_SUCCESS)
        test[args->test - 1].pass_fail = FAIL;

    flag = 1;
}
