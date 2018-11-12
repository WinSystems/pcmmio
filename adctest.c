//****************************************************************************
//	
//	Copyright 2018 by WinSystems Inc.
//
//****************************************************************************
//
//	Name	 : adcTest.cpp
//
//	Project	 : PCM-MIO Console Application
//
//	Author	 : Paul DeMetrotion
//
//****************************************************************************
//
//	  Date		  Rev	                Description
//	--------    -------	   ---------------------------------------------
//	03/09/18	  1.0		Original Release
//
//****************************************************************************
//
// Need to test all ADC functions:
//   int AdcSetChannelMode(unsigned int device, unsigned int channel, int mode, int duplex, int range)
//   int AdcGetChannelValue(unsigned int device, unsigned int channel, unsigned short *value)
//   int AdcGetChannelVoltage(unsigned int device, unsigned int channel, float *voltage
//   int AdcGetAllChannelValues(unsigned int device, unsigned short *valBuf)
//   int AdcGetAllChannelVoltages(unsigned int device, float *voltBuf);
//   int AdcAutoGetChannelVoltage(unsigned int device, unsigned int channel, float *voltage)
//   int AdcConvertSingleChannelRepeated(unsigned int device, unsigned int channel, int count, unsigned short *valBuf)
//   int AdcBufferedChannelConversions(unsigned int device, unsigned int *chanBuf, unsigned short *outBuf)
//   int AdcConvertToVolts(unsigned int device, unsigned int channel, int value, float *voltage)
//   int AdcEnableInterrupt(unsigned int device, unsigned int channel)
//   int AdcDisableInterrupt(unsigned int device, unsigned int channel)
//   int AdcWaitForConversion(unsigned int device, unsigned int channel, unsigned short *value, unsigned long timeout)
//   int AdcWriteCommand(unsigned int device, unsigned int channel, unsigned int adcCommand)
//   int AdcWaitForReady(unsigned int device, unsigned int channel)
//   int AdcReadData(unsigned int device, unsigned int channel, unsigned int *adcData)
//   int AdcStartConversion(unsigned int device, unsigned int channel)
//
//****************************************************************************

#include <stdio.h>
#include <stdlib.h>  
#include <tchar.h>  
#include <windows.h>
#include "pcmmioDLL.h"

#define MAJOR_VER           1
#define MINOR_VER           0
#define DEVICE              1
#define ERROR_MARGIN        0.1

int verify_voltage(unsigned int ch, unsigned int num);

static float vArray[] = {
    (float)-9.9, (float)-8.8, (float)-7.7, (float)-6.6, (float)-5.5,
    (float)-4.4, (float)-3.3, (float)-2.5, (float)-2.2, (float)-1.8,
    (float)-1.1, (float)-0.5, (float) 0.0, (float) 0.5, (float) 1.1,
    (float) 1.8, (float) 2.2, (float) 2.5, (float) 3.3, (float) 4.4,
    (float) 5.5, (float) 6.6, (float) 7.7, (float) 8.8, (float) 9.9,
    (float)-10.001, (float) 10.001
};

static int voltageMap[] = { 17, 14, 17, 16, 17, 19, 17, 24, 
                             2, 17, 4, 17, 6, 17, 9, 17 };
static int voltageMap2[] = { 12, 14, 12, 16, 12, 19, 12, 24,
                             2, 12, 4, 12, 6, 12, 9, 12 };
static int chanMap[] = { 8, 10, 12, 14, 1, 3, 5, 7 };

int _tmain(int argc, _TCHAR* argv[])
{
    int channel = 0;
    float adcChVoltage[16];
    unsigned short chanBuf[] = { 0, 1, 2, 3, 4, 5, 6, 7, 0xff };
    float voltBuf[] = { vArray[2], vArray[4], vArray[6], vArray[9], vArray[14], vArray[16], vArray[19], vArray[24] };
    float temp;
    unsigned short temp1;
    unsigned short buffer[16];
    unsigned int ch_buffer[] = { 7, 8, 8, 7, 3, 12, 12, 14, 0xFF };
    int dllReturn = SUCCESS;

    printf("PCM-MIO Application : adcTest\n");
    printf("Version %d.%d\n\n", MAJOR_VER, MINOR_VER);

    if (argc != 1)
    {
        printf("Usage error:\n");
        printf("  adcTest\n");
        exit(1);
    }

    dllReturn = InitializeSession(DEVICE);

    if (dllReturn)
    {
        printf("Error initializing device.\n");
        exit(dllReturn);
    }
    else   
    {
        printf("Device opened.\n");
    }

    // configure adc channels
    for (int i = 0; i < 16; i++)
        dllReturn = AdcSetChannelMode(DEVICE, i, ADC_SINGLE_ENDED, ADC_BIPOLAR, ADC_TOP_10V);

    // voltage for all dac channels
    dllReturn = DacBufferedVoltage(DEVICE, chanBuf, voltBuf);

    if (dllReturn)
    {
        printf("Error setting DAC channels.\n");
        exit(dllReturn);
    }
    else
    {
        for (int i = 0; i < 8; i++)
            printf("DAC channel %d voltage set to %.3f\n", chanBuf[i], voltBuf[i]);
    }

    // read each channel individually
    printf("\nTest 1: AdcSetChannelMode + AdcGetChannelVoltage\n");

    for (int i = 0; i < 16; i++)
        dllReturn = verify_voltage(i, voltageMap[i]);

    // check error conditions
    dllReturn = AdcGetChannelVoltage(DEVICE, 16, &temp);

    printf("Attempted to access illegal channel ... ");

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    dllReturn = AdcGetChannelVoltage(DEVICE, 2, nullptr);

    printf("Attempted to use null pointer ... ");

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    // read each channel individually
    printf("\nTest 2: AdcGetAllChannelVoltages\n");

    // read all channels
    dllReturn = AdcGetAllChannelVoltages(DEVICE, adcChVoltage);

    if (dllReturn)
    {
        printf("Error reading ADC channels.\n");
        exit(dllReturn);
    }
    else
    {
        for (int i = 0; i < 16; i++)
        {
            printf("ADC Channel %d voltage read is %.3f ... ", i, adcChVoltage[i]);

            if (adcChVoltage[i] < (vArray[voltageMap[i]] - ERROR_MARGIN) || adcChVoltage[i] > (vArray[voltageMap[i]] + ERROR_MARGIN))
                printf("FAIL\n");
            else
                printf("PASS\n");
        }
    }

    // check error conditions
    dllReturn = AdcGetAllChannelVoltages(DEVICE, nullptr);

    printf("Attempted to use null pointer ... ");

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("\n\n");

    // auto read each channel individually
    printf("\nTest 3: AdcAutoGetChannelVoltage\n");

    for (int i = 0; i < 16; i++)
    {
        dllReturn = AdcAutoGetChannelVoltage(DEVICE, i, &adcChVoltage[i]);

        printf("ADC Channel %d voltage auto read is %.3f ... ", i, adcChVoltage[i]);

        if (adcChVoltage[i] < (vArray[voltageMap2[i]] - ERROR_MARGIN) || adcChVoltage[i] > (vArray[voltageMap2[i]] + ERROR_MARGIN))
            printf("FAIL\n");
        else
            printf("PASS\n");
    }

    // check error conditions
    printf("Attempted to access illegal channel ... ");

    dllReturn = AdcAutoGetChannelVoltage(DEVICE, 16, &adcChVoltage[0]);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to use null pointer ... ");

    dllReturn = AdcAutoGetChannelVoltage(DEVICE, 8, nullptr);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    // repeated read of single channel 
    printf("\nTest 4: AdcConvertSingleChannelRepeated\n");

    // repeated conversion for channel 12
    dllReturn = AdcConvertSingleChannelRepeated(DEVICE, 12, sizeof(buffer) / sizeof(unsigned short), buffer);

    printf("Repeated reads of ADC Channel 12 ... ");

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
    printf("Attempted to access illegal channel ... ");

    dllReturn = AdcConvertSingleChannelRepeated(DEVICE, 16, sizeof(buffer) / sizeof(unsigned short), buffer);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to use null pointer ... ");

    dllReturn = AdcConvertSingleChannelRepeated(DEVICE, 6, sizeof(buffer) / sizeof(unsigned short), nullptr);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    // buffered channel conversions
    printf("\nTest 5: AdcBufferedChannelConversions\n");

    dllReturn = AdcBufferedChannelConversions(DEVICE, ch_buffer, buffer);

    for (int i = 0; i < (sizeof(ch_buffer) / sizeof(unsigned int)) - 1; i++)
    {
        AdcConvertToVolts(DEVICE, ch_buffer[i], buffer[i], &temp);
        printf("ADC Channel %d voltage read is %.5f ... ", ch_buffer[i], temp);

        if (temp < (vArray[voltageMap[ch_buffer[i]]] - ERROR_MARGIN) || temp >(vArray[voltageMap[ch_buffer[i]]] + ERROR_MARGIN))
        {
            printf("FAIL\n");
        }
        else
            printf("PASS\n");
    }

    // discrete read of channel 8
    printf("\nTest 6: AdcStartConversion + AdcWriteCommand + AdcWaitForReady + AdcReadData\n");

    printf("Discrete read of ADC Channel 8 ... ");

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
    printf("Attempted to start conversion on illegal channel ... ");

    dllReturn = AdcStartConversion(DEVICE, 16);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to write command to illegal channel ... ");

    dllReturn = AdcWriteCommand(DEVICE, 16, 0x84); // single ended, positive, ch 0, -10V to +10V

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to wait for ready on illegal channel ... ");

    dllReturn = AdcWaitForReady(DEVICE, 16);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to read data from illegal channel ... ");

    dllReturn = AdcReadData(DEVICE, 16, &temp1);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to read data with null pointer ... ");

    dllReturn = AdcReadData(DEVICE, 15, nullptr);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    // close device
    dllReturn = CloseSession(DEVICE);

    if (dllReturn)
    {
        printf("\nError closing device.\n");
        exit(dllReturn);
    }
    else
    {
        printf("\nDevice closed.\n");
    }

    return(0);
}

int verify_voltage(unsigned int ch, unsigned int num)
{
    float adcVoltage;
    int dllReturn = SUCCESS;

    printf("Channel %d should read %.3fV ... ", ch, vArray[num]);

    dllReturn = AdcGetChannelVoltage(DEVICE, ch, &adcVoltage);

    if (dllReturn)
    {
        printf("Error reading ADC voltage.\n");
        return dllReturn;
    }
    else
    {
        printf("voltage read is %.3fV ... ", adcVoltage);

        if (adcVoltage < (vArray[num] - ERROR_MARGIN) || adcVoltage > (vArray[num] + ERROR_MARGIN))
            printf("FAIL\n");
        else
            printf("PASS\n");
    }

    return dllReturn;
}
