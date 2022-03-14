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
  \brief  Handle the update on the receiver

  This set of functions handles the update on the receiver and its progress
*/



#include <stdlib.h>
#include <assert.h>
#include "updateCore.h"

/*!
 * Determines if the parent process can be sent commands
 *
 * \param upd               handler
 * \return TRUE if parent interested in commands. FALSE if not.
 */
static BOOL CanSendParentCommands(UPD_CORE_t *upd)
{
    return ((verbose < 2) && (upd->Rx->mPortHandle->type == STDINOUT));
}

/*!
 * Send one flash packet write command to the receiver.
 *
 * \param upd               handler
 * \param Packet            packet number to be written
 * \return TRUE if successful
 */
static BOOL updSendWrite(UPD_CORE_t *upd, U4 Packet)
{
    assert(upd);
    U4 tgtAddr = upd->FwBase + Packet * PACKETSIZE;
    U1* srcAddr = (U1*)upd->pData + Packet * PACKETSIZE;
    U4 WriteSize = PACKETSIZE;

    if (upd->ImageSize < ((Packet+1) * PACKETSIZE))
    {
        //the last packet can be a cut-off packet
        WriteSize = upd->ImageSize - Packet * PACKETSIZE;
    }

    U4 PayloadLength = WriteSize + 8 /*Addr, Size*/;

    CH* pSendData = (CH*) malloc(sizeof(CH)*PayloadLength);
    if (pSendData == NULL)
    {
        MESSAGE(MSG_ERR, "Alloc failed");
        return FALSE;
    }
    memset(pSendData, 0xFF, PayloadLength);
    memcpy(pSendData+0, &tgtAddr,   4); //Address
    memcpy(pSendData+4, &WriteSize, 4); //Data size
    //copy data to send buffer
    memcpy(pSendData+8, srcAddr, WriteSize);

    BOOL success = rcvSendMessage(upd->Rx, UBX_CLASS_UPD, UBX_UPD_FLWRI, pSendData, PayloadLength);
    free(pSendData);

    return success;
}

/*!
 * Dump the current progress of the update. The output is done
 * if either the DUMPINTERVAL timeout expired or the force
 * parameter is TRUE. Note that verbose has to be larger than 1
 * for this output to be generated
 *
 * \param upd               handler
 * \param force         force the output ignoring the timeout
 */
static void updDumpAck(UPD_CORE_t *upd, BOOL force)
{
    assert(upd);

    if ((verbose>1) && ((TIME_GET() - upd->sLastDumpTime) > DUMPINTERVAL || force))
    {
        CONSOLE_RESTORE_POS();
        upd->sLastDumpTime = TIME_GET();
        I4 pos = 0;
        CH substr[CONSOLE_WIDTH];
        substr[CONSOLE_WIDTH - 1] = 0;
        I4 numPacketsOverall = (upd->NumberSectors == 0)?upd->NumberPackets:GetPacketNrForSector(upd->NumberSectors, upd->FlashOrg, PACKETSIZE);
        while (pos < numPacketsOverall)
        {
            memset(substr, 0, CONSOLE_WIDTH - 1);
            // first part is from pWriteState
            U4 lenPackets = MAX(0, MIN(upd->NumberPackets-pos, CONSOLE_WIDTH - 1));
            memcpy(substr, upd->pWriteState+pos, lenPackets);
            // second part is from pEraseState -- remind, it's sectors in this array!
            U4 lenRemain = CONSOLE_WIDTH - 1 - lenPackets;
            if (lenRemain)
            {
                U4 idx = lenPackets;
                for (; idx < CONSOLE_WIDTH - 1; idx++)
                {
                    U4 packOffset = (pos+idx) * PACKETSIZE;
                    if (packOffset >= upd->FlashSize)
                    {
                        break;
                    }
                    U4 packAddr = packOffset + upd->FwBase;
                    I4 packSect = GetSectorNrForAddress(packAddr, upd->FwBase, 0, upd->FlashOrg);
                    if (packSect == -1)
                    {
                        MESSAGE(MSG_DBG, "could not get sector for addr 0x%08X", packAddr);
                        break;
                    }
                    if (packSect < upd->NumberSectors)
                    {
                        CH secState = upd->pEraseState[packSect];
                        substr[idx] = secState;
                    }
                }
            }

            MESSAGE_PLAIN("%s\n", substr);
            pos += (CONSOLE_WIDTH - 1);
        }
        U1 maxWrRetries    = 0;
        U4 maxWrRetriesCnt = 0;
        U1 maxErRetries    = 0;
        U4 maxErRetriesCnt = 0;
        I4 packet;
        for(packet = 0; packet < upd->NumberPackets; packet++)
        {
            if (maxWrRetries < upd->pWriteRetryCnt[packet])
            {
                maxWrRetries = upd->pWriteRetryCnt[packet];
                maxWrRetriesCnt = 0;
            }
            else if (maxWrRetries == upd->pWriteRetryCnt[packet] &&
                     maxWrRetries)
            {
                maxWrRetriesCnt++;
            }
        }
        I4 sector;
        for(sector = 0; sector < upd->NumberSectors; sector++)
        {
            if (maxErRetries < upd->pEraseRetryCnt[sector])
            {
                maxErRetries = upd->pEraseRetryCnt[sector];
                maxErRetriesCnt = 0;
            }
            else if (maxErRetries == upd->pEraseRetryCnt[sector] &&
                     maxErRetries)
            {
                maxErRetriesCnt++;
            }
        }
        MESSAGE_PLAIN("max retries: e:%2u(%3u) w:%2u(%3u)", maxErRetries, (int)maxErRetriesCnt,
            maxWrRetries, (int)maxWrRetriesCnt);
        MESSAGE_PLAIN("\n\n");
    }
}

/*!
 * Receive data from the receiver and process it
 *
 * \param upd               handler
 * \return TRUE if successful
 */
static BOOL updProcessMessages(UPD_CORE_t *upd)
{
    assert(upd);
    for(;;)
    {
        UBX_HEAD_t *msg = rcvReceiveMessage(upd->Rx, 0, UBX_CLASS_UPD, -1);
        if(msg == NULL)
        {
            TIME_SLEEP(1);
            return TRUE;
        }
        if (msg->size == UBX_UPD_ERASE_DATA1_PAYLOAD_SIZE && msg->msgId == UBX_UPD_ERASE)
        {
            U4 Address;
            memcpy(&Address, ((U1*)msg)+UBX_HEAD_SIZE, 4);
            I4 Sector = GetSectorNrForAddress(Address, upd->FwBase, 0, upd->FlashOrg);
            U1 Success;
            memcpy(&Success, ((U1*)msg)+UBX_HEAD_SIZE+4, 1);
            if ((Sector != -1) && Success)
            {
                if(upd->pEraseState[Sector] != ACK_ERASE_ACK)
                {
                    upd->pEraseState[Sector] = ACK_ERASE_ACK;
                    U4 begin = GetPacketNrForSector(Sector, upd->FlashOrg, PACKETSIZE);
                    U4 end = GetPacketNrForSector(Sector+1, upd->FlashOrg, PACKETSIZE);
                    //flag packets to be acknowledged erased
                    U4 packet;
                    for(packet = begin; packet < end && packet < (U4)upd->NumberPackets; packet++)
                    {
                        upd->pWriteState[packet] = ACK_ERASE_ACK;
                    }
                    upd->PendingErases--;
                    if (CanSendParentCommands(upd))
                    {
                        MESSAGE_PLAIN("<INF>ERASE_ACK %i<\\INF>", Sector);
                    }
                }
                updDumpAck(upd, FALSE);
            }
            else
            {
                MESSAGE(MSG_ERR, "Possibly defect flash (erase failed), sector %02d at 0x%08X",  Sector, Address);
                //don't wait for timeout, immediately retry
                upd->pEraseRetryCnt[Sector]++;
                upd->pEraseState[Sector] = ACK_INIT;
            }
        }
        else if (msg->size == UBX_UPD_FLWRI_DATA1_PAYLOAD_SIZE && msg->msgId == UBX_UPD_FLWRI)
        {
            U4 Address;
            memcpy(&Address, ((U1*)msg)+UBX_HEAD_SIZE, 4);
            I4 Packet = GetPacketNrForAddress(Address, upd->FwBase, PACKETSIZE);
            U1 Success;
            memcpy(&Success, ((U1*)msg)+UBX_HEAD_SIZE+4, 1);
            if (Packet != -1)
            {
                if (Success)
                {
                    if (CanSendParentCommands(upd))
                    {
                        MESSAGE_PLAIN("<INF>WRITE_ACK %i<\\INF>", Packet);
                    }
                    //flag packet to be acknowledged written if we got the erase acked before
                    if (upd->pWriteState[Packet] == ACK_WRITE_SENT ||    //as expected
                        upd->pWriteState[Packet] == ACK_WRITE_ACK)       //written twice successfully
                    {
                        upd->pWriteState[Packet] = ACK_WRITE_ACK;
                        if(upd->PendingWrites)
                        {
                            upd->PendingWrites--;
                        }
                    }
                    else
                    {
                        MESSAGE(MSG_WARN, "Packet %03d (0x%08X) W rec but %c",
                            Packet, Address, upd->pWriteState[Packet]);
                    }
                    updDumpAck(upd, FALSE);
                }
                else
                {
                    //write failed, don't retry -> flash seems to be corrupt
                    MESSAGE(MSG_ERR, "Defect flash (write failed) in range 0x%08X:0x%08X",
                        Address, Address+PACKETSIZE);
                    free(msg);
                    return FALSE;
                }
            }
            else
            {
                MESSAGE(MSG_ERR, "Received ack for unknown packet");
            }
        }
        else if (msg->size == UBX_UPD_CERASE_DATA1_PAYLOAD_SIZE && msg->msgId == UBX_UPD_CERASE)
        {
            if (upd->eraseInProgres)
            {
                if (((U1*)msg)[UBX_HEAD_SIZE] != 1)
                {
                    MESSAGE(MSG_ERR, "Chip erase failed");
                    return FALSE;
                }
                else
                {
                    upd->eraseInProgres = FALSE; // erase finished
                }
            }
        }
        free(msg);
    }
}

/*!
 * Send one erase sector command to the receiver.
 * Either a new erase command or an erase command for a sector
 * with expired timeout is sent.
 *
 * \param upd               handler
 * \return TRUE if successful
 */
static BOOL updEraseSector(UPD_CORE_t *upd)
{
    assert(upd);

    BOOL foundLastErased = FALSE;
    I4 sector;
    for(sector = upd->erasedUntil; sector < upd->NumberSectors; sector++)
    {
        //map the sector to the packet number
        I4 packetNr = GetPacketNrForSector(sector, upd->FlashOrg, PACKETSIZE);
        if (packetNr == -1)
        {
            MESSAGE(MSG_ERR, "ERASE: Invalid PacketNr determined for Sector.");
            return FALSE;
        }
        if (upd->pEraseState[sector] != ACK_ERASE_ACK)
        {
            foundLastErased = TRUE;
            U4 now = TIME_GET();
            //if we sent a erase but did not get an ack within timeout, decrement
            //pending erase counter to be able to send the next erase
            if ( (upd->pEraseState[sector] == ACK_ERASE_SENT) &&
                (upd->pEraseTimeout[sector] <= now) )
            {
                upd->PendingErases--;
                if (++upd->pEraseRetryCnt[sector] > ERASE_RETRIES)
                {
                    MESSAGE(MSG_ERR, "Erase retries for sector %d exceeded.", sector);
                    return FALSE;
                }
                MESSAGE(MSG_WARN, "Sending erase retry for sector %d", sector);
            }
            // don't overflow the receiver
            if ( (upd->PendingErases < MAX_PENDING_ERASES) &&
                (upd->pEraseTimeout[sector] <= now) )
            {
                updDumpAck(upd, FALSE);

                U4 Packet = GetPacketNrForSector(sector, upd->FlashOrg, PACKETSIZE);
                U4 Address = upd->FwBase + Packet * PACKETSIZE;
                if ( rcvSendMessage(upd->Rx, UBX_CLASS_UPD, UBX_UPD_ERASE, (CH*)&Address, 4) )
                {
                    upd->PendingErases++;
                    upd->pEraseTimeout[sector] = TIME_GET() + ERASE_TIMEOUT;
                    upd->pEraseState[sector]   = ACK_ERASE_SENT;
                    if (packetNr < upd->NumberPackets)
                    {
                        upd->pWriteState[packetNr] = ACK_ERASE_SENT;
                    }
                    if (CanSendParentCommands(upd))
                    {
                        MESSAGE_PLAIN("<INF>ERASE %i %i<\\INF>", sector, upd->NumberSectors);
                    }
                }
                else
                {
                    MESSAGE(MSG_ERR, "SendErase failed.");
                    return FALSE;
                }
            }
        }
        else if( !foundLastErased )
        {
            upd->erasedUntil = sector + 1;
        }
    }
    return TRUE;
}


/*!
 * Send one flash write packet command to the receiver (calls updSendWrite).
 * Either a new write command or a write command for a packet with
 * expired timeout is sent.
 *
 * \param upd               handler
 * \return TRUE if successful
 */
static BOOL updWritePacket(UPD_CORE_t *upd)
{
    assert(upd);

    //check each packet
    const U4 timeout = upd->eraseInProgres ? CHIP_ERASE_TIMEOUT : WRITE_TIMEOUT;
    I4 packet = upd->writtenUntil;
    BOOL foundLastWritten = FALSE;
    BOOL sent = FALSE;
    // send the not yet sent write packets
    // don't overflow the receiver
    while (packet < upd->NumberPackets && !sent && upd->PendingWrites < upd->MaxPendingWritesNum)
    {
        // check for written packets
        if( upd->pWriteState[packet] == ACK_WRITE_ACK && !foundLastWritten )
        {
            upd->writtenUntil = packet+1;
        }
        else
        {
            foundLastWritten = TRUE;
            // check for unwritten packets
            if (upd->pWriteState[packet] == ACK_ERASE_ACK)
            {
                // send the download packet to receiver
                upd->pWriteState[packet] = ACK_WRITE_SENT;
                if (updSendWrite(upd, packet))
                {
                upd->PendingWrites++;
                    upd->pWriteTimeout[packet] = TIME_GET() + timeout;
                    sent = TRUE; //do not send further packets in this loop
                    if (CanSendParentCommands(upd))
                    {
                        MESSAGE_PLAIN("<INF>WRITE %i %i<\\INF>", packet, upd->NumberPackets);
                    }
                }
                else
                {
                    MESSAGE(MSG_ERR, "SendWrite failed.");
                    return FALSE;
                }
            }
        }
        packet++;
    }
    // send the timeouted write packets
    packet = 0;
    while (packet < upd->NumberPackets && !sent)
    {
        U4 acttime = TIME_GET();
        // check for timeouted packets
        if (upd->pWriteState[packet]    == ACK_WRITE_SENT &&
            upd->pWriteTimeout[packet]  <= acttime)
        {
            if (upd->PendingWrites)
            {
                --upd->PendingWrites;
            }
            upd->pWriteRetryCnt[packet]++;
            if (upd->pWriteRetryCnt[packet] > WRITE_RETRIES)
            {
                MESSAGE(MSG_ERR, "Write retries for packet %d exceeded.", packet);
                return FALSE;
            }
            if (upd->PendingWrites < upd->MaxPendingWritesNum)
            {
                // send the download packet to receiver
                upd->pWriteState[packet] = ACK_WRITE_SENT;
                if (updSendWrite(upd, packet))
                {
                    upd->PendingWrites++;
                    upd->pWriteTimeout[packet] = TIME_GET() + timeout;
                    sent = TRUE; //do not send further packets in this loop
                    if (CanSendParentCommands(upd))
                    {
                        MESSAGE_PLAIN("<INF>WRITE_AGAIN %i %i<\\INF>", packet, upd->NumberPackets);
                    }
                }
                else
                {
                    MESSAGE(MSG_ERR, "SendWrite failed.");
                    return FALSE;
                }
            }
        }
        packet++;
    }

    return TRUE;
}

UPD_CORE_t* updInit( RCV_DATA_t *rx
                   , I4 numberSectors
                   , I4 numberPackets
                   , BLOCK_ARR_t *flashOrg
                   , U4 flashSize
                   , U4 MaxPendingWritesNum
                   , BOOL eraseInProgres)
{
    assert(rx);

    UPD_CORE_t *upd= (UPD_CORE_t*) malloc(sizeof(*upd));
    if(!upd)
        return NULL;

    memset(upd, 0, sizeof(*upd));

    upd->Rx = rx;

    // we did not yet erase / write anything
    upd->erasedUntil = 0;
    upd->writtenUntil = 0;

    upd->NumberSectors = numberSectors;
    upd->NumberPackets = numberPackets;

    upd->FlashOrg = flashOrg;
    upd->FlashSize = flashSize;

    upd->MaxPendingWritesNum = MaxPendingWritesNum;
    upd->eraseInProgres = eraseInProgres;
    upd->PendingErases  = 0;
    upd->PendingWrites  = 0;
    upd->sLastDumpTime  = 0;

    upd->pEraseTimeout  = (U4*) malloc(sizeof(U4)*upd->NumberSectors);
    upd->pEraseRetryCnt = (U1*) malloc(sizeof(U1)*upd->NumberSectors);
    upd->pEraseState    = (CH*) malloc(sizeof(CH)*upd->NumberSectors);

    upd->pWriteTimeout  = (U4*) malloc(sizeof(U4)*upd->NumberPackets);
    upd->pWriteRetryCnt = (U1*) malloc(sizeof(U1)*upd->NumberPackets);
    upd->pWriteState    = (CH*) malloc(sizeof(CH)*upd->NumberPackets);



    if( !upd->pEraseTimeout
     || !upd->pEraseRetryCnt
     || !upd->pEraseState
     || !upd->pWriteTimeout
     || !upd->pWriteRetryCnt
     || !upd->pWriteState )
    {
        updDeinit(upd);
        return NULL;
    }

    memset(upd->pEraseTimeout,  0,        upd->NumberSectors*sizeof(U4));
    memset(upd->pEraseRetryCnt, 0,        upd->NumberSectors*sizeof(U1));
    memset(upd->pEraseState,    ACK_INIT, upd->NumberSectors*sizeof(CH));

    memset(upd->pWriteTimeout,  0,        upd->NumberPackets*sizeof(U4));
    memset(upd->pWriteRetryCnt, 0,        upd->NumberPackets*sizeof(U1));
    memset(upd->pWriteState,    (upd->NumberSectors == 0)?ACK_ERASE_ACK:ACK_INIT, upd->NumberPackets*sizeof(CH));

    MESSAGE(MSG_LEV1, "Receiver info collected, downloading to flash...");

    if(verbose > 1)
    {
        I4 numPacketsOverall = (upd->NumberSectors == 0)?upd->NumberPackets:GetPacketNrForSector(upd->NumberSectors, upd->FlashOrg, PACKETSIZE);

        // compute the number of lines needed to do a full dump
        // CONSOLE_WIDTH - 1 to make sure that the newline character is not
        // placed on a new line (seen in windows 7)
        U4 numberOfLines = numPacketsOverall / (CONSOLE_WIDTH - 1);
        if(numPacketsOverall % (CONSOLE_WIDTH - 1) != 0)
            ++numberOfLines;

        // for the max retries and the newline
        numberOfLines += 2;
        CONSOLE_SAVE_POS(numberOfLines);
    }
    return upd;
}

void updDeinit(UPD_CORE_t *upd)
{
    assert(upd);

    free(upd->pEraseTimeout);
    free(upd->pEraseRetryCnt);
    free(upd->pEraseState);

    free(upd->pWriteTimeout);
    free(upd->pWriteRetryCnt);
    free(upd->pWriteState);

    free(upd);
}

BOOL updUpdate(UPD_CORE_t *upd, FWHEADER_t* data, size_t size, U4 fwBase)
{
    assert(upd);

    upd->pData = data;
    upd->ImageSize = size;
    upd->FwBase = fwBase;

    BOOL writeComplete = (upd->NumberPackets == 0);
    BOOL eraseComplete = (upd->NumberSectors == 0);
    updDumpAck(upd, TRUE);
    // loop around until everything is written and erased
    while( !writeComplete || !eraseComplete )
    {
        if( !eraseComplete )
        {
            // try to send an erase command
            if( !updEraseSector(upd) )
                return FALSE;

            updDumpAck(upd, FALSE);

            // receive messages
            if( !updProcessMessages(upd) )
                return FALSE;

            // check if everything is erased completely
            eraseComplete = TRUE;
            I4 sector;
            for(sector = 0; sector < upd->NumberSectors; sector++)
            {
                if (upd->pEraseState[sector] != ACK_ERASE_ACK)
                {
                    eraseComplete = FALSE;
                    break;
                }
            }
        }
        if( !writeComplete )
        {
            // try to send write command
            if (!updWritePacket(upd))
            {
                return FALSE;
            }
            updDumpAck(upd, FALSE);

            // receive messages
            if( !updProcessMessages(upd) )
            {
                return FALSE;
            }

            // check if everything was written completely
            writeComplete = TRUE;
            I4 pack;
            for(pack = 0; pack < upd->NumberPackets; pack++)
            {
                if (upd->pWriteState[pack] != ACK_WRITE_ACK)
                {
                    writeComplete = FALSE;
                    break;
                }
            }
        }
    }
    updDumpAck(upd, TRUE);
    if (upd->eraseInProgres)
    {
        UBX_HEAD_t* cErase = rcvReceiveMessage(upd->Rx, CHIP_ERASE_TIMEOUT, UBX_CLASS_UPD, UBX_UPD_CERASE);
        if (cErase == NULL)
        {
            MESSAGE(MSG_ERR, "Chip erase timed out");
            return FALSE;
        }
        if (cErase->size != 1)
        {
            if (((U1*)cErase)[UBX_HEAD_SIZE] == 1)
            {
                MESSAGE(MSG_DBG, "Chip erase finished later than update proces");
            }
            else
            {
                MESSAGE(MSG_ERR, "Chip erase failed");
                return FALSE;
            }
        }
    }
    return TRUE;
}
