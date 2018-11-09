//*****************************************************************************
//	
//	Copyright 2010 by WinSystems Inc.
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
//*****************************************************************************
//
//	Name	 : kbhit.c
//
//	Project	 : PCMMIO Sample Application Program
//
//	Author	 : Paul DeMetrotion
//
//*****************************************************************************
//
//	  Date		Revision	                Description
//	--------	--------	---------------------------------------------
//	11/11/10	  1.0		Original Release	
//
//*****************************************************************************

// These functions are from the Wrox Press Book "Beginning Linux
// Programming - Second Edition" by Richard Stones and Neil Matthew.
// (C) 1996 & 1999 and are included here in compliance with the GNU
// Public license.

#include <stdio.h>
#include <termios.h>
#include <unistd.h>

static struct termios initial_settings, new_settings;
static int peek_character = -1;

// Keyboard support function prototypes  
void init_keyboard(void);
void close_keyboard(void);
int kbhit(void);
int readch(void);

void init_keyboard(void)
{
    tcgetattr(0,&initial_settings);
    new_settings = initial_settings;
    new_settings.c_lflag &= ~ICANON;
    new_settings.c_lflag &= ~ECHO;
    new_settings.c_lflag &= ~ISIG;
    new_settings.c_cc[VMIN] = 1;
    new_settings.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_settings);
}

void close_keyboard(void)
{
    tcsetattr(0, TCSANOW, &initial_settings);
}

int kbhit(void)
{
    char ch;
    int nread;

    if(peek_character != -1)
        return 1;

    new_settings.c_cc[VMIN] = 0;
    tcsetattr(0, TCSANOW, &new_settings);
    nread = read(0, &ch, 1);
    new_settings.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &new_settings);

    if(nread == 1)
    {
        peek_character = ch;
        return 1;
    }

    return 0;
}

int readch(void)
{
    char ch;

    if(peek_character != -1)
    {
        ch = peek_character;
        peek_character = -1;
        return ch;
    }
    read(0,&ch,1);
    return ch;
}
