/*******************************************************************************
 *
 * Copyright (C) u-blox AG
 * u-blox AG, Thalwil, Switzerland
 *
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY. IN PARTICULAR, NEITHER THE AUTHOR NOR U-BLOX MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 *******************************************************************************
 *
 * Project: firmwareUpdateTool v21.05
 * Purpose: Provide sample code to do a FW update
 *
 ******************************************************************************/

/*!
  \file
  \brief  Defines and Prototypes for platform specifics
*/

#ifndef _PLATFORM_H
#define _PLATFORM_H

#include "types.h"

#define ENABLE_AARDVARK_SUPPORT   //!< AARDVARK USB->I2C/SPI converter


#ifdef WIN32
// for Diolan U2C-12 support you have to download a third party library provided from Diolan
// http://www.diolan.com/products/u2c12/downloads.html
// Install the latest version of the library and add the file
// DIOLAN_INSTALLATION_DIRECTORY/Src/Common/i2cbridge.h to this project.
//# define ENABLE_DIOLAN_SUPPORT  //!< DIOLAN USB->I2C/SPI converter

#endif // WIN32

#define ENABLE_NET_SUPPORT        //!< Network sockets

//! verbosity
extern int verbose;

// uncomment the line below if two stop bits has to be used
//#define UART_TWO_STOP_BITS

//! communication interface type
typedef enum SER_TYPE_e
{
    COM,                          //!< Serial COM-Port
    STDINOUT,                     //!< stdin/stdout port
    U2C,                          //!< I2C Port over Diolan
    I2C,                          //!< I2C Port over Aardvark
    SPI,                          //!< SPI Port over Aardvark
    SPU,                          //!< SPI Port over Diolan
    NET                           //!< Serial port over Ethernet
} SER_TYPE_t;

//! serial port information handling type
typedef struct SER_HANDLE_s
{
    HANDLE     handle;            //!< communication device handle
    SER_TYPE_t type;              //!< communication device type
    U4         devAddr;           //!< address of device
    const CH*  pName;             //!< opening name of device
    void*      pData;             //!< custom data pointer
    U4         baudrate;          //!< current baudrate
} SER_HANDLE_t;
typedef SER_HANDLE_t* SER_HANDLE_pt; //!< pointer to SER_HANDLE_t type


//=====================================================================
// I2C DEFAULTS
//=====================================================================
#define BaudrateDefaultI2C        100000   //!< The default I2C Baudrate

//=====================================================================
// TIME FUNCTIONS
//=====================================================================

//! Sleep
/*!
    Sleep for \a time milliseconds

    \param time \b IN: number of milliseconds to sleep
*/
void TIME_SLEEP(U4 time);

//! Get Timer
/*!
    Number of milliseconds that have elapsed since a OS/platform specific event. Don't use
    absolute values, but differences between subsequent calls to TIME_GET() to get
    a predictable behavior.

    \return number of milliseconds
*/
U4   TIME_GET(void);

//=====================================================================
// SERIAL IO
//=====================================================================

//! Open Serial Port
/*!
    Open serial port indicated by \a name.

    \param name \b IN: name of serial port to open
    \return handle to opened port on success, #NULL on failure
*/
SER_HANDLE_pt SER_OPEN(const CH* name);

//! Write to SPI Port
/*!
    Write data to SPI port. Successive 0xFF will be filtered out.

    \param h \b IN: handle to open serial port
    \param p \b IN: buffer to data to be written
    \param size \b IN: number of bytes to be written
    \return number of bytes written
*/
U4 SER_WRITE_SPI(SER_HANDLE_pt     h,
            const void        *p,
            U4                 size);


//! Write to Serial Port
/*!
    Write data to serial port

    \param h \b IN: handle to open serial port
    \param p \b IN: buffer to data to be written
    \param size \b IN: number of bytes to be written
    \return number of bytes written
*/
U4 SER_WRITE(SER_HANDLE_pt     h,
            const void        *p,
            U4                 size);

//! Read from Serial Port
/*!
    Read data from serial port

    \param h \b IN: handle to open serial port
    \param p \b OUT: buffer to write data to
    \param size \b IN: number of bytes to be read
    \return number of bytes read
*/
U4 SER_READ(SER_HANDLE_pt h,
            void*         p,
            U4            size);

//! Flush Serial Port
/*!
    Flush pending bytes from serial buffer

    \param h \b IN: handle to open serial port
*/
void SER_CLEAR(SER_HANDLE_pt h);

//! Close Serial Port
/*!
    Close serial port

    \param h \b IN: handle to open serial port
*/
void SER_CLOSE(SER_HANDLE_pt h);

//! changes the baudrate of the communication device
/*!
    \param h    Handle to the device
    \param br    baudrate to set for the device
    \return #TRUE if the setting succeeded, #FALSE else
*/
BOOL SER_BAUDRATE(SER_HANDLE_pt h,
                  U4            br);

//! get number of pending bytes at I2C interface
/*!
    \param h \b IN: handle to open port
    \return number of pending bytes
*/
U4 SER_PENDING(SER_HANDLE_pt h);

//! Flush Serial Port
/*!
    Write the buffered data to the port

    \param h \b IN: handle to open serial port
*/
void SER_FLUSH(SER_HANDLE_pt h);

//! Re-Enumerate Port
/*!
    Re-Enumerates Port, if applicable

    \param h \b IN: handle to open serial port
    \param IsUsb \b IN: set to TRUE if serial port is really a USB connection
    \return success state
*/
BOOL SER_REENUM(SER_HANDLE_pt h, BOOL IsUsb);

//=====================================================================
// STATUS MESSAGES
//=====================================================================

//! Message Level
typedef    enum
{
    MSG_ERR,   //!< Error message
    MSG_WARN,  //!< Warning message
#ifdef _TEST
    MSG_TEST,  //!< Test message
#endif // _TEST
    MSG_LEV0,  //!< Verbosity Level 0 message
    MSG_LEV1,  //!< Verbosity Level 1 message
    MSG_LEV2,  //!< Verbosity Level 2 message
    MSG_DBG,   //!< Debug message
    MSG_DBGV   //!< Very verbose debug message
} MSG_LEVEL;

//! Print status message
/*!
    Print status message to standard output.
    The message is printed if \a level is lower or equal to MSG_LEV2 or \a verbose is TRUE and
    \a level is lower or equal to MSG_DBG; it is discarded otherwise.

    Messages assigned a message level #MSG_ERR or #MSG_WARN are prefixed with "ERROR:" or "WARNING:",
    respectively. Messages of type #MSG_DBG are prefixed with a hyphen. See details for #MSG_LEVEL
    for a definition of valid message levels.

    \param level \b IN: message level
    \param format \b IN: format string
    \param ... \b IN: optional arguments
*/
void MESSAGE(MSG_LEVEL level,
             const CH* format, ...);

void MESSAGE_PLAIN(const CH* format, ...);

//=====================================================================
// CONSOLE
//=====================================================================

//! initialize console
void CONSOLE_INIT(void);

//! cleanup console
void CONSOLE_DONE(void);

//! save current cursor position
void CONSOLE_SAVE_POS(U4 offs);

//! restore previously saved cursor position
void CONSOLE_RESTORE_POS(void);



#endif // _PLATFORM_H
