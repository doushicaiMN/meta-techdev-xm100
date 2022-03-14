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
  \brief  Platform abstraction layer
*/

#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "spi_read_buffer.h"

#include "ubxmsg.h"

#ifdef ENABLE_AARDVARK_SUPPORT
# include "aardvark.h"
#endif //ENABLE_AARDVARK_SUPPORT

#ifdef WIN32
# include <windows.h>
# include <winsock.h>
# include <wininet.h>
# include <stdio.h>
# include <io.h>
# include <fcntl.h>
#else
# include <unistd.h>
# include <fcntl.h>
# include <stdlib.h>
# include <stdarg.h>

# if defined(__APPLE__)
#  include <termios.h>
# else
#  if defined(__CYGWIN__)
#   include <termio.h>
#  else
#   include <termio.h>
#  endif
# endif

# include <sys/time.h>
# include <sys/fcntl.h>
# include <sys/file.h>
# include <sys/stat.h>
# include <sys/socket.h>
# include <sys/select.h>
# include <sys/types.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <signal.h>
# include <errno.h>
# include <unistd.h>
#endif

#ifdef ENABLE_DIOLAN_SUPPORT
#include "u2cbridge.h"    // Diolan support
#endif //ENABLE_DIOLAN_SUPPORT


#ifdef ENABLE_NET_SUPPORT
# ifndef WIN32
#  define SOCKADDR struct sockaddr                  //!< cross-platform compatibility
#  define SOCKADDR_IN struct sockaddr_in            //!< cross-platform compatibility
#  define HOSTENT struct hostent                    //!< cross-platform compatibility
#  define FDSET  fd_set                             //!< cross-platform compatibility
#  define TIMEVAL struct timeval                    //!< cross-platform compatibility
#  define closesocket(s) close(s)                   //!< cross-platform compatibility
#  define LAST_ERR() errno                          //!< cross-platform compatibility
#  define LAST_NET_ERR() errno                      //!< cross-platform compatibility
# else
#  define FDSET FD_SET                       //!< cross-platform compatibility
#  define LAST_ERR()     GetLastError()      //!< cross-platform compatibility
#  define LAST_NET_ERR() WSAGetLastError()   //!< cross-platform compatibility
# endif
#endif


#define DEFAULT_I2C_ADDR    0x42             //!< default slave address
#define I2C_SLEEPTIME        50              //!< time to sleep on full rx buffer to give msgpp time to consume payload

//#define AARDVARK_DEBUG_PIN                 //!< set a GPIO pin on error



int verbose = 0;




//=====================================================================
// TIME FUNCTIONS
//=====================================================================

void TIME_SLEEP(U4 ms)
{
#ifdef WIN32
    Sleep(ms);
#else
    usleep(1000*ms);
#endif
}

U4 TIME_GET(void)
{
#ifdef WIN32
    return GetTickCount();
#else
    struct timeval tv;
    return (gettimeofday(&tv,NULL)==0) ?
        (U4)(tv.tv_sec*1000 + tv.tv_usec/1000) : 0;
#endif
}

//=====================================================================
// COM PORT IO
//=====================================================================

//! Open a serial port
/*!
    \param name        Symbolic link name for port
    \return handle to port
*/
HANDLE COM_OPEN(const CH* name)
{
#ifdef WIN32
    HANDLE h;
    DCB dcb;
    COMMTIMEOUTS cto;
    h = CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        memset(&cto, 0, sizeof(cto));
        cto.ReadIntervalTimeout = MAXDWORD;
        memset(&dcb, 0, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);
        dcb.fDtrControl = 1;
        dcb.fRtsControl = 1;
        if (!SetupComm(h, 4096, 32768))
            MESSAGE(MSG_DBG, "SetupComm err=%d", GetLastError());
        if (!SetCommTimeouts(h, &cto))
            MESSAGE(MSG_DBG, "SetCommTimeouts err=%d", GetLastError());
        if (!BuildCommDCB("baud=9600 parity=N data=8 stop=1", &dcb))
            MESSAGE(MSG_DBG, "BuildCommDCB err=%d", GetLastError());
        if (!SetCommState(h, &dcb))
            MESSAGE(MSG_DBG, "SetCommState err=%d", GetLastError());
        return h;
    }
    return NULL;
#else
    // open the serial port with the NDELAY option
    // otherwise this may block forever because u-blox chips
    // don't support the DCD line
    int fd = open(name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0)
    {
        return (HANDLE)0;
    }

    // remove the NDELAY option again, it will be
    // handled on the lower layer when setting the baudrate
    fcntl(fd, F_SETFL, 0);
    return (HANDLE)fd;
#endif
}

//! set baudrate of serial COM port
/*!
    \param h    Handle to device
    \param br    Baudrate
    \return #TRUE if success, #FALSE else
*/
BOOL COM_BAUDRATE(HANDLE h, U4 br)
{
#ifdef WIN32
    DCB dcb;
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb))
    {
        MESSAGE(MSG_DBG, "GetCommState err=%d", GetLastError());
        return FALSE;
    }
    dcb.BaudRate = br;
    dcb.ByteSize = 8;
    dcb.fBinary  = TRUE;
    dcb.DCBlength = sizeof(dcb);
    if (!SetCommState(h, &dcb))
    {
        MESSAGE(MSG_DBG, "SetCommState err=%d", GetLastError());
        return FALSE;
    }
    return TRUE;
#else
    struct termios tp;
    int baudrate;

    switch(br)
    {
#if !defined(__APPLE__)
        case 921600:baudrate=B921600;break;
        case 460800:baudrate=B460800;break;
#endif
        case 230400:baudrate=B230400;break;
        case 115200:baudrate=B115200;break;
        case 57600: baudrate=B57600;break;
        case 38400: baudrate=B38400;break;
        case 19200: baudrate=B19200;break;
        case 9600:  baudrate=B9600;break;
        default:    baudrate=B9600;
    }

    tcgetattr((int)h, &tp);

    //cfmakeraw(&tp);
    tp.c_lflag = 0;
    tp.c_iflag = 0;
    tp.c_oflag = 0;
    tp.c_cflag &= ~(CSIZE|PARENB);
    tp.c_cflag |= CLOCAL | CS8;
    tp.c_cflag |= baudrate;
#ifdef UART_TWO_STOP_BITS
    tp.c_cflag |= CSTOPB;
#else
    tp.c_cflag &= ~CSTOPB;
#endif
    tp.c_cc[VMIN] = 0;
    tp.c_cc[VTIME] = 0;

    cfsetospeed(&tp, baudrate);
    cfsetispeed(&tp, baudrate);

    if (tcsetattr((int)h, TCSANOW, &tp) < 0)
    { // Apply change immediately
        return FALSE;
    }
    return TRUE;
#endif
}

//! close serial COM port
/*!
    \param h    Handle to device
*/
void COM_CLOSE(HANDLE h)
{
#ifdef WIN32
    CloseHandle(h);
#else
    close((int)h);
#endif
}

//! write data to device
/*!
    \param h    Handle to serial COM device
    \param p    Pointer to data to write
    \param size Size of data to write
    \return number of bytes written
*/
U4 COM_WRITE(HANDLE h, const void* p, U4 size)
{
#ifdef WIN32
    U4 dw;
    if (!WriteFile(h, p, size, (LPDWORD)&dw, NULL))
    {
        MESSAGE(MSG_DBG, "WriteFile err=%d", GetLastError());
        return 0;
    }
    return dw;
#else
    I4 dw = write((int)h,p,size);
    //if (dw>0) dump_data("COM_WRITE ",(U1 *)p, dw);
    if ((U4)dw != size)
        MESSAGE(MSG_ERR, "can not write all data (req: %li, actual: %li)", size, dw);

    return dw>0?(U4)dw:0;
#endif
}

#ifdef WIN32
//! write data to stdout
/*!
    \param p    Pointer to data to write
    \param size Size of data to write
    \return number of bytes written
*/
U4 STDOUT_WRITE(const void* p, U4 size)
{
    I4 dw = write(1,p,size);
    if ((U4)dw != size)
        MESSAGE(MSG_ERR, "can not write all data (req: %li, actual: %li)", size, dw);

    return dw>0?(U4)dw:0;
}
#endif //ifdef WIN32

//! Read data from device
/*!
    \param h    Handle to serial COM device
    \param p    Pointer to user-allocated buffer of at least \a size size
    \param size Number of bytes to read from device
    \return number of bytes which were read from device
*/
U4 COM_READ(HANDLE h, void* p, U4 size)
{
#ifdef WIN32
    U4 dwRead = 0;
    if (!ReadFile(h, p, size, (LPDWORD)&dwRead, NULL))
    {
        MESSAGE(MSG_DBG, "ReadFile err=%d", GetLastError());
        return 0;
    }
    return dwRead;
#else
    I4 dw = read((int)h,p,size);
    //if (dw>0) dump_data("COM_READ ",(U1 *)p, dw);

    return dw>0 ? (U4)dw : 0;
#endif
}

#ifdef WIN32
//! Read data from stdin
/*!
    \param p    Pointer to user-allocated buffer of at least \a size size
    \param size Number of bytes to read from device
    \return number of bytes which were read from device
*/
U4 STDIN_READ(void* p, U4 size)
{
    DWORD dw = 0;
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    ReadFile(hStdIn, p, size, &dw, NULL);

    return dw>0 ? (U4)dw : 0;
}
#endif //ifdef WIN32

#ifdef WIN32
void STDIN_SETBAUD(int br)
{
    MESSAGE_PLAIN("<AC>Setbaud %i<\\AC>", br);
}
#endif //ifdef WIN32

//! discard all characters from the output or input buffer
/*!
    \param h    Handle to the serial COM port device
*/
void COM_CLEAR(HANDLE h)
{
#ifdef WIN32
    U4 dw;
    U1 buf[256];
    if (!PurgeComm(h, PURGE_RXCLEAR))
        MESSAGE(MSG_DBG, "PurgeComm err=%d", GetLastError());
    while (ReadFile(h, buf, sizeof(buf), (LPDWORD)&dw, NULL) && (dw == sizeof(buf)))
        /*nop*/;
#else
    tcflush((int)h,TCIFLUSH);
#endif
}

//! clears the buffers for the specified file and causes all buffered data to be written to the file
/*!
    \param h    Handle to the serial COM port device
*/
void COM_FLUSH(HANDLE h)
{
#ifdef WIN32
    if (!FlushFileBuffers(h))
        MESSAGE(MSG_DBG, "FlushFileBuffers err=%d", GetLastError());
#else
    ((void)h);
    MESSAGE(MSG_WARN, "COM_FLUSH not implemented.");
#endif
}


//=====================================================================
// DIOLAN I2C PORT IO
//=====================================================================
#ifdef ENABLE_DIOLAN_SUPPORT

//! open I2C port
/*!
    \param name     name of the I2C port ("I2C0:0x42", "I2C1:0x42", ...)
    \param pAddr    pointer to receive i2c address
    \return handle to the device
*/
HANDLE U2C_OPEN(const CH* name, int *pAddr)
{
    int devIndex = atoi(name+3);
    int i2cAddr;
    // find device index and i2c slave address
    int parsedFields = sscanf(name+3,"%d:0x%x",&devIndex,&i2cAddr);
    // assign defaults
    if (parsedFields < 2)
    {
        i2cAddr = DEFAULT_I2C_ADDR;
    }
    if (parsedFields < 1)
    {
        devIndex = 0;
    }
    // get access to Diolan
    HANDLE dev = U2C_OpenDevice(devIndex);

    MESSAGE(MSG_DBG,"Diolan port %d I2C address 0x%x",devIndex,i2cAddr);

    // bail on invalid dev handle
    U2C_RESULT res = U2C_IsHandleValid(dev);
    if (res != U2C_SUCCESS)
    {
        MESSAGE(MSG_ERR, "device handle not valid: %d", res);
        *pAddr = 0;
        return (HANDLE)NULL;
    }

    *pAddr = i2cAddr;

    // enable I2C mode
    U2C_SetClockSynch(dev, TRUE);
    return (HANDLE)dev;
}

//! close I2C port
/*!
    \param h handle to device
*/
void U2C_CLOSE(HANDLE h)
{
    U2C_CloseDevice(h);
}

//! set baudrate of I2C device
/*!
    \param h    handle to device
    \param br   baudrate to set
    \return #TRUE
*/
BOOL U2C_BAUDRATE(HANDLE h,
                  U4     br)
{
    BYTE freqWord = U2C_Freq2Word(br);
    U2C_RESULT res = U2C_SetI2cFreq(h, freqWord);
    if (res != U2C_SUCCESS)
    {
        MESSAGE(MSG_ERR, "U2C set freq: %s (%d)", U2C_StatusString(res), res);
        return FALSE;
    }
    //U2C_GetI2cFreq(h, &freqWord); // documented but unresolved symbol
    U4 br_set = U2C_Word2Freq(freqWord, FALSE);
    MESSAGE(MSG_DBG, "Baudrate %dkHz",br_set/1000);
    return TRUE;
}

//! write data to I2C port
/*!
    \param h                handle to device
    \param deviceAddress    address of the I2C device
    \param p                pointer to data to write
    \param size             size of data to write
    \return number of bytes written
*/
U4 U2C_WRITE(HANDLE h, int deviceAddress, const void* p, U4 size)
{
    U4 writtenBytes = 0;
    U2C_TRANSACTION trans;
    trans.nMemoryAddress = 0;
    trans.nMemoryAddressLength = 0;
    trans.nSlaveDeviceAddress = deviceAddress;

    while (size)
    {
        U2 sizeTrans = (U2)MIN(size, sizeof(trans.Buffer));
        trans.nBufferLength = sizeTrans;
        memcpy(trans.Buffer,p,trans.nBufferLength);
        U2C_RESULT res = U2C_Write(h,&trans);
        if (res != U2C_SUCCESS)
        {
            MESSAGE(MSG_ERR, "U2C write failed: %s (%d)", U2C_StatusString(res), res);
            break;
        }
        writtenBytes += sizeTrans;
        size -= sizeTrans;
        p = (const void*)((U1*)p+sizeTrans);
    }
    return (U4)writtenBytes;
}

//! read number of pending bytes from I2C port
/*!
    \param h                handle to device
    \param deviceAddress    address of the I2C device
    \return number of bytes pending
*/
U4 U2C_PENDING(HANDLE h, int deviceAddress)
{
    // set address
    const BYTE sizeAddr = 0xFD;
    U2C_TRANSACTION trans;
    trans.nBufferLength = 2;
    trans.nMemoryAddress = sizeAddr;
    trans.nMemoryAddressLength = 1;
    trans.nSlaveDeviceAddress = deviceAddress;

    U2C_RESULT res = U2C_Read(h, &trans);
    if (res != U2C_SUCCESS)
    {
        MESSAGE(MSG_ERR, "U2C read size err: %s (%d)", U2C_StatusString(res), res);
        return 0;
    }
    U2 bytes = (trans.Buffer[0]<<8) + (trans.Buffer[1]<<0);
    return bytes;
}

//! read data from I2C port
/*!
    \param h                handle to device
    \param deviceAddress    address of the I2C device
    \param p                pointer to user-allocated buffer of at least \a size size to receive data
    \param size             number of bytes to read
    \return number of bytes read
*/
U4 U2C_READ(HANDLE h, int deviceAddress, void* p, U4 size)
{
    U2C_TRANSACTION trans;
    U2 bytes = (U2)U2C_PENDING(h, deviceAddress);

    U2 readBytes = (U2)MIN(MIN(size,bytes),sizeof(trans.Buffer));
    if (readBytes)
    {
        trans.nMemoryAddress = 0;
        trans.nMemoryAddressLength = 0;
        trans.nBufferLength = readBytes;
        trans.nSlaveDeviceAddress = deviceAddress;
        U2C_RESULT res = U2C_Read(h, &trans);
        if (res != U2C_SUCCESS)
        {
            MESSAGE(MSG_ERR, "U2C read err: %s (%d)", U2C_StatusString(res), res);
            return 0;
        }
        memcpy(p, trans.Buffer, readBytes);
    }
    return (U4)readBytes;
}


//=====================================================================
// DIOLAN SPI PORT IO
//=====================================================================

//! open SPI port
/*!
    \param name     name of the SPI port ("SPU0", "SPU1", ...)
    \param pData    pointer to receive read buffer memory this function will allocate
    \return handle to the device
*/
HANDLE SPU_OPEN(const CH* name, void **pData)
{
    int devIndex;
    // find device index
    int parsedFields = sscanf(name+3,"%d",&devIndex);
    // assign defaults
    if (parsedFields < 1)
    {
        devIndex = 0;
    }

    *pData = malloc(sizeof(SPI_READBUFFER_t));
    if (!*pData)
    {
        MESSAGE(MSG_ERR,"Could not get memory for read buffer");
        return (HANDLE)NULL;
    }
    memset(*pData,0,sizeof(SPI_READBUFFER_t));
    SPI_READBUFFER_pt pBuf = (SPI_READBUFFER_pt)(*pData);
    pBuf->pBuffer = pBuf->buffer;
    pBuf->size = 0;

    HANDLE dev = U2C_OpenDevice(devIndex);

    MESSAGE(MSG_DBG,"Diolan port %d SPI ",devIndex);

    // bail on invalid dev handle
    U2C_RESULT res = U2C_IsHandleValid(dev);
    if (res != U2C_SUCCESS)
    {
        MESSAGE(MSG_ERR, "device handle not valid: %d", res);
        return (HANDLE)NULL;
    }

    U2C_SetIoDirection(dev, 1<<5, 1<<5);
    U2C_SingleIoWrite(dev, 5, TRUE);

    //res = U2C_SpiConfigSS(dev,
    return dev;
}

//! close SPI port
/*!
    \param h handle to device
*/
void SPU_CLOSE(HANDLE h)
{
    U2C_CloseDevice(h);
}

//! set baudrate of SPI device
/*!
    \param h    handle to device
    \param br   baudrate to set
    \return #TRUE
*/
BOOL SPU_BAUDRATE(HANDLE h,
                  U4     br)
{
    BYTE freqWord = U2C_Freq2Word(br);
    U2C_RESULT res = U2C_SpiSetFreq(h, freqWord);
    if (res != U2C_SUCCESS)
    {
        MESSAGE(MSG_ERR, "SPI set freq: %s (%d)", U2C_StatusString(res), res);
        return FALSE;
    }
    //U2C_SpiGetFreq(h, &freqWord);    // documented but unresolved symbol
    U4 br_set = U2C_Word2Freq(freqWord, FALSE);
    MESSAGE(MSG_DBG, "Baudrate %dkHz",br_set/1000);
    return TRUE;
}

//! write data to SPI port
/*!
    \param h        handle to device
    \param pReadBuf pointer to device buffer structure
    \param p        pointer to data to write
    \param size     size of data to write
    \return number of bytes written
*/
U4 SPU_WRITE(HANDLE h, SPI_READBUFFER_pt pReadBuf, const void* p, U4 size)
{
    // manage read buffer. as SPI always performs a read access and a write access at the
    //  same time, we temporarily store the data read and return it on an SPI_READ call
    U4 room = sizeof(pReadBuf->buffer)-(pReadBuf->size);
    U4 transfSize = MIN(room, size);
    if (!transfSize)
    {
        return 0;
    }

    U2C_SingleIoWrite(h, 5, FALSE); // slave select
    U4 alreadyTransferred = 0;
    while (transfSize)
    {
        // Diolan allows single-transfers of no more than 256 byte
        U2 thisSize = (U2)MIN(transfSize, 256);

        // perform write/read
        U2C_RESULT res = U2C_SpiReadWrite(h,            // handle
            (U1*)p+alreadyTransferred,                  // write buffer
            (BYTE*)pReadBuf->pBuffer + pReadBuf->size,  // read buffer pointer
            (U2)thisSize);                              // r/w size

        if (res != U2C_SUCCESS)
        {
            MESSAGE(MSG_ERR, "SPI write data: %s (%d)", U2C_StatusString(res), res);
            U2C_SingleIoWrite(h,5,TRUE); // slave deselect
            return alreadyTransferred;
        }

        pReadBuf->size     += thisSize;
        alreadyTransferred += thisSize;
        transfSize         -= thisSize;

    }
    U2C_SingleIoWrite(h, 5, TRUE); // slave deselect
    return (U4)size;
}

#define COMPSZ 50 //!< number of subsequent bytes at start of reception buffer compared for equality with 0xFF
//! read data from SPI port
/*!
    \param h        handle to device
    \param pReadBuf pointer to device buffer structure
    \param p        pointer to user-allocated buffer of at least \a size size to receive data
    \param size     number of bytes to read
    \return number of bytes read
*/
U4 SPU_READ(HANDLE h, SPI_READBUFFER_pt pReadBuf, void* p, U4 size)
{
    if (pReadBuf->size)
    {
        // we have data buffered from last write access
        size = MIN(pReadBuf->size, size);
        memcpy(p, pReadBuf->pBuffer,size);
        pReadBuf->pBuffer += size;
        pReadBuf->size    -= size;
        memmove(pReadBuf->buffer, pReadBuf->pBuffer, pReadBuf->size);
        pReadBuf->pBuffer = pReadBuf->buffer;
    }
    else
    {
        // there's no buffered data -> perform a read with dummy write
        // setup a dummy FF buffer to perform dummy write
        U1 tmpBuf[COMPSZ];
        memset(tmpBuf, 0xFF, sizeof(tmpBuf));
        size = MIN(MIN(256, size), sizeof(tmpBuf));
        U2 size1 = (U2)(MIN(size, COMPSZ));

        // first read some bytes and check if they are all FF
        // perform read
        U2C_SingleIoWrite(h, 5, FALSE); // slave select
        U2C_RESULT res = U2C_SpiRead(h, (U1*)p, size1);
        U2C_SingleIoWrite(h, 5, TRUE); // slave deselect
        if (res != U2C_SUCCESS)
        {
            MESSAGE(MSG_ERR, "SPU read data1: %s (%d)", U2C_StatusString(res), res);
            return 0;
        }

        if (memcmp(p,tmpBuf,size1) != 0)
        {
            if (size > size1)
            {
                // some bytes are not FF, so we have data. the UPD messages are short,
                // so perform only short read accesses. the longer messages are normally fetched
                // as a side effect of the longer write access when writing packets
                p     = (void*)((U1*)p+size1);
                U2C_SingleIoWrite(h,5,FALSE); // slave select
                U2C_RESULT res = U2C_SpiRead(h, (U1*)p, (U2)(size-size1));
                U2C_SingleIoWrite(h,5,TRUE); // slave deselect
                if (res != U2C_SUCCESS)
                {
                    MESSAGE(MSG_ERR, "SPI read data2: %s (%d)", U2C_StatusString(res), res);
                    return size1;
                }
            }
        }
        else
        {
            // we received only FFs -> discard
            return 0;
        }
    }

    return size;
}

#endif //ENABLE_DIOLAN_SUPPORT

//=====================================================================
// AARDVARK I2C PORT IO
//=====================================================================

#ifdef ENABLE_AARDVARK_SUPPORT

//! open I2C port
/*!
    \param name     name of the I2C port ("I2C0:0x42", "I2C1:0x42", ...)
    \param pAddr    pointer to receive i2c address
    \return handle to the device
*/
HANDLE I2C_OPEN(const CH* name, int *pAddr)
{
    int devIndex = atoi(name+3);
    int i2cAddr;
    // find device index and i2c slave address
    int parsedFields = sscanf(name+3,"%d:0x%x",&devIndex,&i2cAddr);
    // assign defaults
    if (parsedFields < 2)
    {
        i2cAddr = DEFAULT_I2C_ADDR;
    }
    if (parsedFields < 1)
    {
        devIndex = 0;
    }
    // get access to aardvark
    Aardvark dev = aa_open(devIndex);

    MESSAGE(MSG_DBG,"aardvark port %d I2C address 0x%x",devIndex,i2cAddr);

    // bail on invalid dev handle
    if ((int)dev <= 0)
    {
        MESSAGE(MSG_ERR, "%s", aa_status_string((int)dev));
        *pAddr = 0;
        return (HANDLE)0;
    }

    *pAddr = i2cAddr;
    // enable I2C mode
    aa_configure(dev,  AA_CONFIG_GPIO_I2C);
    // enable pullups
    aa_i2c_pullup(dev, AA_I2C_PULLUP_BOTH);
    return (HANDLE)dev;
}

//! close I2C port
/*!
    \param h    handle to device
*/
void I2C_CLOSE(HANDLE h)
{
    aa_close((Aardvark)h);
}

//! set baudrate of I2C device
/*!
    \param h    handle to device
    \param br   baudrate to set
    \return #TRUE
*/
BOOL I2C_BAUDRATE(HANDLE h,
                  U4     br)
{
    br = aa_i2c_bitrate((Aardvark)h,br/1000);
    MESSAGE(MSG_DBG, "Baudrate %dkHz",br);
    return TRUE;
}

//! write data to I2C port
/*!
    \param h                handle to device
    \param deviceAddress    address of the I2C device
    \param p                pointer to data to write
    \param size             size of data to write
    \return number of bytes written
*/
U4 I2C_WRITE(HANDLE h, int deviceAddress, const void* p, U4 size)
{
    u16 writtenBytes;
    int status = aa_i2c_write_ext((Aardvark)h,(u16)deviceAddress,AA_I2C_NO_FLAGS,(u16)size,(u08*)p,&writtenBytes);
    if (status < 0)
    {
        MESSAGE(MSG_ERR, "I2C_WRITE write data:%s (%d)", aa_status_string((int)status),status);
        return 0;
    }
    if (writtenBytes < size)
    {
        // rxbuffer seems to be full
        //MESSAGE(MSG_DBG,"wait",size);
        aa_sleep_ms(I2C_SLEEPTIME);
    }
    return (U4)writtenBytes;
}

//! get status string from aardvark i2c status
/*!
    \param status   status identifier
    \return string containing status text
*/
const CH* I2C_STATUS_STR(int status)
{
    switch (status)
    {
    case AA_I2C_STATUS_BUS_ERROR:
        return "bus error";
    case AA_I2C_STATUS_SLA_ACK:
        return "arbitration lost";
    case AA_I2C_STATUS_SLA_NACK:
        return "slave addr NACK";
    case AA_I2C_STATUS_DATA_NACK:
        return "data NACK";
    case AA_I2C_STATUS_ARB_LOST:
        return "ARB lost";
    case AA_I2C_STATUS_BUS_LOCKED:
        return "BUS locked";
    case AA_I2C_STATUS_LAST_DATA_ACK:
        return "last data ACK";
    default:
        return "unknown";
    }
}

//! read number of pending bytes from I2C port
/*!
    \param h                handle to device
    \param deviceAddress    address of the I2C device
    \return number of bytes pending
*/
U4 I2C_PENDING(HANDLE h, int deviceAddress)
{
    // set address
    const u08 sizeAddr = 0xFD;
    u16 writtenBytes = 0;
    int status = aa_i2c_write_ext((Aardvark)h,(u16)deviceAddress,AA_I2C_NO_STOP,1,&sizeAddr,&writtenBytes);
    if (status != 0)
    {
#ifdef AARDVARK_DEBUG_PIN
        aa_gpio_set((Aardvark)h, AA_GPIO_MISO);
        aa_gpio_direction((Aardvark)h, AA_GPIO_MISO);
        TIME_SLEEP(5);
        aa_gpio_set((Aardvark)h, 0);
#endif //AARDVARK_DEBUG_PIN
        const char * status_string = aa_status_string((int)status);
        if (!status_string) status_string = I2C_STATUS_STR(status);
        MESSAGE(MSG_ERR, "I2C_PENDING write ext addr: %s (%d)", status_string, status);
        status = aa_i2c_free_bus((Aardvark)h);
        if (status != 0)
        {
            status_string = aa_status_string((int)status);
            if (!status_string) status_string = I2C_STATUS_STR(status);
            MESSAGE(MSG_ERR, "I2C_PENDING free bus: %s (%d)", status_string, status);
        }
        return 0;
    }
    // read length
    u16 readBytes  = 0;
    u08 bytesAvail[2] = {0};
    status = aa_i2c_read_ext((Aardvark)h,(u16)deviceAddress,AA_I2C_NO_FLAGS,(u16)2,bytesAvail,&readBytes);
    if (status != 0)
    {
#ifdef AARDVARK_DEBUG_PIN
        aa_gpio_set((Aardvark)h, AA_GPIO_MISO);
        aa_gpio_direction((Aardvark)h, AA_GPIO_MISO);
        TIME_SLEEP(10);
        aa_gpio_set((Aardvark)h, 0);
#endif //AARDVARK_DEBUG_PIN
        const char * status_string = aa_status_string((int)status);
        if (!status_string) status_string = I2C_STATUS_STR(status);
        MESSAGE(MSG_ERR, "I2C_PENDING read data: %s (%d)", status_string, status);
        return 0;
    }
    // minimum of both lengths
    u16 length = (bytesAvail[0]<<8) + bytesAvail[1];
    return length;
}

//! read data from I2C port
/*!
    \param h                handle to device
    \param deviceAddress    address of the I2C device
    \param p                pointer to user-allocated buffer of at least \a size size to receive data
    \param size             number of bytes to read
    \return number of bytes read
*/
U4 I2C_READ(HANDLE h, int deviceAddress, void* p, U4 size)
{
    u16 length = (u16)I2C_PENDING(h, deviceAddress);
    u16 readBytes = 0;
    length = length > size ? (u16)size : length;
    // read data
    if (length)
    {
        int status = aa_i2c_read_ext((Aardvark)h,(u16)deviceAddress,AA_I2C_NO_FLAGS,length,(u08*)p,&readBytes);
        if (status != 0)
        {
#ifdef AARDVARK_DEBUG_PIN
            aa_gpio_set((Aardvark)h, AA_GPIO_MISO);
            aa_gpio_direction((Aardvark)h, AA_GPIO_MISO);
            TIME_SLEEP(20);
            aa_gpio_set((Aardvark)h, 0);
#endif //AARDVARK_DEBUG_PIN
            const char * status_string = aa_status_string((int)status);
            if (!status_string)    status_string = I2C_STATUS_STR(status);
            MESSAGE(MSG_ERR, "I2C_READ read data: %s (%d)", status_string, status);
            return 0;
        }
    }
    else
    {
        aa_i2c_free_bus((Aardvark)h);
        readBytes = 0;
    }
    return (U4)readBytes;
}

#endif //ENABLE_AARDVARK_SUPPORT


#ifdef ENABLE_AARDVARK_SUPPORT
//=====================================================================
// AARDVARK SPI PORT IO
//=====================================================================

//! open SPI port
/*!
    \param name     name of the SPI port ("SPI0", "SPI1", ...)
    \param pData    pointer to receive read buffer memory this function will allocate
    \return handle to the device
*/
HANDLE SPI_OPEN(const CH* name, void **pData)
{
    int devIndex;
    // find device index
    int parsedFields = sscanf(name+3,"%d",&devIndex);
    // assign defaults
    if (parsedFields < 1)
    {
        devIndex = 0;
    }
    // get access to aardvark
    Aardvark dev = aa_open(devIndex);

    MESSAGE(MSG_DBG,"aardvark port %d SPI ",devIndex);

    // bail on invalid dev handle
    if ((int)dev < 0)
    {
        MESSAGE(MSG_ERR, "%s", aa_status_string((int)dev));
        *pData = 0;
        return (HANDLE)0;
    }

    // get memory for read buffer
    *pData = malloc(sizeof(SPI_READBUFFER_t));
    if (!*pData)
    {
        MESSAGE(MSG_ERR,"Could not get memory for read buffer");
        return (HANDLE)0;
    }
    memset(*pData,0,sizeof(SPI_READBUFFER_t));
    SPI_READBUFFER_pt pBuf = (SPI_READBUFFER_pt)(*pData);
    pBuf->pBuffer = pBuf->buffer;
    pBuf->size = 0;

    // enable SPI mode
    aa_configure(dev,  AA_CONFIG_SPI_GPIO);
    // configure
    aa_spi_configure(dev, AA_SPI_POL_RISING_FALLING, AA_SPI_PHASE_SAMPLE_SETUP, AA_SPI_BITORDER_MSB);
    aa_spi_master_ss_polarity(dev, AA_SPI_SS_ACTIVE_LOW);
    return (HANDLE)dev;
}

//! close SPI port
/*!
    \param h handle to device
*/
void SPI_CLOSE(HANDLE h)
{
    aa_close((Aardvark)h);
}

//! set baudrate of SPI device
/*!
    \param h    handle to device
    \param br   baudrate to set
    \return #TRUE
*/
BOOL SPI_BAUDRATE(HANDLE h,
                  U4     br)
{
    br = aa_spi_bitrate((Aardvark)h,br/1000);
    MESSAGE(MSG_DBG, "Baudrate %dkHz",br);
    return TRUE;
}

//! write data to SPI port
/*!
    \param h        handle to device
    \param pReadBuf pointer to device buffer structure
    \param p        pointer to data to write
    \param size     size of data to write
    \return number of bytes written
*/
U4 SPI_WRITE(HANDLE h, SPI_READBUFFER_pt pReadBuf, const void* p, U4 size)
{
    // manage read buffer. as SPI always performs a read access and a write access at the
    //  same time, we temporarily store the data read and return it on an SPI_READ call
    U4 room = sizeof(pReadBuf->buffer)-(pReadBuf->size);
    size = MIN(room, size);

    // perform write/read
    int status = aa_spi_write((Aardvark)h,(u16)size,(u08*)p,(u16)size,(u08*)(pReadBuf->pBuffer + pReadBuf->size));
    if (status < 0)
    {
        MESSAGE(MSG_ERR, "SPI_WRITE write data:%s (%d)", aa_status_string((int)status),status);
        return 0;
    }

    pReadBuf->size += size;
    return size;
}

#define COMPSZ 50 //!< number of subsequent bytes at start of reception buffer compared for equality with 0xFF
//! read data from SPI port
/*!
    \param h        handle to device
    \param pReadBuf pointer to device buffer structure
    \param p        pointer to user-allocated buffer of at least \a size size to receive data
    \param size     number of bytes to read
    \return number of bytes read
*/
U4 SPI_READ(HANDLE h, SPI_READBUFFER_pt pReadBuf, void* p, U4 size)
{
    if (pReadBuf->size)
    { // we have data buffered from last write access
        size = MIN(pReadBuf->size, size);
        memcpy(p, pReadBuf->pBuffer, size);
        pReadBuf->pBuffer += size;
        pReadBuf->size -= size;
        memmove(pReadBuf->buffer, pReadBuf->pBuffer, pReadBuf->size);
        pReadBuf->pBuffer = pReadBuf->buffer;
    }
    else
    {
        // there's no buffered data -> perform a read with dummy write
        // setup a dummy FF buffer to perform dummy write
        u08 tmpBuf[COMPSZ];
        memset(tmpBuf, 0xFF, sizeof(tmpBuf));
        size = MIN(size, sizeof(tmpBuf));
        U2 size1 = (U2)MIN(size, COMPSZ);

        // first read some bytes and check if they are all FF
        int written = aa_spi_write((Aardvark)h, size1, tmpBuf, size1, (u08*)p);
        if (written == AA_SPI_WRITE_ERROR)
        {
            MESSAGE(MSG_ERR, "SPI_READ read data1:%s (%d)", aa_status_string((int)written), written);
            return 0;
        }
        if (memcmp(p, tmpBuf, size1) != 0)
        {
            if (size > size1)
            {
                // some bytes are not FF, so we have data. the UPD messages are short,
                // so perform only short read accesses. the longer messages are normally fetched
                // as a side effect of the longer write access when writing packets
                p = (void*)((U1*)p + size1);
                int written2 = aa_spi_write((Aardvark)h, (u16)size - size1, tmpBuf, (u16)size - size1, (u08*)p);
                if (written2 == AA_SPI_WRITE_ERROR)
                {
                    MESSAGE(MSG_ERR, "SPI_READ read data2:%s (%d)", aa_status_string((int)written2), written2);
                    return size1;
                }
            }
        }
        else
        {
            // we received only FFs -> discard
            return 0;
        }
    }
    return size;
}

#endif // ENABLE_AARDVARK_SUPPORT


//=====================================================================
// NET PORT IO
//=====================================================================

#ifdef ENABLE_NET_SUPPORT

//! Open a network port
/*!
    \param name        name for port, format hostname:port
    \return handle to port
*/
HANDLE NET_OPEN(const CH* name)
{
    CH nameCopy[64];
    strncpy(nameCopy,name,sizeof(nameCopy)-1);
#ifdef WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(1,1),&wsaData))
    {
        MESSAGE(MSG_ERR,"WinSock not available");
        return 0;
    }
    else
    {
        if (LOBYTE(wsaData.wVersion) != 1 ||
            HIBYTE(wsaData.wVersion) != 1 )
        {
            MESSAGE(MSG_ERR,"no WinSock 1.1");
            WSACleanup();
            return 0;
        }
    }
#endif

    SOCKET sock = socket( AF_INET, SOCK_STREAM, 0 );
    if (sock == INVALID_SOCKET)
    {
        MESSAGE(MSG_ERR,"unable to create socket");
#ifdef WIN32
        WSACleanup();
#endif
        return (HANDLE)0;
    }
    CH* host    = nameCopy;
    CH* portStr = strchr(nameCopy,':');
    if(!portStr)
        return 0;
    *portStr = 0;
    portStr++;
    U portNum = atoi(portStr);

    SOCKADDR_IN sa;
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(portNum);
    HOSTENT *pHost = gethostbyname(host);
    if (pHost == NULL)
    {
        MESSAGE(MSG_ERR, "Cannot resolve hostname %s!", host);
        return (HANDLE)0;
    }
    struct in_addr* pHexAddress = (struct in_addr*)pHost->h_addr_list[ 0 ];
    char* dottedAddress = inet_ntoa( *pHexAddress );
    sa.sin_addr.s_addr = inet_addr( dottedAddress );

    MESSAGE(MSG_DBG,"opening port %d on host %s (%s)",portNum,host,dottedAddress);

    if (connect(sock,(const SOCKADDR*) &sa,
        sizeof( SOCKADDR_IN )) == SOCKET_ERROR)
    {
        MESSAGE(MSG_ERR,"connection to socket failed");
#ifdef WIN32
        WSACleanup();
#endif
        return (HANDLE)0;
    }

    return (HANDLE)sock;
}

//! Write data to net
/*!
    \param h    Handle to net socket
    \param p    Pointer to user-allocated buffer of at least \a size size
    \param size Number of bytes to write to device
    \return number of bytes which were written to device
*/
U4 NET_WRITE(HANDLE h, const void* p, U4 size)
{
    SOCKET sock = (SOCKET)h;
    int ret = send(sock,(const char*)p,size,0);
    if (ret == SOCKET_ERROR)
    {
        MESSAGE(MSG_ERR,"write to socket failed: %d",LAST_NET_ERR());
        return 0;
    }
    return (U4)ret;
}

//! Read data from net
/*!
    \param h    Handle to net socket
    \param p    Pointer to user-allocated buffer of at least \a size size
    \param size Number of bytes to read from device
    \return number of bytes which were read from device
*/
U4 NET_READ(HANDLE h, void* p, U4 size)
{
    SOCKET sock = (SOCKET)h;
    FDSET fds;
    FD_ZERO(&fds);
    FD_SET(sock,&fds);
    TIMEVAL tv = {0,0};
    int nReady = select(sock+1,&fds,NULL,NULL,&tv);
    if (nReady)
    {
        int ret = recv(sock,(char *)p,size,0);
        if (ret == SOCKET_ERROR)
        {
            MESSAGE(MSG_ERR,"read from socket failed: %d",LAST_NET_ERR());
            return 0;
        }
        return (U4)ret;
    }
    return 0;
}

//! close net socket
/*!
    \param h    Handle to device
*/
void NET_CLOSE(HANDLE h)
{
    SOCKET sock = (SOCKET)h;
    closesocket(sock);
#ifdef WIN32
    WSACleanup();
#endif
}

//! Set baudrate of Ethernet device
/*!
    \param h    handle to device
    \param br   baudrate to set
    \return #TRUE
*/
BOOL NET_BAUDRATE(SER_HANDLE_pt h,
                  U4     br)
{
    char serverName[64];
    strncpy(serverName,h->pName,sizeof(serverName)-1);
    char *pEnd = strchr(serverName,':');
    if (!pEnd)
    {
        MESSAGE(MSG_ERR,"failed extracting host name: %s",h->pName);
        return FALSE;
    }
    *pEnd = 0; // terminate
    int portNum = (atoi(++pEnd) % 100)-1; // only use the two least significant digits
    MESSAGE(MSG_DBG,"setting port %d on %s to %d baud",portNum,serverName,br);
    char requestStr[128];

    // prepare request URL to be sent to ttycat
    sprintf(requestStr,"/goform/setPortConfig?portNum=%d&baudRate=%u&portEnable=1&listenEnable=1",portNum,br);
    MESSAGE(MSG_DBG,"req: %s",requestStr);

    // prepare and connect socket to HTTP port of ttycat
    SOCKET sock = socket( AF_INET, SOCK_STREAM, 0 );
    if (sock == INVALID_SOCKET)
    {
        MESSAGE(MSG_ERR,"unable to create socket");
        return FALSE;
    }
    SOCKADDR_IN sa;
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(80);
    HOSTENT *pHost = gethostbyname(serverName);
    struct in_addr* pHexAddress = (struct in_addr*)pHost->h_addr_list[ 0 ];
    char* dottedAddress = inet_ntoa( *pHexAddress );
    sa.sin_addr.s_addr = inet_addr( dottedAddress );
    if (connect(sock,(const SOCKADDR*) &sa,
        sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
    {
        MESSAGE(MSG_ERR,"connection to socket failed");
        closesocket(sock);
        return FALSE;
    }

    // no construct HTTP request header
    char httpRequest[512];
    sprintf(httpRequest,
             "GET %s HTTP/1.1\r\n"
             "User-Agent: FWUpdateG5/linux\r\n"
             "Host: %s\r\n"
             "Connection: Keep-Alive\r\n"
             "Cache-Control: no-cache\r\n"
             "\r\n",requestStr,serverName);
    //MESSAGE(MSG_DBG,"request: %s",httpRequest);

    // send request
    int ret = send(sock,(const char*)httpRequest,strlen(httpRequest),0);
    if (ret == SOCKET_ERROR)
    {
        MESSAGE(MSG_ERR,"sending request failed");
        closesocket(sock);
        return FALSE;
    }

    // get response
    char httpResponse[512];
    ret = recv(sock,httpResponse,sizeof(httpResponse),0);
    if (ret == SOCKET_ERROR)
    {
        MESSAGE(MSG_ERR,"receiving response failed");
        closesocket(sock);
        return FALSE;
    }
    int status;
    if (sscanf(httpResponse,"HTTP/1.1 %d ",&status))
    {
        MESSAGE(MSG_DBG,"status: %d",status);
        if (status != 200 && status != 302) // comtrol devicemaster sends redirection
        {
            MESSAGE(MSG_ERR,"response returned failing status");
            closesocket(sock);
            return FALSE;
        }
    }
    //MESSAGE(MSG_DBG,"response: %s",httpResponse);
    closesocket(sock);
    return TRUE;
}

#endif //ENABLE_NET_SUPPORT




//=====================================================================
// GENERIC SERIAL IO
//=====================================================================


SER_HANDLE_pt SER_OPEN(const CH* name)
{
    SER_TYPE_t serType = COM;
    HANDLE     handle = (HANDLE)0;
    SER_HANDLE_pt pSerHandle;
    void *pData = NULL;
    int        addr = 0;

#ifdef ENABLE_DIOLAN_SUPPORT
    if (!handle && (strncmp(name, "U2C",3) == 0 || strncmp(name, "u2c",3) == 0))
    {
        serType = U2C;
        handle  = U2C_OPEN(name,&addr);
    }
    if (!handle && (strncmp(name, "SPU",3) == 0 || strncmp(name, "spu",3) == 0))
    {
        serType = SPU;
        handle  = SPU_OPEN(name,&pData);
    }
#endif // ENABLE_DIOLAN_SUPPORT
#ifdef ENABLE_AARDVARK_SUPPORT
    if (!handle && strncmp(name, "I2C", 3) == 0)
    {
        serType = I2C;
        handle = I2C_OPEN(name, &addr);
    }
    if (!handle && (strncmp(name, "SPI", 3) == 0 ||
                    strncmp(name, "spi", 3) == 0))
    {
        serType = SPI;
        handle = SPI_OPEN(name, &pData);
    }
#endif // ENABLE_AARDVARK_SUPPORT
#ifdef ENABLE_NET_SUPPORT
    if (!handle && strchr(name, ':'))
    {
        serType = NET;
        handle  = NET_OPEN(name);
    }
    else
#endif // ENABLE_NET_SUPPORT
    if (!handle)
    {
        serType = COM;
        handle  = COM_OPEN(name);
    }

#ifdef WIN32
    if (!handle && (strncmp(name, "STDIO", 5) == 0 || strncmp(name, "stdio", 5) == 0))
    {
        serType = STDINOUT;
        handle  = (HANDLE) -1; // Indicates stdin/stdout. Never used -> unique identifier
        // Make sure windows does not replace "\n" to "\r\n"
        if(_setmode(fileno(stdout), O_BINARY) == -1)
        {
            // In case of error abort here
            handle = (HANDLE)0;
        }
    }
#endif //ifdef WIN32


    if (handle)
    {
        pSerHandle = (SER_HANDLE_pt)malloc(sizeof(SER_HANDLE_t));
        if (pSerHandle)
        {
            pSerHandle->handle   = handle;  // device handle
            pSerHandle->type     = serType; // device type
            pSerHandle->devAddr  = addr;    // device address
            pSerHandle->pData    = pData;   // custom data pointer
            pSerHandle->pName    = name;    // backup name
            pSerHandle->baudrate = 0;       // baudrate not set yet
            return pSerHandle;
        }
    }
    return NULL;
}

BOOL SER_REENUM(SER_HANDLE_pt h, BOOL IsUsb)
{
    MESSAGE(MSG_DBG, "Re-enumerating...");
    switch(h->type)
    {
    case COM:
        {
            U4 retries = 10;
            COM_CLOSE(h->handle);
            do
            {
                TIME_SLEEP(1000);
                h->handle = COM_OPEN(h->pName);
                if (h->handle)
                {
                    return COM_BAUDRATE(h->handle, h->baudrate);
                }
            }
            while ((h->handle == (HANDLE)0) && retries--);
        }
        break;

#ifdef WIN32
    case STDINOUT:
        MESSAGE_PLAIN("<AC>Reenum %i<\\AC>", IsUsb);
        TIME_SLEEP(1000);   // Pause to allow host to perform operation
        return TRUE;
#endif //ifdef WIN32

#ifdef ENABLE_AARDVARK_SUPPORT
    case I2C:
        TIME_SLEEP(500);
        return I2C_BAUDRATE(h->handle,h->baudrate);
    case SPI:
        TIME_SLEEP(500);
        return SPI_BAUDRATE(h->handle,h->baudrate);
#endif // ENABLE_AARDVARK_SUPPORT
#ifdef ENABLE_DIOLAN_SUPPORT
    case U2C:
        TIME_SLEEP(500);
        return U2C_BAUDRATE(h->handle,h->baudrate);
    case SPU:
        TIME_SLEEP(500);
        return SPU_BAUDRATE(h->handle,h->baudrate);
#endif // ENABLE_DIOLAN_SUPPORT
#ifdef ENABLE_NET_SUPPORT
    case NET:
        TIME_SLEEP(500);
        return NET_BAUDRATE(h,h->baudrate);
#endif // ENABLE_NET_SUPPORT
    default:
        break;
    }
    return FALSE;
}

U4 SER_WRITE_SPI(SER_HANDLE_pt h,
                 const void *p,
                 U4         size)
{
    // get the header of the UBX message
    UBX_HEAD_pt pUbxHdr = (UBX_HEAD_pt)p;

    // check for UPD-FLWRI message
    if(pUbxHdr->classId == UBX_CLASS_UPD && pUbxHdr->msgId == UBX_UPD_FLWRI)
    {
        // content of UPD-FLWRI message
        U4 targetAddress = 0;    // address where to write the data to
        U4 writeSize = 0;        // length of the payload to be written

        U4 startOfData = 0;      // where the data section starts
        U4 pos = 0;              // current position inside the complete packet
        U1 *pData = NULL;        // pointer to data section of packet
        U1 totalFF = 8;          // how many 0xFF in success are allowed
        U curSearch = 0;         // how man 0xFF have been already found

        // get the content of the UPD-FLWRI message
        memcpy(&targetAddress, (U4 *)((U1 *)p+sizeof(UBX_HEAD_t)), sizeof(targetAddress));
        memcpy(&writeSize,     (U4 *)((U1 *)p+sizeof(UBX_HEAD_t)+sizeof(targetAddress)), sizeof(writeSize));

        // set the pointer to the data
        pData = (U1 *)p + sizeof(UBX_HEAD_t) + sizeof(targetAddress) + sizeof(writeSize);

        while(pos < writeSize)
        {
            // get the current character
            U1 current = pData[pos];

            // compare to 0xFF
            if(current == 0xFF)
            {
                // increment the 0xFF counter
                curSearch++;
            }
            // something else found
            else
            {
                // check if already more 0xFF have passed than allowed
                if(curSearch > totalFF)
                {
                    // check if anything has to be written
                    if(pos-startOfData-curSearch)
                    {
                        // calculate the new write address and the new size of the payload
                        U4 writeAddress = targetAddress+startOfData;
                        U4 newWriteSize    = pos-startOfData-curSearch;

                        CH *pMessage=NULL;
                        CH *pSendData = (CH*)malloc(newWriteSize+8);
                        if(!pSendData)
                            return 0;
                        size_t Size;

                        // get the data
                        memcpy(pSendData+0, &writeAddress, 4);
                        memcpy(pSendData+4, &newWriteSize, 4);
                        memcpy(pSendData+8, pData+startOfData, newWriteSize);

                        // create UBX message
                        UbxCreateMessage(UBX_CLASS_UPD, UBX_UPD_FLWRI, pSendData, newWriteSize+8, &pMessage, &Size);

                        // send the data to the receiver
                        switch(h->type)
                        {
#ifdef ENABLE_AARDVARK_SUPPORT
                        case SPI:
                            SPI_WRITE(h->handle,(SPI_READBUFFER_pt)h->pData,pMessage,Size);
                            break;
#endif // ENABLE_AARDVARK_SUPPORT
#ifdef ENABLE_DIOLAN_SUPPORT
                        case SPU:
                            SPU_WRITE(h->handle,(SPI_READBUFFER_pt)h->pData,pMessage,Size);
                            break;
#endif // ENABLE_DIOLAN_SUPPORT
                        default:
                            break;
                        }

                        // cleanup
                        free(pMessage);
                        free(pSendData);

                    }

                    // set the start of the new data to the current position (where data was found)
                    startOfData = pos;

                }

                // reset the 0xFF counter
                curSearch = 0;
            }

            // increment the position pointer
            pos++;
        }


        // check if anything has to be written
        if(pos-startOfData-curSearch)
        {
            // calculate the new write address and the new size of the payload
            U4 writeAddress = targetAddress+startOfData;
            U4 newWriteSize2    = pos-startOfData-curSearch;

            CH *pMessage=NULL;
            CH *pSendData = (CH*) malloc(newWriteSize2+8);
            if(!pSendData)
                return 0;
            size_t Size;

            // get the data
            memcpy(pSendData+0, &writeAddress, 4);
            memcpy(pSendData+4, &newWriteSize2, 4);
            memcpy(pSendData+8, pData+startOfData, newWriteSize2);

            // create UBX message
            UbxCreateMessage(UBX_CLASS_UPD, UBX_UPD_FLWRI, pSendData, newWriteSize2+8, &pMessage, &Size);

            // send the data to the receiver
            switch(h->type)
            {
#ifdef ENABLE_AARDVARK_SUPPORT
            case SPI:
                SPI_WRITE(h->handle,(SPI_READBUFFER_pt)h->pData,pMessage,Size);
                break;
#endif // ENABLE_AARDVARK_SUPPORT
#ifdef ENABLE_DIOLAN_SUPPORT
            case SPU:
                SPU_WRITE(h->handle,(SPI_READBUFFER_pt)h->pData,pMessage,Size);
                break;
#endif // ENABLE_DIOLAN_SUPPORT
            default:
                break;
            }

            // cleanup
            free(pMessage);
            free(pSendData);

        }
        // no data could be found to be written -> send one single byte
        else
        {
            U4 newSize = 1;       // size is one single byte
            CH *pMessage=NULL;
            CH *pSendData = (CH*) malloc(newSize+8);
            if(!pSendData)
                return 0;

            U1 newData = 0xFF;    // data is 0xFF
            size_t Size;

            // get the data
            memcpy(pSendData+0, &targetAddress, 4);
            memcpy(pSendData+4, &newSize, 4);
            memcpy(pSendData+8, &newData, newSize);

            // create the message
            UbxCreateMessage(UBX_CLASS_UPD, UBX_UPD_FLWRI, pSendData, newSize+8, &pMessage, &Size);

            // send the data to the receiver
            switch(h->type)
            {
#ifdef ENABLE_AARDVARK_SUPPORT
            case SPI:
                SPI_WRITE(h->handle,(SPI_READBUFFER_pt)h->pData,pMessage,Size);
                break;
#endif // ENABLE_AARDVARK_SUPPORT
#ifdef ENABLE_DIOLAN_SUPPORT
            case SPU:
                SPU_WRITE(h->handle,(SPI_READBUFFER_pt)h->pData,pMessage,Size);
                break;
#endif // ENABLE_DIOLAN_SUPPORT
            default:
                break;
            }


            // cleanup
            free(pMessage);
            free(pSendData);
        }

        // fake the number of written bytes
        // such that the higher levels wont freak out
        return size;
    }
    // no UPD-FLWRI message
    else
    {
        // write data using standard write
        switch(h->type)
        {
#ifdef ENABLE_AARDVARK_SUPPORT
        case SPI:
            return SPI_WRITE(h->handle,(SPI_READBUFFER_pt)h->pData,p,size);
#endif // ENABLE_AARDVARK_SUPPORT
#ifdef ENABLE_DIOLAN_SUPPORT
        case SPU:
            return SPU_WRITE(h->handle,(SPI_READBUFFER_pt)h->pData,p,size);
#endif // ENABLE_DIOLAN_SUPPORT
        default:
            return 0;
        }
    }
}

U4 SER_WRITE(SER_HANDLE_pt h,
            const void    *p,
            U4             size)
{
    switch(h->type)
    {
    case COM:
        return COM_WRITE(h->handle,p,size);
#ifdef WIN32
    case STDINOUT:
        return STDOUT_WRITE(p,size);
#endif //ifdef WIN32

#ifdef ENABLE_AARDVARK_SUPPORT
    case I2C:
        return I2C_WRITE(h->handle,h->devAddr,p,size);
    case SPI:
        return SER_WRITE_SPI(h, p, size);
#endif // ENABLE_AARDVARK_SUPPORT
#ifdef ENABLE_DIOLAN_SUPPORT
    case U2C:
        return U2C_WRITE(h->handle,h->devAddr,p,size);
    case SPU:
        return SER_WRITE_SPI(h, p, size);
#endif //ENABLE_DIOLAN_SUPPORT
#ifdef ENABLE_NET_SUPPORT
    case NET:
        return NET_WRITE(h->handle,p,size);
#endif // ENABLE_NET_SUPPORT
    default:
        break;
    }
    return 0;
}

U4 SER_READ(SER_HANDLE_pt h,
            void*         p,
            U4            size)
{
    switch(h->type)
    {
    case COM:
        return COM_READ(h->handle,p,size);
#ifdef WIN32
    case STDINOUT:
        return STDIN_READ(p,size);
#endif //ifdef WIN32
#ifdef ENABLE_AARDVARK_SUPPORT
    case I2C:
        return I2C_READ(h->handle,h->devAddr,p,size);
    case SPI:
        return SPI_READ(h->handle,(SPI_READBUFFER_pt)h->pData,p,size);
#endif // ENABLE_AARDVARK_SUPPORT
#ifdef ENABLE_DIOLAN_SUPPORT
    case U2C:
        return U2C_READ(h->handle,h->devAddr,p,size);
    case SPU:
        return SPU_READ(h->handle,(SPI_READBUFFER_pt)h->pData,p,size);
#endif // ENABLE_DIOLAN_SUPPORT
#ifdef ENABLE_NET_SUPPORT
    case NET:
        return NET_READ(h->handle,p,size);
#endif // ENABLE_NET_SUPPORT
    default:
        break;
    }
    return 0;
}

void SER_CLEAR(SER_HANDLE_pt h)
{
    switch(h->type)
    {
    case COM:
        COM_CLEAR(h->handle);
        break;
    default:
        break;
    }
}

BOOL SER_BAUDRATE(SER_HANDLE_pt h,
                  U4            br)
{
    BOOL brSet = FALSE;
    //MESSAGE(MSG_DBG, "platform: switching baudrate to %d baud", br);
    switch(h->type)
    {
    case COM:
        brSet = COM_BAUDRATE(h->handle,br);
        break;
#ifdef WIN32
    case STDINOUT:
        STDIN_SETBAUD(br);
        brSet = TRUE; // There is no baudrate here
        break;
#endif //ifdef WIN32
#ifdef ENABLE_AARDVARK_SUPPORT
    case I2C:
        brSet = I2C_BAUDRATE(h->handle,br);
        break;
    case SPI:
        brSet = SPI_BAUDRATE(h->handle,br);
        break;
#endif // ENABLE_AARDVARK_SUPPORT
#ifdef ENABLE_DIOLAN_SUPPORT
    case SPU:
        brSet = SPU_BAUDRATE(h->handle,br);
        break;
    case U2C:
        brSet = U2C_BAUDRATE(h->handle,br);
        break;
#endif // ENABLE_DIOLAN_SUPPORT
#ifdef ENABLE_NET_SUPPORT
    case NET:
        brSet = NET_BAUDRATE(h,br);
        break;
#endif // ENABLE_NET_SUPPORT
    default:
        break;
    }

    if (brSet)
    {
        h->baudrate = br;
    }

    return brSet;
}

void SER_FLUSH(SER_HANDLE_pt h)
{
    if (!h)
        return;

    switch (h->type)
    {
    case COM:
        COM_FLUSH(h->handle);
        break;
    default:
        break;
    }
}

void SER_CLOSE(SER_HANDLE_pt h)
{
    if (!h)
        return;

    switch(h->type)
    {
    case COM:
        COM_CLOSE(h->handle);
        break;
#ifdef WIN32
    case STDINOUT:
        break;
#endif //ifdef WIN32
#ifdef ENABLE_AARDVARK_SUPPORT
    case I2C:
        I2C_CLOSE(h->handle);
        break;
    case SPI:
        SPI_CLOSE(h->handle);
        break;
#endif // ENABLE_AARDVARK_SUPPORT
#ifdef ENABLE_DIOLAN_SUPPORT
    case U2C:
        U2C_CLOSE(h->handle);
        break;
    case SPU:
        SPU_CLOSE(h->handle);
        break;
#endif // ENABLE_DIOLAN_SUPPORT
#ifdef ENABLE_NET_SUPPORT
    case NET:
        NET_CLOSE(h->handle);
        break;
#endif // ENABLE_NET_SUPPORT
    default:
        break;
    }
    // free custom data
    if (h->pData)
        free(h->pData);
    // free handle
    free(h);
}

U4 SER_PENDING(SER_HANDLE_pt h)
{
    if (!h)
        return 0;

    switch (h->type)
    {
#ifdef ENABLE_AARDVARK_SUPPORT
    case I2C:
        return I2C_PENDING(h->handle, h->devAddr);
#endif // ENABLE_AARDVARK_SUPPORT
#ifdef ENABLE_DIOLAN_SUPPORT
    case U2C:
        return U2C_PENDING(h->handle, h->devAddr);
#endif // ENABLE_DIOLAN_SUPPORT
    case COM:
    default:
        break;
    }
    return 0;
}


//=====================================================================
// STATUS MESSAGES
//=====================================================================

void MESSAGE_PLAIN(const CH* format, ...)
{
    CH mem[1024];
    U4 cnt = 0;

    va_list args;
    va_start(args, format);
    cnt += vsprintf(&mem[cnt], format, args);
    va_end(args);

#ifdef WIN32
    HANDLE hErrOut = GetStdHandle(STD_ERROR_HANDLE);
    U4 dw;
    WriteFile(hErrOut, mem, cnt, (LPDWORD)&dw, NULL);
#else
    fprintf(stderr, "%s", mem);
    fflush(stderr);
#endif // WIN32
}

// write to stdout
void MESSAGE(MSG_LEVEL level, const CH* format, ...)
{
    static U4 tick = 0;
    // Must be done like this because the result of TIME_GET()
    // can not be the initializer of a compile-time constant
    if(tick==0)
        tick=TIME_GET();

#ifdef WIN32
    HANDLE hErrOut = GetStdHandle(STD_ERROR_HANDLE);
    U4 dw;
#endif // WIN32
    // maximum line length
    CH mem[1024];
    U4 cnt;
    if (level > ((verbose) ? MSG_DBG : MSG_LEV2))
    {
        return;
    }
    cnt = 0;
    cnt += sprintf(&mem[cnt], "%5.1f ", 0.001 * (double) (TIME_GET() - tick));
    switch (level)
    {
#ifdef _TEST
        case MSG_TEST:  break;
#endif // _TEST
        case MSG_ERR:    cnt+=sprintf(&mem[cnt], "ERROR: ");
#ifdef WIN32
            SetConsoleTextAttribute(hErrOut,FOREGROUND_RED|FOREGROUND_INTENSITY);
#endif // WIN32
            break;
        case MSG_WARN:    cnt+=sprintf(&mem[cnt], "WARNING: ");
#ifdef WIN32
            SetConsoleTextAttribute(hErrOut,FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY);
#endif // WIN32
            break;
        case MSG_LEV0:    break;
        case MSG_LEV1:    cnt+=sprintf(&mem[cnt], " ");        break;
        case MSG_LEV2:
#ifdef WIN32
            SetConsoleTextAttribute(hErrOut,FOREGROUND_BLUE|FOREGROUND_INTENSITY);
#endif // WIN32
            break;
        default:
        case MSG_DBG:    cnt+=sprintf(&mem[cnt], "  - ");    break;
    }
    va_list args;
    va_start(args, format);
    cnt += vsprintf(&mem[cnt], format, args);
    va_end(args);
    cnt += sprintf(&mem[cnt], "\r\n");
#ifdef WIN32
    WriteFile(hErrOut, mem, cnt, (LPDWORD)&dw, NULL);
    SetConsoleTextAttribute(hErrOut,FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
#else
    fprintf(stderr, "%s", mem);
#endif // WIN32
}


//=====================================================================
// CONSOLE
//=====================================================================


#ifdef WIN32
static BOOL WINAPI CONSOLE_CTRLHANDLER(DWORD dwCtrlType)
{
    if (dwCtrlType == CTRL_C_EVENT)
    {
        SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE),FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
    }
    if (dwCtrlType == CTRL_BREAK_EVENT)
    {
        // toggle verbosity
        verbose = !verbose;
        return TRUE;
    }
    return FALSE;
}
#endif // WIN32

static int sCONSOLE_SAVEPOS_y;
static int sCONSOLE_SAVEPOS_x;

#ifndef WIN32
static struct termios sCONSOLE_tioSave;
#endif // WIN32

void CONSOLE_INIT(void)
{
#ifdef WIN32
    SetConsoleCtrlHandler(CONSOLE_CTRLHANDLER, TRUE);
#else
    // make stdin unbuffered, disable echo, etc.
    struct termios tio;
    if (isatty(fileno(stderr)))
    {
        tcgetattr(fileno(stderr), &tio);
        memcpy(&sCONSOLE_tioSave, &tio, sizeof(struct termios));
        tio.c_lflag     &= (~ICANON);
        tio.c_lflag     &= (~ECHO);
        tio.c_cc[VTIME]  = 1;
        tio.c_cc[VMIN]   = 0;
        tcsetattr(fileno(stderr), TCSANOW, &tio);
    }
#endif // WIN32
    sCONSOLE_SAVEPOS_y = 1;
    sCONSOLE_SAVEPOS_x = 1;
}

void CONSOLE_DONE(void)
{
#ifndef WIN32
    if (isatty(fileno(stderr)))
    {
        fflush(stdin);
        tcsetattr(fileno(stdin), TCSANOW, &sCONSOLE_tioSave);
    }
#endif // WIN32
}


void CONSOLE_SAVE_POS(U4 offs)
{

    // scroll terminal
    U4 i;
    for(i =  0;  i < offs; i++)
    {
        MESSAGE_PLAIN("\n");
    }
    fflush(stderr);
#ifdef WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE),&info);
    sCONSOLE_SAVEPOS_x = info.dwCursorPosition.X;
    sCONSOLE_SAVEPOS_y = info.dwCursorPosition.Y;
#else // WIN32
    if (isatty(fileno(stderr)))
    {
        char buf[16];
        char *pBuf = buf;
        // request cursor position
        fprintf(stderr, "\e[6n");
        fflush(stderr);
        // read cursor position (expecting: "^[[xxx;yyyR")
        int timeout = 10;
        while ( (pBuf - buf < 12) && (*pBuf != 'R') && timeout--)
        {
            if (read(0, pBuf, 1))
            {
                pBuf++;
            }
        }
        int x = 0, y = 0;
        if (sscanf(buf+2, "%i;%iR", &y, &x) == 2)
        {
            sCONSOLE_SAVEPOS_y = y;
            sCONSOLE_SAVEPOS_x = x;
        }
    }
#endif // WIN32
    sCONSOLE_SAVEPOS_y -= offs;
    if (sCONSOLE_SAVEPOS_y < 1)
    {
        sCONSOLE_SAVEPOS_y = 1;
    }
}

void CONSOLE_RESTORE_POS(void)
{
#ifdef WIN32
    COORD coord;
    coord.X = (SHORT)sCONSOLE_SAVEPOS_x;
    coord.Y = (SHORT)sCONSOLE_SAVEPOS_y;
    SetConsoleCursorPosition(GetStdHandle(STD_ERROR_HANDLE),coord);
#else
    if (isatty(fileno(stderr)))
    {
        fprintf(stderr, "\e[%u;%uH", sCONSOLE_SAVEPOS_y, sCONSOLE_SAVEPOS_x);
        fflush(stderr);
    }
#endif // WIN32
}
