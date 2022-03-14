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

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mergefis.h"

#ifdef WIN32
# include <io.h>
# ifndef read
#  define _read read
# endif //read
# ifndef O_RDONLY
#  define O_RDONLY _O_RDONLY
# endif
#else //WIN32
# include <unistd.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
#endif //else WIN32


//! Rule for an XML element or attribute
typedef struct
{
    char name[256];              //!< Name of the element or attribute
                                 //!< concerned by this rule. If empty string:
                                 //!< A reference to text content
    yxml_ret_t contentType;      //!< YXML_ATTRVAL if this rule concerns an
                                 //!< attribute. YXML_CONTENT if this rule
                                 //!< concerns an element
    char belongsTo[1024];        //!< Pipe ('|') separated list of elements
                                 //!< this attribute might be part of,
                                 //!< respectively this element might be a
                                 //!< child of. An empty string indicates a
                                 //!< potential root element.
} XML_RULE_t;

//! An XML Attribute can be defined by this structure
typedef struct
{
    char *name;                  //!< Name of the attribute
    char *value;                 //!< Value of the attribute
} XML_ATTR_t;

//! Defines an XML element in a XML tree
typedef struct XML_ELEM_s
{
    char *name;                  //!< Name of the element
    struct XML_ELEM_s *parent;   //!< Parent of the element. This value is
                                 //!< NULL in case this is a root element
    unsigned int numChildren;    //!< Number of elements defined within this
                                 //!< element
    struct XML_ELEM_s *children; //!< Array of elements that are defined
                                 //!< within this element
    unsigned int numAttr;        //!< Number of attributes defined within
                                 //!< this element
    XML_ATTR_t *attr;            //!< Array of attributes defined within
                                 //!< this element
    char *value;                 //!< Text string defined within this
                                 //!< element
} XML_ELEM_t;


/*!
    This table provides a lookup table for the 32 Bit CRC algorithm
    \sa lib_crc_crc32
 */
const unsigned int lib_crc_kCrc32table[256] =
{
    /* 0x00 */ 0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    /* 0x04 */ 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    /* 0x08 */ 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    /* 0x0C */ 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    /* 0x10 */ 0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
    /* 0x14 */ 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    /* 0x18 */ 0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
    /* 0x1C */ 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    /* 0x20 */ 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    /* 0x24 */ 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    /* 0x28 */ 0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
    /* 0x2C */ 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    /* 0x30 */ 0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
    /* 0x34 */ 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    /* 0x38 */ 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    /* 0x3C */ 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    /* 0x40 */ 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
    /* 0x44 */ 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    /* 0x48 */ 0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
    /* 0x4C */ 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    /* 0x50 */ 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    /* 0x54 */ 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    /* 0x58 */ 0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
    /* 0x5C */ 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    /* 0x60 */ 0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
    /* 0x64 */ 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    /* 0x68 */ 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    /* 0x6C */ 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    /* 0x70 */ 0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
    /* 0x74 */ 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    /* 0x78 */ 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
    /* 0x7C */ 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    /* 0x80 */ 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    /* 0x84 */ 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    /* 0x88 */ 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
    /* 0x8C */ 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    /* 0x90 */ 0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
    /* 0x94 */ 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    /* 0x98 */ 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    /* 0x9C */ 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    /* 0xA0 */ 0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
    /* 0xA4 */ 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    /* 0xA8 */ 0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
    /* 0xAC */ 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    /* 0xB0 */ 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    /* 0xB4 */ 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    /* 0xB8 */ 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
    /* 0xBC */ 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    /* 0xC0 */ 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
    /* 0xC4 */ 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    /* 0xC8 */ 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    /* 0xCC */ 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    /* 0xD0 */ 0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
    /* 0xD4 */ 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    /* 0xD8 */ 0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
    /* 0xDC */ 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    /* 0xE0 */ 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    /* 0xE4 */ 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    /* 0xE8 */ 0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
    /* 0xEC */ 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    /* 0xF0 */ 0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
    /* 0xF4 */ 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    /* 0xF8 */ 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    /* 0xFC */ 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D,
};

//! Free all contents in all elements of the XML tree to which data belongs
/*! All contents and elements contained in the tree to which data belongs
    will be freed.

    \param data             : An element of the tree which has to be freed
*/
static void freeXmlTree(XML_ELEM_t *data)
{
    if(data)
    {
        const size_t MAX_DEPTH = 1024;

        // Two options
        // - If this is the first call to this function, find the root,
        //   initiate the freeing of all children by calling this function
        //   again after having set the children parent to their own pointer
        //   value and clean up the own stuff as well as the data pointer as
        //   well!
        // - If this is a consecutive call to this function,
        //   (data == data->parent), initiate the deletion of all children
        //   clean up the own stuff and return
        // Find the top element if parent is self,
        // a parent exists and has reset the parent to
        // that value
        size_t i;
        for(i=0; i < MAX_DEPTH && data->parent && data != data->parent; ++i)
        {
            data = data->parent;
        }
        // Only the tree is small enough to be cleaned up
        // by this function
        if(i < MAX_DEPTH)
        {
            for(i = 0; i < data->numChildren; ++i)
            {
                data->children[i].parent = &data->children[i];
                freeXmlTree(&data->children[i]);
            }
            for(i = 0; i < data->numAttr; ++i)
            {
                free(data->attr[i].name);
                free(data->attr[i].value);
            }
            free(data->attr);
            free(data->children);
            free(data->name);
            free(data->value);
            // This is the root element, clean up the own pointer
            if(!data->parent)
                free(data);
        }
    }
}

//! Find the JEDEC attribute and compare it to the integer argument
/*! Find the JEDEC attribute in the passed element and compare it to
    the integer JEDEC passed as argument.

    \param elem              : Element in which the JEDEC attribute is
                               searched for.
    \param jedec             : The JEDEC value to which the attribute
                               must be compared if found.
    \return                    On success a pointer to the JEDEC attribute,
                               otherwise NULL.
*/
static XML_ATTR_t * findJedec(XML_ELEM_t *elem, unsigned int const jedec)
{
    // Check if this device child has the right JEDEC attribute
    unsigned int j;
    XML_ATTR_t * result = NULL;
    for(j = 0; j < elem->numAttr; ++j)
    {
        if(!strcmp(elem->attr[j].name, "jedec"))
        {
            // Replace starting 'x' if existing
            if(elem->attr[j].value[0] == 'x')
            {
                elem->attr[j].value[0] = ' ';
            }
            // Correct JEDEC?
            if((unsigned int) strtol(elem->attr[j].value, NULL, 16)==jedec)
                result = &elem->attr[j];
        }
    }
    return result;
}

//! Find the attribute value with the specified name in the provided element
/*! Get the attribute value with the specified name in the provided element

    \param attrName          : Name of the attribute of which the value
                               should be returned
    \param elem              : Element in which the attribute should
                               should be located
    \return                    On success the pointer to the string value
                               of the specified attribute. Otherwise NULL
*/
static char const * getAttrValue( char const *attrName
                                , XML_ELEM_t const *elem )
{
    if(!attrName || !elem)
        return NULL;

    char * result=NULL;
    unsigned int i;
    for(i = 0; i < elem->numAttr; ++i)
    {
        if(!strcmp(elem->attr[i].name, attrName))
            result = elem->attr[i].value;
    }
    return result;
}

//! Find the value of the child-element with the specified name
/*! Get the value of the child-element with the specified name
    in the provided element

    \param elemName          : Name of the element of which the value
                               should be returned
    \param elem              : Element in which the element of the given
                               name should exist as child
    \return                    On success the pointer to the string value
                               of the specified element. Otherwise NULL
*/
static char const * getElemValue( char const *elemName
                                , XML_ELEM_t const *elem )
{
    if(!elemName || !elem)
        return NULL;

    char * result=NULL;
    unsigned int i;
    for(i = 0; i < elem->numChildren; ++i)
    {
        if(!strcmp(elem->children[i].name, elemName))
            result = elem->children[i].value;
    }
    return result;
}

//! Does the current XML parsing state point to the element or attribute name?
/*! Check if the current state of parsing is occupied with the element and/or
    attribute provided. If only the element is specified it is enough if the
    parser is processing an element of the specified name at the moment
    to make the test succeed. If additionally an attribute is specified
    (non-NULL), the parser must currently also be processing an attribute
    of the specified name within the specified element.

    \param xmlData           : Current XML parsing state
    \param tag               : Name of the tag for which it should be
                               checked if the the parser is currently
                               processing it.
    \param attr              : Name of the attribute for which it should be
                               checked if the the parser is currently
                               processing it. (May be NULL)
    \return                    TRUE on success, FALSE otherwise
*/
static BOOL isElementOrAttr( XML_HNDL_t const *xmlData
                           , char const *tag
                           , char const *attr )
{
    if(!tag || !xmlData)
        return FALSE;

    BOOL result=FALSE;
    if(xmlData->state.elem && !strcmp(tag, xmlData->state.elem))
    {
        if(attr)
        {
            if(xmlData->state.attr && !strcmp(attr, xmlData->state.attr))
                result=TRUE;
        }
        else if(!xmlData->state.attr) // attr must not be set
        {
            // Only Element requested and
            result=TRUE;
        }
    }
    return result;
}

//! Find the next value for the passed element or attribute name in an XML file
/*! If only a tag is passed to this function it will search for the
    content of the next tag element of this name. If additionally an attribute
    is specified, the value of the the next attribute of that name in the
    next tag of the specified name is returned.

    \param xmlData           : Reference to the XML tree (Must not be NULL)
    \param tag               : The tag name (Must no be NULL)
    \param attr              : Attribute name (May be NULL)
    \param result            : Found string. (Must not be NULL)
    \return                    -1 on failure. 0 if not found. Otherwise the
                               length of the zero terminated string in result
*/
static long long findNextInXml(XML_HNDL_t *xmlData, char const * tag, char const * attr, char **result)
{
    if(!xmlData || !tag || !result)
        return -1;

    const size_t blockSize=128;
    char *data=NULL;
    size_t allocMemory=0;

    BOOL endLoop=FALSE;

    // Read and parse the remaining parts of the XML
    // to make sure it is not malformed
    do
    {
        // Every read error is handled as unexpected EOF
        char byte;
        int rd=read(xmlData->fd, &byte, sizeof(byte));
        if( rd == sizeof(byte))
        {
            yxml_ret_t ret=yxml_parse(&xmlData->state, byte);
            // Has an error occurred?
            if(ret<YXML_OK)
            {
                free(data);
                return -1;
            }

            // Check if memory has been allocated. If yes
            // we are already in the right location to copy
            // the content data to this destination. Otherwise
            // check if the current byte has changed our location
            // to the intended value-field
            if(!data)
            {
                // Is this the content of the element
                // or attribute searched for?
                if( ((!attr && ret==YXML_CONTENT) || ( attr && ret==YXML_ATTRVAL))
                 && isElementOrAttr(xmlData, tag, attr) )
                {
                    // Element with right tag name has begun
                    data = malloc(blockSize);
                    if(!data)
                        return -1;

                    assert(blockSize >= sizeof(xmlData->state.data));

                    // Copy the initial content bytes to the destination buffer
                    // and take care of the terminating '\0' as well this time,
                    // so we can ignore it afterwards
                    allocMemory = blockSize;
                    memcpy(data, xmlData->state.data, strlen(xmlData->state.data) + 1);
                }
            }
            else // Copy data
            {
                if(ret!=YXML_CONTENT && ret!=YXML_ATTRVAL)
                {
                    // The data copying must be stopped
                    endLoop=TRUE;
                }
                else
                {
                    size_t numNewBytes = strlen(xmlData->state.data)+1;
                    size_t numBytesStored=strlen(data)+1;
                    // Is it required to enlarge the destination buffer?
                    // Don't count both '\0', one will be deleted (that's the -1)
                    if(numNewBytes + numBytesStored - 1 > allocMemory)
                    {
                        do
                        {
                            allocMemory += blockSize;
                        } while(numNewBytes + numBytesStored - 1 > allocMemory);

                        // Get the new memory size which is enough
                        // to store all the data
                        char * tmpData = realloc(data, allocMemory);
                        if(!tmpData)
                        {
                            free(data);
                            return -1;
                        }
                        data=tmpData;
                    }

                    // Copy the data by overwriting the previous \0
                    // termination (that's the -1) of the string and
                    // appending
                    // he remaining data
                    assert(numBytesStored);
                    memcpy(data + numBytesStored - 1, xmlData->state.data, numNewBytes);
                }
            }
        }
        else if (rd == 0)
        {
            // File ended, but the caller has to find that out again
            endLoop=TRUE;
        }
    } while(!endLoop);

    // Return result if something found
    *result = data;
    size_t resultLen = data ? strlen(data) : 0;
    return resultLen;
}

static void hex_string_to_char_array(const char *hex_string, char *data)
{
    assert(hex_string && data);
    unsigned int i;
    for(i = 0; i < strlen(hex_string); i+=2)
    {
        char buf[3];
        buf[0] = hex_string[i];
        buf[1] = hex_string[i+1];
        buf[2] = '\0';

        unsigned int c = 0;
        sscanf(buf, "%x", &c);
        data[i>>1] = (char)c;
    }
}

static void GetUbxChecksumU4(unsigned int * pCrc_a,
                 unsigned int * pCrc_b,
                 const unsigned int * pData,
                 size_t length)
{
    unsigned int crc_a = 0;
    unsigned int crc_b = 0;
    length /= 4;
    while (length--)
    {
        crc_a += *pData++;
        crc_b += crc_a;
    }
    *pCrc_a = crc_a;
    *pCrc_b = crc_b;
}

static BOOL CheckUbxChecksumU4(const unsigned int* pData,
                   size_t    length)
{
    unsigned int crc_a = 0;
    unsigned int crc_b = 0;
    GetUbxChecksumU4(&crc_a, &crc_b, pData, length);
    pData += (length/4);
    return (crc_a == *pData && crc_b == *(pData+1));
}

unsigned int lib_crc_crc32(unsigned int seed,
                           const void *pkAdr,
                           unsigned int size)
{
    const unsigned char* pk = (const unsigned char*)pkAdr;
    unsigned int crc = seed;
    while (size --)
    {
        crc = (((crc >> 8) & 0x00FFFFFF) ^ lib_crc_kCrc32table[(crc ^ *pk++) & 0x000000FF]);
    }
    return crc;
}

//! Update the CRC of the flash image
/*!
    \param  pImage      Pointer to the image
    \param  ImageSize   Size of the image
    \return TRUE        Updating of the CRC succeeded
*/
static BOOL UpdateImageCrc(char * pImage, const size_t ImageSize)
{
    unsigned int crc_a;
    unsigned int crc_b;
    unsigned int* pCrcrangestart = (unsigned int*)((char*)pImage+4);
    unsigned int crcpos = ImageSize - 8; // CRC is two words long
    unsigned int crcrangesize = crcpos - 4; // CRC starts behind magic word
    GetUbxChecksumU4(&crc_a, &crc_b, pCrcrangestart, crcrangesize);
    //MESSAGE(MSG_LEV2, "Updating CRC of buffered image to 0x%08X 0x%08X", crc_a, crc_b);
    unsigned int* pCrc = (unsigned int*)((char*)(pImage) + crcpos);
    *pCrc = crc_a;
    *(pCrc+1) = crc_b;
    return CheckUbxChecksumU4(pCrcrangestart, crcrangesize);
}

//! Parse the provided XML file and extract a tree according to the rules
/*! The provided XML file will be parsed according to the rules of the
    array of allowed entries. On success the generated tree should
    contain all children, attributes and values in the specified section.

    \param xmlData           : The current state of the XML parsing
                               at which parsing continues within
                               this file
    \param allowedEntries    : The structure that defines what
                               elements and attributes are allowed
                               where and which elements are potential
                               root elements for the generated tree
    \param numAllowedEntries : Number of entries in allowedEntries
    \param root              : The passed pointer will on success
                               point to the highest element of the XML
                               tree. This has to be freed by the
                               calling function with freeXmlTree on
                               success.
    \return                    TRUE on success, FALSE otherwise
*/
BOOL buildXmlTree( XML_HNDL_t *xmlData
                 , XML_RULE_t const *allowedEntries
                 , unsigned int numAllowedEntries
                 , XML_ELEM_t **root)
{
    if(!xmlData || !allowedEntries || !numAllowedEntries || !root)
        return FALSE;

    // Make sure there is exactly one root element.
    unsigned int countRoot = 0;
    unsigned int i;
    for(i = 0; i < numAllowedEntries; ++i)
    {
        if( allowedEntries[i].belongsTo[0] == '\0'
         && allowedEntries[i].contentType == YXML_CONTENT )
            ++countRoot;
    }
    // At least one root element must exist
    if(countRoot == 0)
        return FALSE;

    XML_ELEM_t *curr=malloc(sizeof(*curr));
    if(!curr)
        return FALSE;
    XML_ELEM_t *dataRoot=curr;
    memset(curr, 0, sizeof(*curr));
    BOOL rootFound=FALSE;
    // Read and parse the remaining parts of the XML
    // to make sure it is not malformed
    yxml_ret_t prevState = YXML_OK;
    BOOL contentLeadingWs = FALSE;
    do
    {
        assert(curr && dataRoot);
        // Every read error is handled as unexpected EOF
        char byte;
        int rd=read(xmlData->fd, &byte, sizeof(byte));
        if( rd == sizeof(byte))
        {
            yxml_ret_t ret = yxml_parse(&xmlData->state, byte);
            // Has an error occurred?
            if(ret < YXML_OK)
            {
                freeXmlTree(dataRoot);
                return FALSE;
            }


            // Find root element if not done so yet
            if(!rootFound)
            {
                // Is this a possible root beginning?
                if(ret == YXML_ELEMSTART)
                {
                    assert(dataRoot==curr);
                    // Check that this element is to be allowed a root element
                    for(i = 0; i < numAllowedEntries && !rootFound; ++i)
                    {
                        if( allowedEntries[i].belongsTo[0] == '\0'
                         && allowedEntries[i].contentType == YXML_CONTENT )
                        {
                            if(!strcmp(allowedEntries[i].name, xmlData->state.elem))
                                rootFound=TRUE;
                        }
                    }
                    // Copy element name
                    if(rootFound)
                    {
                        dataRoot->name=strdup(xmlData->state.elem);
                        if(!dataRoot->name)
                        {
                            freeXmlTree(dataRoot);
                            return FALSE;
                        }
                    }
                }
            }
            else // root already found, process children and siblings
            {
                switch(ret)
                {
                    case YXML_ELEMSTART:
                    {
                        // XML Tag: root, child or 'sibling'?
                        // Check if child is allowed to be child of this parent
                        BOOL valid=FALSE;
                        for(i = 0; i < numAllowedEntries; ++i)
                        {
                            // Is this new element found in the list?
                            if( !strcmp(allowedEntries[i].name, xmlData->state.elem)
                             && allowedEntries[i].contentType == YXML_CONTENT )
                            {
                                // If yes, the parent also has to be an allowed one
                                if(strstr(allowedEntries[i].belongsTo, curr->name))
                                {
                                    valid=TRUE;
                                }
                            }
                        }
                        if(!valid)
                        {
                            freeXmlTree(dataRoot);
                            return FALSE;
                        }

                        // create child
                        XML_ELEM_t *tmpChildren = realloc(curr->children, (1+curr->numChildren)*sizeof(*tmpChildren));
                        if(tmpChildren)
                        {
                            curr->children = tmpChildren;
                            ++curr->numChildren;
                        }
                        else
                        {
                            freeXmlTree(dataRoot);
                            return FALSE;
                        }

                        // switch to child
                        XML_ELEM_t * parent = curr;
                        curr = &curr->children[curr->numChildren-1];
                        memset(curr, 0, sizeof(*curr));

                        // fill child data
                        curr->parent=parent;

                        // Copy element name
                        curr->name=strdup(xmlData->state.elem);
                        if(!curr->name)
                        {
                            freeXmlTree(dataRoot);
                            return FALSE;
                        }
                        break;
                    }
                    case YXML_ATTRSTART:
                    {
                        assert(!strcmp(xmlData->state.elem, curr->name));

                        // Is this attribute allowed here?
                        BOOL valid=FALSE;
                        for(i = 0; i < numAllowedEntries; ++i)
                        {
                            // Is this new element found in the list?
                            if( !strcmp(allowedEntries[i].name, xmlData->state.attr)
                             && allowedEntries[i].contentType == YXML_ATTRVAL )
                            {
                                // If yes, the parent also has to be an allowed one
                                if(strstr(allowedEntries[i].belongsTo, curr->name))
                                    valid=TRUE;
                            }
                        }
                        if(!valid)
                        {
                            freeXmlTree(dataRoot);
                            return FALSE;
                        }

                        // Create the attribute
                        XML_ATTR_t *tmpAttr = realloc(curr->attr, (1+curr->numAttr)*sizeof(*tmpAttr));
                        if(tmpAttr)
                        {
                            curr->attr = tmpAttr;
                            ++curr->numAttr;
                        }
                        else
                        {
                            freeXmlTree(dataRoot);
                            return FALSE;
                        }

                        memset(&curr->attr[curr->numAttr-1], 0, sizeof(*curr->attr));

                        // Copy element name
                        curr->attr[curr->numAttr-1].name=strdup(xmlData->state.attr);
                        if(!curr->attr[curr->numAttr-1].name)
                        {
                            freeXmlTree(dataRoot);
                            return FALSE;
                        }
                        break;
                    }
                    case YXML_ATTRVAL:
                    {
                        assert(curr->attr && curr->numAttr);
                        char * attrVal = curr->attr[curr->numAttr-1].value;
                        size_t attrValLen = 1 + (attrVal ? strlen(attrVal) : 0);
                        char * tmpAttrVal = realloc(attrVal, attrValLen+strlen(xmlData->state.data));
                        if(tmpAttrVal)
                        {
                            curr->attr[curr->numAttr-1].value = attrVal = tmpAttrVal;
                        }
                        else
                        {
                            freeXmlTree(dataRoot);
                            return FALSE;
                        }
                        memcpy( attrVal + attrValLen - 1          // Overwrite the previous '\0'
                              , xmlData->state.data
                              , strlen(xmlData->state.data) +1 ); // Copy the '\0' as well
                        break;
                    }
                    case YXML_CONTENT:
                    {
                        // Is this the first whitespace content byte after another state?
                        if(prevState != YXML_CONTENT && isspace(xmlData->state.data[0]))
                            contentLeadingWs = TRUE;
                        else if(!isspace(xmlData->state.data[0]))
                            contentLeadingWs = FALSE;


                        // Only process content after the first non-whitespace character is found
                        if(!contentLeadingWs)
                        {
                            // Is element allowed to have content?
                            BOOL valid = FALSE;
                            for(i = 0; i < numAllowedEntries; ++i)
                            {
                                // Is this new element found in the list?
                                if( allowedEntries[i].name[0] == '\0'
                                 && allowedEntries[i].contentType == YXML_CONTENT)
                                {
                                    // If yes, the parent also has to be an allowed one
                                    if(strstr(allowedEntries[i].belongsTo, curr->name))
                                    {
                                        valid=TRUE;
                                    }
                                }
                            }
                            if(!valid)
                            {
                                freeXmlTree(dataRoot);
                                return FALSE;
                            }
                            size_t valueLen = 1 + (curr->value ? strlen(curr->value) : 0);
                            char * tmpContentVal = realloc(curr->value, valueLen+strlen(xmlData->state.data));
                            if(tmpContentVal)
                            {
                                curr->value = tmpContentVal;
                            }
                            else
                            {
                                freeXmlTree(dataRoot);
                                return FALSE;
                            }
                            memcpy(curr->value + valueLen - 1, xmlData->state.data, strlen(xmlData->state.data)+1); // Copy the '\0' as well
                        }
                        break;
                    }
                    case YXML_ELEMEND:
                    {
                        // Ends the loop if root is curr->parent
                        curr=curr->parent;
                        break;
                    }
                    case YXML_ATTREND:
                    case YXML_OK:
                    default:
                    {
                        // Do nothing
                        break;
                    }
                }
                // Preserve the previous return value. YXML_OK information
                // is not very helpful, so ignore it
                if(ret != YXML_OK)
                {
                    prevState = ret;
                }
            }
        }
        else if (rd == 0)
        {
            // File ended, but the caller has to find that out again
            freeXmlTree(dataRoot);
            return FALSE;
        }
    } while(curr);

    *root = dataRoot;

    return TRUE;
}

//! Find the device with the JEDEC given
/*!
    \param  xmlData            XML data access descriptor
    \param  allowedEntries     Allowed XML setup
    \param  numEntries         Number of elements in allowedEntries
    \param  jedec              JEDEC of the device
    \return The node of the device or NULL if not found
*/
static XML_ELEM_t * findDevice( XML_HNDL_t *xmlData
                              , XML_RULE_t const * allowedEntries
                              , size_t numEntries
                              , const unsigned int jedec )
{
    if(!xmlData || !allowedEntries || !numEntries)
        return NULL;

    // Read categories until correct JEDEC attribute found or
    // no more categories available
    XML_ELEM_t *rootElem=NULL;
    XML_ELEM_t *dev=NULL;
    do
    {
        if(!buildXmlTree(xmlData, allowedEntries, numEntries, &rootElem))
            break;

        assert(rootElem);
        // Check if the root element is already the child
        if(!strcmp(rootElem->name, "device"))
        {
            if(findJedec(rootElem, jedec))
                dev = rootElem;
        }
        else
        {
            // Iterate through all 'device'-children in rootElem
            unsigned int i;
            for(i = 0; i < rootElem->numChildren && !dev; ++i)
            {
                if(!strcmp(rootElem->children[i].name, "device"))
                {
                    if(findJedec(&rootElem->children[i], jedec))
                        dev = &rootElem->children[i];
                }
            }
        }
        if(!dev)
            freeXmlTree(rootElem);
    } while(!dev);

    return dev;
}

//! Find the device with the JEDEC given
/*!
    \param  xmlData            XML data access descriptor
    \param  allowedEntries     Allowed XML setup
    \param  numEntries         Number of elements in allowedEntries
    \param  cmdsetName         Find the command set with the
                               right name attribute
    \return The node of the device or NULL if not found
*/
static XML_ELEM_t *findCmdSet( XML_HNDL_t *xmlData
                             , XML_RULE_t const * allowedEntries
                             , size_t numEntries
                             , char const * cmdsetName )
{
    if(!xmlData || !allowedEntries || !numEntries || !cmdsetName)
        return NULL;

    // Read categories until correct JEDEC attribute found or
    // no more categories available
    XML_ELEM_t *cmdsettree=NULL;
    XML_ELEM_t *result=NULL;
    do
    {
        if(!buildXmlTree(xmlData, allowedEntries, numEntries, &cmdsettree))
            break;

        assert(cmdsettree);
        // Iterate through all attributes and look for the name category
        unsigned int i;
        for(i = 0; i < cmdsettree->numAttr; ++i)
        {
            if( !strcmp(cmdsettree->attr[i].name, "name")
             && !strcmp(cmdsettree->attr[i].value, cmdsetName) )
                    result=cmdsettree;
        }
        if(!result)
            freeXmlTree(cmdsettree);
    } while(!result);

    return result;
}

static BOOL stringToU4(char *string, U4 *res)
{
    if(!string || !res)
        return FALSE;

    U4 tmp = 0;
    if ( (string[0] == '0' && string[1] == 'x') || string[0] == 'x' )
    {
        int offset = 2;
        //we have a hex string that is in MSB order!
        //0x1000 -> 4096
        if(string[0] == 'x')
        {
            offset = 1;
        }
        size_t len = strlen(&string[offset]);
        if (len%2 != 0)
        {
            string[--offset] = '0';
            len++;
        }
        len /= 2;
        if (len > 4)
        {
            len = 4;
        }
        char *value = malloc(sizeof(*value)*len);
        if(!value)
            return FALSE;
        memset(value, 0x0, len);
        hex_string_to_char_array(&string[offset], value);
        size_t  i;
        for(i = 0; i < len; i++)
        {
            tmp = tmp << 8;
            tmp |= 0xFF & value[i];
        }
        free(value);
    }
    else
    {
        //this is a normal decimal string
        int atoiRes = atoi(string);
        if(atoiRes < 0)
            return FALSE;
        tmp = (U4) atoiRes;
    }
    *res=tmp;
    return TRUE;
}

//! Edit the leading table of the FIS version 2
/*!
    \param  fis         Pointer to the FIS image
    \param  container   The device itself or the containing category
*/
static void edit_table2(char *fis, XML_ELEM_t const *container)
{
    assert(fis && container);
    char const *supply = getAttrValue("supply", container);
    char *secSize = (char*)getElemValue("sectorSize", container);
    char *secCount = (char*)getElemValue("sectorCount", container);
    XML_ELEM_t *cap=NULL;
    unsigned int i;
    for(i = 0; i < container->numChildren; ++i)
    {
        if(!strcmp(container->children[i].name, "cap"))
        {
            cap = &container->children[i];
        }
    }
    char *hsp = (char*)getElemValue("hsp", container);
    char *cfg = (char*)getElemValue("cfg", container);
    char *minEraseSuspend = (char*)getElemValue("minEraseSuspend", container);
    char *numBytesWrite = (char*)getElemValue("numBytesWrite", container);
    char *deepSleepOpcode = (char*)getElemValue("deepSleepOpcode", container);
    char *deepSleepWakeupOpcode = (char*)getElemValue("deepSleepWakeupOpcode", container);
    char *deepSleepWakeupTimeout = (char*)getElemValue("deepSleepWakeupTimeout", container);
    char *maxSpeed= (char*)getElemValue("maxSpeed", container);


    if (supply != NULL)
    {
        char s = (strncmp(supply, "1.8V", 4) == 0) ? 1 : 0;
        fis[SUPPLY_TYPE_POSITION] = (fis[SUPPLY_TYPE_POSITION] & 0xFE) | (s & 0x1);
    }

    if (secSize != NULL)
    {
        U4 sSize = 0;
        stringToU4(secSize, &sSize);
        fis[SECTOR_SIZE_POSITION + 0] = (char) ((sSize >>  0) & 0xFF);
        fis[SECTOR_SIZE_POSITION + 1] = (char) ((sSize >>  8) & 0xFF);
        fis[SECTOR_SIZE_POSITION + 2] = (char) ((sSize >> 16) & 0xFF);
        fis[SECTOR_SIZE_POSITION + 3] = (char) ((sSize >> 24) & 0xFF);
    }

    if (secCount != NULL)
    {
        U4 sCount = 0;
        stringToU4(secCount, &sCount);
        fis[SECTOR_COUNT_POSITION + 0] = (char) ((sCount >>  0) & 0xFF);
        fis[SECTOR_COUNT_POSITION + 1] = (char) ((sCount >>  8) & 0xFF);
        fis[SECTOR_COUNT_POSITION + 2] = (char) ((sCount >> 16) & 0xFF);
        fis[SECTOR_COUNT_POSITION + 3] = (char) ((sCount >> 24) & 0xFF);
    }

    if (cap != NULL)
    {
        const char *writeSuspend = getAttrValue("writeSuspend", cap);
        const char *fastProgram = getAttrValue("fastProgram", cap);
        const char *deepSleep = getAttrValue("deepSleep", cap);
        const char *capMaxSpeed = getAttrValue("speed", cap);
        unsigned char fisValue = fis[CAPABILITY_POSITION];
        if (writeSuspend != NULL)
        {
            unsigned char wS = (atoi(writeSuspend) == 1) ? DRV_SQI_CAP_WRITE_SUSPEND : 0;
            fisValue = (fisValue & ~DRV_SQI_CAP_WRITE_SUSPEND) | wS;
        }

        if (fastProgram != NULL)
        {
            unsigned char fP = (atoi(fastProgram) == 1) ? DRV_SQI_CAP_FAST_PROGRAM : 0;
            fisValue = (fisValue & ~DRV_SQI_CAP_FAST_PROGRAM) | fP;
        }

        if (deepSleep != NULL)
        {
            unsigned char dS = (atoi(deepSleep) == 1) ? DRV_SQI_CAP_DEEP_SLEEP : 0;
            fisValue = (fisValue & ~DRV_SQI_CAP_DEEP_SLEEP) | dS;
        }

        if (capMaxSpeed != NULL)
        {
            unsigned char dS = (atoi(capMaxSpeed) == 1) ? DRV_SQI_CAP_MAX_SPEED : 0;
            fisValue = (fisValue & ~DRV_SQI_CAP_MAX_SPEED) | dS;
        }

        fis[CAPABILITY_POSITION] = fisValue;
    }

    if (hsp != NULL)
    {
        U4 HSP = 0;
        stringToU4(hsp, &HSP);
        fis[HSP_POSITION + 0] = (char) ((HSP >>  0) & 0xFF);
        fis[HSP_POSITION + 1] = (char) ((HSP >>  8) & 0xFF);
        fis[HSP_POSITION + 2] = (char) ((HSP >> 16) & 0xFF);
        fis[HSP_POSITION + 3] = (char) ((HSP >> 24) & 0xFF);
    }

    if (cfg != NULL)
    {
        U4 CFG = 0;
        stringToU4(cfg, &CFG);
        fis[CFG_POSITION + 0] = (char) ((CFG >>  0) & 0xFF);
        fis[CFG_POSITION + 1] = (char) ((CFG >>  8) & 0xFF);
        fis[CFG_POSITION + 2] = (char) ((CFG >> 16) & 0xFF);
        fis[CFG_POSITION + 3] = (char) ((CFG >> 24) & 0xFF);
    }

    if (minEraseSuspend != NULL)
    {
        U4 mES = 0;
        stringToU4(minEraseSuspend, &mES);
        fis[MIN_ERASE_SUSPEND_POSITION + 0] = (char) ((mES >>  0) & 0xFF);
        fis[MIN_ERASE_SUSPEND_POSITION + 1] = (char) ((mES >>  8) & 0xFF);
        fis[MIN_ERASE_SUSPEND_POSITION + 2] = (char) ((mES >> 16) & 0xFF);
        fis[MIN_ERASE_SUSPEND_POSITION + 3] = (char) ((mES >> 24) & 0xFF);
    }

    if (numBytesWrite != NULL)
    {
        U4 nBW = 0;
        stringToU4(numBytesWrite, &nBW);
        fis[NUM_BYTES_WRITE_POSITION + 0] = (char) ((nBW >>  0) & 0xFF);
        fis[NUM_BYTES_WRITE_POSITION + 1] = (char) ((nBW >>  8) & 0xFF);
        fis[NUM_BYTES_WRITE_POSITION + 2] = (char) ((nBW >> 16) & 0xFF);
        fis[NUM_BYTES_WRITE_POSITION + 3] = (char) ((nBW >> 24) & 0xFF);
    }

    if (deepSleepOpcode != NULL)
    {
        U4 deepSleepOp = 0;
        stringToU4(deepSleepOpcode, &deepSleepOp);
        fis[DEEP_SLEEP_OPCODE_POSITION] = (char) (deepSleepOp & 0xFF);
    }

    if (deepSleepWakeupOpcode != NULL)
    {
        U4 deepSleepWakeOp = 0;
        stringToU4(deepSleepWakeupOpcode, &deepSleepWakeOp);
        fis[DEEP_SLEEP_WAKEUP_OPCODE_POSITION] = (char) (deepSleepWakeOp & 0xFF);
    }

    if (deepSleepWakeupTimeout != NULL)
    {
        U4 deepSleepWakeTime = 0;
        stringToU4(deepSleepWakeupTimeout, &deepSleepWakeTime);
        fis[DEEP_SLEEP_WAKEUP_TIMEOUT_POSITION] = (char) (deepSleepWakeTime & 0xFF);
    }

    if (maxSpeed != NULL)
    {
        U4 maxSpeedInt = 0;
        stringToU4(maxSpeed, &maxSpeedInt);
        fis[MAX_SPEED_POSITION] = (char) (maxSpeedInt & 0xFF);
    }
}


unsigned int mergefis_get_sector_count(char const *fis)
{
    if(fis[MAJOR_REV_POSITION] == 0x3 && fis[MINOR_REV_POSITION] == 0x0)
    {
        return (((unsigned int)((unsigned char)fis[SECTOR_COUNT_POSITION_VER3 + 0]) & 0xFF) <<   0) |
               (((unsigned int)((unsigned char)fis[SECTOR_COUNT_POSITION_VER3 + 1]) & 0xFF) <<   8);
    }
    else
    {
        return (((unsigned int)((unsigned char)fis[SECTOR_COUNT_POSITION + 0]) & 0xFF) <<   0) |
               (((unsigned int)((unsigned char)fis[SECTOR_COUNT_POSITION + 1]) & 0xFF) <<   8) |
               (((unsigned int)((unsigned char)fis[SECTOR_COUNT_POSITION + 2]) & 0xFF) <<  16) |
               (((unsigned int)((unsigned char)fis[SECTOR_COUNT_POSITION + 3]) & 0xFF) <<  24);
    }
}

unsigned int mergefis_get_sector_size(char const *fis)
{
    if(fis[MAJOR_REV_POSITION] == 0x3 && fis[MINOR_REV_POSITION] == 0x0)
    {
        return SECTOR_SIZE_VER3;
    }
    else
    {
        return (((unsigned int)(unsigned char)(fis[SECTOR_SIZE_POSITION + 0]) & 0xFF) <<   0) |
               (((unsigned int)(unsigned char)(fis[SECTOR_SIZE_POSITION + 1]) & 0xFF) <<   8) |
               (((unsigned int)(unsigned char)(fis[SECTOR_SIZE_POSITION + 2]) & 0xFF) <<  16) |
               (((unsigned int)(unsigned char)(fis[SECTOR_SIZE_POSITION + 3]) & 0xFF) <<  24);
    }
}


static MERGEFIS_RETVAL_t mergefis_load2( char **fis
                                       , size_t *fisSize
                                       , XML_HNDL_t *xmlData
                                       , const unsigned int jedec
                                       , char const * revisionString )
{
    if(!fis || !fisSize || !xmlData)
        return MERGEFIS_UNKNOWN;

    //search for our device in the flash section of the XML file
    const XML_RULE_t devEntries[] = {
        // Attributes start
        { "jedec",                  YXML_ATTRVAL, "device"                   },
        { "cmd",                    YXML_ATTRVAL, "category|device"          },
        { "supply",                 YXML_ATTRVAL, "category|device"          },
        { "writeSuspend",           YXML_ATTRVAL, "cap"                      },
        { "fastProgram",            YXML_ATTRVAL, "cap"                      },
        { "deepSleep",              YXML_ATTRVAL, "cap"                      },
        { "speed",                  YXML_ATTRVAL, "cap"                      },
        // Element start
        { "category",               YXML_CONTENT, ""                         },
        { "device",                 YXML_CONTENT, ""                         },
        { "device",                 YXML_CONTENT, "category"                 },
        { "numBytesWrite",          YXML_CONTENT, "category|device"          },
        { "minEraseSuspend",        YXML_CONTENT, "category|device"          },
        { "hsp",                    YXML_CONTENT, "category|device"          },
        { "cfg",                    YXML_CONTENT, "category|device"          },
        { "cap",                    YXML_CONTENT, "category|device"          },
        { "sectorSize",             YXML_CONTENT, "category|device"          },
        { "sectorCount",            YXML_CONTENT, "category|device"          },
        { "deepSleepOpcode",        YXML_CONTENT, "category|device"          },
        { "deepSleepWakeupOpcode",  YXML_CONTENT, "category|device"          },
        { "deepSleepWakeupTimeout", YXML_CONTENT, "category|device"          },
        { "maxSpeed",               YXML_CONTENT, "category|device"          },
        { "",                       YXML_CONTENT, "sectorSize"
                                                  "|sectorCount"
                                                  "|hsp"
                                                  "|cfg"
                                                  "|minEraseSuspend"
                                                  "|numBytesWrite"
                                                  "|deepSleepOpcode"
                                                  "|deepSleepWakeupTimeout"
                                                  "|maxSpeed"                }
    };

    XML_ELEM_t *device = findDevice( xmlData
                                   , devEntries
                                   , sizeof(devEntries)/sizeof(devEntries[0])
                                   , jedec );

    //still not found
    if (device == NULL)
        return MERGEFIS_JEDEC_NOT_SUPPORTED;

    char *deviceCmd = NULL;

    // Search for command attribute
    unsigned int i;
    for(i = 0;  i < device->numAttr && !deviceCmd; ++i)
    {
        if(!strcmp(device->attr[i].name, "cmd"))
            deviceCmd = strdup(device->attr[i].value);
    }

    // If no device cmd has been found and a parent exists
    // search in the corresponding parent
    if(device->parent && !deviceCmd)
    {
        for(i = 0;  i < device->parent->numAttr; ++i)
        {
            if(!strcmp(device->parent->attr[i].name, "cmd"))
                deviceCmd = strdup(device->parent->attr[i].value);
        }
    }

    // If no cmd value has been found, this is a problem.
    if (deviceCmd == NULL)
    {
        freeXmlTree(device);
        return MERGEFIS_INCORRECT_XML;
    }

    const XML_RULE_t cmdsetEntries[]= {
        // Attributes start
        { "name",                            YXML_ATTRVAL, "cmdset"                },
        // Element start
        { "cmdset",                          YXML_CONTENT, ""                      },
        { "length",                          YXML_CONTENT, "cmdset"                },
        { "checksum",                        YXML_CONTENT, "cmdset"                },
        { "code",                            YXML_CONTENT, "cmdset"                },
        { "",                                YXML_CONTENT, "length|code|checksum"  }
    };

    // Find the corresponding command set
    XML_ELEM_t *cmdset = findCmdSet( xmlData
                                   , cmdsetEntries
                                   , sizeof(cmdsetEntries)/sizeof(cmdsetEntries[0])
                                   , deviceCmd );

    if(cmdset == NULL)
    {
        freeXmlTree(device);
        free(deviceCmd);
        return MERGEFIS_INCORRECT_XML;
    }

    // Get the required code, lenStr and checksum
    char * lenStr=NULL;
    char * checksum=NULL;
    char * code=NULL;
    for(i = 0; i < cmdset->numChildren; ++i)
    {
        if(!strcmp(cmdset->children[i].name, "length"))
        {
            lenStr = cmdset->children[i].value;
        }
        else if(!strcmp(cmdset->children[i].name, "checksum"))
        {
            checksum = cmdset->children[i].value;
        }
        else if(!strcmp(cmdset->children[i].name, "code"))
        {
            code = cmdset->children[i].value;
        }
    }
    if(!lenStr || !code)
    {
        freeXmlTree(device);
        freeXmlTree(cmdset);
        free(deviceCmd);
        return MERGEFIS_INCORRECT_XML;
    }
    U4 len = 0;
    stringToU4(lenStr, &len);

    //create buffer to convert the code into
    char *data = malloc(len);
    size_t dataSize = len;
    if(!data)
    {
        freeXmlTree(device);
        freeXmlTree(cmdset);
        free(deviceCmd);
        return MERGEFIS_UNKNOWN;
    }

    //fill it with 0xFF
    memset(data, 0xff, dataSize);

    //copy the code into the buffer
    hex_string_to_char_array(code, data);

    //copy the CRC into the buffer
    hex_string_to_char_array(checksum, &data[dataSize-4]);

    //check CRC
    unsigned int crc = lib_crc_crc32(DRV_SQI_FIS_CHECKSUM_SEED, (const void *)data, dataSize-4);
    unsigned int crcWritten = (((unsigned int)((unsigned char)data[dataSize-4]) & 0xFF) <<  0) |
                              (((unsigned int)((unsigned char)data[dataSize-3]) & 0xFF) <<  8) |
                              (((unsigned int)((unsigned char)data[dataSize-2]) & 0xFF) << 16) |
                              (((unsigned int)((unsigned char)data[dataSize-1]) & 0xFF) << 24);
    if( crc != crcWritten )
    {
        free(data);
        freeXmlTree(device->parent);
        freeXmlTree(cmdset);
        free(deviceCmd);
        return MERGEFIS_CODE_CORRUPTED;
    }

    //apply first the settings of the category, if existing
    if(device->parent)
    {
        edit_table2(data, device->parent);
    }

    //and now the settings of the device
    edit_table2(data, device);

    // Update JEDEC
    data[4] = (char) ((jedec >> 16) & 0xFF);
    data[5] = (char) ((jedec >>  8) & 0xFF);
    data[6] = (char) ((jedec >>  0) & 0xFF);

    // Update revision string
    if(revisionString != NULL)
    {
        unsigned int revision = 0xFFFFFFFF;
        sscanf(revisionString, "$Rev: %u $", &revision);

        data[REVISION_POSITION + 0] = (char) ((revision >>  0) & 0xFF);
        data[REVISION_POSITION + 1] = (char) ((revision >>  8) & 0xFF);
        data[REVISION_POSITION + 2] = (char) ((revision >> 16) & 0xFF);
        data[REVISION_POSITION + 3] = (char) ((revision >> 24) & 0xFF);
    }

    //update the CRC of the FIS
    crc = lib_crc_crc32(DRV_SQI_FIS_CHECKSUM_SEED, (const void *)data, dataSize-4);
    data[dataSize - 4] = (char) ((crc >>  0) & 0xFF);
    data[dataSize - 3] = (char) ((crc >>  8) & 0xFF);
    data[dataSize - 2] = (char) ((crc >> 16) & 0xFF);
    data[dataSize - 1] = (char) ((crc >> 24) & 0xFF);

    *fis = data;
    *fisSize = dataSize;

    // Clean up
    freeXmlTree(device);
    freeXmlTree(cmdset);
    free(deviceCmd);

    return MERGEFIS_OK;
}


MERGEFIS_RETVAL_t mergefis_load(char **fis, size_t *fisSize, const char *fisFile, const unsigned int jedec)
{
    if(!fis || !fisSize || !fisFile)
        return MERGEFIS_UNKNOWN;

    //open the FIS XML file
    XML_HNDL_t xmlData;
    xmlData.fd = open(fisFile, O_RDONLY);
    if ( xmlData.fd < 0 )
        return MERGEFIS_FILE_NOT_FOUND;

    MERGEFIS_RETVAL_t result=MERGEFIS_UNKNOWN;

    // Find optional revision information
    yxml_init(&xmlData.state, xmlData.buf, sizeof(xmlData.buf));
    char *revision=NULL;
    findNextInXml(&xmlData, "flash", "revision", &revision); // optional
    // reinitialize yxml, to make sure we start from the beginning again
    if(lseek(xmlData.fd, 0, SEEK_SET)==1)
        return MERGEFIS_UNKNOWN;
    yxml_init(&xmlData.state, xmlData.buf, sizeof(xmlData.buf));
    char *version=NULL;
    long long versionStrLen=findNextInXml(&xmlData, "flash", "fisVersion", &version);
    // At least one character must have been found (excluding terminating character)
    if(versionStrLen < 1)
    {
        result=MERGEFIS_INCORRECT_XML;
    }
    else
    {
        if ( strncmp(version, "2", 1) == 0 )
            result=mergefis_load2(fis, fisSize, &xmlData , jedec, revision);
        else
            result=MERGEFIS_VERSION_ERROR;

        // Only if the previous operations were all successful
        // it is required to parse until the end of the file
        // to make sure the XML is not malformed
        if(result==MERGEFIS_OK)
        {
            // Read and parse the remaining parts of the XML
            // to make sure it is not malformed
            yxml_ret_t ret=YXML_EEOF;
            BOOL exitLoop = FALSE;
            do
            {
                // Every read error is handled as unexpected EOF
                ret=YXML_EEOF;
                char byte;
                int rd = read(xmlData.fd, &byte, sizeof(byte));
                if( rd == sizeof(byte))
                {
                    ret=yxml_parse(&xmlData.state, byte);
                    // Error case?
                    if(ret < YXML_OK)
                        exitLoop = TRUE;
                }
                else if (rd == 0)
                {
                    ret=yxml_eof(&xmlData.state);
                    exitLoop = TRUE;
                }

            } while(!exitLoop);

            // yxml_eof must now have ended with YXML_OK,
            // otherwise the file is malformed
            if(ret != YXML_OK)
                result = MERGEFIS_INCORRECT_XML;

        }
    }

    free(revision);
    free(version);
    close(xmlData.fd);
    return result;
}

MERGEFIS_RETVAL_t mergefis_merge(char *pData, unsigned int ImageSize, char const *fis)
{
    //merge the images
    memcpy(&pData[0x40], fis, 0x1000 - 0x40);

    //Update CRC of the whole image
    return (UpdateImageCrc(pData, ImageSize))?MERGEFIS_OK:MERGEFIS_CRC_FAILURE;
}

