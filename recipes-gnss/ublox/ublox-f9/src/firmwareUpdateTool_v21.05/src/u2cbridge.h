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

#ifndef __U2CBRIDGE_H__

#ifdef ENABLE_DIOLAN_SUPPORT

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "i2cbridge.h"

BYTE U2C_Freq2Word(DWORD freq);
DWORD U2C_Word2Freq(BYTE word, BOOL isSpi);
char * U2C_StatusString(U2C_RESULT result);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ENABLE_DIOLAN_SUPPORT

#endif // __U2CBRIDGE_H__
