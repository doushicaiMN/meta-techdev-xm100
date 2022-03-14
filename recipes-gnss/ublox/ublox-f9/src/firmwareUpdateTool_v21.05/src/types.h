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
  \brief  Types definitions
*/

#ifndef __TYPES_H
#define __TYPES_H

//=====================================================================
// TYPE and CONST DEFINITIONS
//=====================================================================

typedef double              R8; //!< 64-bit floating point number
typedef float               R4; //!< 32-bit floating point number
#if defined(__CYGWIN__) || defined(linux) || defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#include <stdint.h>
typedef int64_t             I8; //!< 64-bit signed integer number
typedef uint64_t            U8; //!< 64-bit unsigned integer number
typedef int32_t             I4; //!< 32-bit signed integer number
typedef uint32_t            U4; //!< 32-bit unsigned integer number
typedef int16_t             I2; //!< 16-bit signed integer number
typedef uint16_t            U2; //!< 16-bit unsigned integer number
typedef int8_t              I1; //!< 8-bit signed integer  number(signed character)
typedef uint8_t             U1; //!< 8-bit unsigned integer  number(unsigned character)
#else
typedef signed __int64      I8; //!< 64-bit signed integer number
typedef unsigned __int64    U8; //!< 64-bit unsigned integer number
typedef signed __int32      I4; //!< 32-bit signed integer number
typedef unsigned __int32    U4; //!< 32-bit unsigned integer number
typedef signed __int16      I2; //!< 16-bit signed integer number
typedef unsigned __int16    U2; //!< 16-bit unsigned integer number
typedef signed __int8       I1; //!< 8-bit signed integer  number(signed character)
typedef unsigned __int8     U1; //!< 8-bit unsigned integer  number(unsigned character)

#endif
typedef char                L1; //!< Logical state
typedef signed int          I;  //!< Signed integer number
typedef unsigned int        U;  //!< Unsigned integer number
typedef char                CH; //!< 8-bit character, (signed/unsigned, depending on compiler)
#if defined( __CYGWIN__) || defined(linux)
typedef int                 HANDLE; //!< Generic handle
typedef int                 SOCKET; //!< Socket type
#else
typedef void*               HANDLE; //!< Generic handle
#endif


typedef int                 BOOL;   //!< Boolean type

#define FALSE               0 //!< Boolean FALSE
#define TRUE                1 //!< Boolean TRUE
#ifndef NULL
#define NULL                0 //!< Null
#endif

#if defined(__CYGWIN__) || defined(linux) || (defined(__APPLE__) && defined(__MACH__))
#define INVALID_SOCKET (-1) //!< Invalid Socket value
#define SOCKET_ERROR   (-1) //!< Socket Error Code
#endif
#if defined(linux) || (defined(__APPLE__) && defined(__MACH__))
#define strcmpi(a,b) strcasecmp((a),(b))
#endif

#if defined(__APPLE__) && defined(__MACH__)
#define ENDIAN_SWAP(n) (((n&0xFF)<<24) |        \
                        (((n>>8)&0xFF)<<16) |   \
                        (((n>>16)&0xFF)<<8) |   \
                        ((n>>24)&0xFF)) //!< Endian conversion macro
#else
#ifndef ENDIAN_SWAP
#define ENDIAN_SWAP(n) n //!< Endian conversion macro
#endif
#endif

//! minimum define
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
//! maximum define
#define MAX(a, b)  (((a) < (b)) ? (b) : (a))

//! number of elements in array
#define NUMOF(a) (sizeof(a) / sizeof(*a))

#define IN      //!< dummy define for input argument
#define OUT     //!< dummy define for output argument
#define INOUT   //!< dummy define for input and output argument

//------------------------------------------------------------------------------
// Global magic words
//------------------------------------------------------------------------------
extern const U4 magicG50; //!< Magic double word for G50 receiver
extern const U4 magicG51; //!< Magic double word for G51 receiver
extern const U4 magicG70; //!< Magic double word for G70 receiver
extern const U4 magicG80; //!< Magic double word for G80 receiver

#endif
