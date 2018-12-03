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
    {4, "dac_write_command + dac_write_data + dac_wait_ready + dac_read_status", PASS},
    {5, "dac_enable_interrupt + dac_disable_interrupt + dac_wait_int", PASS}
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

#if 0
    dllReturn = DacWriteData(DEVICE, 2, DAC_SPAN_UNI5);
    dllReturn = DacWriteCommand(DEVICE, 2, DAC_CMD_WR_UPDATE_SPAN);

    dllReturn = DacWriteData(DEVICE, 2, 0x4000);
    dllReturn = DacWriteCommand(DEVICE, 2, DAC_CMD_WR_UPDATE_CODE);
    printf("DAC Channel 2 value set to 4000 ... ");

    dllReturn = AdcSetChannelMode(DEVICE, chanMap[2], ADC_SINGLE_ENDED, ADC_UNIPOLAR, ADC_TOP_5V);
    dllReturn = AdcGetChannelValue(DEVICE, chanMap[2], &adcValue);

    if (dllReturn)
        printf("error reading ADC value.\n");
    else
    {
        printf("value read is %04x ... ", adcValue);

        if (adcValue < (0x4000 - HEX_ERROR_MARGIN) || adcValue >(0x4000 + HEX_ERROR_MARGIN))
            printf("FAIL\n");
        else
            printf("PASS\n");
    }

#endif

                break;
                
            case 5: 
            
#if 0
    set_value(1, DAC_SPAN_BI5, 0x05fc);
    set_value(7, DAC_SPAN_UNI10, 0xcdef);

    printf("DAC Channel 1 span set to 2 ... ");

    dllReturn = DacWriteCommand(DEVICE, 1, DAC_CMD_RD_B1_SPAN);
    dllReturn = DacReadData(DEVICE, 1, &dacValue);

    printf("span read is %d ... ", dacValue);

    if (dacValue != DAC_SPAN_BI5)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("DAC Channel 7 span set to 1 ... ");

    dllReturn = DacWriteCommand(DEVICE, 7, DAC_CMD_RD_B1_SPAN);
    dllReturn = DacReadData(DEVICE, 7, &dacValue);

    printf("span read is %d ... ", dacValue);

    if (dacValue != DAC_SPAN_UNI10)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("DAC Channel 1 code set to 05fc ... ");

    dllReturn = DacWriteCommand(DEVICE, 1, DAC_CMD_RD_B1_CODE);
    dllReturn = DacReadData(DEVICE, 1, &dacValue);

    printf("value read is %04x ... ", dacValue);

    if (dacValue < (0x5fc - HEX_ERROR_MARGIN) || dacValue > (0x5fc + HEX_ERROR_MARGIN))
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("DAC Channel 7 code set to cdef ... ");

    dllReturn = DacWriteCommand(DEVICE, 7, DAC_CMD_RD_B1_CODE);
    dllReturn = DacReadData(DEVICE, 7, &dacValue);

    printf("value read is %04x ... ", dacValue);

    if (dacValue < (0xcdef - HEX_ERROR_MARGIN) || dacValue > (0xcdef + HEX_ERROR_MARGIN))
        printf("FAIL\n");
    else
        printf("PASS\n");
#endif

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

#if 0
void *thread_function(void *arguments)
{
    struct arg_struct *args = (struct arg_struct *)arguments;

    // wait for interrupt here ...
    adc_wait_int(DEVICE, 0);

    if (mio_error_code != MIO_SUCCESS)
        test[args->test - 1].pass_fail = FAIL;

    flag = 1;
}
#endif
