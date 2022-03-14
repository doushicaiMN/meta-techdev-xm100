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
  \brief  Firmware update process for u-blox 5/6
*/

#ifndef __UPDATE_H
#define __UPDATE_H

#include "types.h"
#include "flash.h"

#define DEFAULT_MAX_PACKETS 10 //!< Default maximum pending commands in receiver
//! Perform Firmware update process
/*!
    Handles the Firmware update process for u-blox receivers
    \param  BinaryFileName      File [directory and] name of firmware binary
    \param  FlashDefFileName    File name of flash definition file
    \param  FisFileName         File name of the FIS definition file
    \param  ComPort             COM-Port name (either SymbolicLink or DeviceObject name)
    \param  Baudrate            Communication port baudrate
    \param  BaudrateSafe        Serial port baudrate when starting safeboot
    \param  BaudrateUpd         Serial port baudrate during update
    \param  DoSafeBoot          Send safeboot command before updating
    \param  DoReset             Send reset command after updating
    \param  DoAutobaud          Perform autobauding if given baudrate fails
    \param  EraseWholeFlash     Clear not only area to fit image but the whole flash
    \param  EraseOnly           Only erase flash, don't perform write
    \param  TrainingSequence    Send training sequence
    \param  doChipErase         Do a full chip erase instead of erasing the sectors separately
    \param  Verbose             Verbose mode (0: rather quiet, 1: not so quiet, 2: dump acknowledges, not recommended when writing log files)
    \param  fisOnly             Program only the FIS sector to the flash device
    \param  noFisMerging        don't merge the FIS but directly program the image
    \param  updateRam           if not 0 update the u-blox 9 RAM
    \return success state
    \param usbAltMode           Use USB alternative mode (Invalidate flash only)
*/
BOOL UpdateFirmware(IN const char*          BinaryFileName,
                    IN const char*          FlashDefFileName,
                    IN const char*          FisFileName,
                    IN const char*          ComPort,
                    IN const unsigned int   Baudrate,
                    IN const unsigned int   BaudrateSafe,
                    IN const unsigned int   BaudrateUpd,
                    IN       BOOL           DoSafeBoot,
                    IN const BOOL           DoReset,
                    IN const BOOL           DoAutobaud,
                    IN const BOOL           EraseWholeFlash,
                    IN const BOOL           EraseOnly,
                    IN const BOOL           TrainingSequence,
                    IN const BOOL           doChipErase,
                    IN       BOOL           noFisMerging,
                    IN       BOOL           updateRam,
                    IN const BOOL           usbAltMode,
                    IN const int            Verbose,
                    IN const BOOL           fisOnly);

#endif //__UPDATE_H
