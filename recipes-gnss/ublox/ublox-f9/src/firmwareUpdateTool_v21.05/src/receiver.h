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
  \brief  Declares the receiver communication interface
*/

#include <string.h>
#include <stdio.h>
#include "platform.h"
#include "ubxmsg.h"


//! receive buffer size
#define RECEIVEBUF_SIZE     65536

//! timeout for autobauding
#define AUTOBAUD_TIMEOUT      300

//! timeout for getting a polled message
#define POLL_TIMEOUT         1000

//! Number of retries when autobauding
#define RETRY_COUNT_AUTOBAUD    5

//! Number of retries for status messages (except for erase/write)
#define RETRY_COUNT             3

//! List of baudrates to use when autobauding
extern const U4 gAutoBaudRates[];

//! The receive buffer to write into
typedef struct RECEIVEBUF_s
{
    U1  Buf[RECEIVEBUF_SIZE];   //!< buffer containing received data
    U1* pCurrent;               //!< pointer to begin of valid data
    U1* pEnd;                   //!< pointer to end of valid data
} RECEIVEBUF_t;

//! Structure that defines a connection
typedef struct
{
    RECEIVEBUF_t mRecBuf;            //!< local instance of the receive buffer
    SER_HANDLE_t *mPortHandle;       //!< handle of the port connected to
} RCV_DATA_t;

/*!
 * Connect to the receiver
 *
 * \param rcv                   receiver control structure
 * \param comPort               com port to connect to
 * \param baudrate              baudrate to setup
 * \return TRUE if successful
 */
BOOL rcvConnect(INOUT RCV_DATA_t *rcv, IN const CH* comPort, IN U4 baudrate);

/*!
 * Disconnect from the receiver if connected. This
 * is done in the destructor.
 *
 * \param rcv                   receiver control structure
 */
void rcvDisconnect(INOUT RCV_DATA_t *rcv);

/*!
 * Poll a message from the receiver. When the timeout expires it retries RETRY_COUNT times.
 *
 * \param rcv                   receiver control structure
 * \param classId               class id of the message to poll
 * \param msgId                 the message id of the message to poll
 * \param payload               pointer to the payload to include in the message to poll, may be NULL
 * \param payloadSize           size of the payload, must be NULL if payload is
 * \param timeout               timeout that has to expire before retry
 * \return pointer to the received message or NULL if the polling failed
 */
UBX_HEAD_t* rcvPollMessage( INOUT RCV_DATA_t *rcv
                          , IN U1 classId
                          , IN U1 msgId
                          , IN CH* payload
                          , IN U4 payloadSize
                          , IN U4 timeout );

/*!
 * Send a message to the receiver and wait until it is ack'd (or nack'd).
 * When the timeout expires it retries RETRY_COUNT times.
 *
 * \param rcv                   receiver control structure
 * \param classId               class id of the message to send
 * \param msgId                 the message id of the message to send
 * \param payload               pointer to the payload to include in the message to send
 * \param payloadSize           size of the payload
 * \param timeout               timeout that has to expire before retry
 * \return 0 if successful and nack received
 *         1 if successful and ack received,
 *        -1 if timed out or something went wrong
 */
int rcvAckMessage( INOUT RCV_DATA_t *rcv
                 , IN U1 classId
                 , IN U1 msgId
                 , IN CH* payload
                 , IN U4 payloadSize
                 , IN U4 timeout );

/*!
 * Receive a message with given class and message id from the receiver. If other messages with
 * different class or message ids are received within the timeout they are discarded.
 *
 * \param rcv                   receiver control structure
 * \param timeout               wait for timeout
 * \param classId               class id of the message to receive
 * \param msgId                 message id of the message to receive
 * \return pointer to the received message or null if the timeout expired
 */
UBX_HEAD_t* rcvReceiveMessage( INOUT RCV_DATA_t *rcv
                             , IN U4 timeout
                             , IN I4 classId
                             , IN I4 msgId );

/*!
 * Send a message to the receiver
 *
 * \param rcv                   receiver control structure
 * \param classId               class id of the message to send
 * \param msgId                 message id of the message to send
 * \param payload               pointer to the payload to include in the message to send
 * \param payloadSize           size of the payload
 * \return TRUE if successful
 */
BOOL rcvSendMessage( INOUT RCV_DATA_t *rcv
                   , IN U1 classId
                   , IN U1 msgId
                   , IN CH* payload
                   , IN U4 payloadSize );

/*!
 * Poll the message MON-VER from the receiver trying different baudrates
 *
 * \param rcv                   receiver control structure
 * \param sendTrainingSequence  should the training sequence be sent or not
 * \return pointer to the MON-VER message received
 *         or NULL if communication failed on all baudrates
 */
UBX_HEAD_t* rcvDoAutobaud(INOUT RCV_DATA_t *rcv, IN BOOL sendTrainingSequence);

/*!
 * Reinitialize the connection
 *
 * \param rcv                   receiver control structure
 * \param isUsbPort             set to TRUE if connected to a USB port
 * \return TRUE if successful
 */
BOOL rcvReenumerate(INOUT RCV_DATA_t *rcv, IN BOOL isUsbPort);

/*!
 * Send the training sequence to the receiver
 *
 * \param rcv                   receiver control structure
 * \return TRUE if successful
 */
BOOL rcvSendTrainingSequence(INOUT RCV_DATA_t *rcv);

/*!
 * Flush the receive buffer of the receiver by sending 1kB of garbage.
 *
 * \param rcv                   receiver control structure
 */
void rcvFlushBuffer(INOUT RCV_DATA_t *rcv);

/*!
 * Set the baudrate of the connection
 *
 * \param rcv                   receiver control structure
 * \param baud                  baudrate to set
 * \return TRUE if successful
 */
BOOL rcvSetBaud(INOUT RCV_DATA_t *rcv, IN int baud);

