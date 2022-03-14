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
  \brief  Flash organization stuff

  Provides an interface to transform Addresses to sector numbers and
  Packet numbers and parsing flash definition text files
*/

#ifndef __FLASH_H
#define __FLASH_H

#include "types.h"


//! Flash sector block organization description
typedef struct BLOCK_DEF_s
{
    U4 Count;        //!< number of sectors
    U4 Size;         //!< size of sector in kB
} BLOCK_DEF_t;
typedef BLOCK_DEF_t* BLOCK_DEF_pt; //!< pointer to BLOCK_DEF_t type

//! Array of block definitions
typedef struct BLOCK_ARR_s
{
    INOUT BLOCK_DEF_pt pBlocks; //!< blocks
    U4 NumBlocks;               //!< Number of blocks
} BLOCK_ARR_t;
typedef BLOCK_ARR_t* BLOCK_ARR_pt; //!< pointer to BLOCK_ARR_t type

//! Get the number of sectors the firmware fits in
/*!
    \param Offset        the offset of the firmware from flash base
    \param FwSize        the size of the firmware
    \param pFlashdef     pointer to array containing flash organization
    \return number of sectors required, -1 if it cannot be determined
*/
I4 GetSectorNrForSize(IN const U4 Offset,
                      IN const U4 FwSize,
                      IN const BLOCK_ARR_pt pFlashdef);

//! Get the sector number at address
/*!
    \param Address        Address to get sector number at
    \param Base           Base address of firmware
    \param Offset         Offset of firmware from flash base
    \param pFlashdef      Pointer to flash sector definition information
    \return sector number if found, -1 if it cannot be determined
*/
I4 GetSectorNrForAddress(IN const U4 Address,
                         IN const U4 Base,
                         IN const U4 Offset,
                         IN const BLOCK_ARR_pt pFlashdef);


//! Get the number of the packet for the given sector
/*!
    \param Sector         number of the sector
    \param pFlashdef      pointer to flash organization structure array
    \param PacketSize     packet size
    \return    number of the according packet, -1 if packet cannot be determined
*/
I4 GetPacketNrForSector(IN const U4 Sector,
                        IN const BLOCK_ARR_pt pFlashdef,
                        IN const U4 PacketSize);

//! Get the number of the packet for the given address
/*!
    \param Address        Address to get packet number for
    \param Base           Address of firmware base
    \param PacketSize     Size of one download packet
    \return Packet number if success, -1 if packet cannot be determined
*/
U4 GetPacketNrForAddress(IN const U4 Address,
                         IN const U4 Base,
                         IN const U4 PacketSize);

//! Dump Flash organization info
/*!
    \param pkFlashOrg     pointer to flash organization vector
    \param FlashSize      size of Flash
*/
void DumpFlashInfo(IN BLOCK_ARR_t const * const pkFlashOrg,
                   IN const U4 FlashSize);

//! Parse a file for flash memory architecture descriptions
/*!
    \param ManId          the manufacturer id of the flash
    \param DevId          the device id of the flash
    \param pBlocks        pointer where to store the flash definition
    \param pSize          pointer to size of the flash
    \param Filename       name of the flash definition file
    \return TRUE if successful, FALSE otherwise
*/
BOOL GetFlashOrganisation(IN    U2 const ManId,
                          IN    U2 const DevId,
                          INOUT BLOCK_ARR_pt pBlocks,
                          INOUT U4* const pSize,
                          IN    CH const * const Filename);

//! Clear the array of block contents
/*!
    \param pBlocks        Block array to be cleared
*/
void clearBlocks(INOUT BLOCK_ARR_pt pBlocks);

//! Clear the array of block contents
/*!
    \param pBlocks        Block array to be cleared
    \param pNewBlock      Block to be added to pBlocks
    \return TRUE on success, FALSE otherwise
*/
BOOL addBlock(INOUT BLOCK_ARR_pt pBlocks,
              IN    BLOCK_DEF_pt const pNewBlock);

#endif

