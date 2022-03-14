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

#include "platform.h"

#ifdef ENABLE_DIOLAN_SUPPORT
#include <stdio.h>
#include <windows.h>
#include "u2cbridge.h"

#define DLL_HANDLE           HINSTANCE
#define dlopen(name, flags)  LoadLibraryA(name)
#define dlsym(handle, name)  GetProcAddress(handle, name)
#define SO_NAME              "i2cbrdg.dll"

static U2C_RESULT lastError = U2C_SUCCESS;

static void *_loadFunction (const char *name, U2C_RESULT *result) {
    static DLL_HANDLE handle = 0;
    void * function = 0;

    /* Load the shared library if necessary */
    if (handle == 0) {
        handle = dlopen(SO_NAME, RTLD_LAZY);
        if (handle == 0) {
            *result = U2C_UNKNOWN_ERROR;
            return 0;
        }
    }

    /* Bind the requested function in the shared library */
    function = (void *)dlsym(handle, name);
    *result  = function ? U2C_SUCCESS : U2C_UNKNOWN_ERROR;
    return function;
}

#define BIND_AND_EXECUTE(res,func,params,args) \
static res (_stdcall *__##func) params = 0; \
res _stdcall func params \
{ \
  if (__##func == 0) {\
        U2C_RESULT res = U2C_SUCCESS;\
        if (!(__##func = _loadFunction(#func, &res)))\
        {\
            lastError = res;\
            return 0;\
        }\
     }\
    lastError = U2C_SUCCESS;\
    return __##func args; \
}

BIND_AND_EXECUTE(BYTE,
                 U2C_GetDeviceCount,
                 (void),
                 ())

BIND_AND_EXECUTE(HANDLE,
                 U2C_OpenDevice,
                 (BYTE nDevice),
                 (nDevice))

BIND_AND_EXECUTE(U2C_RESULT,
                U2C_CloseDevice,
                (HANDLE hDevice),
                (hDevice))

BIND_AND_EXECUTE(U2C_RESULT,
                U2C_IsHandleValid,
                (HANDLE hDevice),
                (hDevice))

BIND_AND_EXECUTE(U2C_RESULT,
                U2C_GetSerialNum,
                (HANDLE hDevice, long* pSerialNum),
                (hDevice, pSerialNum))

BIND_AND_EXECUTE(U2C_RESULT,
                 U2C_ScanDevices,
                 (HANDLE hDevice, PU2C_SLAVE_ADDR_LIST pList),
                 (hDevice,pList))

BIND_AND_EXECUTE(U2C_RESULT,
                 U2C_Read,
                 (HANDLE hDevice, PU2C_TRANSACTION pTransaction),
                 (hDevice,pTransaction))

BIND_AND_EXECUTE(U2C_RESULT,
                U2C_Write,
                (HANDLE hDevice, PU2C_TRANSACTION pTransaction),
                (hDevice,pTransaction))

BIND_AND_EXECUTE(U2C_RESULT,
                 U2C_SetI2cFreq,
                 (HANDLE hDevice, BYTE Frequency),
                 (hDevice,Frequency))

BIND_AND_EXECUTE(U2C_RESULT,
                 U2C_SetClockSynch,
                 (HANDLE hDevice, BOOL Enable),
                 (hDevice,Enable))

BIND_AND_EXECUTE(U2C_RESULT,
                 U2C_SetIoDirection,
                 (HANDLE hDevice, ULONG Value, ULONG Mask),
                 (hDevice, Value, Mask))

BIND_AND_EXECUTE(U2C_RESULT,
                 U2C_SingleIoWrite,
                 (HANDLE hDevice, ULONG IoNumber, BOOL Value),
                 (hDevice, IoNumber, Value))

BIND_AND_EXECUTE(U2C_RESULT,
                 U2C_SpiSetFreq,
                 (HANDLE hDevice, BYTE Frequency),
                 (hDevice, Frequency))

BIND_AND_EXECUTE(U2C_RESULT,
                 U2C_SpiReadWrite,
                 (HANDLE hDevice, BYTE *pOutBuffer, BYTE *pInBuffer, WORD Length),
                 (hDevice, pOutBuffer, pInBuffer, Length))

BIND_AND_EXECUTE(U2C_RESULT,
                 U2C_SpiRead,
                 (HANDLE hDevice, BYTE *pInBuffer, WORD Length),
                 (hDevice, pInBuffer, Length))

////

#define ERRCASE(ec) case ec: return #ec

char * U2C_StatusString(U2C_RESULT result)
{
    result = lastError == U2C_SUCCESS ? result : lastError;
    switch (result)
    {
    ERRCASE(U2C_SUCCESS);
    ERRCASE(U2C_BAD_PARAMETER);
    ERRCASE(U2C_HARDWARE_NOT_FOUND);
    ERRCASE(U2C_SLAVE_DEVICE_NOT_FOUND);
    ERRCASE(U2C_TRANSACTION_FAILED);
    ERRCASE(U2C_SLAVE_OPENNING_FOR_WRITE_FAILED);
    ERRCASE(U2C_SLAVE_OPENNING_FOR_READ_FAILED);
    ERRCASE(U2C_SENDING_MEMORY_ADDRESS_FAILED);
    ERRCASE(U2C_SENDING_DATA_FAILED);
    ERRCASE(U2C_NOT_IMPLEMENTED);
    ERRCASE(U2C_NO_ACK);
    ERRCASE(U2C_DEVICE_BUSY);
    ERRCASE(U2C_MEMORY_ERROR);
    ERRCASE(U2C_UNKNOWN_ERROR);
    ERRCASE(U2C_I2C_CLOCK_SYNCH_TIMEOUT);
    default:
        return "unknown error";
    }
}

DWORD U2C_Word2Freq(BYTE word, BOOL isSpi)
{
    switch(word)
    {
    case U2C_SPI_FREQ_200KHZ: return isSpi ? 200000 : 400000;
    case U2C_SPI_FREQ_100KHZ: return 100000;
    case U2C_SPI_FREQ_83KHZ:  return  83000;
    case U2C_SPI_FREQ_71KHZ:  return  71000;
    case U2C_SPI_FREQ_62KHZ:  return  62000;
    case U2C_SPI_FREQ_50KHZ:  return  50000;
    case U2C_SPI_FREQ_25KHZ:  return  25000;
    case U2C_SPI_FREQ_10KHZ:  return  10000;
    case U2C_SPI_FREQ_5KHZ:   return   5000;
    case U2C_SPI_FREQ_2KHZ:   return   2000;
    default: return 0;
    }
}

BYTE U2C_Freq2Word(DWORD freq)
{

    if (freq < 5000)
        return U2C_SPI_FREQ_2KHZ;
    else if (freq < 10000)
        return U2C_SPI_FREQ_5KHZ;
    else if (freq < 25000)
        return U2C_SPI_FREQ_10KHZ;
    else if (freq < 50000)
        return U2C_SPI_FREQ_25KHZ;
    else if (freq < 62000)
        return U2C_SPI_FREQ_50KHZ;
    else if (freq < 71000)
        return U2C_SPI_FREQ_62KHZ;
    else if (freq < 83000)
        return U2C_SPI_FREQ_71KHZ;
    else if (freq < 100000)
        return U2C_SPI_FREQ_83KHZ;
    else if (freq < 200000)
        return U2C_SPI_FREQ_100KHZ;
    else
        return U2C_SPI_FREQ_200KHZ;
}

// I2C bus frequency values:
#define U2C_I2C_FREQ_FAST   0
#define U2C_I2C_FREQ_STD    1
#define U2C_I2C_FREQ_83KHZ  2
#define U2C_I2C_FREQ_71KHZ  3
#define U2C_I2C_FREQ_62KHZ  4
#define U2C_I2C_FREQ_50KHZ  6
#define U2C_I2C_FREQ_25KHZ  16
#define U2C_I2C_FREQ_10KHZ  46
#define U2C_I2C_FREQ_5KHZ   96
#define U2C_I2C_FREQ_2KHZ   242


// SPI bus frequency values:
#define U2C_SPI_FREQ_200KHZ  0
#define U2C_SPI_FREQ_100KHZ  1
#define U2C_SPI_FREQ_83KHZ  2
#define U2C_SPI_FREQ_71KHZ  3
#define U2C_SPI_FREQ_62KHZ  4
#define U2C_SPI_FREQ_50KHZ  6
#define U2C_SPI_FREQ_25KHZ  16
#define U2C_SPI_FREQ_10KHZ  46
#define U2C_SPI_FREQ_5KHZ   96
#define U2C_SPI_FREQ_2KHZ   242

#endif // ENABLE_DIOLAN_SUPPORT
