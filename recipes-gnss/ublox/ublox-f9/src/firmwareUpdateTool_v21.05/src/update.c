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
  \brief  Firmware update process

  This is the core of the firmware update which specifies the update procedure.

  \note If the receiver has ROM version 03 or 04 AND it is directly
  connected to and communicating with the host via USB (no USB-hub in between),
  this update process will not work as stated below. If you use such a setup and
  need to implement the update for ROM03 on your own, please contact the support
  at u-blox to receive further information on this task.
  Note that this is only necessary for ROM03. From ROM04 on the process can be
  implemented as it is stated below in the example code.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "platform.h"

#include "update.h"
#include "image.h"
#include "flash.h"
#include "mergefis.h"
#include "version.h"
#include "updateCore.h"
#include "checksum.h"



#define HW_IMG_MAGIC_DOM    ( ('U' <<  0) | ('B' <<  8) | ('X' << 16) | ('8' << 24) ) //!< EXT image magic word for DOM
static MERGEFIS_RETVAL_t getNoFisMergingData(char **fis, size_t *fisSize, RCV_DATA_t * rx)
{
    if (!fis || !fisSize || !rx)
        return MERGEFIS_UNKNOWN;

    MERGEFIS_RETVAL_t ret = MERGEFIS_UNKNOWN;
    UBX_HEAD_t *fisMsg = rcvPollMessage(rx, UBX_CLASS_UPD, UBX_UPD_FIS, 0, 0, POLL_TIMEOUT);

    if (!fisMsg)
    {
        MESSAGE(MSG_ERR, "FIS read timed out");
    }
    else
    {
        if (fisMsg->size == sizeof(DRV_SPI_MEM_FIS_t) ||
            fisMsg->size == sizeof(DRV_SPI_MEM_FIS_t) - sizeof(U4))
        {
            size_t tmpFisSize = sizeof(DRV_SPI_MEM_FIS_t);
            char *tmpFis = malloc(tmpFisSize);
            if (tmpFis)
            {
                memcpy(tmpFis, (U1*)((U1*)fisMsg + UBX_HEAD_SIZE), tmpFisSize);
                if (tmpFis[REVISION_POSITION_VER3] == 0x00
                    && tmpFis[REVISION_POSITION_VER3 + 1] == 0x00
                    && tmpFis[REVISION_POSITION_VER3 + 2] == 0x00
                    && tmpFis[REVISION_POSITION_VER3 + 3] == 0x00)
                { // information is from SFDP structure override major and minor revision
                    tmpFis[MAJOR_REV_POSITION] = 3;
                    tmpFis[MINOR_REV_POSITION] = 0;
                }
                ret = MERGEFIS_OK;
                *fis = tmpFis;
                *fisSize = tmpFisSize;
            }
        }
        else
        {
            MESSAGE(MSG_ERR, "Received unexpected answer.");
        }
        free(fisMsg);
    }


    return ret;
}

//! Extracts the receiver hardware and software generation
/*!
    \param  monVer      Pointer to the UBX-MON-VER content
    \return             Hardware generation of the receiver

    \note   Don't use the firmware revision (number in brackets) to identify
            an upcoming firmware version as this number is not guaranteed to
            increment over time
*/
static int extractHwGeneration(UBX_HEAD_t* monVer)
{
    // check if there is a string delimiting zero to prevent printing
    // too much if it is not a string that is at this place
    CH* pSwVer = (CH*)((U1*)(monVer)+UBX_HEAD_SIZE);
    CH* pChar  = pSwVer;
    while (*pChar && (U1*)pChar < ((U1*)(monVer)+UBX_HEAD_SIZE+monVer->size))
    {
        pChar++;
    }

    if (*pChar==0)
    {
        MESSAGE(MSG_DBG, "Receiver currently running SW '%s'", pSwVer);
        // get HW version
        CH* pHwVer = (CH*)((U1*)(monVer)+UBX_HEAD_SIZE+30);
        pChar  = pHwVer; // pointer to HW ver string
        while (*pChar && (U1*)pChar < ((U1*)(monVer)+UBX_HEAD_SIZE+monVer->size))
        {
            pChar++;
        }
        if (*pChar==0)
        {
            U4 generation =
                (strncmp(pHwVer, "00190000", 9) == 0) ? 90 :
                (strncmp(pHwVer, "00080000", 9) == 0) ? 80 :
                (strncmp(pHwVer, "00070000", 9) == 0) ? 70 :
                (strncmp(pHwVer, "00040007", 9) == 0) ? 60 :
                (strncmp(pHwVer, "00040006", 9) == 0) ? 51 :
                (strncmp(pHwVer, "00040005", 9) == 0) ? 50 :
                0;
            MESSAGE(MSG_DBG, "Receiver HW '%s', Generation %d.%d",
                    pHwVer, generation/10, generation%10);
            return generation;
        }
        else
        {
            MESSAGE(MSG_WARN, "HW Version information seems corrupt!");
        }
    }
    else
    {
        MESSAGE(MSG_WARN, "SW Version information seems corrupt!");
    }
    return 0;
}



static BOOL verifyImage(RCV_DATA_t *pRx, FWHEADER_t const *pData, U4 fileSize, U4 address, BOOL updateRam, U4 version)
{
    assert(pRx && pData);

    U4* pImage = (U4*)pData;
    U4 imageSize = fileSize;
    // compute a fletcher32 checksum of the image as loaded to the flash
    // we don't want to care about the checksums done internally by the FW here...
    U4 a = 0;
    U4 b = 0;
    GetUbxChecksumU4(&a, &b, pImage, imageSize);

    // build the payload to do the same on the hardware
    U4 dataAligned[4] = { address, imageSize, a, b };
    UBX_HEAD_t *msg;
    if (version >= 2)
    {
        //              version region
        U1 data[18] = { 0x01, !updateRam };
        memcpy(&data[2], dataAligned, sizeof(dataAligned));
        // check the CRC on the receiver
        msg = rcvPollMessage(pRx, UBX_CLASS_UPD, UBX_UPD_CRC, (CH*)data, sizeof(data), CRC_TIMEOUT);
    }
    else
    {
        // check the CRC on the receiver
        msg = rcvPollMessage(pRx, UBX_CLASS_UPD, UBX_UPD_CRC, (CH*)dataAligned, sizeof(dataAligned), CRC_TIMEOUT);
    }
    if (msg == NULL)
    {
        MESSAGE(MSG_ERR, "Polling verify message failed");
        return FALSE;
    }

    U1 success = 0;
    if (msg->size == 5)
    {
        memcpy(&success, (U1*)(msg)+UBX_HEAD_SIZE + 4, 1);
    }

    free(msg);
    return success ? TRUE : FALSE;
}

static void doReset(RCV_DATA_t *pRx, BOOL reset)
{
    assert(pRx);
    if (reset)
    {
        MESSAGE(MSG_LEV1, "Rebooting receiver");
        if( !rcvSendMessage(pRx, UBX_CLASS_UPD, UBX_UPD_RBOOT, NULL, 0) )
        {
            MESSAGE(MSG_WARN, "reboot failed");
        }
    }
}

static BOOL updateImageToRam(RCV_DATA_t* pRx, const CH* pImageStart, U4 ImageSize)
{
    APP_UBX_UPD_IMG_PAYLOAD_t imgPayload;
    U2 sent = 0;
    U sendStart = 0;
    U acked = 0;
    U timeLimit = TIME_GET() + POLL_TIMEOUT;
    BOOL success = FALSE;
    if (pRx == NULL || pImageStart == NULL)
    {
        return FALSE;
    }
    while ((acked * sizeof(imgPayload.chunkData)) < ImageSize)
    {
        // check if less than 5 packets are pending (receiver only allocates 4*1000butes, 1 buffer has to be always free to avoid rxBuf alloc, One message is 514bytes)
        if ((sent - acked) < 5 && (sendStart < ImageSize))
        {
            const size_t packetSize = sizeof(imgPayload.chunkData);
            size_t sendSize;
            if ((sendStart + packetSize) > ImageSize)
            {
                sendSize = ImageSize - sendStart;
            }
            else
            {
                sendSize = packetSize;
            }
            // send one packet
            imgPayload.chunkNum = sent;

            memcpy(imgPayload.chunkData, (CH*)pImageStart + sendStart, sendSize);
            if (sendSize < packetSize)
            {
                // clear the rest of the buffer
                memset(&imgPayload.chunkData[sendSize], 0, packetSize - sendSize);
            }
            if (rcvSendMessage(pRx, UBX_CLASS_UPD, UBX_UPD_IMG, (CH*)&imgPayload, sizeof(imgPayload)) != 1)
            {
                MESSAGE(MSG_ERR, "Chunk %d not accepted by receiver", sent);
                break;
            }
            sent++;
            sendStart += packetSize;
        }

        // check if we received an ack
        UBX_HEAD_t *ack = rcvReceiveMessage(pRx, 0, UBX_CLASS_ACK, -1);
        if (ack != NULL)
        {
            UBX_ACK_ACK_t *pAck = (UBX_ACK_ACK_t*)((U1*)ack + UBX_HEAD_SIZE);
            if (pAck->clsId == UBX_CLASS_UPD &&
                pAck->msgId == UBX_UPD_IMG)
            {
                if (ack->msgId != UBX_ACK_ACK)
                {
                    MESSAGE(MSG_ERR, "Received NACK for chunk %d ", (acked+1));
                    break;
                }

                // we received an ack message for one UPD-IMG message
                // push out the timeout
                timeLimit = TIME_GET() + POLL_TIMEOUT;
                acked++;
            }
            else
            {
                MESSAGE(MSG_WARN, "Received unexpected (N)ACK");
            }
            free(ack);
        }

        // check if we timed out
        if (TIME_GET() > timeLimit)
        {
            MESSAGE(MSG_ERR, "Downloading to RAM timed out. sent %u, acked %u", sent, acked);
            break;
        }
    }
    // check if we finished
    if ((acked * sizeof(imgPayload.chunkData)) >= ImageSize)
    {
        success = TRUE;
    }
    return success;
}


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
                    IN const BOOL           fisOnly)
{
    FWHEADER_t* pData = NULL;
    size_t fileSize = 0;
    //'verbose' is globally declared in platform.h
    verbose = Verbose;
    BOOL success = FALSE;
    BOOL isUsbPort = FALSE;
    BOOL rcvConnected = FALSE;
    BOOL eraseInProgres = FALSE;
    BOOL flashNotNeeded = FALSE;
    BOOL isSpiPort = FALSE;
    U4 generation = 0;
    U4 FwBase = 0;
    U4 imageGeneration = 0;
    RCV_DATA_t rx;
    FWFOOTERINFO_t fwFooter={0};
    BLOCK_ARR_t FlashOrg;
    memset(&FlashOrg, 0, sizeof(FlashOrg));

    UPD_CORE_t *upd=NULL;
    if (updateRam != 0)
    {
        flashNotNeeded = TRUE;
    }
    MESSAGE(MSG_LEV0, "u-blox Firmware Update Tool version %s", PRODUCTVERSTR);

    do
    {
        if (!EraseOnly && !fisOnly)
        {
            MESSAGE(MSG_LEV0, "Updating Firmware '%s' of receiver over '%s'",
                    BinaryFileName, ComPort);
            MESSAGE(MSG_DBG, "Opening and buffering image file");
            if (!OpenAndBufferFile(BinaryFileName, &pData, &fileSize))
            {
                break;
            }
            MESSAGE(MSG_DBG, "Verifying image");
            //we got the file content, check if it is a valid image
            imageGeneration = ValidateImage(pData, fileSize, &fwFooter);

            if (imageGeneration == 0)
            {
                MESSAGE(MSG_ERR, "Image not valid.");
                break;
            }

        }




        /***************************************************
         * connect to the receiver with the given baudrate *
         ***************************************************/
            rcvConnected = rcvConnect(&rx, ComPort, Baudrate);

        if (!rcvConnected)
            break;


        /***************************************************
         * try to communicate with the receiver (MON-VER)  *
         ***************************************************/
        if(TrainingSequence)
            rcvSendTrainingSequence(&rx);

        UBX_HEAD_t* monVer = DoAutobaud ?
            rcvDoAutobaud(&rx, TrainingSequence) :
            rcvPollMessage(&rx, UBX_CLASS_MON, UBX_MON_VER, NULL, 0, POLL_TIMEOUT);
        if( monVer == NULL )
        {
            MESSAGE(MSG_ERR, "Version poll failed.");
            break;
        }
        MESSAGE(MSG_DBG, "Received Version information");
        generation = extractHwGeneration(monVer);
        free(monVer);
        U4 romSize =
            (generation == 50) ? 384*1024 : // 384kB ROM in u-blox5
            (generation == 51 ||
             generation == 60) ? 448*1024 : // 448kB ROM in G51 && u-blox6
            (generation == 70) ? 512*1024 : // 512kB ROM in u-blox7
            (generation == 80) ? 544*1024 : // 544kB ROM in u-blox8
            (generation == 90) ? 672*1024 : // 650kB ROM in u-blox9
             0;

        // check for valid ROM size not needed for u-blox10
        if ( (!romSize) && (generation < 100) )
        {
            MESSAGE(MSG_ERR, "Could not get correct ROM size");
            break;
        }





        /***************************************************
         * read the CRC of the ROM                         *
         ***************************************************/
        U4 crcVal = 0x66666666;
        if (generation >= 90 && imageGeneration >= 91)
        {
            UBX_HEAD_t* romCrc = NULL;
            FwBase = (updateRam != 0) ? RAM_BASE : sizeof(DRV_SPI_MEM_FIS_t);
            MESSAGE(MSG_DBG, "Sending ROM CRC Poll");
            romCrc = rcvPollMessage(&rx, UBX_CLASS_UPD, UBX_UPD_ROM, NULL, 0, POLL_TIMEOUT);
            if (romCrc == NULL)
            {
                MESSAGE(MSG_ERR, "Could not get ROM CRC");
                break;
            }
            else
            {
                memcpy(&crcVal, (U1*)romCrc + UBX_HEAD_SIZE + 2 * sizeof(U4), sizeof(crcVal));
                free(romCrc);
            }
        }
        else
        {
            BOOL fail = TRUE;
            U4 CRCSize = 4;
            U4 romBase = (generation >= 70) ? 0x00000000 : 0x00200000;
            U4 payload[3] = { romBase + romSize - CRCSize, CRCSize, 0 };
            U1 payloadAuth[] = { 0x9C, 0x59, 0xC5, 0x22, 0xEC, 0x34, 0x1A, 0x1A, 0x30, 0xCC, 0xB1, 0xFB, 0x69, 0xCB, 0xAD, 0x9A, 0x41, 0x83, 0x6E, 0xDD, 0x27, 0xE4, 0xFB, 0xA6, 0x8C, 0x71, 0xE3, 0xAB, 0x8A, 0xA4, 0x0D, 0x20, 0xFC, 0x7F, 0x08, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // Read u-blox8 CRC auth
            U4 retryCount;
            FwBase = (pData) ? pData->v1.pBase : FLASH_BASE;
            MESSAGE(MSG_DBG, "Sending ROM CRC-Mix-Read");
            for (retryCount = 0; (retryCount < RETRY_COUNT) && fail; retryCount++)
            {
                if (!rcvSendMessage(&rx, UBX_CLASS_UPD, UBX_UPD_UPLOAD, (CH *)payload, sizeof(payload)) ||
                    !rcvSendMessage(&rx, UBX_CLASS_UPD, UBX_UPD_AUTHREAD, (CH *)payloadAuth, sizeof(payloadAuth)))
                {
                    break; // fail
                }
                const U4 TOTIME = TIME_GET() + POLL_TIMEOUT; // compute the deadline
                do
                {
                    U4 now = TIME_GET();
                    UBX_HEAD_t* msg = rcvReceiveMessage(&rx, TOTIME > now ? TOTIME - now : 0, UBX_CLASS_UPD, -1);
                    if (msg == NULL)
                    {
                        break;
                    }
                    if (msg->msgId == UBX_UPD_UPLOAD)
                    {
                        memcpy(&crcVal, (U1*)msg + UBX_HEAD_SIZE + 12, sizeof(crcVal));
                        fail = FALSE;
                    }
                    else if (msg->msgId == UBX_UPD_AUTHREAD)
                    {
                        memcpy(&crcVal, (U1*)msg + UBX_HEAD_SIZE + sizeof(payloadAuth), sizeof(crcVal));
                        fail = FALSE;
                    }
                    free(msg);
                } while ((TIME_GET() < TOTIME) && fail);
            }
            if(fail)
            {
                MESSAGE(MSG_ERR, "Could not get ROM CRC");
                break;
            }

        }
        MESSAGE(MSG_LEV2, "ROM CRC: 0x%08X", crcVal);
        U4 hwRomVer = (crcVal == 0x00000000) ? 200 : // u-blox5
                      (crcVal == 0xE046F6C8) ? 300 :
                      (crcVal == 0x3CB3E4FF) ? 400 :
                      (crcVal == 0x806AF596) ? 500 :
                      (crcVal == 0xEA00D0BD) ? 510 :
                      (crcVal == 0xB5CD6FC1) ? 600 : // u-blox6
                      (crcVal == 0x2BA123BA) ? 601 :
                      (crcVal == 0x288646C9) ? 602 :
                      (crcVal == 0xB94D4114) ? 701 :
                      (crcVal == 0x494DF1F9) ? 703 :
                      (crcVal == 0xd5fce753) ?  10 : // u-blox7 ROM0.10
                      (crcVal == 0xfce598b1) ?  11 : //         ROM0.11
                      (crcVal == 0xed152ade) ?  14 : //         ROM0.14
                      (crcVal == 0x100E368D) ? 100 : //         ROM1.00
                      (crcVal == 0xDFAA666C) ?  21 : // u-blox8 ROM0.21
                      (crcVal == 0x041D115D) ?  22 : // u-blox8 ROM0.22
                      (crcVal == 0xA15AF099) ? 201 : // u-blox8 ROM2.01
                      (crcVal == 0x2FEF89EA) ? 301 : //         ROM3.01
                      (crcVal == 0x1893C329) ? 351 : //         ROM3.51
                      (crcVal == 0xCAAF619C) ? 40  : // u-blox9 ROM0.40
                      (crcVal == 0xDD3FE36C) ? 101 : //         ROM1.01
                      (crcVal == 0x118B2060) ? 102 : //         ROM1.02
                      (crcVal == 0x3BFC8935) ? 404 : //         ROM4.04
                      0;
        if (!hwRomVer && !EraseOnly)
        {
            MESSAGE(MSG_ERR, "u-blox %d.%d ROM version unknown (0x%08X)",
                    generation/10, generation%10, crcVal);
            break;
        }
        MESSAGE(MSG_LEV2, "u-blox%d ROM%d.%02d hardware detected (0x%08X)",
            generation/10, hwRomVer/100, hwRomVer%100, crcVal);



        /***************************************************
         * check that the image is compatible              *
         ***************************************************/
        if (EraseOnly || fisOnly)
        {
            // no image available, no check needed
        }
        else if ( (imageGeneration / 10) != (generation / 10) &&
                 ((imageGeneration == 91) && (generation != 100)))
        {
            MESSAGE(MSG_ERR, "Receiver generation (%d) incompatible with this image (%d)!",
                generation, imageGeneration);
            break;
        }





        /***************************************************
         * find out if connected via USB                   *
         ***************************************************/
        {
            // autodetect the receiver port

            MESSAGE(MSG_LEV1, "Getting Port connection to receiver");
            UBX_HEAD_t *portCfgMsg = rcvPollMessage(&rx, UBX_CLASS_CFG, UBX_CFG_PORT, NULL, 0, POLL_TIMEOUT);
            if(portCfgMsg == NULL)
            {
                MESSAGE(MSG_DBG, "Getting Port connection timed out");
                break;
            }
            UBX_CFG_PRT_t *prt = (UBX_CFG_PRT_t*)((U1*)portCfgMsg+UBX_HEAD_SIZE);
            const CH* portName[6] = {"I2C", "UART1", "UART2", "USB", "SPI", "?\?\?" };
            MESSAGE(MSG_DBG, "Connected port is: %s", portName[MIN(prt->portId,5)]);

            if (prt->portId == 3)
            {
                isUsbPort = TRUE;
            }
            if (prt->portId == 4)
            {
                isSpiPort = TRUE;
            }

            free(portCfgMsg);
        }

        /***************************************************
         * Prepare the receiver for firmware update        *
         * - either send to safeboot                       *
         * - or start the loader task and disable GPS      *
         ***************************************************/
        if (usbAltMode)
        {
            U4 payload[3];
            memset(payload, 0, sizeof(payload));
            payload[0] = FLASH_BASE + 12;   //base address + 12 is base address
            payload[1] = 0x00000100;        //flags -> BIT8 = FLASH-Write
            payload[2] = 0x00000000;        //data
            if ( rcvAckMessage(&rx, UBX_CLASS_UPD, UBX_UPD_DOWNL, (CH*)payload, sizeof(payload), POLL_TIMEOUT)!= 1 )
            {
                // compose the payload for the new UBX-UPD-AUTHWRITE message (introduced with FW3.00)
                // !IMPORTANT regenerate hash if any data in this message is changed!
                const U1 hash[32] = { 0x55, 0x70, 0xC3, 0x2B, 0x19, 0xA7, 0xA5, 0xC9, 0x28, 0x3B, 0xDD, 0xE8, 0xD5, 0x89, 0xC4, 0x91, 0x8F, 0xE5, 0x32, 0x8B, 0x20, 0x24, 0x1B, 0x45, 0x54, 0xDB, 0x30, 0x0D, 0x35, 0xBB, 0xE1, 0x1E };
                U4 payloadAuth[11];
                memset(payloadAuth, 0, sizeof(payloadAuth));
                memcpy(payloadAuth, hash, sizeof(hash)); // initialize hash
                payloadAuth[8] = FLASH_BASE;        // base address of FLASH
                payloadAuth[9] = 0x00000100;        // flags -> flashWrite
                payloadAuth[10] = HW_IMG_MAGIC_DOM;       // UBLOX8 magic word (To prevent FS from starting in FW301)
                if (rcvAckMessage(&rx, UBX_CLASS_UPD, UBX_UPD_AUTHWRITE, (CH*)payloadAuth, sizeof(payloadAuth), POLL_TIMEOUT) != 1)
                {
                    MESSAGE(MSG_ERR, "Invalidating flash failed");
                    break;
                }
            }

            doReset(&rx, TRUE);

            TIME_SLEEP(100); // wait until receiver is booted up (reset only)
            // Reenumerate the port
            if (!rcvReenumerate(&rx, isUsbPort))
            {
                MESSAGE(MSG_ERR, "Reenumerate failed");
                break;
            }
            DoSafeBoot = FALSE;
        }
        if (DoSafeBoot)
        {
            //send safeboot command
            MESSAGE(MSG_LEV1, "Commanding Safeboot");
            if( !rcvSendMessage(&rx, UBX_CLASS_UPD, UBX_UPD_SAFE, NULL, 0) )
            {
                MESSAGE(MSG_ERR, "Safeboot failed.");
                break;
            }

            // wait for the message to be transmitted and the receiver to finish
            // re-boot (some extended POST checks are performed)
            TIME_SLEEP(500);

            // checks takes more time in u-blox9
            if (generation == 90)
            {
                TIME_SLEEP(300);
            }

            // Reenumerate the port
            if(!rcvReenumerate(&rx, isUsbPort))
            {
                MESSAGE(MSG_ERR, "Reenumerate failed");
                break;
            }

            // set the safeboot baudrate if not USB port
            if(!isUsbPort)
            {
                if( !rcvSetBaud(&rx, BaudrateSafe) )
                {
                    MESSAGE(MSG_ERR, "Safeboot Baud rate set failed.");
                    break;
                }
            }

            // send the training sequence
            if(TrainingSequence)
            {
                if( !rcvSendTrainingSequence(&rx) )
                {
                    MESSAGE(MSG_ERR, "Sending training sequence failed");
                    break;
                }
            }


            monVer = DoAutobaud ?
                     rcvDoAutobaud(&rx, TrainingSequence) :
                     rcvPollMessage(&rx, UBX_CLASS_MON, UBX_MON_VER, NULL, 0, POLL_TIMEOUT);

            if(monVer == NULL)
            {
                MESSAGE(MSG_ERR, "Version is null");
                break;
            }
            free(monVer);
        }
        else
        {
            // Start the loader task
            MESSAGE(MSG_LEV1, "Starting LDR TSK");
            CH pPayload[1] = { 0x01 };

            // this message is assumed to be OK if ACKed or NAKed
            if( rcvAckMessage(&rx, UBX_CLASS_UPD, UBX_UPD_SAFE, pPayload, sizeof(pPayload), POLL_TIMEOUT) == -1 )
            {
                MESSAGE(MSG_ERR, "Starting flash loader failed");
            }
            else
            {
                MESSAGE(MSG_DBG, "LDR TSK started successfully");
            }


            // Identify the flash loader
            MESSAGE(MSG_LEV1, "Identify flash loader");
            UBX_HEAD_t *msg = rcvPollMessage(&rx, UBX_CLASS_UPD, UBX_UPD_IDEN, NULL, 0, POLL_TIMEOUT);
            if( msg == NULL || msg->size != 1)
            {
                MESSAGE(MSG_ERR, "Identify of flash loader failed");
                if(msg != NULL)
                    free(msg);

                break;
            }
            U1 majorN = (*((U1*)(msg)+UBX_HEAD_SIZE) & 0xF0) >> 4;
            U1 minorN = (*((U1*)(msg)+UBX_HEAD_SIZE) & 0x0F);
            MESSAGE(MSG_DBG, "Uploader version %u.%u detected", majorN, minorN);
            free(msg);

            // disable GPS
            MESSAGE(MSG_LEV1, "Stop GPS operation");
            CH data[4] = { 0, 0, 8, 0 };
            // don't expect ACK
            if (!rcvSendMessage(&rx, UBX_CLASS_CFG, UBX_CFG_RST, data, sizeof(data)))
            {
                MESSAGE(MSG_ERR, "Stopping GPS failed");
                break;
            }
        }



        /***************************************************
         * Detect the flash                                *
         ***************************************************/

        U2 FlashManId = 0;
        U2 FlashDevId = 0;
        if (generation < 90 || !flashNotNeeded)
        {

            MESSAGE(MSG_LEV1, "Detecting Flash manufacturer and device IDs");
            // detect flash version
            UBX_HEAD_t *flashMsg = rcvPollMessage(&rx, UBX_CLASS_UPD, UBX_UPD_FLDET, (CH*)&FwBase, 4, POLL_TIMEOUT);
            if(flashMsg == NULL)
            {
                MESSAGE(MSG_ERR, "Flash Detection timed out");
                break;
            }
            else if(flashMsg->size == 8)
            {
                memcpy(&FlashManId,(U1*)((U1*)flashMsg+UBX_HEAD_SIZE+4),sizeof(FlashManId));
                memcpy(&FlashDevId,(U1*)((U1*)flashMsg+UBX_HEAD_SIZE+6),sizeof(FlashDevId));
                MESSAGE(MSG_DBG, "Flash ManId: 0x%04X DevId: 0x%04X", FlashManId, FlashDevId);
                free(flashMsg);
            }
            else
            {
                MESSAGE(MSG_ERR, "Received unexpected answer.");
                free(flashMsg);
                break;
            }
        }
        const U4 jedec = ((FlashManId & 0xFFFF) << 16) + (FlashDevId & 0xFFFF);
        /***************************************************
         * Do the FIS merging                              *
         ***************************************************/
        U4 FlashSize = 0;
        CH *fis = NULL;
        size_t fisSize=0;
        MERGEFIS_RETVAL_t ret = MERGEFIS_OK;

        if(generation >= 70)
        {
            BLOCK_DEF_t flashDef;

            if (generation < 90 || !flashNotNeeded)
            {
                if (generation >= 90 && noFisMerging) // Don't try to load FIS from file
                {
                    ret = getNoFisMergingData(&fis, &fisSize, &rx);
                }
                else
                {
                    // try to load the FIS file
                    ret = mergefis_load(&fis, &fisSize, FisFileName, jedec);
                }
                if (ret == MERGEFIS_OK)
                {
                    flashDef.Count = mergefis_get_sector_count(fis);
                    flashDef.Size = mergefis_get_sector_size(fis);
                    addBlock(&FlashOrg, &flashDef);
                    FlashSize = flashDef.Count * flashDef.Size;
                    if (fisOnly)
                    {
                        // we don't have to merge but just send the FIS
                        fileSize = 0x1000;

                        // Is FIS size to big?
                        if (fisSize > (fileSize - 0x40) || !fisSize || !fis)
                        {
                            if (fis)
                                free(fis);
                            fis = NULL;
                            fisSize = 0;
                            break;
                        }
                        if (pData != NULL)
                        {
                            free(pData);
                            pData = NULL;
                        }

                        CH *pFis = malloc(fileSize);
                        if (!pFis)
                        {
                            free(fis);
                            fis = NULL;
                            fisSize = 0;
                            break;
                        }
                        if (generation >= 90)
                        {
                            fisSize = sizeof(DRV_SPI_MEM_FIS_t);
                            pData = (FWHEADER_t*)pFis;
                            // just copy the fis into the image at the start
                            memcpy(pData, fis, sizeof(DRV_SPI_MEM_FIS_t));
                            fileSize = sizeof(DRV_SPI_MEM_FIS_t);
                        }
                        else
                        {
                            // Set header part to 0, the rest will be overwritten
                            memset(pFis, 0x0, 0x40);
                            // Set the rest of pFis to 0xff
                            memset(&pFis[0x40], 0xff, fileSize - 0x40);
                            // Copy the actual data after the header
                            memcpy(&pFis[0x40], fis, fisSize);
                            pData = (FWHEADER_t*)pFis;
                        }
                    }
                    else if (pData != NULL && noFisMerging)
                    {
                        MESSAGE(MSG_DBG, "Not merging anything");
                    }
                    else if (pData != NULL)
                    {
                        if (generation >= 90)
                        {
                            U4 newSize = fileSize + sizeof(DRV_SPI_MEM_FIS_t);
                            CH* pData2 = malloc(newSize);
                            if (pData2 == NULL)
                            {
                                MESSAGE(MSG_ERR, "malloc failed");
                                break;
                            }
                            // just copy the fis into the image at the start
                            memcpy(pData2, fis, sizeof(DRV_SPI_MEM_FIS_t));
                            memcpy(pData2 + sizeof(DRV_SPI_MEM_FIS_t), pData, fileSize);
                            free(pData);

                            pData = (FWHEADER_t*)pData2;
                            fileSize = newSize;
                            FwBase = 0;
                        }
                        else
                        {
                            // merge the FIS information into the firmware
                            U4 imageSize = (pData->v1.pEnd & ~0x1) - pData->v1.pBase + sizeof(U8);
                            ret = mergefis_merge((CH*)pData, imageSize, fis);
                        }
                    }


                    // check for failure
                    if (ret != MERGEFIS_OK && !noFisMerging)
                    {
                        MESSAGE(MSG_LEV1, "The merging failed");
                        break;
                    }
                    else if (!noFisMerging)
                    {
                        // success
                        MESSAGE(MSG_DBG, "FIS information merged");
                    }

                }
                else
                {
                    // something went wrong when loading the FIS file
                    // check the reason
                    switch (ret)
                    {
                    case MERGEFIS_FILE_NOT_FOUND:
                        MESSAGE(MSG_ERR, "Could not open the FIS file %s", FisFileName);
                        break;
                    case MERGEFIS_VERSION_ERROR:
                        MESSAGE(MSG_ERR, "FIS version not matching\n");
                        break;
                    case MERGEFIS_INCORRECT_XML:
                        MESSAGE(MSG_ERR, "There is an error in the XML file\n");
                        break;
                    case MERGEFIS_JEDEC_NOT_SUPPORTED:
                        MESSAGE(MSG_ERR, "Flash (ManId: 0x%04X DevId: 0x%04X) not supported\n", FlashManId, FlashDevId);
                        break;
                    case MERGEFIS_CODE_CORRUPTED:
                        MESSAGE(MSG_ERR, "Code in XML file is corrupted\n");
                        break;
                    case MERGEFIS_CRC_FAILURE:
                        MESSAGE(MSG_ERR, "CRC failure\n");
                        break;
                    default:
                        MESSAGE(MSG_ERR, "An unknown error occurred\n");
                        break;
                    }
                    break;
                }
            }
        }
        else
        {
            // search for flash info in provided flash definition file
            if(!GetFlashOrganisation(FlashManId, FlashDevId,
                &FlashOrg, &FlashSize, FlashDefFileName))
            {
                MESSAGE(MSG_ERR, "Could not open file '%s'", FlashDefFileName);
                MESSAGE(MSG_ERR, "No flash organization information available for ManID 0x%04X, DevID 0x%04X", FlashManId, FlashDevId);
                break;
            }
        }

        DumpFlashInfo(&FlashOrg, FlashSize);
        // check if flash size is at least as big as firmware to load
        if (generation < 90 || !(updateRam != 0))
        {
            if (FlashSize < fileSize)
            {
                MESSAGE(MSG_ERR, "Flash too small for image.");
                break;
            }
        }

        /***************************************************
         * Switch to the update baudrate                   *
         ***************************************************/
        UBX_CFG_PRT_t prtcfg;
        memset(&prtcfg, 0, sizeof(prtcfg));
        prtcfg.portId       = 1;            //LEA-5 has only USART1 for serial comm
        prtcfg.mode         = (1<<7) |
                              (1<<6) |
                              (1<<11);      //8N1
        prtcfg.baudrate     = BaudrateUpd;
        prtcfg.inProtoMask  = 0x1;          //UBX only
        prtcfg.outProtoMask = 0x1;          //UBX only
        rcvSendMessage(&rx, UBX_CLASS_CFG, UBX_CFG_PORT, (CH*)&prtcfg, sizeof(prtcfg));

        TIME_SLEEP(200);

        rcvSetBaud(&rx, BaudrateUpd);
        rcvFlushBuffer(&rx);

        monVer = rcvPollMessage(&rx, UBX_CLASS_MON, UBX_MON_VER, NULL, 0, POLL_TIMEOUT);
        if(monVer == NULL)
        {
            MESSAGE(MSG_ERR, "Failed polling MON-VER\n");
            break;
        }
        else
        {
            free(monVer);
        }




        /***************************************************
         * Send patch for u-blox 7                         *
         ***************************************************/
        if(generation == 70 && hwRomVer == 100)
        {
            MESSAGE(MSG_DBG, "Sending patch to fix too small erase timeout");
            U1 ram[] = { 0x98, 0x01, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x9c, 0x1c };
            U1 fpb[] = { 0x20, 0x20, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0xed, 0x78, 0x04, 0x00 };
            if( (rcvAckMessage(&rx, UBX_CLASS_UPD, UBX_UPD_DOWNL, (CH*)ram, sizeof(ram), POLL_TIMEOUT) != 1) ||
                (rcvAckMessage(&rx, UBX_CLASS_UPD, UBX_UPD_DOWNL, (CH*)fpb, sizeof(fpb), POLL_TIMEOUT) != 1) )
            {
                MESSAGE(MSG_ERR, "Failed to send patch");
                break;
            }
        }
        /***************************************************
         * Update FIS for U-blox 9                         *
         ***************************************************/
        if(!(updateRam != 0) && generation >= 90 && !noFisMerging)
        {
            if (!fis)
            {
                MESSAGE(MSG_ERR, "FIS erroneously empty");
                break;
            }

            int state = rcvAckMessage(&rx, UBX_CLASS_UPD, UBX_UPD_FIS, fis, sizeof(DRV_SPI_MEM_FIS_t), POLL_TIMEOUT);

            if (state == -1)
            {
                MESSAGE(MSG_WARN, "FIS message timed out");
            }
            else if (state == 0)
            {
                MESSAGE(MSG_WARN, "FIS not accepted by receiver");
            }
            else
            {
                MESSAGE(MSG_DBG, "FIS sent to the receiver");
            }
        }
        free(fis); // free the memory







        if(generation >= 90 && updateRam != 0)
        {
            if(!pData)
                break;
            /***************************************************
             * Do the RAM update                               *
             ***************************************************/
            MESSAGE(MSG_LEV1, "Receiver info collected, downloading to RAM...");
            BOOL ramSuc = FALSE;
            if (isSpiPort)
            {
                CH spiCfgPayload[] = { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 }; // UBX only, FF sup disabled, CPOL=CPHA=0, Tx ready disabled

                if (rcvAckMessage(&rx, UBX_CLASS_CFG, UBX_CFG_PORT, spiCfgPayload, sizeof(spiCfgPayload), POLL_TIMEOUT) != 1)
                {
                    MESSAGE(MSG_ERR, "SPI configuration not accepted");
                    if (imageGeneration == 90)
                    {
                        MESSAGE(MSG_ERR, "MPW chip needs to be fused with patch 2 (spiCfg)");
                    }
                    break;
                }
            }
            ramSuc = updateImageToRam(&rx, (CH*)pData, fileSize);
            if (ramSuc == FALSE)
            {
                MESSAGE(MSG_ERR, "RAM image update failed");
                break;
            }

        }
        else
        {
            /***************************************************
             * Do the flash update                             *
             ***************************************************/
            I4 numberSectors;
            if(doChipErase)
            {
                if(generation > 70)
                {
                    // we don't have to erase any sector afterwards because we do a chip erase
                    numberSectors = 0;

                    if(rcvAckMessage(&rx, UBX_CLASS_UPD, UBX_UPD_CERASE, NULL, 0, POLL_TIMEOUT) != 1)
                    {
                        MESSAGE(MSG_ERR, "Could not send chip erase command. Does the ROM support it?");
                        break;
                    }
                    else
                    {
                        MESSAGE(MSG_LEV2, "Chip erase started");
                        eraseInProgres = TRUE;
                    }
                }
                else
                {
                    MESSAGE(MSG_ERR, "Chip erase no supported on this platform");
                    break;
                }
            }
            else
            {
                if(EraseWholeFlash)
                {
                    // erase all the sectors
                    numberSectors = GetSectorNrForSize(0, FlashSize, &FlashOrg);
                }
                else
                {
                    // erase only the needed sectors
                    numberSectors = (EraseOnly) ? 1 : GetSectorNrForSize(0, fileSize, &FlashOrg);
                }
            }

            I4 numberPackets = (fileSize % PACKETSIZE) ?
                fileSize / PACKETSIZE + 1 : fileSize / PACKETSIZE;
            upd = updInit(&rx, numberSectors, numberPackets, &FlashOrg, FlashSize, DEFAULT_MAX_PACKETS, eraseInProgres);
            if(!upd)
                break;

            if (!updUpdate(upd, pData, fileSize, FwBase))
                break;

        }




        /***************************************************
         * Invalidate patch for u-blox 7                   *
         ***************************************************/
        if(generation == 70 && hwRomVer == 100)
        {
            MESSAGE(MSG_DBG, "Invalidating timeout patch");
            U1 payloadInvPatch[] = { 0x20, 0x20, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
            if( rcvAckMessage(&rx, UBX_CLASS_UPD, UBX_UPD_DOWNL, (CH*)payloadInvPatch, sizeof(payloadInvPatch), POLL_TIMEOUT) != 1 )
            {
                MESSAGE(MSG_ERR, "Failed to invalidate patch");
                break;
            }
        }





        /***************************************************
         * Write the marker to the flash to indicate       *
         * the FS that the flash was completely erased     *
         * Only u-blox8 supports this feature              *
         * later platforms address this issue differently  *
         * without the need for FW update tool support     *
         ***************************************************/
        if (EraseWholeFlash && generation == 80)
        {
            // check if the marker has to be written
            U4 address = 0;
            U4 marker = 0xffffffff;
            BOOL imageSupportsFeature = FALSE;
            if (fisOnly)
            {
                // we are going to run from ROM -> write the marker to the second sector
                address = 0x1000;

                // u-blox8 ROM 3.01 supports this feature
                if ( hwRomVer == 301 )
                {
                    imageSupportsFeature = TRUE;
                    marker = 0xee7b8f34;
                }
            }
            else if (!EraseOnly)
            {
                if(!pData)
                    break;
                // we are going to run from flash -> write the marker to the first sector following the image
                address = (((FwBase + fileSize) / 0x1000) + 1) * 0x1000;
                if( pData->v1.fsErasedMarker != 0xFFFFFFFF )
                {
                    imageSupportsFeature = TRUE;
                    marker = pData->v1.fsErasedMarker;
                }
            }

            if( imageSupportsFeature )
            {
                if(!pData)
                    break;
                MESSAGE(MSG_LEV1, "Writing marker for the file system to speed up first initialization");

                const U4 writeSize = sizeof(pData->v1.fsErasedMarker);
                const U4 length = writeSize + 8;
                CH *data = malloc(length);
                if (!data)
                    break;

                //copy data to the send buffer
                memcpy(data + 0, &address,   4);                // Address
                memcpy(data + 4, &writeSize, 4);                // Data size
                memcpy(data + 8, &marker,    sizeof(marker));   // Marker
                UBX_HEAD_t *msg = rcvPollMessage(&rx, UBX_CLASS_UPD, UBX_UPD_FLWRI,
                        data, length, WRITE_TIMEOUT);
                free(data);
                if( msg == NULL )
                {
                    MESSAGE(MSG_ERR, "Failed to write the marker");
                    break;
                }
                free(msg);
            }
        }





        /***************************************************
         * Restart receiver if image is too large          *
         * (Workaround for ROM bug)                        *
         ***************************************************/
        BOOL isGen70Rom100 = (generation == 70 && hwRomVer == 100);
        BOOL isGen80Rom22Or201 = (generation == 80 && (hwRomVer == 22 || hwRomVer == 201));
        if( (fileSize > 128 * 4096) && (isGen70Rom100 || isGen80Rom22Or201))
        {
            //send safeboot command
            if (!isUsbPort)
            {
                MESSAGE(MSG_LEV1, "Commanding Safeboot");
                if (!rcvSendMessage(&rx, UBX_CLASS_UPD, UBX_UPD_SAFE, NULL, 0))
                {
                    MESSAGE(MSG_ERR, "Safeboot failed.");
                    break;
                }
                TIME_SLEEP(500); // wait until receiver is booted up
            }
            else
            {
                doReset(&rx, TRUE);
                TIME_SLEEP(100); // wait until receiver is booted up
            }


            // Reenumerate the port
            if (!rcvReenumerate(&rx, isUsbPort))
            {
                MESSAGE(MSG_ERR, "Reenumerate failed");
                break;
            }

            // set the safeboot baudrate if not USB port
            if (!isUsbPort)
            {
                if (!rcvSetBaud(&rx, BaudrateSafe))
                {
                    MESSAGE(MSG_ERR, "Safeboot Baud rate set failed.");
                    break;
                }
                // send the training sequence
                if (TrainingSequence)
                {
                    if (!rcvSendTrainingSequence(&rx))
                    {
                        MESSAGE(MSG_ERR, "Sending training sequence failed");
                        break;
                    }
                }
            }
            else
            {
                // disable GPS
                MESSAGE(MSG_LEV1, "Stop GPS operation");
                CH data[4] = { 0, 0, 8, 0 };
                // don't expect ACK
                if (!rcvSendMessage(&rx, UBX_CLASS_CFG, UBX_CFG_RST, data, sizeof(data)))
                {
                    MESSAGE(MSG_ERR, "Stopping GPS failed");
                    break;
                }
                // Start the loader task
                MESSAGE(MSG_LEV1, "Starting LDR TSK");
                CH pPayload[1] = { 0x01 };
                // this message is assumed to be OK if ACKed or NAKed
                if (rcvAckMessage(&rx, UBX_CLASS_UPD, UBX_UPD_SAFE, pPayload, sizeof(pPayload), POLL_TIMEOUT) == -1)
                {
                    MESSAGE(MSG_ERR, "Starting flash loader failed");
                }
                else
                {
                    MESSAGE(MSG_DBG, "LDR TSK started successfully");
                }

            }
            monVer = DoAutobaud ?
                rcvDoAutobaud(&rx, TrainingSequence) :
                rcvPollMessage(&rx, UBX_CLASS_MON, UBX_MON_VER, NULL, 0, POLL_TIMEOUT);

            if (monVer == NULL)
            {
                MESSAGE(MSG_ERR, "Version is null");
                break;
            }
            free(monVer);
        }





        /***************************************************
         * Verify that the image was written correctly     *
         ***************************************************/
        if (!fisOnly && !EraseOnly)
        {
            BOOL verifySuccess;
            MESSAGE(MSG_LEV1, "Verifying Image on hardware");
            verifySuccess = verifyImage(&rx, pData, fileSize, FwBase, (updateRam != 0), (generation >= 90) ? 2 : 1);
            if (!verifySuccess)
            {
                MESSAGE(MSG_LEV1,"CRC check ERROR");
                MESSAGE(MSG_ERR, "Verify failed");
                break;
            }
            MESSAGE(MSG_LEV1, "CRC check SUCCESS");

        }





        /***************************************************
         * Reset the receiver                              *
         ***************************************************/
        if (DoReset && updateRam != 0)
        {
            {
                // we cannot use UPD-RBOOT in this case
                // -> use CFG-RST instead
                CH data[4] = { -1, -1, 1, 0 };
                rcvSendMessage(&rx, UBX_CLASS_CFG, UBX_CFG_RST, data, sizeof(data));
            }
        }
        else
        {
            doReset(&rx, DoReset);
        }
        // Everything is fine
        success = TRUE;

    } while (FALSE);

    // Clean up receiver interface if required
    if( rcvConnected )
        rcvDisconnect(&rx);

    //clean up and exit
    if (pData)
    {
        free(pData);
    }

    // Free update core structure
    if(upd)
        updDeinit(upd);

    clearBlocks(&FlashOrg);
    return success;
}
