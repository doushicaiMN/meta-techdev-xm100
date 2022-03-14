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

#include "checksum.h"

void GetUbxChecksumU4(OUT       U4* pChk_a,
                      OUT       U4* pChk_b,
                      IN  const U4* pData,
                      IN  size_t    numBytes)
{
    U4 chk_a = 0;
    U4 chk_b = 0;
    numBytes /= 4;
    while (numBytes--)
    {
        chk_a += *pData++;
        chk_b += chk_a;
    }
    *pChk_a = chk_a;
    *pChk_b = chk_b;
}

BOOL CheckUbxChecksumU4(IN const U4* pData,
                        IN size_t    numBytes)
{
    U4 chk_a = 0;
    U4 chk_b = 0;
    GetUbxChecksumU4(&chk_a, &chk_b, pData, numBytes);
    pData += (numBytes/4);
    return (chk_a == *pData && chk_b == *(pData+1));
}


U2 GetUbxChecksumU1(IN const U1* pData,
                    IN size_t    length)
{
    U1 chk_a = 0;
    U1 chk_b = 0;
    while (length--)
    {
        chk_a += *pData++;
        chk_b += chk_a;
    }
    return ((U2)(chk_b)<<8) | (U2)(chk_a);
}

