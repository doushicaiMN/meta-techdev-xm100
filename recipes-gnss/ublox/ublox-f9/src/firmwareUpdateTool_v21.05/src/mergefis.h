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
  \brief  Loads and parses a FIS file and merges the file with the binary.
*/

#ifndef __MERGEFIS_H
#define __MERGEFIS_H

#include "yxml.h"
#include "types.h"

#define DRV_SQI_CAP_WRITE_SUSPEND           0x01        //!< Program/Erase suspend command available
#define DRV_SQI_CAP_FAST_PROGRAM            0x02        //!< Sub-page program command available
#define DRV_SQI_CAP_DEEP_SLEEP              0x04        //!< Deep sleep command available
#define DRV_SQI_CAP_REVISION                0x08        //!< The FIS contains a revision
#define DRV_SQI_CAP_MAX_SPEED               0x10        //!< The FIS contains a revision
#define DRV_SQI_FIS_CHECKSUM_SEED           0x12345678  //!< Seed used in FIS checksum

#define CAPABILITY_POSITION                 0x1         //!< Position of the capability in the FIS
#define SUPPLY_TYPE_POSITION                0x7         //!< Position of the supply type in the FIS
#define SECTOR_SIZE_POSITION                0x8         //!< Position of the sector size in the FIS
#define SECTOR_COUNT_POSITION               0xC         //!< Position of the sector count in the FIS
#define HSP_POSITION                        0x10        //!< Position of the HSP in the FIS
#define CFG_POSITION                        0x14        //!< Position of the CFG in the FIS
#define MIN_ERASE_SUSPEND_POSITION          0x18        //!< Position of the minimum erase suspend in the FIS
#define NUM_BYTES_WRITE_POSITION            0x1C        //!< Position of the number of bytes
#define DEEP_SLEEP_OPCODE_POSITION          0x38        //!< Position of the deep sleep opcode in the FIS
#define DEEP_SLEEP_WAKEUP_OPCODE_POSITION   0x39        //!< Position of the deep sleep wakeup opcode in the FIS
#define DEEP_SLEEP_WAKEUP_TIMEOUT_POSITION  0x3A        //!< Position of the deep sleep wakeup timeout in the FIS
#define REVISION_POSITION                   0x3C        //!< Position of the revision in the FIS
#define MAX_SPEED_POSITION                  0x43        //!< Position of the maximum speed in the FIS

#define SECTOR_SIZE_VER3                    0x1000      //!< Sector size in the Version 3 of FIS
#define SECTOR_COUNT_POSITION_VER3          0x8         //!< Position of the sector count in Version 3 of FIS
#define MAJOR_REV_POSITION                  0x4         //!< Position of the Major revison in Version 3 of FIS
#define MINOR_REV_POSITION                  0x5         //!< Position of the Minor revison in Version 3 of FIS
#define REVISION_POSITION_VER3              0x20        //!< Position of the Git revision in Version 3 of FIS


/*U-blox 9 FIS */
typedef struct DRV_SPI_MEM_JEDEC_s {
 U1 manuId; //!< Manufacturer ID
 U1 devType;//!< Device type
 U1 devId;  //!< Device ID
 L1 discardJEDEC; //!< if TRUE JEDEC will not be checked
} DRV_SPI_MEM_JEDEC_t;

typedef struct DRV_SPI_MEM_INFO_s {
    U1 majorRevision;                   //!< Major revision if other then default info is loaded it is >0
    U1 minorRevision;                   //!< Minor revision
    U1 eraseSectorOpcode;               //!< Erase 4KB sector
    U1 statusRegWriteEnableOpcode;      //!< Status write enable command either 0x50 or 0x06

    U2 sectorCount;                     //!< Number of 4KB sectors

    ////// Further data only in minor revision >= 5
    U1 deepSleepOpcode;                 //!< opcode used to put the device into deep sleep
    U1 wakeupOpcode;                    //!< opcode used to wake up the device from deep sleep

    U1 busyBitMask;                     //!< Busy bit mask
    U1 readyBitMask;                    //!< Busy bit mask
    U1 readStatusRegOpcode;             //!< Read status register command
    U1 wakeupTimeout;                   //!< time in us for which CS has to stay high after sending the wake up opcode

    U2 pageSize;                        //!< Page size
    U1 softResetOpcodes[2];             //!< Soft reset commands issued one after another witch CS transition 0 means no command so {0, 0} means no reset sequence.

    /// Additional info not present in SFDP table
    U1 fastReadOpcode;                  //!< 1-1-1 fast read usually 0x0B
    U1 fastReadDummyBytes;              //!< number of dummy bytes after fast reed usually 1
    U1 writeProtectRegOpcode;           //!< write protection register 0x00 means that flash is unprotected on default.
    U1 protectRegContent;               //!< protection register content which disable protection

    U1 protectRegContentRepeat;         //!< how many times content should be repeated after Opcode (max 63)
    U1 readJedecOpcode;                 //!< Read JEDEC command
    U1 enableWriteOpcode;               //!< Enable write command
    U1 disableWriteOpcode;              //!< Disable write command

    U1 eraseFullChipOpcode;             //!< Erase full chip
    U1 writeOpcode;                     //!< Write command
    U1 supply;                          //!< supply voltage x10
    U1 fastReadMaxSpeed;                //!< fast read max speed in MHz

    U4 revision;                        //!< Revision of FIS in case of SFDP it is 0

} DRV_SPI_MEM_INFO_t;

//! FIS header structure
typedef struct DRV_SPI_MEM_FIS_s {
    DRV_SPI_MEM_JEDEC_t jedec;          //!< JEDEC ID
    DRV_SPI_MEM_INFO_t infoStruct;      //!< Informations structure
    U1 reserved[32];                    //!< 32 bytes reserved for future use
    U4 checksum;                        //!< Checksum
} DRV_SPI_MEM_FIS_t;

//! XML parser handler used to extract all information from an XML file
typedef struct
{
    int fd;                             //!< File descriptor of the XML file
    yxml_t state;                       //!< State of the parser
    char buf[4096];                     //!< Buffer used by the parser
} XML_HNDL_t;

/******************************************************************************************/
//! Defines the return value of the mergefis_load function
typedef enum {
    MERGEFIS_OK = 0,                    //!< All OK
    MERGEFIS_FILE_NOT_FOUND,            //!< Could not find the FIS file
    MERGEFIS_VERSION_ERROR,             //!< Versions do not match
    MERGEFIS_INCORRECT_XML,             //!< Error in the XML structure
    MERGEFIS_JEDEC_NOT_SUPPORTED,       //!< JEDEC not supported by this FIS file
    MERGEFIS_CODE_CORRUPTED,            //!< Code corrupted
    MERGEFIS_CRC_FAILURE,               //!< Overall CRC failure
    MERGEFIS_UNKNOWN                    //!< Unknown state
} MERGEFIS_RETVAL_t;

//! Get the image of the FIS
/*!
    \param    fis          Pointer to pointer of the FIS image
    \param    fisSize      Size of the allocated structure
    \param    fisFile      Path to the FIS file
    \param    jedec        JEDEC of the device the FIS image should be retrieved for
    \return   success / error code
*/
MERGEFIS_RETVAL_t mergefis_load(char **fis, size_t *fisSize, const char *fisFile, const unsigned int jedec);

//! Merge the flash image with the corresponding FIS data
/*!
    \param    pData        Pointer to the flash image
    \param    ImageSize    Size of the flash image
    \param    fis          Pointer to the FIS image
    \return   success / error code
*/
MERGEFIS_RETVAL_t mergefis_merge(char *pData, unsigned int ImageSize, char const *fis);

//! Return the sector count in the FIS
/*! Return the sector count in the FIS

    \param fis             The FIS definition from which to extract
                           the sector count
    \return                The calculated sector count
*/
unsigned int mergefis_get_sector_count(char const *fis);

//! Return the sector size in the FIS
/*! Return the sector size in the FIS

    \param fis             The FIS definition from which to extract
                           the sector size
    \return                The calculated sector size
*/
unsigned int mergefis_get_sector_size(char const *fis);

//! Calculate 4 byte CRC
/*! Returns 4 byte CRC of provided data

\param seed            Seed of CRC
\param pkAdr           pointer to the data
\param size            size of the data
\return                The calculated 32bit CRC
*/
unsigned int lib_crc_crc32(unsigned int seed,
    const void *pkAdr,
    unsigned int size);
#endif //__MERGEFIS_H
