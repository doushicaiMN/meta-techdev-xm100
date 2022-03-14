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
  \brief  Implements the receiver communication interface
*/

#include <assert.h>
#include <stdlib.h>
#include "receiver.h"

const U4 gAutoBaudRates[] = {9600, 115200, 57600, 19200, 38400, 230400};

/*!
 * Send raw data to the receiver
 *
 * \param rcv                   receiver control structure
 * \param msg                   pointer to the data
 * \param size                  size of the data
 * \return the number of bytes written
 */
static size_t rcvRawSend(INOUT RCV_DATA_t *rcv, CH* msg, size_t size)
{
    assert(rcv);

    return SER_WRITE(rcv->mPortHandle, msg, size);
}


/*!
 * Clear the local receive buffer
 *
 * \param rcv                   receiver control structure
 */
static void rcvClearBuffer(INOUT RCV_DATA_t *rcv)
{
    assert(rcv);
    rcv->mRecBuf.pCurrent = rcv->mRecBuf.Buf;
    rcv->mRecBuf.pEnd     = rcv->mRecBuf.Buf;
}

/*!
 * Poll a message once. If the timeout expires this function does not retry
 *
 * \param rcv                   receiver control structure
 * \param classId               class id of the message to poll
 * \param msgId                 the message id of the message to poll
 * \param payload               pointer to the payload to include in the message to poll
 * \param payloadSize           size of the payload
 * \param timeout               when timeout is expired NULL is returned
 * \return pointer to the received message or NULL if the polling failed
 */
static UBX_HEAD_t* rcvPollMessageOnce( INOUT RCV_DATA_t *rcv
                                     , IN U1 classId
                                     , IN U1 msgId
                                     , IN CH* payload
                                     , IN U4 payloadSize
                                     , IN U4 timeout)
{
    assert(rcv);
    assert( ( payload &&  payloadSize)
         || (!payload && !payloadSize));

    if (!rcvSendMessage(rcv, classId, msgId, payload, payloadSize))
    {
        return NULL;
    }
    UBX_HEAD_t* pUbxHead = rcvReceiveMessage(rcv, timeout, classId, msgId);
    return pUbxHead;
}

BOOL rcvConnect(INOUT RCV_DATA_t *rcv, IN const CH* comPort, IN U4 baudrate)
{
    assert(rcv);

    rcv->mPortHandle = NULL;
    MESSAGE(MSG_DBG, "Trying to open port %s", comPort);
    rcv->mPortHandle = SER_OPEN(comPort);

    if (rcv->mPortHandle == (SER_HANDLE_t *)0)
    {
        MESSAGE(MSG_ERR, "Could not open communications port.");
        return FALSE;
    }

    // initialize the receiver buffer
    rcvClearBuffer(rcv);

    // clear the serial port
    SER_CLEAR(rcv->mPortHandle);

    return rcvSetBaud(rcv, baudrate);
}

void rcvDisconnect(INOUT RCV_DATA_t *rcv)
{
    assert(rcv);
    if(rcv->mPortHandle)
    {
        SER_CLEAR(rcv->mPortHandle);
        rcvClearBuffer(rcv);
        SER_CLOSE(rcv->mPortHandle);
        rcv->mPortHandle = NULL;
    }
}

BOOL rcvSendTrainingSequence(INOUT RCV_DATA_t *rcv)
{
    assert(rcv);

    CH trainingSequence[] = {0x55, 0x55};
    MESSAGE(MSG_DBG, "Sending training sequence");
    size_t size = sizeof(trainingSequence);
    BOOL success = rcvRawSend(rcv, trainingSequence, size) == size;
    TIME_SLEEP(10);
    return success;
}

BOOL rcvSendMessage( INOUT RCV_DATA_t *rcv
                   , IN U1 classId
                   , IN U1 msgId
                   , IN CH* payload
                   , IN U4 payloadSize )
{
    assert(rcv);
    assert( ( payload &&  payloadSize)
         || (!payload && !payloadSize));

    CH* pMessage;
    size_t Size;
    if (!UbxCreateMessage( classId
                         , msgId
                         , payload
                         , payloadSize
                         , &pMessage
                         , &Size ))
    {
        return FALSE;
    }

    U4 written = rcvRawSend(rcv, pMessage, Size);
    free(pMessage);

    return written == Size;
}

UBX_HEAD_t* rcvPollMessage( INOUT RCV_DATA_t *rcv
                          , IN U1 classId
                          , IN U1 msgId
                          , IN CH* payload
                          , IN U4 payloadSize
                          , IN U4 timeout)
{
    assert(rcv);
    assert( ( payload &&  payloadSize)
         || (!payload && !payloadSize));

    U4 retryCount;
    for(retryCount = 0; retryCount < RETRY_COUNT; retryCount++)
    {
        UBX_HEAD_t* msg = rcvPollMessageOnce(rcv, classId, msgId,  payload, payloadSize, timeout);
        if(msg != NULL)
            return msg;

        MESSAGE(MSG_DBG, "Retry poll");
    }
    return NULL;
}

int rcvAckMessage( INOUT RCV_DATA_t *rcv
                 , IN U1 classId
                 , IN U1 msgId
                 , IN CH* payload
                 , IN U4 payloadSize
                 , IN U4 timeout )
{
    assert(rcv);
    assert( ( payload &&  payloadSize)
         || (!payload && !payloadSize));

    U4 retryCount;
    for(retryCount = 0; retryCount < RETRY_COUNT; retryCount++)
    {
        if( rcvSendMessage(rcv, classId, msgId,  payload, payloadSize) )
        {
            const U4 toTime = TIME_GET() + timeout;
            do // loop so message is not retransmitted when ACK from different (previous) message is already in buffor or is received.
            {
                UBX_HEAD_t *ack = rcvReceiveMessage(rcv, toTime - TIME_GET(), UBX_CLASS_ACK, -1);
                if (ack != NULL)
                {
                    UBX_ACK_ACK_t *pAck = (UBX_ACK_ACK_t*)((U1*)ack + UBX_HEAD_SIZE);
                    if (pAck->clsId == classId &&
                        pAck->msgId == msgId)
                    {
                        BOOL success = ack->msgId == UBX_ACK_ACK;
                        free(ack);
                        return success ? 1 : 0;
                    }
                    free(ack);
                }
            }
            while (TIME_GET() < toTime);
        }
    }
    return -1;
}

UBX_HEAD_t* rcvReceiveMessage(INOUT RCV_DATA_t *rcv, IN U4 timeout, IN I4 classId, IN I4 msgId)
{
    assert(rcv);

    const U4 toTime = TIME_GET() + timeout;
    do
    {
        U4 availableSize = RECEIVEBUF_SIZE - (rcv->mRecBuf.pEnd - rcv->mRecBuf.pCurrent);
        U4 received = SER_READ(rcv->mPortHandle, rcv->mRecBuf.pEnd, availableSize);

        rcv->mRecBuf.pEnd += received;
        U1* pMessageBegin = NULL;

        //parse buffer if a valid UBX message can be found
        //start at position pCurrent
        if (UbxSearchMsg(rcv->mRecBuf.pCurrent, rcv->mRecBuf.pEnd - rcv->mRecBuf.pCurrent, &pMessageBegin))
        {
            UBX_HEAD_t ubxhead;
            memcpy(&ubxhead, pMessageBegin, sizeof(ubxhead));
            CH* message = (CH*)malloc((ubxhead.size + UBX_FRAME_SIZE)*sizeof(CH));
            if (!message)
            {
                return NULL;
            }

            // copy the message to the buffer
            memcpy(message, pMessageBegin, ubxhead.size + UBX_FRAME_SIZE);

            rcv->mRecBuf.pCurrent = pMessageBegin + ubxhead.size + UBX_FRAME_SIZE;

            // move the rest of the data to the beginning of the buffer
            memmove(rcv->mRecBuf.Buf, rcv->mRecBuf.pCurrent, rcv->mRecBuf.pEnd - rcv->mRecBuf.pCurrent);
            rcv->mRecBuf.pEnd -= rcv->mRecBuf.pCurrent - rcv->mRecBuf.Buf;
            rcv->mRecBuf.pCurrent = rcv->mRecBuf.Buf;

            if ((classId == -1 || classId == ubxhead.classId)
                && (msgId == -1 || msgId == ubxhead.msgId))
            {
                return (UBX_HEAD_t*)message;
            }
            free(message);
        }
        else
        {
            TIME_SLEEP(1); // don't loop at 100% CPU
        }
    }
    while(TIME_GET() < toTime);

    return NULL;
}

BOOL rcvReenumerate(INOUT RCV_DATA_t *rcv, BOOL isUsbPort)
{
    assert(rcv);
    BOOL result=FALSE;

    if( (rcv->mPortHandle->type == COM && !isUsbPort)
     ||  SER_REENUM(rcv->mPortHandle, isUsbPort) )
    {
        // we just booted in safeboot mode, try to
        // re-fetch communication channel
        result=TRUE;
    }
    else
    {
        MESSAGE(MSG_ERR, "Reconnect failed.");
    }

    return result;
}

UBX_HEAD_t* rcvDoAutobaud(INOUT RCV_DATA_t *rcv, IN BOOL sendTraining)
{
    assert(rcv);
    U4 retryCount = 0;
    U4 autoBaudIndex = 0;

    for(;;)
    {
        UBX_HEAD_t* msg = rcvPollMessage(rcv, UBX_CLASS_MON, UBX_MON_VER, NULL, 0, AUTOBAUD_TIMEOUT);
        if(msg != NULL)
        {
            MESSAGE(MSG_DBG,"Detected Baudrate is %d", rcv->mPortHandle->baudrate);
            return msg;
        }

        if (retryCount++ <= RETRY_COUNT_AUTOBAUD)
        {
            MESSAGE(MSG_DBG,"...retrying autobaud");
        }
        else if(autoBaudIndex < NUMOF(gAutoBaudRates))
        {
            // advance to the next baudrate
            MESSAGE(MSG_DBG, "Retrying with baudrate %d", gAutoBaudRates[autoBaudIndex]);
            rcvSetBaud(rcv, gAutoBaudRates[autoBaudIndex]);

            // send the training sequence
            if(sendTraining)
                rcvSendTrainingSequence(rcv);

            ++autoBaudIndex;
            retryCount = 0;
        }
        else
        {
            MESSAGE(MSG_ERR, "Unable to Communicate on any Baudrate");
            return NULL;
        }
    }
}

BOOL rcvSetBaud(INOUT RCV_DATA_t *rcv, int baud)
{
    assert(rcv);
    rcvClearBuffer(rcv);

    MESSAGE(MSG_DBG, "Setting baudrate to %d", baud);

    //we got a serial port, configure it
    if (!SER_BAUDRATE(rcv->mPortHandle, baud))
    {
        MESSAGE(MSG_ERR, "Could not configure communications port.");
        return FALSE;
    }
    TIME_SLEEP(200);
    return TRUE;
}

void rcvFlushBuffer(INOUT RCV_DATA_t *rcv)
{
    assert(rcv);
    SER_CLEAR(rcv->mPortHandle);
    rcvClearBuffer(rcv);

    CH pCrap[1024];
    memset(pCrap, 0xFF, sizeof(pCrap));
    SER_WRITE(rcv->mPortHandle, pCrap, sizeof(pCrap));
}
