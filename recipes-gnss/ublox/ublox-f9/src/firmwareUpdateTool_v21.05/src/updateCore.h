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
  \brief  Declaration of function that handle the update on the receiver
*/


#include <stdio.h>
#include "receiver.h"
#include "flash.h"
#include "image.h"

#define PACKETSIZE               512   //!< size of a packet to send to the receiver
#define ERASE_TIMEOUT          12000   //!< timeout for erasing a sector
#define CRC_TIMEOUT             3000   //!< timeout for the CRC computation
#define ERASE_RETRIES              4   //!< number of retries to erase block
#define WRITE_RETRIES              4   //!< number of retries to write block
#define WRITE_TIMEOUT           3000   //!< timeout for writing a block
#define CHIP_ERASE_TIMEOUT     45000   //!< chip erase timeout
#define FLASH_BASE        0x00800000   //!< Flash base address
#define RAM_BASE          0x00800000   //!< RAM base address
#define DUMPINTERVAL            1000   //!< timeout between two dumps when verbose > 1
#define CONSOLE_WIDTH             80   //!< Width of the console

#define MAX_PENDING_ERASES         2   //!< The maximum number of erase commands to be present in the receiver queue

//! Acknowledge states
typedef enum ACK_STATE_e
{
    ACK_INIT        = '.',  //!< initial state
    ACK_ERASE_SENT  = 'e',  //!< erase pending for packet (sector-blocks)
    ACK_ERASE_ACK   = 'E',  //!< erase acknowledged for packet
    ACK_WRITE_SENT  = 'w',  //!< write pending for packet
    ACK_WRITE_ACK   = 'W',  //!< write acknowledged for packet
    ACK_UNKNOWN     = '?'   //!< unknown state
} ACK_STATE_t;

//! Preserves the state of the upgrade and the organization of the flash
typedef struct
{
    RCV_DATA_t *Rx;             //!< receiver to communicate with
    I4 NumberSectors;           //!< number of sectors to erase
    I4 NumberPackets;           //!< number of packets to write
    I4 writtenUntil;            //!< used to optimize the write loop
    I4 erasedUntil;             //!< used to optimize the erase loop

    // organization of the flash
    BLOCK_ARR_t *FlashOrg;      //!< flash organization
    U4 FlashSize;               //!< size of the flash
    U4 FwBase;                  //!< start address of the firmware on the flash
    FWHEADER_t* pData;          //!< pointer to the data to write
    U4 ImageSize;               //!< size of the image

    U4 *pEraseTimeout;          //!< array to store timeouts for the to be erased sectors
    U1 *pEraseRetryCnt;         //!< array to store retry counts for the to be erased sectors
    CH *pEraseState;            //!< array to store erase status for the to be erased sectors

    U4 *pWriteTimeout;          //!< array to store timeouts for the to be written packets
    U1 *pWriteRetryCnt;         //!< array to store retry counts for the to be written packets
    CH *pWriteState;            //!< array to store erase status for the to be written packets

    U4 PendingErases;           //!< number of erases pending
    U4 PendingWrites;           //!< number of writes pending

    U4 sLastDumpTime;           //!< time of the last dump (for verbose > 1)

    U4 MaxPendingWritesNum;     //!< max number of which can be queued in the receiver writes pending
    BOOL eraseInProgres;        //!< Erase in progress
} UPD_CORE_t;

/*!
 * Initializer for UPD_CORE_t.
 *
 * \param rx                    receiver to do the update on
 * \param numberSectors         number of sectors to erase
 * \param numberPackets         number of packets to write
 * \param flashOrg              information about the organization of the flash
 * \param flashSize             size of the flash
 * \param MaxPendingWritesNum   Maximum possible number of pending writes
 * \param eraseInProgres        Means that flash erase was started but not finished before entering update.
 * \return The control structure on success, NULL on fail
 */
UPD_CORE_t* updInit( RCV_DATA_t *rx
                   , I4 numberSectors
                   , I4 numberPackets
                   , BLOCK_ARR_t *flashOrg
                   , U4 flashSize
                   , U4 MaxPendingWritesNum
                   , BOOL eraseInProgres);

/*!
 * De-Initializer for UPD_CORE_t
 *
 * \param upd                   control structure
 */
void updDeinit(UPD_CORE_t *upd);


/*!
 * Do the update.
 *
 * \param upd                   control structure
 * \param data                  pointer to the data to write
 * \param size                  size of the data
 * \param fwBase                start address of the firmware on the flash
 * \return TRUE if successful
 */
BOOL updUpdate(UPD_CORE_t *upd, FWHEADER_t* data, size_t size, U4 fwBase);

