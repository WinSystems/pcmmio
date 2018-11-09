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
//	Name	 : mio_io.h
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
//	10/09/12	  3.0		Removed IOCTL_NUM		
//	11/07/18	  4.0		Minor code clean up		
//
//****************************************************************************

#ifndef __MIO_IO_H
#define __MIO_IO_H

#include <linux/ioctl.h> 

#define IOCTL_NUM   'i'

#define MAX_DEV     4

#define ADC_WRITE_COMMAND	    _IOWR(IOCTL_NUM, 1, int)

#define ADC_READ_DATA 		    _IOWR(IOCTL_NUM, 2, int)

#define ADC_READ_STATUS		    _IOWR(IOCTL_NUM, 3, int)

#define ADC1_WAIT_INT 		    _IOWR(IOCTL_NUM, 4, int)
    
#define ADC2_WAIT_INT 		    _IOWR(IOCTL_NUM, 5, int)

#define DAC_WRITE_DATA 		    _IOWR(IOCTL_NUM, 6, int)

#define DAC_READ_STATUS 	    _IOWR(IOCTL_NUM, 7, int)

#define DAC_WRITE_COMMAND 	    _IOWR(IOCTL_NUM, 8, int)

#define DAC1_WAIT_INT 		    _IOWR(IOCTL_NUM, 9, int)
    
#define DAC2_WAIT_INT 		    _IOWR(IOCTL_NUM, 10, int)

#define DIO_WRITE_BYTE 		    _IOWR(IOCTL_NUM, 11, int)
    
#define DIO_READ_BYTE 		    _IOWR(IOCTL_NUM, 12, int)

#define DIO_WAIT_INT 		    _IOWR(IOCTL_NUM, 13, int)

#define DIO_GET_INT			    _IOWR(IOCTL_NUM, 14, int)

#define MIO_WRITE_REG 		    _IOWR(IOCTL_NUM, 16, int)

#define MIO_READ_REG 		    _IOWR(IOCTL_NUM, 17, int)

// The name of the device file
#define DEVICE_FILE_NAME "pcmmio_ws"

// These are the error codes for mio_error_code 
#define MIO_SUCCESS               0
#define MIO_OPEN_ERROR            1
#define MIO_TIMEOUT_ERROR         2
#define MIO_BAD_CHANNEL_NUMBER    3
#define MIO_BAD_MODE_NUMBER       4
#define MIO_BAD_RANGE             5
#define MIO_COMMAND_WRITE_FAILURE 6
#define MIO_READ_DATA_FAILURE     7
#define MIO_MISSING_IRQ           8
#define MIO_ILLEGAL_VOLTAGE       9
#define MIO_BAD_VALUE             10
#define MIO_BAD_POLARITY          11
#define MIO_BAD_DEVICE            12

// register map
#define ADC1_DATA_LO    0
#define ADC1_DATA_HI    1
#define ADC1_COMMAND    2  // Reg3[4:3] = 00
#define ADC1_RESOURCE   2  // Reg3[4:3] = 01
#define DIO_RESOURCE    2  // Reg3[4:3] = 1X
#define ADC1_RSRC_ENBL  3  // write only
#define ADC1_STATUS     3  // read only
#define ADC2_DATA_LO    4
#define ADC2_DATA_HI    5
#define ADC2_COMMAND    6  // Reg7[3] = 0
#define ADC2_RESOURCE   6  // Reg7[3] = 1
#define ADC2_RSRC_ENBL  7  // write only
#define ADC2_STATUS     7  // read only
#define DAC1_DATA_LO    8  // Reg11[4] = 0
#define DAC1_RDBACK_LO  8  // Reg11[4] = 1, read only
#define DAC1_DATA_HI    9  // Reg11[4] = 0
#define DAC1_RDBACK_HI  9  // Reg11[4] = 1, read only
#define DAC1_COMMAND    10 // Reg11[3] = 0
#define DAC1_RESOURCE   10 // Reg11[3] = 1
#define DAC1_RSRC_ENBL  11 // write only
#define DAC1_STATUS     11 // read only
#define DAC2_DATA_LO    12 // Reg15[4] = 0
#define DAC2_RDBACK_LO  12 // Reg15[4] = 1, read only
#define DAC2_DATA_HI    13 // Reg15[4] = 0
#define DAC2_RDBACK_HI  13 // Reg15[4] = 1, read only
#define DAC2_COMMAND    14 // Reg15[3] = 0
#define DAC2_RESOURCE   14 // Reg15[3] = 1
#define DAC2_RSRC_ENBL  15 // write only
#define DAC2_STATUS     15 // Reg15[5] = 0, read only
#define DAC2_IRQ_REG    15 // Reg15[5] = 1, read only
#define DIO_PORT0       16
#define DIO_PORT1       17
#define DIO_PORT2       18
#define DIO_PORT3       19
#define DIO_PORT4       20
#define DIO_PORT5       21
#define DIO_INT_PENDING 22
#define DIO_PAGE_LOCK   23
#define DIO_POLARTIY0   24 // reg23[7:6] = 01
#define DIO_ENABLE0     24 // reg23[7:6] = 10
#define DIO_INT_ID0     24 // reg23[7:6] = 11
#define DIO_POLARTIY1   25 // reg23[7:6] = 01
#define DIO_ENABLE1     25 // reg23[7:6] = 10
#define DIO_INT_ID1     25 // reg23[7:6] = 11
#define DIO_POLARTIY2   26 // reg23[7:6] = 01
#define DIO_ENABLE2     26 // reg23[7:6] = 10
#define DIO_INT_ID2     26 // reg23[7:6] = 11
 
// Page defintions
#define PAGE0		    0x0
#define PAGE1		    0x40
#define PAGE2		    0x80
#define PAGE3		    0xc0

// These are DAC specific defines
#define DAC_BUSY        0x80

#define DAC_SPAN_UNI5   0
#define DAC_SPAN_UNI10  1
#define DAC_SPAN_BI5    2
#define DAC_SPAN_BI10   3
#define DAC_SPAN_BI2    4
#define DAC_SPAN_BI7    5

// These are ADC specific defines
#define	ADC_SINGLE_ENDED  0x80
#define ADC_DIFFERENTIAL  0x00

#define ADC_UNIPOLAR      0x08
#define ADC_BIPOLAR       0x00

#define ADC_TOP_5V	      0x00
#define ADC_TOP_10V	      0x04

#define ADC_CH0_SELECT    0x00
#define ADC_CH1_SELECT    0x40
#define ADC_CH2_SELECT    0x10
#define ADC_CH3_SELECT    0x50
#define ADC_CH4_SELECT    0x20
#define ADC_CH5_SELECT    0x60
#define ADC_CH6_SELECT    0x30
#define ADC_CH7_SELECT    0x70

// These are DIO specific defines
#define FALLING    1
#define RISING     0

#ifdef LIB_DEFINED

// These are used by the library functions
int mio_error_code;
char mio_error_string[128];
float adc_bitval[MAX_DEV][16] = {.00, .00, .00, .00, .00, .00, .00, .00,
                                 .00, .00, .00, .00, .00, .00, .00, .00,
                                 .00, .00, .00, .00, .00, .00, .00, .00,
                                 .00, .00, .00, .00, .00, .00, .00, .00,
                                 .00, .00, .00, .00, .00, .00, .00, .00,
                                 .00, .00, .00, .00, .00, .00, .00, .00,
                                 .00, .00, .00, .00, .00, .00, .00, .00,
                                 .00, .00, .00, .00, .00, .00, .00, .00};

unsigned short adc_adjust[MAX_DEV][16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                                          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                                          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                                          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

float adc_offset[MAX_DEV][16] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                  0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                  0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                  0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                  0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                  0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                  0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                  0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

#else

// The rest of this file is made up of global variables available to application
// code and the function prototypes for all available functions
extern int mio_error_code;
extern char mio_error_string[128];
extern float adc_bitval[MAX_DEV][16];
extern unsigned short adc_adjust[MAX_DEV][16];
extern float adc_offset[MAX_DEV][16];

#endif

// adc functions
int adc_start_conversion(int dev_num, int channel);
float adc_get_channel_voltage(int dev_num, int channel);
int adc_convert_all_channels(int dev_num, unsigned short *buffer);
float adc_convert_to_volts(int dev_num, int channel, unsigned short value);
int adc_convert_single_repeated(int dev_num, int channel, unsigned short count, unsigned short *buffer);
int adc_buffered_channel_conversions(int dev_num, unsigned char *input_channel_buffer, unsigned short *buffer);
int adc_wait_ready(int dev_num, int channel);
int adc_write_command(int dev_num, int adc_num, unsigned char value);
unsigned char adc_read_status(int dev_num, int adc_num);
int adc_set_channel_mode(int dev_num, int channel, int input_mode, int duplex, int range);
unsigned short adc_read_conversion_data(int dev_num, int channel);
float adc_auto_get_channel_voltage(int dev_num, int channel);
int adc_disable_interrupt(int dev_num, int adc_num);
int adc_enable_interrupt(int dev_num, int adc_num);
int adc_wait_int(int dev_num, int adc_num);

// dac functions
int dac_set_span(int dev_num, int channel, unsigned char span_value);
int dac_wait_ready(int dev_num, int channel);
int dac_set_output(int dev_num, int channel, unsigned short dac_value);
int dac_set_voltage(int dev_num, int channel, float voltage);
int dac_write_command(int dev_num, int dac_num, unsigned char value);
int dac_buffered_output(int dev_num, unsigned char *cmd_buff, unsigned short *data_buff);
int dac_write_data(int dev_num, int dac_num, unsigned short value);
unsigned char dac_read_status(int dev_num, int dac_num);
int dac_disable_interrupt(int dev_num, int dac_num);
int dac_enable_interrupt(int dev_num, int dac_num);
int dac_wait_int(int dev_num, int dac_num);

// dio functions
void dio_reset_device(int dev_num);
int dio_read_bit(int dev_num, int bit_number);
void dio_write_bit(int dev_num, int bit_number, int val);
void dio_set_bit(int dev_num, int bit_number);
void dio_clr_bit(int dev_num, int bit_number);
unsigned char dio_read_byte(int dev_num, int offset);
void dio_write_byte(int dev_num, int offset, unsigned char value);
void dio_enab_bit_int(int dev_num, int bit_number, int polarity);
void dio_disab_bit_int(int dev_num, int bit_number);
void dio_clr_int(int dev_num, int bit_number);
int dio_get_int(int dev_num);
int dio_wait_int(int dev_num);

// misc functions
unsigned char mio_read_reg(int dev_num, int offset);
void mio_write_reg(int dev_num, int offset, unsigned char value);
void mio_dump_config(int dev_num);

#endif /* __MIO_IO_H */
