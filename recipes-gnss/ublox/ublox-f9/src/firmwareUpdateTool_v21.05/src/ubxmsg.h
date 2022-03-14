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
  \brief  UBX message protocol wrapper
*/

#ifndef __UBXMSG_H
#define __UBXMSG_H

#include "types.h"

/*! UBX Protocol sync chars
 *  @{
 */
#define UBX_SYNC_CHAR_1 0xB5u     //!< First synchronization character of UBX Protocol
#define UBX_SYNC_CHAR_2 0x62u     //!< Second synchronization character of UBX Protocol
#define UBX_PREFIX      (UBX_SYNC_CHAR_2<<8|UBX_SYNC_CHAR_1) //!< UBX Protocol Prefix
/*! @} */

/*! UBX messages payload sizes
*  @{
*/
#define UBX_UPD_CERASE_DATA1_PAYLOAD_SIZE  1   //!< Payload size of chip erase acknowladge message
#define UBX_UPD_ERASE_DATA1_PAYLOAD_SIZE   5   //!< Payload size of sector erase acknowladge message
#define UBX_UPD_FLWRI_DATA1_PAYLOAD_SIZE   5   //!< Payload size of payload write acknowladge message
/*! @}*/


//! UBX Protocol Header
typedef struct UBX_HEAD_s
{
    U2 prefix;                    //!< Prefix, always #UBX_PREFIX
    U1 classId;                   //!< UBX Class Id
    U1 msgId;                     //!< UBX Message Id
    U2 size;                      //!< Payload Size
} UBX_HEAD_t;
typedef UBX_HEAD_t* UBX_HEAD_pt;  //!< Pointer to UBX_HEAD_t type

//! UBX-ACK-ACK and UBX-ACK-NAK messages
typedef struct UBX_ACK_ACK_s
{
    U1 clsId;                     //!< Class ID to be ACK/NAKed
    U1 msgId;                     //!< Message ID to be ACK/NAKed
} UBX_ACK_ACK_t;

//! UBX-CFG-PRT message payload
typedef struct UBX_CFG_PRT_s
{
    U1    portId;                 //!< Port Identifier Number
    U1    res0;                   //!< Reserved
    U2    res1;                   //!< Reserved
    U4    mode;                   //!< UART mode
    U4    baudrate;               //!< Baudrate bit/s
    U2    inProtoMask;            //!< Active input protocols
    U2    outProtoMask;           //!< Active output protocols
    U2    flags;                  //!< Autobauding
    U1    fwdTargetMask;          //!< Forward target mask
    U1    pad0;                   //!< Padding
} UBX_CFG_PRT_t;

//! UBX-UPD-IMG message payload
typedef struct APP_UBX_UPD_IMG_PAYLOAD_s
{
    U2 chunkNum;                  //!< 512 byte chunk number
    U1 chunkData[512];            //!< Image data
} APP_UBX_UPD_IMG_PAYLOAD_t;


//! Sizes for UBX protocol header
//@{
#define UBX_PREFIX_SIZE 2u                             //!< UBX Protocol Prefix Size in bytes
#define UBX_CHKSUM_SIZE 2u                             //!< UBX Protocol Checksum Size in bytes
#define UBX_HEAD_SIZE sizeof(UBX_HEAD_t)               //!< UBX Protocol Header Size
#define UBX_FRAME_SIZE (UBX_HEAD_SIZE+UBX_CHKSUM_SIZE) //!< Total size of the UBX Frame
//@}

//! Used UBX message class and identifiers
enum UBX_ID_e
{
    // classes
    UBX_CLASS_ACK   = 0x05,         //!< Class acknowledge
    UBX_CLASS_CFG   = 0x06,         //!< Class configuration
    UBX_CLASS_UPD   = 0x09,         //!< Class update
    UBX_CLASS_MON   = 0x0A,         //!< Class monitor

    // class ACK
    UBX_ACK_NAK     = 0x00,         //!< Not acknowledged           (PUB 10+)
    UBX_ACK_ACK     = 0x01,         //!< Acknowledged               (PUB 10+)

    // class CFG
    UBX_CFG_PORT    = 0x00,         //!< Configure port             (PUB 10+)
    UBX_CFG_RST     = 0x04,         //!< Reset receiver             (PUB 10+)

    // class UPD
    UBX_UPD_AUTHREAD  = 0x20,       //!< Read with signature        (RD 18+)
    UBX_UPD_CERASE    = 0x16,       //!< Chip erase                 (NDA 15+)
    UBX_UPD_CRC       = 0x0D,       //!< Check CRC                  (NDA 10+)
    UBX_UPD_DOWNL     = 0x01,       //!< Memory write               (NDA 10-17)
    UBX_UPD_ERASE     = 0x0B,       //!< Flash erase                (NDA 10+)
    UBX_UPD_FLDET     = 0x08,       //!< Detect flash               (NDA 10+)
    UBX_UPD_FLWRI     = 0x0C,       //!< Flash write                (NDA 10+)
    UBX_UPD_IDEN      = 0x06,       //!< Identify                   (NDA 10+)
    UBX_UPD_QSIZE     = 0x09,       //!< Get queue size             (NDA 10+)
    UBX_UPD_RBOOT     = 0x0E,       //!< Reboot                     (NDA 10+)
    UBX_UPD_SAFE      = 0x07,       //!< Execute safeboot           (NDA 10+)
    UBX_UPD_SETQ      = 0x0F,       //!< Set max queue size         (NDA 10+)
    UBX_UPD_UPLOAD    = 0x02,       //!< Memory read                (NDA 10-17)
    UBX_UPD_FIS       = 0x19,       //!< Update FIS                 (NDA 23)
    UBX_UPD_IMG       = 0x1A,       //!< Update RAM image           (NDA 23)
    UBX_UPD_AUTHWRITE = 0x21,       //!< Write with signature       (NDA 18+)
    UBX_UPD_ROM       = 0x25,       //!< Rom CRC                    (NDA 24+)

    // class MON
    UBX_MON_VER       = 0x04,       //!< version information        (PUB 10+)


};

//! Create an UBX message of specified type
/*! Creates a UBX message from the payload provided.
    Allocates memory that has to be freed by the caller. Wraps the UBX Frame
    around the Payload and calculates CRC.

    \param ClassId        message's class ID
    \param MsgId          message's message ID
    \param pPayload       pointer to Payload data
    \param PayloadSize    Size of Payload
    \param ppMessage      pointer to Message
    \param pMsgSize       Message size including Frame
    \return TRUE if memory could be allocated, FALSE else
*/
BOOL UbxCreateMessage( IN  U1            ClassId
                     , IN  U1            MsgId
                     , IN  const void*   pPayload
                     , IN  const size_t  PayloadSize
                     , OUT CH**          ppMessage
                     , OUT size_t*       pMsgSize );

//! Search for UBX message header
/*! Search buffer for valid UBX message
    \param pBuffer        Buffer containing received stream
    \param Size           Number of characters in Buffer
    \param ppMsg          pointer to the first message found if return value
                          is TRUE, else pointer to the first occurrence of a
                          possible message start (where crap is discarded)
    \return TRUE if a valid message was found, FALSE else
*/
BOOL UbxSearchMsg( IN  U1 *   pBuffer
                 , IN  size_t Size
                 , OUT U1 **  ppMsg );

#endif

