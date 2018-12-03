//****************************************************************************
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
//****************************************************************************
//
//	Name	 : mio_io.c
//
//	Project	 : PCMMIO Linux Device Driver
//
//	Author	 : Paul DeMetrotion
//
//****************************************************************************
//
//	  Date		Revision	                Description
//	--------	--------	---------------------------------------------
//	11/11/10	  1.0		Original Release	
//	08/30/11	  2.1		Fixed bug in write_dio_byte function	
//	10/09/12	  3.0		Added comments for all functions		
//	10/09/12	  3.1		Renamed file to pcmmio_ws
//	11/07/18	  4.0		Changed some function names
//                          Minor code clean-up
//
//****************************************************************************

#define LIB_DEFINED

#include "mio_io.h"    

#include <stdio.h>
#include <fcntl.h>      // open  
#include <unistd.h>     // exit 
#include <sys/ioctl.h>  // ioctl 

// These image variable help out where a register is not
// capable of a read/modify/write operation 
unsigned char dio_port_images[MAX_DEV][6];
unsigned char adc1_port_image[MAX_DEV] = {0, 0, 0, 0};
unsigned char adc2_port_image[MAX_DEV] = {0, 0, 0, 0};
unsigned char dac1_port_image[MAX_DEV] = {0x10, 0x10, 0x10, 0x10};
unsigned char dac2_port_image[MAX_DEV] = {0x30, 0x30, 0x30, 0x30};

// The channel selects on the ADC are non contigous. In order to avoid shifting and such
// with each channel select, we simple bild an array for selection. 
unsigned char adc_channel_select[MAX_DEV][16] = {
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
    ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
    ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT, 
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
    ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
    ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT, 
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
    ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
    ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT, 
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
    ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
    ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT };

// Mode selection can also be time consuming and we'd also like the mode to "Stick" from
// call to call. This array will eventually undefined reference to `main'hold the actual command byte to send to the
// ADC controller for each channel according to the mode set with adc_set_channel_mode 
unsigned char adc_channel_mode[MAX_DEV][16] = {
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
    ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
    ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT, 
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
    ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
    ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT, 
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
    ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
    ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT, 
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
    ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
    ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
    ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT };

// This array and the index value are used internally for the adc_convert_all_channels
// and for the adc_buffered_conversion routines. 
unsigned char adc_channel_buff[18] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,0xff,0xff};
unsigned char *adc_input_buffer;
unsigned short *adc_user_buffer;

// Becaues of the nature of the ADC beast. It's necessary to keep track of the last
// channel and the previous channel in order to retrieve the data amd know who it belongs to 
int adc_last_channel[MAX_DEV], adc_current_channel[MAX_DEV];

// Various index pointers for the above arrays and misc globals 
int adc_ch_index[MAX_DEV] = {0, 0, 0, 0};
int adc_out_index[MAX_DEV] = {0, 0, 0, 0};
int adc_repeat_channel[MAX_DEV];
int adc_repeat_count[MAX_DEV];

//------------------------------------------------------------------------
//		USER LIBRARY FUNCTIONS
//------------------------------------------------------------------------

int handle[MAX_DEV] = {0, 0, 0, 0};
char *device_id[MAX_DEV] ={"/dev/pcmmio_wsa",
                           "/dev/pcmmio_wsb",
                           "/dev/pcmmio_wsc",
                           "/dev/pcmmio_wsd"};

//------------------------------------------------------------------------
//
// check_handle
//
// Arguments:
//			dev_num		The index of the chip
//
// Returns:
//			0	The function completes successfully
//			-1	Error, check error code
//
//------------------------------------------------------------------------
int check_handle(int dev_num)
{
    if (handle[dev_num] > 0)	// If it's already a valid handle  
        return 0;

    if (handle[dev_num] == -1)	// If it's already been tried  
    {
        mio_error_code = MIO_OPEN_ERROR;
        sprintf(mio_error_string, "MIO - Unable to open device PCMMIO\n");
        return -1;
    }

    // Try opening the device file, in case it hasn't been opened yet  
    handle[dev_num] = open(device_id[dev_num], O_RDWR);

    if (handle[dev_num] > 0)	// If it's now a valid handle  
        return 0;

    mio_error_code = MIO_OPEN_ERROR;
    sprintf(mio_error_string, "MIO - Unable to open device PCMMIO\n");
    handle[dev_num] = -1;
    return -1;
}

//------------------------------------------------------------------------
//
// adc_set_channel_mode
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//			input_mode	Desired channel mode
//			duplex		Desired channel duplex
//			range		Desired channel range
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void adc_set_channel_mode(int dev_num, int channel, int input_mode, int duplex, int range)
{
    unsigned char command_byte;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n", dev_num);
        return; 
    }

    if (channel < 0 || channel > 15)
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (ADC) : Bad Channel Number %d\n", channel);
        return;
    }

    // Check for illegal modes
    if ((input_mode != ADC_SINGLE_ENDED) && (input_mode != ADC_DIFFERENTIAL))
    {
        mio_error_code = MIO_BAD_MODE_NUMBER;
        sprintf(mio_error_string, "MIO (ADC) : Bad Mode Number\n");
        return;
    }

    if ((duplex != ADC_UNIPOLAR) && (duplex != ADC_BIPOLAR))
    {
        mio_error_code = MIO_BAD_MODE_NUMBER;
        sprintf(mio_error_string, "MIO (ADC) : Bad Mode Number\n");
        return;
    }

    if ((range != ADC_TOP_5V) && (range != ADC_TOP_10V))
    {
        mio_error_code = MIO_BAD_RANGE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Range Value\n");
        return;
    }

    command_byte = adc_channel_select[dev_num][channel];
    command_byte = command_byte | input_mode | duplex | range;

    // Building these four arrays at mode set time is critical for speed
    // as we don't need to calculate anything when we want to start an ADC
    // conversion. WE simply retrieve the command byte from the array
    // and send it to the controller.
    // Likewise, when doing conversion from raw 16-bit values to a voltage
    // the mode controls the worth of each individual bit as well as binary
    // bias and offset values.  
    adc_channel_mode[dev_num][channel] = command_byte;

    // Calculate bit values, offset, and adjustment values  
    if ((range == ADC_TOP_5V) && (duplex == ADC_UNIPOLAR))
    {
        adc_bitval[dev_num][channel] = 5.00 / 65536.0;
        adc_adjust[dev_num][channel] = 0;
        adc_offset[dev_num][channel] = 0.0;
    }

    if ((range == ADC_TOP_5V) && (duplex == ADC_BIPOLAR))
    {
        adc_bitval[dev_num][channel] = 10.0 / 65536.0;
        adc_adjust[dev_num][channel] = 0x8000;
        adc_offset[dev_num][channel] = -5.000;
    }

    if ((range == ADC_TOP_10V) && (duplex == ADC_UNIPOLAR))
    {
        adc_bitval[dev_num][channel] = 10.0 / 65536.0;
        adc_adjust[dev_num][channel] = 0;
        adc_offset[dev_num][channel] = 0.0;
    }

    if ((range == ADC_TOP_10V) && (duplex == ADC_BIPOLAR))
    {
        adc_bitval[dev_num][channel] = 20.0 / 65536.0;
        adc_adjust[dev_num][channel] = 0x8000;
        adc_offset[dev_num][channel] = -10.0;
    }
}

//------------------------------------------------------------------------
//
// adc_start_conversion
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void adc_start_conversion(int dev_num, int channel)
{
    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n", dev_num);
        return;
    }

    if ((channel < 0) || (channel > 15))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (ADC) : Bad channel number %d\n", channel);
        return;
    }

    adc_last_channel[dev_num] = adc_current_channel[dev_num];
    adc_current_channel[dev_num] = channel;
    adc_write_command(dev_num, channel / 8, adc_channel_mode[dev_num][channel]);
}

//------------------------------------------------------------------------
//
// adc_get_channel_voltage
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//
// Returns:
//			value returned is the voltage
//          mio_error_code must be MIO_SUCCESS 
//          for return value to be valid
//
//------------------------------------------------------------------------
float adc_get_channel_voltage(int dev_num, int channel)
{
    unsigned short value;
    float result;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Set Channel Mode - Bad Device Number %d\n", dev_num);
        return -1;
    }

    if ((channel < 0) || (channel > 15))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (ADC) : Start conversion bad channel number %d\n", channel);
        return -1;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return -1;

    // Start two conversions so that we can have current data
    adc_start_conversion(dev_num, channel);

    if (mio_error_code)
        return 0.0;

    adc_wait_ready(dev_num, channel);

    if (mio_error_code)
        return 0.0;

    adc_start_conversion(dev_num, channel);

    if (mio_error_code)
        return 0.0;

    adc_wait_ready(dev_num, channel);

    if (mio_error_code)
        return 0.0;

    // Read out the conversion's raw data
    value = adc_read_conversion_data(dev_num, channel);

    if (mio_error_code)
        return 0.0;

    // Convert the raw data to a voltage 
    value = value + adc_adjust[dev_num][channel];
    result = value * adc_bitval[dev_num][channel];
    result = result + adc_offset[dev_num][channel];

    return result;
}

//------------------------------------------------------------------------
//
// adc_convert_all_channels
//
// Arguments:
//			dev_num		The index of the chip
//			buffer		Storage of channel data
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void adc_convert_all_channels(int dev_num, unsigned short *buffer)
{
    int i;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n", dev_num);
        return;
    }

    if (buffer == NULL)
    {
        mio_error_code = MIO_NULL_POINTER;
        sprintf(mio_error_string, "MIO (ADC) : Null buffer pointer\n");
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    // Initialize global variables including transferring the
    // address of the user's ouput buffer to an internal buffer pointer
    adc_user_buffer = buffer;
    adc_input_buffer = adc_channel_buff;
    adc_ch_index[dev_num] = 0;
    adc_out_index[dev_num] = 0;

    adc_start_conversion(dev_num, 0);

    if (mio_error_code)
        return;

    adc_wait_ready(dev_num, 0);

    if (mio_error_code)
        return;

    // This is old data throw it out
    adc_read_conversion_data(dev_num, 0);

    if (mio_error_code)
        return;

    // Finish the rest of the channels
    for (i = 1; i < 8; i++)
    {
        adc_start_conversion(dev_num, i);

        if (mio_error_code)
            return;

        adc_wait_ready(dev_num, i);

        if (mio_error_code)
            return;

        // Store the results in the user's buffer
        adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, i);

        if (mio_error_code)
            return;
    }

    // A final dummy conversion is required to get out the last data
    adc_start_conversion(dev_num, 7);

    if (mio_error_code)
        return;

    adc_wait_ready(dev_num, 7);

    if (mio_error_code)
        return;

    adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, 7);

    if (mio_error_code)
        return;

    // Now start on the second controller
    adc_start_conversion(dev_num, 8);

    if (mio_error_code)
        return;

    adc_wait_ready(dev_num, 8);

    if (mio_error_code)
        return;

    // This data is old - Throw it out
    adc_read_conversion_data(dev_num, 8);

    if (mio_error_code)
        return;

    for (i = 9; i < 16; i++)
    {
        adc_start_conversion(dev_num, i);

        if (mio_error_code)
            return;

        adc_wait_ready(dev_num, i);

        if (mio_error_code)
            return;

        adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, i);

        if (mio_error_code)
            return;
    }

    // A final dummy conversion is required to get the last data
    adc_start_conversion(dev_num, 15);

    if (mio_error_code)
        return;

    adc_wait_ready(dev_num, 15);

    if (mio_error_code)
        return;

    adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, 15);
}

//------------------------------------------------------------------------
//
// adc_convert_to_volts
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//			value		Value to be converted to volts
//
// Returns:
//			value returned is the voltage
//          mio_error_code must be MIO_SUCCESS 
//          for return value to be valid
//
//------------------------------------------------------------------------
float adc_convert_to_volts(int dev_num, int channel, unsigned short value)
{
    float result;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n", dev_num);
        return -1;
    }

    if ((channel < 0) || (channel > 15))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (ADC) : Bad channel number %d\n", channel);
        return -1;
    }

    value = value + adc_adjust[dev_num][channel];
    result = value * adc_bitval[dev_num][channel];
    result = result + adc_offset[dev_num][channel];

    return result;
}

//------------------------------------------------------------------------
//
// adc_convert_single_repeated
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//			count		Number of conversions
//			buffer		Storage of channel data
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void adc_convert_single_repeated(int dev_num, int channel, unsigned short count, unsigned short *buffer)
{
    int i;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n", dev_num);
        return;
    }

    if ((channel < 0) || (channel > 15))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (ADC) : Bad channel number %d\n", channel);
        return;
    }

    if (buffer == NULL)
    {
        mio_error_code = MIO_NULL_POINTER;
        sprintf(mio_error_string, "MIO (ADC) : Null buffer pointer\n");
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    // Setup global variables including transferring the address of the
    // user's output buffer to a global variable the ISR knows about.
    adc_user_buffer = buffer;
    adc_out_index[dev_num] = 0;
    adc_repeat_channel[dev_num] = channel;
    adc_repeat_count[dev_num] = count;

    adc_start_conversion(dev_num, adc_repeat_channel[dev_num]);

    if (mio_error_code)
        return;

    adc_wait_ready(dev_num, adc_repeat_channel[dev_num]);

    if (mio_error_code)
        return;

    // This data is old, we don't want it
    adc_read_conversion_data(dev_num, adc_repeat_channel[dev_num]);

    if (mio_error_code)
        return;

    // Perform the requested number of conversions. Place the results into
    // the user's buffer.
    for (i = 0; i < adc_repeat_count[dev_num]; i++)
    {
        adc_start_conversion(dev_num, adc_repeat_channel[dev_num]);

        if (mio_error_code)
            return;

        adc_wait_ready(dev_num, adc_repeat_channel[dev_num]);

        if (mio_error_code)
            return;

        adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, adc_repeat_channel[dev_num]);

        if (mio_error_code)
            return;
    }

    // One last dummy conversion to retrieve our last data
    adc_start_conversion(dev_num, adc_repeat_channel[dev_num]);

    if (mio_error_code)
        return;

    adc_wait_ready(dev_num, adc_repeat_channel[dev_num]);

    if (mio_error_code)
        return;

    adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, adc_repeat_channel[dev_num]);
}

//------------------------------------------------------------------------
//
// adc_buffered_channel_conversions
//
// Arguments:
//			dev_num		The index of the chip
//			input_channel_buffer
//			buffer		Storage of channel data
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void adc_buffered_channel_conversions(int dev_num, unsigned char *input_channel_buffer, unsigned short *buffer)
{
    int adc_next_channel;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n", dev_num);
        return;
    }

    if (input_channel_buffer == NULL || buffer == NULL)
    {
        mio_error_code = MIO_NULL_POINTER;
        sprintf(mio_error_string, "MIO (ADC) : Null buffer pointer\n");
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    // Reset all of the array index pointers
    adc_ch_index[dev_num] = 0;
    adc_out_index[dev_num] = 0;
    
    adc_user_buffer = buffer;
    adc_input_buffer = input_channel_buffer;

    adc_start_conversion(dev_num, adc_input_buffer[adc_ch_index[dev_num]]);

    if (mio_error_code)
        return;

    adc_wait_ready(dev_num, adc_input_buffer[adc_ch_index[dev_num]++]);

    if (mio_error_code)
        return;

    // While there are channel numbers in the buffer (1= 0xff)
    // convert the requested channel and place the result in the
    // user's output buffer
    do {
        adc_next_channel = adc_input_buffer[adc_ch_index[dev_num]];

        // This function is particularly tricky because of the
        // fact that the data is delayed by one conversion and if
        // we switch back and forth between the two controllers
        // we'll need to run an extra conversion in order to get the
        // last data offering from the previous controller. The
        // conditional code in the next several lines handles the
        // switches from one controller to the other.  
        if (adc_current_channel[dev_num] < 8 && (adc_next_channel > 7 && adc_next_channel < 16))
        {
            adc_start_conversion(dev_num, adc_current_channel[dev_num]);

            if (mio_error_code)
                return;

            adc_wait_ready(dev_num, adc_current_channel[dev_num]);

            if (mio_error_code)
                return;

            adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, adc_current_channel[dev_num]);

            if (mio_error_code)
                return;

            adc_start_conversion(dev_num, adc_input_buffer[adc_ch_index[dev_num]]);

            if (mio_error_code)
                return;
 
            adc_wait_ready(dev_num, adc_input_buffer[adc_ch_index[dev_num]++]);

            if (mio_error_code)
                return;
        }
        else if ((adc_current_channel[dev_num] > 7 && adc_next_channel < 15) && adc_next_channel < 8)
        {
            adc_start_conversion(dev_num, adc_current_channel[dev_num]);

            if (mio_error_code)
                return;

            adc_wait_ready(dev_num, adc_current_channel[dev_num]);

            if (mio_error_code)
                return;

            adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, adc_current_channel[dev_num]);

            if (mio_error_code)
                return;

            adc_start_conversion(dev_num, adc_input_buffer[adc_ch_index[dev_num]]);

            if (mio_error_code)
                return;

            adc_wait_ready(dev_num, adc_input_buffer[adc_ch_index[dev_num]++]);

            if (mio_error_code)
                return;
        }
        else if (adc_next_channel < 15)
        {
            adc_start_conversion(dev_num, adc_input_buffer[adc_ch_index[dev_num]]);
 
            if (mio_error_code)
                return;

            adc_wait_ready(dev_num, adc_input_buffer[adc_ch_index[dev_num]++]);

            if (mio_error_code)
                return;

            adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, adc_current_channel[dev_num]);

            if (mio_error_code)
                return;
        }
    } while (adc_next_channel != 0xff);

    // One last conversion allows us to retrieve our real last data
    adc_start_conversion(dev_num, adc_input_buffer[--adc_ch_index[dev_num]]);

    if (mio_error_code)
        return;

    adc_wait_ready(dev_num, adc_input_buffer[adc_ch_index[dev_num]]);

    if (mio_error_code)
        return;

    adc_user_buffer[adc_out_index[dev_num]] = adc_read_conversion_data(dev_num, adc_current_channel[dev_num]);
}

//------------------------------------------------------------------------
//
// adc_wait_ready
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void adc_wait_ready(int dev_num, int channel)
{
    long retry;
    
    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n", dev_num);
        return;
    }

    if ((channel < 0) || (channel > 15))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (ADC) : Bad channel number %d\n", channel);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    retry = 100000l;

    // Like with the DAC timeout routine, under normal circumstances we'll
    // barely make it through the loop one time beacuse the hadrware is plenty
    // fast. We have the delay for the rare occasion and when the hadrware is not
    // responding properly.  
    while (retry--)
    {
        if (adc_read_status(dev_num, channel / 8) & 0x80)
            return;
    }

    mio_error_code = MIO_TIMEOUT_ERROR;
    sprintf(mio_error_string, "MIO (ADC) : Wait ready - Device timeout error\n");
}

//------------------------------------------------------------------------
//
// adc_write_command
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//			value		ADC command
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void adc_write_command(int dev_num, int adc_num, unsigned char value)
{
    // strip value for parameters
    int channel = (adc_num * 8) + (((value >> 3) & 0x6) | ((value >> 6) & 0x1));
    int duplex = value & ADC_UNIPOLAR;
    int range = value & ADC_TOP_10V;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n",dev_num);
        return;
    }

    if ((adc_num < 0) || (adc_num > 1))
    {
        mio_error_code = MIO_BAD_CHIP_NUM;
        sprintf(mio_error_string, "MIO (ADC) : Bad ADC Number %d\n",dev_num);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    ioctl(handle[dev_num], ADC_WRITE_COMMAND, (value << 8) | adc_num);

    // need to update the arrays also
    adc_channel_mode[dev_num][channel] = value;

    // Calculate bit values, offset, and adjustment values  
    if ((range == ADC_TOP_5V) && (duplex == ADC_UNIPOLAR))
    {
        adc_bitval[dev_num][channel] = 5.00 / 65536.0;
        adc_adjust[dev_num][channel] = 0;
        adc_offset[dev_num][channel] = 0.0;
    }

    if ((range == ADC_TOP_5V) && (duplex == ADC_BIPOLAR))
    {
        adc_bitval[dev_num][channel] = 10.0 / 65536.0;
        adc_adjust[dev_num][channel] = 0x8000;
        adc_offset[dev_num][channel] = -5.000;
    }

    if ((range == ADC_TOP_10V) && (duplex == ADC_UNIPOLAR))
    {
        adc_bitval[dev_num][channel] = 10.0 / 65536.0;
        adc_adjust[dev_num][channel] = 0;
        adc_offset[dev_num][channel] = 0.0;
    }

    if ((range == ADC_TOP_10V) && (duplex == ADC_BIPOLAR))
    {
        adc_bitval[dev_num][channel] = 20.0 / 65536.0;
        adc_adjust[dev_num][channel] = 0x8000;
        adc_offset[dev_num][channel] = -10.0;
    }
}

//------------------------------------------------------------------------
//
// adc_read_status
//
// Arguments:
//			dev_num		The index of the chip
//			adc_num		ADC device number
//
// Returns:
//			value returned is the status register contents
//          mio_error_code must be MIO_SUCCESS 
//          for return value to be valid
//
//------------------------------------------------------------------------
unsigned char adc_read_status(int dev_num, int adc_num)
{
    int ret_val;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n",dev_num);
        return -1;
    }

    if ((adc_num < 0) || (adc_num > 1))
    {
        mio_error_code = MIO_BAD_CHIP_NUM;
        sprintf(mio_error_string, "MIO (ADC) : Bad ADC Number %d\n",dev_num);
        return -1;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return -1;

    ret_val = ioctl(handle[dev_num], ADC_READ_STATUS, adc_num);

    return (ret_val & 0xff);
}

//------------------------------------------------------------------------
//
// adc_read_conversion_data
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//
// Returns:
//			value returned is the binary value of the conversion
//          mio_error_code must be MIO_SUCCESS 
//          for return value to be valid
//
//------------------------------------------------------------------------
unsigned short adc_read_conversion_data(int dev_num, int channel)
{
    int ret_val;
    int adc_num;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n", dev_num);
        return -1; 
    }

    if (channel < 0 || channel > 15)
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (ADC) : Bad Channel Number %d\n", channel);
        return -1;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return -1;

    if (channel > 7)
        adc_num = 1;
    else
        adc_num = 0;

    ret_val = ioctl(handle[dev_num], ADC_READ_DATA, adc_num);
    
    return (ret_val & 0xffff);
}

//------------------------------------------------------------------------
//
// adc_auto_get_channel_voltage
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//
// Returns:
//			value returned is the voltage
//          mio_error_code must be MIO_SUCCESS 
//          for return value to be valid
//
//------------------------------------------------------------------------
float adc_auto_get_channel_voltage(int dev_num, int channel)
{
    unsigned short value;
    float result;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n", dev_num);
        return -1; 
    }

    if (channel < 0 || channel > 15)
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (ADC) : Bad Channel Number %d\n", channel);
        return -1;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return -1;

    // Start out on a +/-10 Volt scale
    adc_set_channel_mode(dev_num, channel, ADC_SINGLE_ENDED, ADC_BIPOLAR, ADC_TOP_10V);

    if (mio_error_code)
        return -1;

    adc_start_conversion(dev_num, channel);

    if (mio_error_code)
        return -1;

    adc_wait_ready(dev_num, channel);

    if (mio_error_code)
        return -1;

    adc_start_conversion(dev_num, channel);

    if (mio_error_code)
        return -1;

    adc_wait_ready(dev_num, channel);

    if (mio_error_code)
        return -1;

    value = adc_read_conversion_data(dev_num, channel);

    if (mio_error_code)
        return -1;

    // Convert the raw data to voltage
    value = value + adc_adjust[dev_num][channel];
    result = value * adc_bitval[dev_num][channel];
    result = result + adc_offset[dev_num][channel];

    // If the voltage is less than -5.00 volts, we're as precise as we can get
    if (result <= -5.00)
        return result;

    // If the result is between -4.99 and 0.0 we can  to the +/- 5V scale.
    if (result < 0.0)
        adc_set_channel_mode(dev_num, channel, ADC_SINGLE_ENDED, ADC_BIPOLAR, ADC_TOP_5V);

    if (mio_error_code)
        return -1;

    // If the result is above 5 volts a 0 - 10V range will work best
    if (result >= 5.00)
        adc_set_channel_mode(dev_num, channel, ADC_SINGLE_ENDED, ADC_UNIPOLAR, ADC_TOP_10V);

    if (mio_error_code)
        return -1;

    // Lastly if we're greater than 0 and less than 5 volts the 0-5V scale is best
    if ((result >= 0.0) && (result < 5.00))
        adc_set_channel_mode(dev_num, channel, ADC_SINGLE_ENDED, ADC_UNIPOLAR, ADC_TOP_5V);

    if (mio_error_code)
        return -1;

    // Now that the values is properly ranged, we take two more samples
    // to get a current reading at the new scale.
    adc_start_conversion(dev_num, channel);

    if (mio_error_code)
        return -1;

    adc_wait_ready(dev_num, channel);

    if (mio_error_code)
        return -1;

    adc_start_conversion(dev_num, channel);

    if (mio_error_code)
        return -1;

    adc_wait_ready(dev_num, channel);

    if (mio_error_code)
        return -1;

    value = adc_read_conversion_data(dev_num, channel);

    if (mio_error_code)
        return -1;

    // Convert the raw data to voltage
    value = value + adc_adjust[dev_num][channel];
    result = value * adc_bitval[dev_num][channel];
    result = result + adc_offset[dev_num][channel];

    return result;
}

//------------------------------------------------------------------------
//
// adc_disable_interrupt
//
// Arguments:
//			dev_num		The index of the chip
//			adc_num		ADC device number
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void adc_disable_interrupt(int dev_num, int adc_num)
{
    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n", dev_num);
        return;
    }

    if ((adc_num < 0) || (adc_num > 1))
    {
        mio_error_code = MIO_BAD_CHIP_NUM;
        sprintf(mio_error_string, "MIO (ADC) : Bad ADC Number %d\n", dev_num);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available
        return;

    if (adc_num)
    {
        adc2_port_image[dev_num] = 0;
        mio_write_reg(dev_num, ADC2_RSRC_ENBL, adc2_port_image[dev_num]);	// Disable the interrupt
    }
    else
    {
        adc1_port_image[dev_num] = 0;
        mio_write_reg(dev_num, ADC1_RSRC_ENBL, adc1_port_image[dev_num]);	// Disable the interrupt
    }
}

//------------------------------------------------------------------------
//
// adc_enable_interrupt
//
// Arguments:
//			dev_num		The index of the chip
//			adc_num		ADC device number
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void adc_enable_interrupt(int dev_num, int adc_num)
{
    unsigned char vector;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n", dev_num);
        return;
    }

    if ((adc_num < 0) || (adc_num > 1))
    {
        mio_error_code = MIO_BAD_CHIP_NUM;
        sprintf(mio_error_string, "MIO (ADC) : Bad ADC Number %d\n", dev_num);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available
        return;

    if (adc_num)
    {
        adc2_port_image[dev_num] = 0x01;
        mio_write_reg(dev_num, ADC2_RSRC_ENBL, adc2_port_image[dev_num]);	// Enable the interrupt
    }
    else
    {
        adc1_port_image[dev_num] = 0x01;
        mio_write_reg(dev_num, ADC1_RSRC_ENBL, adc1_port_image[dev_num]);	// Enable the interrupt
    }
}

//------------------------------------------------------------------------
//
// adc_wait_int
//
// Arguments:
//			dev_num		The index of the chip
//			adc_num		ADC device number
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void adc_wait_int(int dev_num, int adc_num)
{
    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (ADC) : Bad Device Number %d\n", dev_num);
        return;
    }

    if ((adc_num < 0) || (adc_num > 1))
    {
        mio_error_code = MIO_BAD_CHIP_NUM;
        sprintf(mio_error_string, "MIO (ADC) : Bad ADC Number %d\n", dev_num);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    if (adc_num)
        ioctl(handle[dev_num], ADC2_WAIT_INT, NULL);
    else
        ioctl(handle[dev_num], ADC1_WAIT_INT, NULL);
}

//------------------------------------------------------------------------
//
// dac_set_span
//
// Arguments:
//			dev_num		The index of the chip
//			channel		DAC channel
//			span_value	Desired value
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dac_set_span(int dev_num, int channel, unsigned char span_value)
{
    unsigned char select_val;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DAC) : Bad Device Number %d\n", dev_num);
        return; 
    }

    if (channel < 0 || channel > 7)
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DAC) : Bad Channel Number %d\n", channel);
        return;
    }

    if (span_value < DAC_SPAN_UNI5 || span_value > DAC_SPAN_BI7)
    {
        mio_error_code = MIO_BAD_SPAN;
        sprintf(mio_error_string, "MIO (DAC) : Bad Span Value %d\n", channel);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    // This function sets up the output range for the DAC channel
    select_val = (channel % 4) << 1;

    dac_write_data(dev_num,  channel / 4, span_value);

    if (mio_error_code)
        return;

    dac_write_command(dev_num,  channel / 4, 0x60 | select_val);

    if (mio_error_code)
        return;

    dac_wait_ready(dev_num, channel);
}

//------------------------------------------------------------------------
//
// dac_wait_ready
//
// Arguments:
//			dev_num		The index of the chip
//			channel		DAC channel
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dac_wait_ready(int dev_num, int channel)
{
    unsigned long retry;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DAC) : Bad Device Number %d\n", dev_num);
        return; 
    }

    if (channel < 0 || channel > 7)
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DAC) : Bad Channel Number %d\n", channel);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    retry = 100000L;

    // This may seem like an absurd way to handle waiting and violates the
    // "no busy waiting" policy. The fact is that the hardware is normally so fast that we
    // usually only need one time through the loop anyway. The longer timeout is for rare
    // occasions and for detecting non-existent hardware.  
    while(retry--)
    {
        if (dac_read_status(dev_num,  channel / 4) & DAC_BUSY)
            return;
    }

    mio_error_code = MIO_TIMEOUT_ERROR;
    sprintf(mio_error_string, "MIO (DAC) : Device timeout error\n");
}

//------------------------------------------------------------------------
//
// dac_set_output
//
// Arguments:
//			dev_num		The index of the chip
//			channel		DAC channel
//			dac_value	Desired output
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dac_set_output(int dev_num, int channel, unsigned short dac_value)
{
    unsigned char select_val;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DAC) : Bad Device Number %d\n", dev_num);
        return; 
    }

    if (channel < 0 || channel > 7)
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DAC) : Bad Channel Number %d\n", channel);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    select_val = (channel % 4) << 1;

    dac_write_data(dev_num,  channel / 4, dac_value);

    if (mio_error_code)
        return;

    dac_write_command(dev_num,  channel / 4, 0x70 | select_val);

    if (mio_error_code)
        return;

    dac_wait_ready(dev_num, channel);
}

//------------------------------------------------------------------------
//
// dac_set_voltage
//
// Arguments:
//			dev_num		The index of the chip
//			channel		DAC channel
//			voltage		Desired voltage
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dac_set_voltage(int dev_num, int channel, float voltage)
{
    unsigned short value;
    float bit_val;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DAC) : Bad Device Number %d\n", dev_num);
        return; 
    }

    if (channel < 0 || channel > 7)
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DAC) : Bad Channel Number %d\n", channel);
        return;
    }

    // This output function is auto-ranging in that it picks the span that will
    // give the most precision for the voltage specified. This has one side-effect that
    // may be objectionable to some applications. When call to dac_set_span completes the
    // new range is set and the output will respond immediately using whatever value was last
    // in the output registers. This may cause a spike (up or down) in the DAC output until the
    // new output value is sent to the controller.  
    if ((voltage < -10.0) || (voltage > 10.0))
    {
        mio_error_code = MIO_ILLEGAL_VOLTAGE;
        sprintf(mio_error_string, "MIO (DAC) : Illegal Voltage %9.5f\n", voltage);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    if ((voltage >= 0.0) && (voltage < 5.0))
    {
        dac_set_span(dev_num, channel, DAC_SPAN_UNI5);

        if (mio_error_code)
            return;

        bit_val = 5.0 / 65536;
        value = (unsigned short) (voltage / bit_val);
    }

    if (voltage >= 5.0)
    {
        dac_set_span(dev_num, channel, DAC_SPAN_UNI10);

        if (mio_error_code)
            return;

        bit_val = 10.0 / 65536;
        value = (unsigned short) (voltage / bit_val);
    }

    if ((voltage < 0.0) && (voltage > -5.0))
    {
        dac_set_span(dev_num, channel, DAC_SPAN_BI5);

        if (mio_error_code)
            return;

        bit_val = 10.0 / 65536;
        value = (unsigned short) ((voltage + 5.0) / bit_val);
    }

    if (voltage <= -5.0)
    {
        dac_set_span(dev_num, channel, DAC_SPAN_BI10);

        if (mio_error_code)
            return;

        bit_val = 20.0 / 65536;
        value  = (unsigned short) ((voltage + 10.0) / bit_val);
    }

    dac_wait_ready(dev_num, channel);

    if (mio_error_code)
        return;

    dac_set_output(dev_num, channel, value);
}

//------------------------------------------------------------------------
//
// dac_write_command
//
// Arguments:
//			dev_num		The index of the chip
//			dac_num		DAC device number
//			value		DAC command
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dac_write_command(int dev_num, int dac_num, unsigned char value)
{
    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DAC) : Bad Device Number %d\n", dev_num);
        return; 
    }

    if (dac_num < 0 || dac_num > 1)
    {
        mio_error_code = MIO_BAD_CHIP_NUM;
        sprintf(mio_error_string, "MIO (DAC) : Bad Channel Number %d\n", dac_num);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    ioctl(handle[dev_num], DAC_WRITE_COMMAND, (value << 8) | dac_num);

    return;
}

//------------------------------------------------------------------------
//
// dac_buffered_output
//
// Arguments:
//			dev_num		The index of the chip
//			cmd_buff	Command list
//			data_buff	Data list
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dac_buffered_output(int dev_num, unsigned char *cmd_buff, unsigned short *data_buff)
{
    int i = 0;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DAC) : Bad Device Number %d\n", dev_num);
        return; 
    }

    if (cmd_buff == NULL || data_buff == NULL)
    {
        mio_error_code = MIO_NULL_POINTER;
        sprintf(mio_error_string, "MIO (ADC) : Null buffer pointer\n");
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    while(1)
    {
        if (cmd_buff[i] == 0xff)
            return;

        dac_set_output(dev_num, cmd_buff[i], data_buff[i]);

        if (mio_error_code) return;
        
        i++;
    }
}

//------------------------------------------------------------------------
//
// dac_write_data
//
// Arguments:
//			dev_num		The index of the chip
//			dac_num		DAC device number
//			value		New DAC value
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dac_write_data(int dev_num, int dac_num, unsigned short value)
{
    int ret_val;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DAC) : Bad Device Number %d\n", dev_num);
        return; 
    }

    if (dac_num < 0 || dac_num > 1)
    {
        mio_error_code = MIO_BAD_CHIP_NUM;
        sprintf(mio_error_string, "MIO (DAC) : Bad Channel Number %d\n", dac_num);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    ret_val = ioctl(handle[dev_num], DAC_WRITE_DATA, (value << 8) | dac_num);
    
    return;
}

//------------------------------------------------------------------------
//
// dac_read_status
//
// Arguments:
//			dev_num		The index of the chip
//			dac_num		DAC device number
//
// Returns:
//			value returned is the status register contents
//          mio_error_code must be MIO_SUCCESS 
//          for return value to be valid
//
//------------------------------------------------------------------------
unsigned char dac_read_status(int dev_num, int dac_num)
{
    int ret_val;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DAC) : Bad Device Number %d\n", dev_num);
        return -1; 
    }

    if (dac_num < 0 || dac_num > 1)
    {
        mio_error_code = MIO_BAD_CHIP_NUM;
        sprintf(mio_error_string, "MIO (DAC) : Bad Channel Number %d\n", dac_num);
        return -1;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return -1;

    ret_val = ioctl(handle[dev_num], DAC_READ_STATUS, dac_num);

    return (ret_val & 0xff);
}

//------------------------------------------------------------------------
//
// dac_disable_interrupt
//
// Arguments:
//			dev_num		The index of the chip
//			dac_num		DAC device number
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dac_disable_interrupt(int dev_num, int dac_num)
{
    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DAC) : Bad Device Number %d\n", dev_num);
        return; 
    }

    if (dac_num < 0 || dac_num > 1)
    {
        mio_error_code = MIO_BAD_CHIP_NUM;
        sprintf(mio_error_string, "MIO (DAC) : Bad Channel Number %d\n", dac_num);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    if (dac_num)
    {
        dac2_port_image[dev_num] = 0x30;
        mio_write_reg(dev_num, DAC2_RSRC_ENBL, dac2_port_image[dev_num]);	// Disable the interrupt
    }
    else
    {
        dac1_port_image[dev_num] = 0x10;
        mio_write_reg(dev_num, DAC1_RSRC_ENBL, dac1_port_image[dev_num]);	// Disable the interrupt
    }

    return;
}

//------------------------------------------------------------------------
//
// dac_enable_interrupt
//
// Arguments:
//			dev_num		The index of the chip
//			dac_num		DAC device number
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dac_enable_interrupt(int dev_num, int dac_num)
{
    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DAC) : Bad Device Number %d\n", dev_num);
        return; 
    }

    if (dac_num < 0 || dac_num > 1)
    {
        mio_error_code = MIO_BAD_CHIP_NUM;
        sprintf(mio_error_string, "MIO (DAC) : Bad Channel Number %d\n", dac_num);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    if (dac_num)
    {
        dac2_port_image[dev_num] = 0x31;
        mio_write_reg(dev_num, DAC2_RSRC_ENBL, dac2_port_image[dev_num]);	// Disable the interrupt
    }
    else
    {
        dac1_port_image[dev_num] = 0x11;
        mio_write_reg(dev_num, DAC1_RSRC_ENBL, dac1_port_image[dev_num]);	// Disable the interrupt
    }

    return;
}

//------------------------------------------------------------------------
//
// dac_wait_int
//
// Arguments:
//			dev_num		The index of the chip
//			dac_num		DAC device number
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dac_wait_int(int dev_num, int dac_num)
{
    int val;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DAC) : Bad Device Number %d\n", dev_num);
        return; 
    }

    if (dac_num < 0 || dac_num > 1)
    {
        mio_error_code = MIO_BAD_CHIP_NUM;
        sprintf(mio_error_string, "MIO (DAC) : Bad Channel Number %d\n", dac_num);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    if (dac_num)
        val = ioctl(handle[dev_num], DAC2_WAIT_INT, NULL);
    else
        val = ioctl(handle[dev_num], DAC1_WAIT_INT, NULL);

    return;
}

//------------------------------------------------------------------------
//
// dio_reset_device
//
// Arguments:
//			dev_num		The index of the chip
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dio_reset_device(int dev_num)
{
    int i;
    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DIO) : Bad device number %d\n", dev_num);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    // 1. disable all interupts
    // 2. set all DIO bits to 0
    for (i = 0; i < 48; i++)
    {
        dio_disab_bit_int(dev_num, i);
        dio_clr_bit(dev_num, i);
    }
}

//------------------------------------------------------------------------
//
// dio_read_bit
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to read
//
// Returns:
//			0	Bit is clear
//			1	Bit is set
//          mio_error_code must be MIO_SUCCESS 
//          for return value to be valid
//
//------------------------------------------------------------------------
int dio_read_bit(int dev_num, int bit_number)
{
    unsigned char port;
    int val;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DIO) : Bad device number %d\n", dev_num);
        return -1;
    }

    if ((bit_number < 1) || (bit_number > 48))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DIO) : Bad bit number %d\n", bit_number);
        return -1;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return -1;

    // Adjust for 0 - 47 bit numbering
    --bit_number;

    port = bit_number / 8;

    val = dio_read_byte(dev_num, port);

    // Get just the bit we specified
    val = val & (1 << (bit_number % 8));

    // adjust the return for a 0 or 1 value
    if (val) return 1;
    else return 0;
}

//------------------------------------------------------------------------
//
// dio_write_bit
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to write
//			val			New value of bit
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dio_write_bit(int dev_num, int bit_number, int val)
{
    unsigned char port;
    unsigned char temp;
    unsigned char mask;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DIO) : Bad device number %d\n", dev_num);
        return;
    }

    if ((bit_number < 1) || (bit_number > 48))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DIO) : Bad bit number %d\n", bit_number);
        return;
    }

    if (val < 0 || val > 1)
    {
        mio_error_code = MIO_BAD_VALUE;
        sprintf(mio_error_string, "MIO (DIO) : Bad value %d\n", val);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    // Adjust bit numbering for 0 based numbering
    --bit_number;

    // Calculate the address of the port based on bit number
    port = bit_number / 8;

    // Use the image value to avoid having to read from the port first
    temp = dio_port_images[dev_num][bit_number / 8];

    // Calculate the bit mask for the specifed bit
    mask = (1 << (bit_number %8));

    // Check whether the request was to set or clear the bit
    if (val)
        temp = temp | mask;
    else
        temp = temp & ~mask;

    dio_write_byte(dev_num, port, temp);
}

//------------------------------------------------------------------------
//
// dio_set_bit
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to set
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dio_set_bit(int dev_num, int bit_number)
{
    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DIO) : Bad device number %d\n", dev_num);
        return;
    }

    if ((bit_number < 1) || (bit_number > 48))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DIO) : Bad bit number %d\n", bit_number);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    dio_write_bit(dev_num, bit_number, 1);
}

//------------------------------------------------------------------------
//
// dio_clr_bit
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to clear
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dio_clr_bit(int dev_num, int bit_number)
{
    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DIO) : Bad device number %d\n", dev_num);
        return;
    }

    if ((bit_number < 1) || (bit_number > 48))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DIO) : Bad bit number %d\n", bit_number);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    dio_write_bit(dev_num, bit_number, 0);
}

//------------------------------------------------------------------------
//
// dio_read_byte
//
// Arguments:
//			dev_num		The index of the chip
//			offset		Register offset
//
// Returns:
//			value returned is the byte read
//          mio_error_code must be MIO_SUCCESS 
//          for return value to be valid
//
//------------------------------------------------------------------------
unsigned char dio_read_byte(int dev_num, int offset)
{
    int val;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DIO) : Bad device number %d\n", dev_num);
        return -1;
    }

    if ((offset < 0) || (offset > 5))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DIO) : Bad port number %d\n", offset);
        return -1;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return -1;

    // All bit operations are handled at this level so we need only
    // read and write bytes from the actual hardware using the driver
    // to handle our ioctl call for it.  
    val = ioctl(handle[dev_num], DIO_READ_BYTE, offset);

    return (unsigned char) (val & 0xff);
}

//------------------------------------------------------------------------
//
// dio_write_byte
//
// Arguments:
//			dev_num		The index of the chip
//			offset		Register offset
//			value		New register value
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dio_write_byte(int dev_num, int offset, unsigned char value)
{
    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DIO) : Bad device number %d\n", dev_num);
        return;
    }

    if ((offset < 0) || (offset > 5))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DIO) : Bad port number %d\n", offset);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    // update image
    dio_port_images[dev_num][offset] = value;

    // All bit operations for the DIO are handled at this level
    // and we need the driver to allow access to the actual
    // DIO registers to update the value.  
    ioctl(handle[dev_num], DIO_WRITE_BYTE, (value << 8) | offset);
}

//------------------------------------------------------------------------
//
// dio_enab_bit_int
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to clear
//			polarity	Rising or falling edge
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dio_enab_bit_int(int dev_num, int bit_number, int polarity)
{
    unsigned char port;
    unsigned char temp;
    unsigned char mask;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DIO) : Bad device number %d\n", dev_num);
        return;
    }

    if ((bit_number < 1) || (bit_number > 24))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DIO) : Bad bit number %d\n", bit_number);
        return;
    }

    if ((polarity != RISING) && (polarity != FALLING))
    {
        mio_error_code = MIO_BAD_POLARITY;
        sprintf(mio_error_string, "MIO (DIO) : Bad interrupt polarity %d\n", polarity);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    // Adjust the bit number for 0 based numbering
    --bit_number;

    // Calculate the offset for the enable port
    port = bit_number / 8;

    // Calculate the proper bit mask for this bit number
    mask = (1 << (bit_number % 8));

    // Turn on access to page 2 registers
    mio_write_reg(dev_num, DIO_PAGE_LOCK, PAGE2);

    // Get the current state of the enable register
    temp = mio_read_reg(dev_num, DIO_ENABLE0 + port);

    // Set the enable bit for our bit number
    temp = temp | mask;

    // Now update the interrupt enable register
    mio_write_reg(dev_num, DIO_ENABLE0 + port, temp);

    // Turn on access to page 1 for polarity control
    mio_write_reg(dev_num, DIO_PAGE_LOCK, PAGE1);

    temp = mio_read_reg(dev_num, DIO_POLARTIY0 + port);

    // Set the polarity according to the argument value
    if (polarity)
        temp = temp | mask;
    else
        temp = temp & ~mask;

    mio_write_reg(dev_num, DIO_POLARTIY0 + port, temp);

    // Set access back to page 3
    mio_write_reg(dev_num, DIO_PAGE_LOCK, PAGE3);
}

//------------------------------------------------------------------------
//
// dio_disab_bit_int
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to clear
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dio_disab_bit_int(int dev_num, int bit_number)
{
    unsigned char port;
    unsigned char temp;
    unsigned char mask;

    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DIO) : Bad device number %d\n", dev_num);
        return;
    }

    if ((bit_number < 1) || (bit_number > 24))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DIO) : Bad bit number %d\n", bit_number);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    // Adjust the bit number for 0 based numbering
    --bit_number;

    // Calculate the offset for the enable port
    port = bit_number / 8;

    // Calculate the proper bit mask for this bit number
    mask = (1 << (bit_number % 8));

    // Turn on access to page 2 registers
    mio_write_reg(dev_num, DIO_PAGE_LOCK, PAGE2);

    // Get the current state of the enable register
    temp = mio_read_reg(dev_num, DIO_ENABLE0 + port);

    // Clear the enable bit for the our bit
    temp = temp & ~mask;

    // Update the enable register with the new data
    mio_write_reg(dev_num, DIO_ENABLE0 + port, temp);

    // Set access back to page 3
    mio_write_reg(dev_num, DIO_PAGE_LOCK, PAGE3);
}

//------------------------------------------------------------------------
//
// dio_clr_int
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to clear
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void dio_clr_int(int dev_num, int bit_number)
{
    unsigned short port;
    unsigned short temp;
    unsigned short mask;

    // Adjust for 0 based numbering
    mio_error_code = MIO_SUCCESS;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DIO) : Bad device number %d\n", dev_num);
        return;
    }

    if ((bit_number < 1) || (bit_number > 24))
    {
        mio_error_code = MIO_BAD_CHANNEL_NUMBER;
        sprintf(mio_error_string, "MIO (DIO) : Bad bit number %d\n", bit_number);
        return;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return;

    --bit_number;

    // Calculate the correct offset for our enable register
    port = bit_number / 8;

    // Calculate the bit mask for this bit
    mask = (1 << (bit_number % 8));

    // Set access to page 2 for the enable register
    mio_write_reg(dev_num, DIO_PAGE_LOCK, PAGE2);

    // Get the current state of the register
    temp = mio_read_reg(dev_num, DIO_ENABLE0 + port);

    // Temporarily clear only our enable. This clears the interrupt
    temp = temp & ~mask;

    // Write out the temporary value
    mio_write_reg(dev_num, DIO_ENABLE0 + port, temp);

    temp = temp | mask;

    mio_write_reg(dev_num, DIO_ENABLE0 + port, temp);

    // Set access back to page 3
    mio_write_reg(dev_num, DIO_PAGE_LOCK, PAGE3);
}

//------------------------------------------------------------------------
//
// dio_get_int
//
// Arguments:
//			dev_num		The index of the chip
//
// Returns:
//			value returned is bit with the interrupt
//          mio_error_code must be MIO_SUCCESS 
//          for return value to be valid
//
//------------------------------------------------------------------------
int dio_get_int(int dev_num)
{
    int val;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DIO) : Bad device number %d\n", dev_num);
        return -1;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return -1;

    val = ioctl(handle[dev_num], DIO_GET_INT, NULL);

    return (val & 0xff);
}

//------------------------------------------------------------------------
//
// dio_wait_int
//
// Arguments:
//			dev_num		The index of the chip
//
// Returns:
//			value returned is bit with the interrupt
//          mio_error_code must be MIO_SUCCESS 
//          for return value to be valid
//
//------------------------------------------------------------------------
int dio_wait_int(int dev_num)
{
    int val;

    if (dev_num < 0 || dev_num > MAX_DEV - 1)
    {
        mio_error_code = MIO_BAD_DEVICE;
        sprintf(mio_error_string, "MIO (DIO) : Bad device number %d\n", dev_num);
        return -1;
    }

    if (check_handle(dev_num))   // Check for chip available  
        return -1;

    val = ioctl(handle[dev_num], DIO_WAIT_INT, NULL);

    return (val & 0xff);
}

//------------------------------------------------------------------------
//
// mio_read_reg
//
// Arguments:
//			dev_num		The index of the chip
//			offset		Register offset
//
// Returns:
//			value returned is the byte read
//          mio_error_code must be MIO_SUCCESS 
//          for return value to be valid
//
//------------------------------------------------------------------------
unsigned char mio_read_reg(int dev_num, int offset)
{
    int val;

    mio_error_code = MIO_SUCCESS;

    if (check_handle(dev_num))   // Check for chip available  
        return -1;

    // This is a catchall register read routine that allows reading of
    // ANY of the registers on the PCM-MIO. It is used primarily for
    // retreiving control and access values in the hardware.  
    val = ioctl(handle[dev_num], MIO_READ_REG, offset);

    return (unsigned char) (val & 0xff);
}

//------------------------------------------------------------------------
//
// mio_write_reg
//
// Arguments:
//			dev_num		The index of the chip
//			offset		Register offset
//			value		New register value
//
// Return value in mio_error_code:
//			0	The function completes successfully
//          any other return value indicates function failed
//
//------------------------------------------------------------------------
void mio_write_reg(int dev_num, int offset, unsigned char value)
{
    mio_error_code = MIO_SUCCESS;

    if (check_handle(dev_num))   // Check for chip available  
        return;

    // This function like the previous allow unlimited
    // write access to ALL of the registers on the PCM-MIO  
    ioctl(handle[dev_num], MIO_WRITE_REG, (value << 8) | offset);
}

//------------------------------------------------------------------------
//
// mio_dump_config for debug
//
//------------------------------------------------------------------------
void mio_dump_config(int dev_num)
{
    if (check_handle(dev_num))   // Check for chip available
        return;

    // ADC1_RESOURCE
    adc1_port_image[dev_num] |= 0x08;
    mio_write_reg(dev_num, ADC1_RSRC_ENBL, adc1_port_image[dev_num]);
    printf("\nADC1_RESOURCE : 0x%0x\n", mio_read_reg(dev_num, ADC1_RESOURCE));
    adc1_port_image[dev_num] &= ~0x08;
    mio_write_reg(dev_num, ADC1_RSRC_ENBL, adc1_port_image[dev_num]);
    
    // DIO_RESOURCE
    adc1_port_image[dev_num] |= 0x10;
    mio_write_reg(dev_num, ADC1_RSRC_ENBL, adc1_port_image[dev_num]);
    printf("DIO_RESOURCE  : 0x%0x\n", mio_read_reg(dev_num, DIO_RESOURCE));
    adc1_port_image[dev_num] &= ~0x10;
    mio_write_reg(dev_num, ADC1_RSRC_ENBL, adc1_port_image[dev_num]);
    
    // ADC1_STATUS
    printf("ADC1_STATUS   : 0x%0x\n", mio_read_reg(dev_num, ADC1_STATUS));
    
    // ADC2_RESOURCE
    adc2_port_image[dev_num] |= 0x08;
    mio_write_reg(dev_num, ADC2_RSRC_ENBL, adc2_port_image[dev_num]);
    printf("ADC2_RESOURCE : 0x%0x\n", mio_read_reg(dev_num, ADC2_RESOURCE));
    adc2_port_image[dev_num] &= ~0x08;
    mio_write_reg(dev_num, ADC2_RSRC_ENBL, adc2_port_image[dev_num]);
    
    // ADC2_STATUS
    printf("ADC2_STATUS   : 0x%0x\n", mio_read_reg(dev_num, ADC2_STATUS));
    
    // DAC1_RESOURCE
    dac1_port_image[dev_num] |= 0x08;
    mio_write_reg(dev_num, DAC1_RSRC_ENBL, dac1_port_image[dev_num]);
    printf("DAC1_RESOURCE : 0x%0x\n", mio_read_reg(dev_num, DAC1_RESOURCE));
    dac1_port_image[dev_num] &= ~0x08;
    mio_write_reg(dev_num, DAC1_RSRC_ENBL, dac1_port_image[dev_num]);
    
    // DAC1_STATUS
    printf("DAC1_STATUS   : 0x%0x\n", mio_read_reg(dev_num, DAC1_STATUS));
    
    // DAC2_RESOURCE
    dac2_port_image[dev_num] |= 0x08;
    mio_write_reg(dev_num, DAC2_RSRC_ENBL, dac2_port_image[dev_num]);
    printf("DAC2_RESOURCE : 0x%0x\n", mio_read_reg(dev_num, DAC2_RESOURCE));
    dac2_port_image[dev_num] &= ~0x08;
    mio_write_reg(dev_num, DAC2_RSRC_ENBL, dac2_port_image[dev_num]);
    
    // DAC2_STATUS
    dac2_port_image[dev_num] &= ~0x20;
    mio_write_reg(dev_num, DAC2_RSRC_ENBL, dac2_port_image[dev_num]);
    printf("DAC2_STATUS   : 0x%0x\n", mio_read_reg(dev_num, DAC2_STATUS));
    dac2_port_image[dev_num] |= 0x20;
    mio_write_reg(dev_num, DAC2_RSRC_ENBL, dac2_port_image[dev_num]);

    // DIO Interrupts
    mio_write_reg(dev_num, DIO_PAGE_LOCK, PAGE2);
    printf("DIO_ENABLE0   : 0x%0x\n", mio_read_reg(dev_num, DIO_ENABLE0));
    printf("DIO_ENABLE1   : 0x%0x\n", mio_read_reg(dev_num, DIO_ENABLE1));
    printf("DIO_ENABLE2   : 0x%0x\n", mio_read_reg(dev_num, DIO_ENABLE2));
    mio_write_reg(dev_num, DIO_PAGE_LOCK, PAGE1);
    printf("DIO_POLARTIY0 : 0x%0x\n", mio_read_reg(dev_num, DIO_POLARTIY0));
    printf("DIO_POLARTIY1 : 0x%0x\n", mio_read_reg(dev_num, DIO_POLARTIY1));
    printf("DIO_POLARTIY2 : 0x%0x\n", mio_read_reg(dev_num, DIO_POLARTIY2));
    mio_write_reg(dev_num, DIO_PAGE_LOCK, PAGE3);
    printf("DIO_INT_ID0   : 0x%0x\n", mio_read_reg(dev_num, DIO_INT_ID0));
    printf("DIO_INT_ID1   : 0x%0x\n", mio_read_reg(dev_num, DIO_INT_ID1));
    printf("DIO_INT_ID2   : 0x%0x\n", mio_read_reg(dev_num, DIO_INT_ID2));
}
