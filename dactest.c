//****************************************************************************
//	
//	Copyright 2018 by WinSystems Inc.
//
//****************************************************************************
//
//	Name	 : dacTest.c
//
//	Project	 : PCM-MIO Test Code - Jenkins Suite
//
//	Author	 : Paul DeMetrotion
//
//****************************************************************************
//
//	  Date		  Rev	                Description
//	--------    -------	   ---------------------------------------------
//	12/03/18	  1.0		Original Release
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
    {1, "dac_set_voltage", PASS},
    {2, "dac_set_span + dac_set_output", PASS},
    {3, "dac_buffered_output", PASS},
    {4, "dac_write_command + dac_write_data + dac_wait_ready", PASS},
    {5, "dac_enable_interrupt + dac_disable_interrupt + dac_wait_int + dac_read_status", PASS}
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
    char *app_name = "dactest";
    unsigned char dac_status;
    unsigned char cmd_buff[5] = { 1, 3, 4, 6, 0xff };
    unsigned short data_buff[4] = { 0x2000, 0x6000, 0xA000, 0xE000 };
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

    // run all tests
    for (int t = 1; t <= sizeof(test) / sizeof(TEST); t++)
    {
        printf("\nTest %d: %s ... ", test[t - 1].number, test[t - 1].name);
        
        switch(t) {
            case 1:
                // set all ouputs
                for (int v = 0; v < 8; v++) {
                    voltBuf[v] = random_float(-10.0, 10.0);
                    dac_set_voltage(DEVICE, v, voltBuf[v]);

                    if (mio_error_code)
                        TEST_FAIL;
                }

                // verify voltages
                for (int v = 0; v < 8; v++) {
                    adcVoltage = adc_auto_get_channel_voltage(DEVICE, chanMap[v]);
 
                    if ((mio_error_code) ||
                        (adcVoltage < voltBuf[v] - ERROR_MARGIN || 
                         adcVoltage > voltBuf[v] + ERROR_MARGIN))
                        TEST_FAIL;
                }
                
                // check error conditions
                dac_set_voltage(4, 1, voltBuf[0]);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dac_set_voltage(DEVICE, 16, voltBuf[0]);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dac_set_voltage(DEVICE, -1, voltBuf[0]);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dac_set_voltage(DEVICE, 1, -10.001);

                if (mio_error_code != MIO_ILLEGAL_VOLTAGE)
                    TEST_FAIL;

                dac_set_voltage(DEVICE, 1, 10.001);

                if (mio_error_code != MIO_ILLEGAL_VOLTAGE)
                    TEST_FAIL;

                break;
                
            case 2:
                // set output to 4000h
                dac_set_output(DEVICE, 2, 0x4000);

                if (mio_error_code)
                    TEST_FAIL;

                dac_set_span(DEVICE, 2, DAC_SPAN_UNI5);

                if (mio_error_code)
                    TEST_FAIL;

                adcVoltage = adc_auto_get_channel_voltage(DEVICE, 12);

                if ((mio_error_code) ||
                    (adcVoltage < 1.250 - ERROR_MARGIN || 
                     adcVoltage > 1.250 + ERROR_MARGIN))
                    TEST_FAIL;

                dac_set_span(DEVICE, 2, DAC_SPAN_UNI10);

                if (mio_error_code)
                    TEST_FAIL;

                adcVoltage = adc_auto_get_channel_voltage(DEVICE, 12);

                if ((mio_error_code) ||
                    (adcVoltage < 2.500 - ERROR_MARGIN || 
                     adcVoltage > 2.500 + ERROR_MARGIN))
                    TEST_FAIL;

                dac_set_span(DEVICE, 2, DAC_SPAN_BI7);

                if (mio_error_code)
                    TEST_FAIL;

                adcVoltage = adc_auto_get_channel_voltage(DEVICE, 12);

                if ((mio_error_code) ||
                    (adcVoltage < 0 - ERROR_MARGIN || 
                     adcVoltage > 0 + ERROR_MARGIN))
                    TEST_FAIL;

                // check error conditions
                dac_set_output(4, 1, 0x8000);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dac_set_output(DEVICE, 16, 0x8000);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dac_set_output(DEVICE, -1, 0x8000);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dac_set_span(4, 1, DAC_SPAN_UNI5);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dac_set_span(DEVICE, 16, DAC_SPAN_UNI5);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dac_set_span(DEVICE, -1, DAC_SPAN_UNI5);

                if (mio_error_code != MIO_BAD_CHANNEL_NUMBER)
                    TEST_FAIL;

                dac_set_span(DEVICE, 1, DAC_SPAN_UNI5 - 1);

                if (mio_error_code != MIO_BAD_SPAN)
                    TEST_FAIL;

                dac_set_span(DEVICE, 1, DAC_SPAN_BI7 + 1);

                if (mio_error_code != MIO_BAD_SPAN)
                    TEST_FAIL;

                break;
                
            case 3: 
                // set all channels for -10V to +10V
                for (int i = 0; i < 8; i++)
                {
                    dac_set_span(DEVICE, i, DAC_SPAN_BI10);

                    if (mio_error_code)
                        TEST_FAIL;
                }

                // set slected channels
                dac_buffered_output(DEVICE, cmd_buff, data_buff);

                if (mio_error_code)
                    TEST_FAIL;
                
                adcVoltage = adc_auto_get_channel_voltage(DEVICE, 10);

                if ((mio_error_code) ||
                    (adcVoltage < -7.500 - ERROR_MARGIN || 
                     adcVoltage > -7.500 + ERROR_MARGIN))
                    TEST_FAIL;

                adcVoltage = adc_auto_get_channel_voltage(DEVICE, 14);

                if ((mio_error_code) ||
                    (adcVoltage < -2.500 - ERROR_MARGIN || 
                     adcVoltage > -2.500 + ERROR_MARGIN))
                    TEST_FAIL;

                adcVoltage = adc_auto_get_channel_voltage(DEVICE, 1);

                if ((mio_error_code) ||
                    (adcVoltage < 2.500 - ERROR_MARGIN || 
                     adcVoltage > 2.500 + ERROR_MARGIN))
                    TEST_FAIL;

                adcVoltage = adc_auto_get_channel_voltage(DEVICE, 5);

                if ((mio_error_code) ||
                    (adcVoltage < 7.500 - ERROR_MARGIN || 
                     adcVoltage > 7.500 + ERROR_MARGIN))
                    TEST_FAIL;

                // check error conditions
                dac_buffered_output(4, cmd_buff, data_buff);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dac_buffered_output(DEVICE, NULL, data_buff);

                if (mio_error_code != MIO_NULL_POINTER)
                    TEST_FAIL;

                dac_buffered_output(DEVICE, cmd_buff, NULL);

                if (mio_error_code != MIO_NULL_POINTER)
                    TEST_FAIL;

                break;

            case 4: 
                dac_write_data(DEVICE, 1, DAC_SPAN_UNI5);

                if (mio_error_code)
                    TEST_FAIL;

                dac_wait_ready(DEVICE, 4);
                
                if (mio_error_code)
                    TEST_FAIL;

                dac_write_command(DEVICE, 1, DAC_CMD_WR_UPDATE_SPAN << 4);

                if (mio_error_code)
                    TEST_FAIL;

                dac_wait_ready(DEVICE, 4);
                
                if (mio_error_code)
                    TEST_FAIL;

                dac_write_data(DEVICE, 1, 0x6000);

                if (mio_error_code)
                    TEST_FAIL;

                dac_wait_ready(DEVICE, 4);
                
                if (mio_error_code)
                    TEST_FAIL;

                dac_write_command(DEVICE, 1, DAC_CMD_WR_UPDATE_CODE << 4);

                if (mio_error_code)
                    TEST_FAIL;

                dac_wait_ready(DEVICE, 4);
                
                if (mio_error_code)
                    TEST_FAIL;

                adcVoltage = adc_auto_get_channel_voltage(DEVICE, 1);

                if ((mio_error_code) ||
                    (adcVoltage < 1.875 - ERROR_MARGIN || 
                     adcVoltage > 1.875 + ERROR_MARGIN))
                    TEST_FAIL;

                // check error conditions
                dac_write_data(4, 1, DAC_SPAN_UNI5);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dac_write_data(DEVICE, 2, DAC_SPAN_UNI5);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                dac_write_data(DEVICE, -1, DAC_SPAN_UNI5);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                dac_write_command(4, 1, DAC_CMD_WR_UPDATE_SPAN);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dac_write_command(DEVICE, 2, DAC_CMD_WR_UPDATE_SPAN);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                dac_write_command(DEVICE, -1, DAC_CMD_WR_UPDATE_SPAN);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                dac_write_command(DEVICE, 1, (unsigned char)((DAC_CMD_WR_B1_SPAN - 1) << 4));

                if (mio_error_code != MIO_BAD_COMMAND)
                    TEST_FAIL;

                dac_write_command(DEVICE, 1, (unsigned char)((DAC_CMD_NOP + 1) << 4));

                if (mio_error_code != MIO_BAD_COMMAND)
                    TEST_FAIL;

                dac_write_command(DEVICE, 1, (unsigned char)((DAC_CMD_NOP << 4) | 8));

                if (mio_error_code != MIO_BAD_COMMAND)
                    TEST_FAIL;

                break;
                
            case 5: 
                // enable interrupt for DAC0
                dac_enable_interrupt(DEVICE, 0);
                
                if (mio_error_code)
                    TEST_FAIL;

                // verify
                dac_status = dac_read_status(DEVICE, 0);

                if (mio_error_code || !(dac_status & 1))
                    TEST_FAIL;

                // start thread function and wait 2 seconds before starting conversion
                args.test = t;
                
                if (pthread_create(&a_thread, NULL, thread_function, (void *)&args))
                    TEST_FAIL;

                usleep(2000000);

                // set voltage will cause interrupt
                dac_set_voltage(DEVICE, 0, 1.000);

                if (mio_error_code)
                    TEST_FAIL;

                // wait for thread to complete
                while (!flag) ;

                // disable irq
                dac_disable_interrupt(DEVICE, 0);

                if (mio_error_code)
                    TEST_FAIL;

                // verify
                dac_status = dac_read_status(DEVICE, 0);

                if (mio_error_code || (dac_status & 1))
                    TEST_FAIL;

                // clean up
                pthread_cancel(a_thread);

                // check error conditions
                dac_read_status(4, 0);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dac_read_status(DEVICE, -1);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                dac_read_status(DEVICE, 2);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                dac_enable_interrupt(4, 0);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dac_enable_interrupt(DEVICE, -1);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                dac_enable_interrupt(DEVICE, 2);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                dac_disable_interrupt(4, 0);

                if (mio_error_code != MIO_BAD_DEVICE)
                    TEST_FAIL;

                dac_disable_interrupt(DEVICE, -1);

                if (mio_error_code != MIO_BAD_CHIP_NUM)
                    TEST_FAIL;

                dac_disable_interrupt(DEVICE, 2);

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
    dac_wait_int(DEVICE, 0);

    if (mio_error_code != MIO_SUCCESS)
        test[args->test - 1].pass_fail = FAIL;

    flag = 1;
}
