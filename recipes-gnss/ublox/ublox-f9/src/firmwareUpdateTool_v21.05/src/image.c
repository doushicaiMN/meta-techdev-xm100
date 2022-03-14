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
  \brief  Image check and buffering routines
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image.h"
#include "checksum.h"
#include "platform.h"
#include "mergefis.h"

#define EXT2_BASE    0x00800000    //!< Flash base address on u-blox 5/6

//
//------------------------------------------------------------------------------
// Initialization global magic words - declaration in types.h
//------------------------------------------------------------------------------
const U4 magicG50 = ((U4)('U')) | ((U4)('B')<<8) | ((U4)('X')<<16) | ((U4)('5')<<24);
const U4 magicG51 = ((U4)('U')) | ((U4)('B')<<8) | ((U4)('5')<<16) | ((U4)('1')<<24);
const U4 magicG70 = ((U4)('U')) | ((U4)('B')<<8) | ((U4)('X')<<16) | ((U4)('7')<<24);
const U4 magicG80 = ((U4)('U')) | ((U4)('B')<<8) | ((U4)('X')<<16) | ((U4)('8')<<24);

U4 ValidateImage(IN FWHEADER_t const *pImage,
                 IN const size_t      fileSize,
                 OUT FWFOOTERINFO_t  *pFwFooter)
{
    CH verString[100];
    //check file size, must be at least header size
    if (fileSize < sizeof(FWHEADER_t))
    {
        return 0;
    }
    //get the header
    U4 generation =
            (pImage->v1.magic == magicG50) ?          50 :  // u-blox5
            (pImage->v1.magic == magicG51) ?          60 :  // u-blox6
            (pImage->v1.magic == magicG70) ?          70 :  // u-blox7
            (pImage->v1.magic == magicG80) ?          80 :  // u-blox8
            0;
    /***************************************************
    * Check if valid image footer is present and parse *
    ***************************************************/
    {
        CH* pMagic;
        BOOL magicFound = FALSE;
        U i;
        for (i = 0; i < 4; i++) // image might be padded with up to 3 0xFF
        {
            pMagic = ((CH*)pImage) + fileSize - 4 - i;
            if (pMagic[0] == 'U' && pMagic[1] == 'B' && pMagic[2] == 'F' && pMagic[3] == 'L')
            {
                magicFound = TRUE;
                break;
            }
        }
        if (magicFound)
        {
            U4 version;
            U4 crc;
            U4 footerSize;
            memcpy(&version, pMagic - sizeof(U4), sizeof(version));
            memcpy(&pFwFooter->numberOfImages, pMagic - (3 * sizeof(U4)), sizeof(version));
            footerSize = 5 * sizeof(U4) + sizeof(U4) * pFwFooter->numberOfImages;
            if (version == 0 &&
                pFwFooter->numberOfImages <= NUMOF(pFwFooter->imagesSize) &&
                memcpy(&crc, pMagic - (footerSize - sizeof(U4)), sizeof(crc)) != 0 &&
                lib_crc_crc32(0xF5D4C69, pMagic - (footerSize - 2 * sizeof(U4)), (footerSize - sizeof(U4))) == crc)
            {
                for (i = 0; i < pFwFooter->numberOfImages; i++)
                {
                    memcpy(&pFwFooter->imagesSize[i], pMagic - (footerSize - (sizeof(U4) * (i + 2))), sizeof(pFwFooter->imagesSize[i]));
                    MESSAGE(MSG_DBG, "image %u size %u", i, pFwFooter->imagesSize[i]);
                }
                memcpy(&pFwFooter->cfgSize, pMagic - 2 * sizeof(U4), sizeof(pFwFooter->cfgSize));
                MESSAGE(MSG_DBG, "config size %u", pFwFooter->cfgSize);
                if (generation == 0)
                {
                    return 91; //Image magic is not present but valid version 0 image footer found so this is U-blox 9 image, cannot check anything more because it is scrumbled
                }
            }
            else
            {
                MESSAGE(MSG_ERR, "Invalid footer found");
                return 0;
            }
        }
    }
    if (!generation)
    {
        // magic word not found
        MESSAGE(MSG_LEV2, "No magic word found");
        return 0;
    }

    if (pImage->v1.pEnd   < pImage->v1.pStart  ||
        pImage->v1.pStart < pImage->v1.pBase   ||
        ((U4)pImage->v1.pEnd   & 0xFF800000) != EXT2_BASE ||
        ((U4)pImage->v1.pBase  & 0xFF800000) != EXT2_BASE ||
        ((U4)pImage->v1.pStart & 0xFF800000) != EXT2_BASE   )
    {
        //pImage->pointer(s) seems to be corrupt
        MESSAGE(MSG_LEV2, "Image addresses invalid");
        return FALSE;
    }
    //magic word found and pointers seem to be OK, check CRC
    U4 crcpos = (pImage->v1.pEnd & ~0x1) - pImage->v1.pBase;
    U4 crcrange = crcpos - 4; // CRC starts behind magic word
    BOOL crcOk = CheckUbxChecksumU4((U4*)((U1*)pImage+4), crcrange);

    // one could return at this stage to detect validity of the image. we
    // additionally grab the version string out of the image
    // the ROM version the firmware was built for is contained in the lower
    // 7 bits of the highest byte of the hardware info word
    if (crcOk)
    {
        memset(verString, 0, sizeof(verString));
        if ((pImage->v1.pVersion & 0xFF800000) == EXT2_BASE)
        {
            CH* FwVerStr = (CH*)((U1*)pImage + (pImage->v1.pVersion - pImage->v1.pBase));
            if (FwVerStr < (CH*)((U1*)pImage + fileSize))
            {
                strncpy(verString, FwVerStr, sizeof(verString));
            }
        }

        MESSAGE(MSG_LEV2, "Image (file size %u) for u-blox%d accepted", fileSize, generation / 10);
        MESSAGE(MSG_LEV2, "Image Ver '%s'", verString);
        MESSAGE(MSG_DBG, "CRC= 0x%08X 0x%08X",
            *((U4*)((U1*)pImage + crcpos)),
            *((U4*)((U1*)pImage + crcpos + 4)));
    }
    return crcOk ? generation : 0;
}



BOOL OpenAndBufferFile(IN  const CH*    BinaryFileName,
                       OUT FWHEADER_t** ppFileContent,
                       OUT size_t*      pFileSize)
{
    if (!BinaryFileName || !strlen(BinaryFileName))
    {
        MESSAGE(MSG_ERR, "File '%s' is not valid", BinaryFileName);
        return FALSE;
    }

    FILE *stream=fopen(BinaryFileName, "rb");

    if (!stream)
    {
        MESSAGE(MSG_ERR, "Could not open file '%s'.", BinaryFileName);
        return FALSE;
    }

    //get filesize and allocate memory
    int ret=fseek(stream, 0, SEEK_END);
    if(ret)
    {
        MESSAGE(MSG_ERR, "Could not set pointer to start in file '%s'", BinaryFileName);
        fclose(stream);
        return FALSE;
    }
    long filesize = ftell(stream);
    long ffAppend = (4 - (filesize & 0x3)) & 0x3;
    CH* fileTmp = (CH*)malloc(filesize + ffAppend);
    if (fileTmp == NULL)
    {
        MESSAGE(MSG_ERR, "Allocation of file buffer memory failed.");
        fclose(stream);
        return FALSE;
    }

    //copy file content to ram buffer
    ret=fseek(stream, 0, SEEK_SET);
    if(ret)
    {
        MESSAGE(MSG_ERR, "Seeking in the file failed.");
        fclose(stream);
        free(fileTmp);
        return FALSE;
    }

    // Read data
    size_t numRead=fread(fileTmp, 1, (size_t)filesize, stream);
    fclose(stream);
    if(numRead!=(size_t)filesize)
    {
        MESSAGE(MSG_ERR, "Wrong number of bytes read. Read = %ld, Expected = %ld", numRead, filesize);
        free(fileTmp);
        return FALSE;
    }
    memset(fileTmp + filesize, 0xFF, ffAppend); // append FFs
    *pFileSize = filesize + ffAppend;
    *ppFileContent = (FWHEADER_t*)fileTmp;
    return TRUE;
}

