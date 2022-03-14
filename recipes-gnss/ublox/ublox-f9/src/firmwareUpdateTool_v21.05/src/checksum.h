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
  \brief  checksum routines for UBX protocol and firmware image
*/

#ifndef __CHECKSUM_H
#define __CHECKSUM_H

#include <stdio.h>
#include "types.h"


//! Check UBX checksum on Words
/*!
    Check if the data beginning at pData with size numBytes has appended the
    correct checksum, operating on 4-Byte words

    \param pData     pointer to checksum calculation range begin
    \param numBytes  length of checksum calculation range in bytes
    \return TRUE if the checksum is correct, FALSE else
*/
BOOL CheckUbxChecksumU4(IN const U4* pData,
                        IN size_t    numBytes);



//! Calculate UBX checksum over pData
/*!
    Calculates the checksum over pData and returns it.

    \param pData     pointer to Data to calculate checksum on
    \param length    length of data to calculate checksum on
    \return 2 Bytes checksum, crc_a in low byte
*/
unsigned short GetUbxChecksumU1(IN const U1* pData,
                                IN size_t    length);

//! Calculate UBX checksum over pData
/*!
    Calculates the checksum over pData and returns it.

    \param pChk_a    pointer to U4 receiving first word of checksum
    \param pChk_b    pointer to U4 receiving second word of checksum
    \param pData     pointer to Data to calculate checksum on
    \param numBytes  length of data in bytes to calculate checksum on
*/
void GetUbxChecksumU4(OUT U4 * pChk_a,
                      OUT U4 * pChk_b,
                      IN  const U4 * pData,
                      IN  size_t numBytes);

#endif

