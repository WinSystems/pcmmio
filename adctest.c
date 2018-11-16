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
   
//void *thread_function(void *arguments);
float random_float(float min, float max);

// adc channel map
static int chanMap[] = { 8, 10, 12, 14, 1, 3, 5, 7 };

int main(int argc, char *argv[])
{
    char *app_name = "adctest";
    float adcVoltage;
    unsigned short adcVoltageArray[16];
    unsigned short chanBuf[] = { 0, 1, 2, 3, 4, 5, 6, 7, 0xff };
    float voltBuf[8] = { 0 };
//    float temp;
//    unsigned short temp1;
//    unsigned short buffer[16];
//    unsigned int ch_buffer[] = { 7, 8, 8, 7, 3, 12, 12, 14, 0xFF };

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
    }
    
    // run all tests
    for (int t = 1; t <= sizeof(test) / sizeof(TEST); t++)
    {
        printf("Test %d: %s ... ", test[t - 1].number, test[t - 1].name);
        
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
#if 0
                // repeated conversion for channel 12
                dllReturn = AdcConvertSingleChannelRepeated(DEVICE, 12, sizeof(buffer) / sizeof(unsigned short), buffer);

                if (dllReturn)
                {
                    printf("Error reading ADC channel 12.\n");
                    exit(dllReturn);
                }
                else
                {
                    for (int i = 0; i < sizeof(buffer) / sizeof(unsigned short); i++)
                    {
                        AdcConvertToVolts(DEVICE, 12, buffer[i], &temp);

                        if (temp < (vArray[voltageMap[12]] - ERROR_MARGIN) || temp >(vArray[voltageMap[12]] + ERROR_MARGIN))
                        {
                            printf("FAIL\n");
                            break;
                        }
                        else if (i == sizeof(buffer) / sizeof(unsigned short) - 1)
                            printf("PASS\n");
                    }
                }

                // check error conditions
                dllReturn = AdcConvertSingleChannelRepeated(DEVICE, 16, sizeof(buffer) / sizeof(unsigned short), buffer);

                if (dllReturn != INVALID_PARAMETER)
                    printf("FAIL\n");
                else
                    printf("PASS\n");

                dllReturn = AdcConvertSingleChannelRepeated(DEVICE, 6, sizeof(buffer) / sizeof(unsigned short), nullptr);

                if (dllReturn != INVALID_PARAMETER)
                    printf("FAIL\n");
                else
                    printf("PASS\n");
#endif
                break;
                
            case 5: 
#if 0
                dllReturn = AdcBufferedChannelConversions(DEVICE, ch_buffer, buffer);

                for (int i = 0; i < (sizeof(ch_buffer) / sizeof(unsigned int)) - 1; i++)
                {
                    AdcConvertToVolts(DEVICE, ch_buffer[i], buffer[i], &temp);

                    if (temp < (vArray[voltageMap[ch_buffer[i]]] - ERROR_MARGIN) || temp >(vArray[voltageMap[ch_buffer[i]]] + ERROR_MARGIN))
                    {
                        printf("FAIL\n");
                    }
                    else
                        printf("PASS\n");
                }
#endif
                break;

            case 6: 
#if 0
                dllReturn = AdcStartConversion(DEVICE, 8);

                if (dllReturn)
                    printf("Error starting conversion ... ");

                dllReturn = AdcWriteCommand(DEVICE, 8, 0x84); // single ended, positive, ch 0, -10V to +10V

                if (dllReturn)
                    printf("Error writing command ... ");

                dllReturn = AdcWaitForReady(DEVICE, 8);

                if (dllReturn)
                    printf("Error waiting for ready ... ");

                dllReturn = AdcReadData(DEVICE, 8, &temp1);

                if (dllReturn)
                    printf("Error reading data ... ");

                AdcConvertToVolts(DEVICE, 8, temp1, &temp);

                if (temp < (vArray[voltageMap[8]] - ERROR_MARGIN) || temp > (vArray[voltageMap[8]] + ERROR_MARGIN))
                    printf("FAIL\n");
                else
                    printf("PASS\n");

                // check error conditions
                dllReturn = AdcStartConversion(DEVICE, 16);

                if (dllReturn != INVALID_PARAMETER)
                    printf("FAIL\n");
                else
                    printf("PASS\n");

                dllReturn = AdcWriteCommand(DEVICE, 16, 0x84); // single ended, positive, ch 0, -10V to +10V

                if (dllReturn != INVALID_PARAMETER)
                    printf("FAIL\n");
                else
                    printf("PASS\n");

                dllReturn = AdcWaitForReady(DEVICE, 16);

                if (dllReturn != INVALID_PARAMETER)
                    printf("FAIL\n");
                else
                    printf("PASS\n");

                dllReturn = AdcReadData(DEVICE, 16, &temp1);

                if (dllReturn != INVALID_PARAMETER)
                    printf("FAIL\n");
                else
                    printf("PASS\n");

                dllReturn = AdcReadData(DEVICE, 15, nullptr);

                if (dllReturn != INVALID_PARAMETER)
                    printf("FAIL\n");
                else
                    printf("PASS\n");
#endif
                break;
                
            case 7: 
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

#if 0
void *thread_function(void *arguments)
{
    int irq;
    struct arg_struct *args = (struct arg_struct *)arguments;

    // wait for interrupt here ...
    irq = dio_wait_int(DEVICE);

    if (mio_error_code != MIO_SUCCESS || irq != args->bit)
        test[args->test - 1].pass_fail = FAIL;
}
#endif

float random_float(float min, float max)
{
    if (max == min) 
        return min;
    else if (min < max) 
        return (max - min) * ((float)rand() / RAND_MAX) + min;

    // return 0 if min > max
    return 0;
}
