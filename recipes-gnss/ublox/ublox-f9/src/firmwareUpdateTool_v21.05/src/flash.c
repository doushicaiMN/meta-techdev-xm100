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
  \brief  Flash organization stuff
*/

#include "flash.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "platform.h"

//! compiled-in version of flash.txt
CH const flashTxt[] = {
"[FLASH] \n"
// lookup table for non cfi compatible flash devices
// manufacturer-device    = <num_sect>x<size_sect> {, <num_sect>x<size_sect>}
//
// ===================================================================================
//  PARALLEL FLASH DEVICES
// ===================================================================================

//  1.8V devices
// -----------------------------------------------------------------------------------
// amd / spansion
"0x0001-0x2270    =     7x64,  1x32,  2x8,  1x16      \n" // AMD        AM29SL400T        4 mbit top
"0x0001-0x22F1    =     1x16,   2x8, 1x32,  7x64      \n" // AMD        AM29SL400B        4 mbit bottom
"0x0001-0x22EA    =    15x64,  1x32,  2x8,  1x16      \n" // AMD        AM29SL800T        8 mbit top
"0x0001-0x226B    =     1x16,   2x8, 1x32, 15x64      \n" // AMD        AM29SL800B        8 mbit bottom
"0x0001-0x22E4    =    31x64,   8x8                   \n" // AMD        AM29SL160T       16 mbit top
"0x0001-0x22E7    =      8x8, 31x64                   \n" // AMD        AM29SL160B       16 mbit bottom
"0x0001-0x2295    =    31x64,   8x8                   \n" // AMD        AM29DS163T       16 mbit top
"0x0001-0x2296    =      8x8, 31x64                   \n" // AMD        AM29DS163B       16 mbit bottom

// atmel
"0x001F-0x00C6    =    15x64,   8x8                   \n" // ATMEL      AT49SV802AT       8 mbit top
"0x001F-0x00C4    =      8x8, 15x64                   \n" // ATMEL      AT49SV802A        8 mbit bottom
//0x001F-0x02C2   =    31x64,   8x8                   \n" // ATMEL      AT49SV163DT      16 mbit top
//0x001F-0x02C0   =      8x8, 31x64                   \n" // ATMEL      AT49SV163D       16 mbit bottom
"0x001F-0x00D1    =    63x64,   8x8                   \n" // ATMEL      AT49SV322AT      32 mbit top
"0x001F-0x00DB    =      8x8, 63x64                   \n" // ATMEL      AT49SV322A       32 mbit bottom
"0x001F-0x01D1    =    63x64,   8x8                   \n" // ATMEL      AT49SV322DT      32 mbit top
"0x001F-0x01DB    =      8x8, 63x64                   \n" // ATMEL      AT49SV322D       32 mbit bottom
// fujistu / spansion
"0x0004-0x2270    =     7x64,  1x32,  2x8,  1x16      \n" // Fujitsu    MBM29SL400T       4 mbit top
"0x0004-0x22F1    =     1x16,   2x8, 1x32,  7x64      \n" // Fujitsu    MBM29SL400B       4 mbit bottom
"0x0004-0x22EA    =    15x64,  1x32,  2x8,  1x16      \n" // Fujitsu    MBM29SL800T       8 mbit top
"0x0004-0x226B    =     1x16,   2x8, 1x32, 15x64      \n" // Fujitsu    MBM29SL800B       8 mbit bottom
"0x0004-0x22E4    =    31x64,   8x8                   \n" // Fujitsu    MBM29SL160T      16 mbit top
"0x0004-0x22E7    =      8x8, 31x64                   \n" // Fujitsu    MBM29SL160B      16 mbit bottom
"0x0004-0x2295    =    31x64,   8x8                   \n" // Fujitsu    MBM29DS163T      16 mbit top
"0x0004-0x2296    =      8x8, 31x64                   \n" // Fujitsu    MBM29DS163B      16 mbit bottom
// sst
"0x00BF-0x272F    =    128x4                          \n" // SST        SST39WF400A       4 mbit
"0x00BF-0x272E    =    128x4                          \n" // SST        SST39WF400B       4 mbit
"0x00BF-0x273F    =    256x4                          \n" // SST        SST39WF800A       8 mbit
"0x00BF-0x273E    =    256x4                          \n" // SST        SST39WF800B       8 mbit
"0x00BF-0x274B    =    512x4                          \n" // SST        SST39WF1601      16 mbit bottom
"0x00BF-0x274A    =    512x4                          \n" // SST        SST39WF1602      16 mbit top
// eon/essi (0x011C = 0x001C on bank two)
"0x011C-0x2270    =     7x64,  1x32,  2x8,  1x16      \n" // EON        EN29SL400T        4 mbit top
"0x011C-0x22F1    =     1x16,   2x8, 1x32,  7x64      \n" // EON        EN29SL400B        4 mbit bottom
"0x011C-0x22EA    =    15x64,  1x32,  2x8,  1x16      \n" // EON        EN29SL800T        8 mbit top
"0x011C-0x226B    =     1x16,   2x8, 1x32, 15x64      \n" // EON        EN29SL800B        8 mbit bottom
"0x011C-0x22E4    =    31x64,   8x8                   \n" // EON        EN29SL160T       16 mbit top
"0x011C-0x22E7    =      8x8, 31x64                   \n" // EON        EN29SL160B       16 mbit bottom
"0x011C-0x273F    =    256x4                          \n" // EON        EN39SL800         8 mbit
"0x011C-0x274B    =    512x4                          \n" // EON        EN39SL160        16 mbit bottom
"0x011C-0x274A    =    512x4                          \n" // EON        EN39SL160        16 mbit top

// 3V devices
//-----------------------------------------------------------------------------------
// alliance
"0x0052-0x22DA    =    15x64,  1x32,  2x8,  1x16      \n" // ALLIANCE   AS29LV800T        8 mbit top
"0x0052-0x225B    =     1x16,   2x8, 1x32, 15x64      \n" // ALLIANCE   AS29LV800B        8 mbit bottom
// amd / spansion
"0x0001-0x22B9    =     7x64,  1x32,  2x8,  1x16      \n" // AMD        AM29LV400BT       4 mbit top
"0x0001-0x22BA    =     1x16,   2x8, 1x32,  7x64      \n" // AMD        AM29LV400BB       4 mbit bottom
"0x0001-0x22DA    =    15x64,  1x32,  2x8,  1x16      \n" // AMD        AM29LV800BT       8 mbit top
"0x0001-0x225B    =     1x16,   2x8, 1x32, 15x64      \n" // AMD        AM29LV800BB       8 mbit bottom
"0x0001-0x22C4    =    31x64,  1x32,  2x8,  1x16      \n" // AMD        AM29LV160B       16 mbit top, cfi 1.0
"0x0001-0x2249    =     1x16,   2x8, 1x32, 31x64      \n" // AMD        AM29LV160B       16 mbit bottom, cfi 1.0
"0x0001-0x222B    =      8x8, 31x64                   \n" // AMD        AM29DL163B       16 mbit bottom
//0x0001-0x227E   =      8x8, 63x64,  8x8             ; AMD        AM29LV320        32 mbit top/bottom, cfi
// amic
"0x0037-0xB334    =     7x64,  1x32,  2x8,  1x16      \n" // Amic       29L400ATV         4 mbit top
"0x0037-0xB3B5    =     1x16,   2x8, 1x32,  7x64      \n" // Amic       29L400AUV         4 mbit bottom
"0x0037-0xB31A    =    15x64,  1x32,  2x8,  1x16      \n" // Amic       29L800ATV         8 mbit top
"0x0037-0xB39B    =     1x16,   2x8, 1x32, 15x64      \n" // Amic       29L800AUV         8 mbit bottom
"0x0037-0x22C4    =    31x64,  1x32,  2x8,  1x16      \n" // Amic       29L160ATV        16 mbit top, cfi 1.0
"0x0037-0x2249    =     1x16,   2x8, 1x32, 31x64      \n" // Amic       29L160AUV        16 mbit bottom, cfi 1.0
// atmel
"0x001F-0x01C3    =    15x64,   8x8                   \n" // ATMEL      AT49LV802AT       8 mbit top, cfi 1.0 atmel
"0x001F-0x01C1    =      8x8, 15x64                   \n" // ATMEL      AT49LV802A        8 mbit bottom, cfi 1.0 atmel
"0x001F-0x01C9    =    63x64,   8x8                   \n" // ATMEL      AT49LV322D       32 mbit top, cfi 1.0 atmel
"0x001F-0x01C8    =      8x8, 63x64                   \n" // ATMEL      AT49LV322DT      32 mbit bottom, cfi 1.0 atmel
// eon/essi (0x011C = 0x001C on bank two)
"0x011C-0x22B9    =     7x64,  1x32,  2x8,  1x16      \n" // EON        EN29LV400AT       4 mbit top
"0x011C-0x22BA    =     1x16,   2x8, 1x32,  7x64      \n" // EON        EN29LV400AB       4 mbit bottom
"0x011C-0x22DA    =    15x64,  1x32,  2x8,  1x16      \n" // EON        EN29LV800BT       8 mbit top
"0x011C-0x225B    =     1x16,   2x8, 1x32, 15x64      \n" // EON        EN29LV800BB       8 mbit bottom
"0x011C-0x22C4    =    31x64,  1x32,  2x8,  1x16      \n" // EON        EN29LV160AT      16 mbit top, cfi 1.0
"0x011C-0x2249    =     1x16,   2x8, 1x32, 31x64      \n" // EON        EN29LV160AB      16 mbit bottom, cfi 1.0
// fujistu / spansion
"0x0004-0x22B9    =     7x64,  1x32,  2x8,  1x16      \n" // Fujitsu    MBM29LV400T       4 mbit top
"0x0004-0x22BA    =     1x16,   2x8, 1x32,  7x64      \n" // Fujitsu    MBM29LV400B       4 mbit bottom
"0x0004-0x22DA    =    15x64,  1x32,  2x8,  1x16      \n" // Fujitsu    MBM29LV800T       8 mbit top
"0x0004-0x225B    =     1x16,   2x8, 1x32, 15x64      \n" // Fujitsu    MBM29LV800B       8 mbit bottom
"0x0001-0x22C4    =    31x64,  1x32,  2x8,  1x16      \n" // Fujitsu    MBM29LV160B      16 mbit top, cfi
"0x0004-0x2249    =     1x16,   2x8, 1x32, 31x64      \n" // Fujitsu    MBM29LV160B      16 mbit bottom, cfi
"0x0004-0x222B    =      8x8, 31x64                   \n" // Fujitsu    MBM29DL163B
// mxic
"0x00C2-0x22B9    =     7x64,  1x32,  2x8,  1x16      \n" // MXIC       MX29LV400T        4 mbit top, cfi 1.0
"0x00C2-0x22BA    =     1x16,   2x8, 1x32,  7x64      \n" // MXIC       MX29LV400B        4 mbit bottom, cfi 1.0
"0x00C2-0x22DA    =    15x64,  1x32,  2x8,  1x16      \n" // MXIC       MX29LV800T        8 mbit top
"0x00C2-0x225B    =     1x16,   2x8, 1x32, 15x64      \n" // MXIC       MX29LV800B        8 mbit bottom
"0x00C2-0x22C4    =    31x64,  1x32,  2x8,  1x16      \n" // MXIC       MX29LV160T       16 mbit top, cfi
"0x00C2-0x2249    =     1x16,   2x8, 1x32, 31x64      \n" // MXIC       MX29LV160B       16 mbit bottom, cfi
// st
"0x0020-0x00EE    =     7x64,  1x32,  2x8,  1x16      \n" // ST         M29W400AT         4 mbit top
"0x0020-0x00EF    =     1x16,   2x8, 1x32,  7x64      \n" // ST         M29W400AB         4 mbit bottom
"0x0020-0x22D7    =    15x64,  1x32,  2x8,  1x16      \n" // ST         M29W800DT         8 mbit top, cfi
"0x0020-0x225B    =     1x16,   2x8, 1x32, 15x64      \n" // ST         M29W800DB         8 mbit bottom, cfi
"0x0001-0x22C4    =    31x64,  1x32,  2x8,  1x16      \n" // ST         M29W160T         16 mbit top, cfi
"0x0020-0x2249    =     1x16,   2x8, 1x32, 31x64      \n" // ST         M29W160B         16 mbit bottom, cfi
"0x0020-0x2256    =    63x64,   8x8                   \n" // ST         M29W320ET        32 mbit top, cfi
"0x0020-0x2257    =      8x8, 63x64                   \n" // ST         M29W320EB        32 mbit bottom, cfi
// sst
"0x00BF-0x2780    =    128x4                          \n" // SST        SST39VF400        4 mbit
"0x00BF-0x2781    =    256x4                          \n" // SST        SST39VF800        8 mbit

// ===================================================================================
//  UNKNOWN DEVICE
// ===================================================================================

// UNKNOWN         =    256x2                          ; if nothing matches use this description

// other possible manufactures are
// amic, excel, intel, macronix, sanyo, sharp, toshiba

};

I4 GetSectorNrForSize(IN const U4 Offset,
                      IN const U4 FwSize,
                      IN const BLOCK_ARR_pt pFlashdef)
{
    assert(pFlashdef);
    U4 size        = 0;  //size
    U4 block       = 0;  //actual block
    U4 blocksec    = 0;  //sector in the actual block
    U4 sectorNr    = 0;  //count sector numbers

    U4 addr = 0;
    while (addr < Offset)
    {
        addr += pFlashdef->pBlocks[block].Size;
        blocksec++;
        if (blocksec >= pFlashdef->pBlocks[block].Count)
        {
            block++;
            blocksec = 0;
            if (block >= pFlashdef->NumBlocks)
            {
                //there are no more sectors behind size of flash
                return -1;
            }
        }
    }

    while (size < FwSize)
    {
        if ((block >= pFlashdef->NumBlocks) && (size < FwSize))
        {
            //somewhat went wrong
            MESSAGE(MSG_ERR, "Cannot determine packet nr for size!");
            return -1;
        }
        sectorNr ++;
        size += pFlashdef->pBlocks[block].Size;
        blocksec++;
        if (blocksec >= pFlashdef->pBlocks[block].Count)
        {
            block++;
            blocksec = 0;
        }
    }
    return sectorNr;
}

I4 GetPacketNrForSector(IN const U4 Sector,
                        IN const BLOCK_ARR_pt pFlashdef,
                        IN const U4 PacketSize)
{
    assert(pFlashdef);
    if(pFlashdef->NumBlocks == 1)
    {
        // uniform flash
        return Sector * pFlashdef->pBlocks[0].Size / PacketSize;
    }
    else
    {
        I4 packet   = 0;
        U4 block    = 0;  //actual block
        U4 blocksec = 0;  //sector in the actual block

        U4 sec_cnt  = 0;
        // sum up the number of packets of the sectors
        while (sec_cnt < Sector)
        {
            sec_cnt++;
            if (block >= pFlashdef->NumBlocks)
            {
                //somewhat went wrong
                MESSAGE(MSG_ERR, "GetPacketNrForSector: Sec %u Packetsize %u", Sector, PacketSize);
                return -1;
            }
            packet += pFlashdef->pBlocks[block].Size / PacketSize;
            blocksec++;
            if (blocksec >= pFlashdef->pBlocks[block].Count)
            {
                block++;
                blocksec = 0;
            }
        }
        return packet;
    }
}

I4 GetSectorNrForAddress(IN const U4 Address,
                         IN const U4 Base,
                         IN const U4 Offset,
                         IN const BLOCK_ARR_pt pFlashdef)
{
    assert(pFlashdef);
    U4 block    = 0;  //actual block
    U4 blocksec = 0;  //sector in the actual block

    // advance block and blocksec to offset address
    U4 addr = 0;
    while (addr < Offset)
    {
        addr += pFlashdef->pBlocks[block].Size;
        blocksec++;
        if (blocksec >= pFlashdef->pBlocks[block].Count)
        {
            //next block
            block++;
            blocksec = 0;
            if (block >= pFlashdef->NumBlocks)
            {
                //cannot erase a section behind size of flash
                return -1;
            }
        }
    }

    // sum up the number of sectors
    U4 sec_cnt = 0;
    while (addr < (Address-Base+Offset))
    {
        addr += pFlashdef->pBlocks[block].Size;
        // address within the limits of the actual sector?
        if (addr > (Address-Base+Offset))
        {
            break;
        }
        // perhaps address fits into next sector's ranges
        sec_cnt ++;
        blocksec++;
        if (blocksec >= pFlashdef->pBlocks[block].Count)
        {
            block++;
            blocksec = 0;
            if (block >= pFlashdef->NumBlocks)
            {
                // there are no more sectors available on flash
                return -1;
            }
        }
    }
    return sec_cnt;
}

U4 GetPacketNrForAddress(IN const U4 Address,
                         IN const U4 Base,
                         IN const U4 PacketSize)
{
    U4 RelAddr = Address - Base;
    U4 Packet = RelAddr / PacketSize;
    return Packet;
}

void DumpFlashInfo(IN BLOCK_ARR_t const * const pkFlashOrg,
                   IN const U4 FlashSize)
{
    assert(pkFlashOrg);
    MESSAGE(MSG_DBG, "Flash size:   %d", FlashSize);
    size_t i;
    for(i = 0; i<pkFlashOrg->NumBlocks; i++)
    {
        MESSAGE(MSG_DBG, " Flash block:  %d x %d", pkFlashOrg->pBlocks[i].Count, pkFlashOrg->pBlocks[i].Size);
    }
}

BOOL GetFlashOrganisation(IN    U2 const ManId,
                          IN    U2 const DevId,
                          INOUT BLOCK_ARR_pt pBlocks,
                          INOUT U4* const pSize,
                          IN    CH const * const Filename)
{
    assert(pBlocks);
    CH* file = NULL;
    CH* fileTmp = NULL;
    if (strlen(Filename)>0) // is there a file at all?
    {
        FILE *stream=fopen(Filename, "r");
        if (!stream)
        {
            //MESSAGE(MSG_ERR, "Could not open file '%s'", Filename);
            return FALSE;
        }

        //clear flash definition description vector
        clearBlocks(pBlocks);

        //get file size and allocate memory
        int ret=fseek(stream, 0, SEEK_END);
        if(ret)
        {
            fclose(stream);
            return FALSE;
        }
        long filesize = ftell(stream);
        fileTmp = (CH*) malloc(filesize);
        if (fileTmp == NULL)
        {
            MESSAGE(MSG_ERR, "Allocation of file buffer memory failed.");
            fclose(stream);
            return FALSE;
        }
        //copy file content to ram buffer
        rewind(stream);
        size_t numRead=fread(fileTmp, 1, filesize, stream);
        fclose(stream);
        if(numRead!=(size_t)filesize)
        {
            free(fileTmp);
            return FALSE;
        }
        file = fileTmp;
    }
    else
    {
        // use compiled-in version
        MESSAGE(MSG_DBG,"Use compiled-in Flash Description");
        fileTmp = (CH*) malloc(sizeof(flashTxt));
        if (fileTmp == NULL)
        {
            MESSAGE(MSG_ERR, "Allocation of file buffer memory failed.");
            return FALSE;
        }
        memcpy(fileTmp, flashTxt, sizeof(flashTxt));
        file = fileTmp;
    }

    //file content now in 'file', process it
    CH* pEnd;
    CH key[32];
    //create search string
    sprintf(key, "0x%04X-0x%04X", ManId, DevId);
    //search for manid, devid
    CH* pPos = strstr(file, key);
    if (pPos == NULL)
    {
        free(file);
        return FALSE;
    }
    //description was found, extract organization
    pPos = strchr(pPos, '=');
    if (pPos == NULL)
    {
        free(file);
        return FALSE;
    }
    // remove equal sign and all leading spaces
    pPos += strspn(pPos, "\t =");
    // remove comments
    pEnd = strpbrk(pPos, ";\r\n\0");
    if(pEnd == NULL)
    {
        free(file);
        return FALSE;
    }
    *pEnd = '\0';
    // remove trailing spaces
    while ((pEnd >= pPos) && (isspace(*pEnd) || (*pEnd == 0) || (*pEnd == '\n') || (*pEnd == '\r')))
    {
        *pEnd = '\0';
        pEnd --;
    }
    // parse syntax
    *pSize = 0;
    while (*pPos != '\0')
    {
        CH* stopstring;
        U4 value = strtoul(pPos, &stopstring, 10);
        if ((stopstring > pPos) && (*stopstring == 'x') && (value > 0))
        {
            BLOCK_DEF_t block;
            block.Count = value;
            pPos = stopstring + 1;
            value = strtoul(pPos, &stopstring, 10);
            if ((stopstring > pPos) && (value > 0))
            {
                block.Size = value * 1024;
                pPos = stopstring;

                *pSize += block.Count * block.Size;
                addBlock(pBlocks, &block);
                //skip spaces
                while (isspace(*pPos) || *pPos == ',')
                {
                    pPos ++;
                }
            }
        }
    }
    if (fileTmp)
        free(fileTmp);

    return (pBlocks->NumBlocks != 0);
}

void clearBlocks(INOUT BLOCK_ARR_pt pBlocks)
{
    assert(pBlocks);
    assert( ( pBlocks->pBlocks &&  pBlocks->NumBlocks)
         || (!pBlocks->pBlocks && !pBlocks->NumBlocks) );
    free(pBlocks->pBlocks);
    pBlocks->pBlocks=NULL;
    pBlocks->NumBlocks=0;
}

BOOL addBlock(INOUT BLOCK_ARR_pt pBlocks,
              IN    BLOCK_DEF_pt const pNewBlock)
{
    assert(pBlocks && pNewBlock);
    assert( ( pBlocks->pBlocks &&  pBlocks->NumBlocks)
         || (!pBlocks->pBlocks && !pBlocks->NumBlocks) );

    BOOL result=FALSE;
    BLOCK_DEF_pt pTmpBlocks = (BLOCK_DEF_pt) realloc(pBlocks->pBlocks, (size_t)((pBlocks->NumBlocks + 1)*sizeof(*pNewBlock)));

    if(pTmpBlocks)
    {
        memcpy(&pTmpBlocks[pBlocks->NumBlocks], pNewBlock, sizeof(*pNewBlock));
        pBlocks->pBlocks=pTmpBlocks;
        ++pBlocks->NumBlocks;
        result=TRUE;
    }

    return result;
}

