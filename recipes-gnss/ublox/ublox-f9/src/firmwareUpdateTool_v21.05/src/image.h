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
  \brief  Definitions concerning the firmware image
*/

#ifndef __IMAGE_H
#define __IMAGE_H

#include "types.h"

//! Header of firmware image
// Warning, put U4 instead of pointers here, because on 64-bit systems, a pointer is 64bit
typedef union FWHEADER_u
{
    struct
    {
        U4    magic;            //!< magic word "UBX5", "UB51", "UBX7" or "UBX8"
        U4    execute;          //!< instruction to execute first (pEntry)
        U4    pEntrySafe;       //!< pEntry of SafeBoot Image
        U4    pBase;            //!< pBase of this Firmware
        U4    pStart;           //!< pStart of this Firmware
        U4    pEnd;             //!< pEnd of this Firmware
        U4    pVersion;         //!< Version String pointer
        U4    reserved[4];      //!< reserved
        U4    fsErasedMarker;   //!< marker to indicate the FS a completely erased flash (not used anymore)
    } v1;                       //!< version 1 firmware header type
} FWHEADER_t;
typedef FWHEADER_t* FWHEADER_pt; //!< pointer to FWHEADER_t

//! structure holding data extracted from image footer
typedef struct FWFOOTERINFO_u
{
    U4 numberOfImages;       //!< 0 means that footer was not found
    U4 imagesSize[2];        //!< curently only up 2 images supported
    U4 cfgSize;              //!< size of configuration attached to the image
} FWFOOTERINFO_t;

//! Validate firmware image
/*!
    Performs several consistency checks on the firmware image to ensure data
    consistency and a correct image.
    \param pImage        pointer to buffer containing firmware image file contents
    \param ImageSize     Size of Buffer containing Image
    \param pFwFooter     Pointer to data extracted from image footer
    \return 0 if the buffer doesn't contains a valid image, the generation otherwise
*/
U4 ValidateImage(IN FWHEADER_t const *pImage,
                 IN const size_t      ImageSize,
                 OUT FWFOOTERINFO_t  *pFwFooter);


//! Open file and buffer it in RAM
/*!
    Opens a file with platform-independent method (file streams) and copies the
    content into a dynamically allocated buffer. The pointer to the beginning
    of the buffer is returned in pFileContent. The caller is responsible of
    freeing the memory at *ppFileContent if the return value is \a TRUE.
    \param BinaryFileName    Filename of file to open
    \param ppFileContent     pointer to buffer to receive file content
    \param pFileSize         pointer to file size
    \return TRUE if the file could be buffered, FALSE else
*/
BOOL OpenAndBufferFile(IN  const CH*    BinaryFileName,
                       OUT FWHEADER_t**    ppFileContent,
                       OUT size_t*      pFileSize);

#endif
