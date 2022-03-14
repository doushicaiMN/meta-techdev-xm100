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
\brief  Data type used when buffering data via SPI interface
*/

#ifndef _SPI_READ_BUFFER_H
#define _SPI_READ_BUFFER_H


typedef struct SPI_READBUFFER_s
{
    U1 buffer[4096];                         //!< buffer containing data
    U1 *pBuffer;                             //!< pointer to data begin
    U4  size;                                //!< buffered data size
} SPI_READBUFFER_t;
typedef SPI_READBUFFER_t* SPI_READBUFFER_pt; //!< pointer to SPI_READBUFFER_t type

#endif
