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
  \brief  ubx message protocol wrapper
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "checksum.h"
#include "ubxmsg.h"
#include "platform.h"
BOOL UbxCreateMessage(IN  U1            ClassId
    , IN  U1            MsgId
    , IN  const void*   pPayload
    , IN  const size_t  PayloadSize
    , OUT CH**          ppMessage
    , OUT size_t*       pMsgSize)
{
    CH* pMsg = NULL;
    CH* pMsgBuf = NULL;
    {
        pMsgBuf = (CH*)malloc(PayloadSize + UBX_FRAME_SIZE);
        if (pMsgBuf == NULL)
            return FALSE;
        pMsg = pMsgBuf;
    }
    UBX_HEAD_t ubxhdr;
    ubxhdr.prefix = UBX_PREFIX;
    ubxhdr.classId = ClassId;
    ubxhdr.msgId = MsgId;
    ubxhdr.size = PayloadSize;
    memcpy(pMsg, &ubxhdr, sizeof(ubxhdr));
    if (PayloadSize && pPayload)
        memcpy(pMsg + UBX_HEAD_SIZE, pPayload, PayloadSize);

    U2 crc = GetUbxChecksumU1((U1*)pMsg + UBX_PREFIX_SIZE, PayloadSize + UBX_HEAD_SIZE - UBX_PREFIX_SIZE);
    memcpy(pMsg + UBX_HEAD_SIZE + PayloadSize, &crc, sizeof(crc));
    {
        *pMsgSize = PayloadSize + UBX_FRAME_SIZE;
    }
    *ppMessage = pMsgBuf;
    return TRUE;
}

//! Check CRC of UBX packet
/*!
    \param pkMsg    pointer to beginning of UBX packet
    \return TRUE if CRC is correct, FALSE otherwise
*/
static BOOL UbxCheckCrc(IN U1 const * pkMsg)
{

    UBX_HEAD_t ubxhdr;
    memcpy(&ubxhdr, pkMsg, sizeof(ubxhdr));
    U2 crc =
        GetUbxChecksumU1((U1*)pkMsg+UBX_PREFIX_SIZE,
        ubxhdr.size+UBX_FRAME_SIZE-UBX_PREFIX_SIZE-UBX_CHKSUM_SIZE);
    //crc_b is in high byte
    U1 crc_a_packet = *(pkMsg+UBX_HEAD_SIZE+ubxhdr.size);
    U1 crc_b_packet = *(pkMsg+UBX_HEAD_SIZE+ubxhdr.size+1);
    U1 crc_a_calc = crc & 0x00FF;
    U1 crc_b_calc = (crc & 0xFF00) >> 8;
    return ((crc_a_packet == crc_a_calc) && (crc_b_packet == crc_b_calc));
}

BOOL UbxSearchMsg( IN  U1 *   pBuffer
                 , IN  size_t Size
                 , OUT U1 **  ppMsg)
{
    BOOL valid = FALSE;
    BOOL ubxSyncFound = FALSE;

    //discard all invalid bytes
    do
    {
        ubxSyncFound = FALSE;

        //search for UBX prefix
        while (Size > 2 &&
               ((*pBuffer     != UBX_SYNC_CHAR_1) ||
                (*(pBuffer+1) != UBX_SYNC_CHAR_2)) )
        {
            pBuffer ++;
            Size --;
        }

        if (Size > 2 &&
            ((*pBuffer     == UBX_SYNC_CHAR_1) &&
             (*(pBuffer+1) == UBX_SYNC_CHAR_2)))
        {
            ubxSyncFound = TRUE;
        }
        else
        {
            //not a whole sync sequence found, return
            //discard bytes where no sync was found in
            *ppMsg = pBuffer;
            return FALSE;
        }

        //everything before prefix is crap
        *ppMsg = pBuffer;

        //check if message is complete and valid
        if (ubxSyncFound && Size >= UBX_FRAME_SIZE)
        {
            //found message begin, this could be a valid message
            UBX_HEAD_t ubxhdr;
            memcpy(&ubxhdr, pBuffer, sizeof(ubxhdr));
            if ((ubxhdr.size + UBX_FRAME_SIZE) > 2*8192)
            {
                // the message length is corrupt, u-blox5 will not send a message
                // bigger than 16384 bytes, discard this message
                MESSAGE(MSG_WARN, "Corrupt Packet (0x%02X-0x%02X): msg-length %u > 2*8192",
                    ubxhdr.classId, ubxhdr.msgId, ubxhdr.size);
                pBuffer ++;
                *ppMsg = pBuffer;
                Size --;
            }
            if (Size >= ubxhdr.size + UBX_FRAME_SIZE)
            {
                //check CRC
                if (UbxCheckCrc(pBuffer))
                {
                    {
                        valid = TRUE;
                    }

                }
                else
                {
                    MESSAGE(MSG_WARN, "Packet (CLSID %02X-%02X): CRC-error", ubxhdr.classId, ubxhdr.msgId);
                    // discard faulty message from buffer
                    pBuffer ++;
                    *ppMsg = pBuffer;
                    Size --;
                }
            }
            else
            {
                // the message header is received completely, but the
                // payload is not complete. wait for it
                // MESSAGE(MSG_DBG, "msg incomplete: %d bytes/%d", Size, pHead->size+UBX_FRAME_SIZE);
                return FALSE;
            }
        }
        else
        {
            // either there are no more valid messages in buffer or the received
            // message is not complete yet.
            return FALSE;
        }
    } while (!valid);
    return valid;
}

