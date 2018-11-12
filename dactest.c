//****************************************************************************
//	
//	Copyright 2018 by WinSystems Inc.
//
//****************************************************************************
//
//	Name	 : dacTest.cpp
//
//	Project	 : PCM-MIO Console Application
//
//	Author	 : Paul DeMetrotion
//
//****************************************************************************
//
//	  Date		  Rev	                Description
//	--------    -------	   ---------------------------------------------
//	04/02/18	  1.0		Original Release
//
//****************************************************************************
//
// Need to test all DAC functions:
//   int DacSetChannelVoltage(unsigned int device, unsigned int channel, float voltage)
//   int DacSetChannelOutput(unsigned int device, unsigned int channel, unsigned short code)
//   int DacSetChannelSpan(unsigned int device, unsigned int channel, unsigned short span)
//   int DacBufferedVoltage(unsigned int device, unsigned short *chanBuf, float *voltBuf)
//   int DacEnableInterrupt(unsigned int device, unsigned int channel)
//   int DacDisableInterrupt(unsigned int device, unsigned int channel)
//   int DacWaitForUpdate(unsigned int device, unsigned int channel, unsigned short code, unsigned long timeout)
//   int DacWriteCommand(unsigned int device, unsigned int channel, unsigned int dacCommand)
//   int DacWriteData(unsigned int device, unsigned int channel, unsigned int dacData)
//   int DacReadData(unsigned int device, unsigned int channel, unsigned int *dacData)
//   int DacWaitForReady(unsigned int device, unsigned int channel)
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
#define ERROR_MARGIN        0.05
#define HEX_ERROR_MARGIN    0x100

int set_voltage(unsigned int ch, unsigned int num);
int set_value(unsigned int ch, unsigned short span, unsigned short value);

static float vArray[] = {
    (float)-9.9, (float)-8.8, (float)-7.7, (float)-6.6, (float)-5.5,
    (float)-4.4, (float)-3.3, (float)-2.5, (float)-2.2, (float)-1.8,
    (float)-1.1, (float)-0.5, (float) 0.0, (float) 0.5, (float) 1.1,
    (float) 1.8, (float) 2.2, (float) 2.5, (float) 3.3, (float) 4.4,
    (float) 5.5, (float) 6.6, (float) 7.7, (float) 8.8, (float) 9.9,
    (float)-10.001, (float) 10.001
};

static int chanMap[] = { 8, 10, 12, 14, 1, 3, 5, 7 };

int _tmain(int argc, _TCHAR* argv[])
{
    unsigned short chanBuf[] = { 0, 1, 2, 3, 4, 5, 6, 7, 0xff };
    unsigned short nullBuf[1];
    float voltBuf[] = { vArray[1], vArray[4], vArray[6], vArray[9], vArray[14], vArray[16], vArray[19], vArray[24] };
    float adcVoltage;
    unsigned short adcValue;
    unsigned int dacValue;
    int dllReturn = SUCCESS;

    printf("PCM-MIO Application : dacTest\n");
    printf("Version %d.%d\n\n", MAJOR_VER, MINOR_VER);

    if (argc != 1)
    {
        printf("Usage error:\n");
        printf("  dacTest\n");
        exit(1);
    }

    dllReturn = InitializeSession(DEVICE);

    if (dllReturn)
    {
        printf("Error initializing device.\n\n");
        exit(dllReturn);
    }
    else   
    {
        printf("Device opened.\n");
    }

    // set voltage on each channel and verify
    printf("\nTest 1: DacSetChannelVoltage\n");

    for (int i = 0; i < 8; i++)
        set_voltage(i, 3 * i);

    // check error conditions
    dllReturn = set_voltage(8, 2);

    printf("Attempted to access illegal channel ... ");

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    dllReturn = set_voltage(0, 25);

    printf("Attempted to set illegal voltage ... ");

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    dllReturn = set_voltage(4, 26);

    printf("Attempted to set illegal voltage ... ");

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    // configure all adc channels for -10V to +10V
    for (int i = 0; i < 16; i++)
        dllReturn = AdcSetChannelMode(DEVICE, i, ADC_SINGLE_ENDED, ADC_BIPOLAR, ADC_TOP_10V);

    // set value and verify
    printf("\nTest 2: DacSetChannelSpan + DacSetChannelOutput\n");

    for (int i = 0; i < 8; i++)
        set_value(i, DAC_SPAN_BI10, 0x1400 * (i + 1));

    // check error conditions
    dllReturn = set_value(11, DAC_SPAN_BI10, 0x0001);

    printf("Attempted to access illegal channel ... ");

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    printf("Attempted to set illegal span ... ");

    dllReturn = set_value(1, DAC_SPAN_BI7 + 1, 0x0001);

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    // set voltage for all dac channels
    printf("\nTest 3: DacBufferedVoltage\n");

    dllReturn = DacBufferedVoltage(DEVICE, chanBuf, voltBuf);

    if (dllReturn)
    {
        printf("Error setting DAC channels.\n");
        exit(dllReturn);
    }
    else
    {
        for (int i = 0; i < 8; i++)
        {
            printf("Channel %d voltage set to %.3f ... ", chanBuf[i], voltBuf[i]);
            dllReturn = AdcAutoGetChannelVoltage(DEVICE, chanMap[i], &adcVoltage);
            printf("voltage read is %.3f ... ", adcVoltage);
            if (adcVoltage < (voltBuf[i] - ERROR_MARGIN) || adcVoltage >(voltBuf[i] + ERROR_MARGIN))
                printf("FAIL\n");
            else
                printf("PASS\n");
        }
    }

    // check error conditions
    dllReturn = DacBufferedVoltage(DEVICE, nullBuf, voltBuf);

    printf("Attempted to use null buffer ... ");

    if (dllReturn != INVALID_PARAMETER)
        printf("FAIL\n");
    else
        printf("PASS\n");

    // discrete programming of channel 2
    printf("\nTest 4: DacWriteCommand + DacWriteData + DacWaitForReady\n");
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

    // readback
    printf("\nTest 5: DacWriteCommand + DacReadData\n");
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

int set_voltage(unsigned int ch, unsigned int num)
{
    float adcVoltage;
    int dllReturn = SUCCESS;

    // set voltage and verify
    dllReturn = DacSetChannelVoltage(DEVICE, ch, vArray[num]);

    if (dllReturn)
        return dllReturn;
    else
    {
        printf("Channel %d voltage set to %.3f ... ", ch, vArray[num]);
        dllReturn = AdcAutoGetChannelVoltage(DEVICE, chanMap[ch], &adcVoltage);

        if (dllReturn)
        {
            printf("Error reading ADC voltage.\n");
            return dllReturn;
        }
        else
        {
            printf("voltage read is %.3f ... ", adcVoltage);
            if (adcVoltage < (vArray[num] - ERROR_MARGIN) || adcVoltage >(vArray[num] + ERROR_MARGIN))
                printf("FAIL\n");
            else
                printf("PASS\n");
        }
    }

    return dllReturn;
}

int set_value(unsigned int ch, unsigned short span, unsigned short value)
{
    unsigned short adcValue;
    int dllReturn = SUCCESS;

    // set span
    dllReturn = DacSetChannelSpan(DEVICE, ch, span);

    if (dllReturn)
        return dllReturn;

    // set value and verify
    dllReturn = DacSetChannelOutput(DEVICE, ch, value);

    printf("Channel %d value set to %04x ... ", ch, value);

    if (dllReturn)
        return dllReturn;
    else
    {
        dllReturn = AdcSetChannelMode(DEVICE, chanMap[ch], ADC_SINGLE_ENDED,
                                         (span < DAC_SPAN_BI5) ? ADC_UNIPOLAR : ADC_BIPOLAR, 
                                         (span == DAC_SPAN_UNI5 || span == DAC_SPAN_BI5) ? ADC_TOP_5V : ADC_TOP_10V);
        dllReturn = AdcGetChannelValue(DEVICE, chanMap[ch], &adcValue);

        if (dllReturn)
        {
            printf("Error reading ADC value.\n");
            return dllReturn;
        }
        else
        {
            // adjust for sign
            if (span >= DAC_SPAN_BI5) adcValue ^= 0x8000;

            printf("value read is %04x ... ", adcValue);

            if (adcValue < (value - HEX_ERROR_MARGIN) || adcValue >(value + HEX_ERROR_MARGIN))
                printf("FAIL\n");
            else
                printf("PASS\n");
        }
    }

    return dllReturn;
}
