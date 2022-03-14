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
  \brief  Firmware Update Sample Application for u-blox 5 onward

  This file includes code for command line parameters parsing only.
*/

/*!
  \mainpage
  This is the source code documentation for the u-blox firmware update
  tool supporting u-blox receivers from generation 5 onward.
  This source code is provided as sample code. A released version of this
  sample code can be expected to be compilable on x86 systems under Windows 7
  and above as well as Ubuntu Linux systems. Compilation and usability on
  other hardware platforms and operating systems might be possible, but is
  not supported.

  This document is not meant to be a user manual. Actually, it should be
  considered a quick help in getting an overview on the source code delivered.

  To get a quick overview over the update algorithm, take a look at main.c,
  where one can find the conversion of the arguments. The core of the update
  algorithm is contained in update.c, this includes a straight-forward state
  machine which is easily understandable.

  \section interfaces Supported interfaces
  The tool supports also the following interfaces:
  <ul>
    <li>Serial port</li>
    <li>Serial port over Ethernet</li>
    <li>SPI with the Aardvark tool (http://www.totalphase.com)</li>
    <li>I2C with the Diolan U2C-12 converter (http://www.diolan.com)</li>
  </ul>

  <b>Important information for version 1.7.2.0 and newer</b>:<br/>
  The library and the header file for the Diolan U2C-12 adapter is no longer
  distributed in this package because of license questions. Please download
  and install the latest version library from the Diolan homepage
  http://www.diolan.com/products/u2c12/downloads.html and add the file
  \<DIOLAN_INSTALLATION_DIRECTORY\>/Src/Common/i2cbridge.h to this project.
  Furthermore you need to enable the define in the file <code>platform.h</code>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "update.h"
#include "version.h"
#include "platform.h"
#include "updateCore.h"

#ifdef WIN32
#ifdef _DEBUG
#include "Windows.h"
#endif
#endif

//! executable name for usage
const char* exename = "";

//! Print usage of tool to console
void Usage(void);

//! Return codes
typedef enum RETURNCODE_e
{
    SUCCESS = 0,        //!< return code in case of success
    ERROR_ARGUMENTS,    //!< return code in case of argument errors
    ERROR_UPDATE        //!< return code in case of update failure

} RETURNCODE_t;

//! Command line arguments
typedef struct CL_ARGUMENTS_s
{
    const char*     BinaryFileName;     //!< Filename of binary firmware image
    const char*     FlashDefFileName;   //!< Filename of flash definition file
    const char*     FisFileName;        //!< Filename of the FIS file
    const char*     ComPort;            //!< COM port name
    unsigned int    Baudrate;           //!< Baudrate for serial port (commanding to safeboot)
    unsigned int    BaudrateSafe;       //!< Baudrate for serial port (after starting safeboot)
    unsigned int    BaudrateUpd;        //!< Baudrate for serial port (performing update)
    BOOL            DoSafeBoot;         //!< Command safeboot start before update
    BOOL            DoReset;            //!< Reset receiver after FW update
    int             Verbose;            //!< Verbose mode
    BOOL            Autobaud;           //!< Autobaud on safeboot identification
    BOOL            EraseWholeFlash;    //!< Erase whole flash (always set to 1)
    BOOL            EraseOnly;          //!< Perform only erase operation
    BOOL            TrainingSequence;   //!< Training sequence
    BOOL            fisOnly;            //!< Program only the FIS image to the flash device
    BOOL            chipErase;          //!< Do a chip erase instead of sector erases
    BOOL            noFisMerging;       //!< Don't merge the image with anything
    unsigned int    updateRam;          //!< Update RAM
    BOOL            usbAltMode;         //!< Use USB alternative mode
} CL_ARGUMENTS_t;
typedef CL_ARGUMENTS_t* CL_ARGUMENTS_pt; //!< pointer to CL_ARGUMENTS_t type

//! Argument enumeration
typedef enum ARG_e
{
    UNKNOWN_ARG = 0,    //!< Unknown
    BAUDRATE,           //!< Baudrate
    PORT,               //!< Port name
    FLASHFILE,          //!< Flash definition filename
    FISFILE,            //!< FIS filename
    HELP,               //!< Help request
    VERSION,            //!< Version request
    FILENAME,           //!< Binary firmware image
    DOSAFEBOOT,         //!< Safeboot flag
    RAMSAFEBOOT,        //!< Ram-safeboot flag
    USBPORT,            //!< Is-usb-port flag
    VERBOSITY,          //!< Verbosity flag
    AUTOBAUD,           //!< Autobaud flag
    RESET,              //!< Reset after update flag
    ERASEALL,           //!< Erase whole flash
    ERASEONLY,          //!< Don't perform write operation, only erase flash
    TRAINING,           //!< Training sequence
    FIS_ONLY,           //!< FIS only
    CHIP_ERASE,         //!< Do a chip erase instead of sector erases
    NO_FIS_MERGING,     //!< Don't merge the image with anything.
    UPDATE_RAM,         //!< Update RAM with external image
    USB_ALT_MODE,       //!< Use USB alternative mode for firmware update
} ARG_t;
typedef ARG_t* ARG_pt; //!< pointer to ARG_t type

//! Arguments association
typedef struct ARG_ASSOC_s
{
    const char* argstr;     //!< argument string from CL
    ARG_t       argass;     //!< associated argument type
} ARG_ASSOC_t;
typedef ARG_ASSOC_t* ARG_ASSOC_pt; //!< pointer to ARG_ASSOC_t type

//! defaults if no command line arguments provided
const CL_ARGUMENTS_t defaultargs =
{
    "",                  //BinaryFileName
    "",                  //FlashDefFileName
    "flash.xml",         //FisFileName
    "\\\\.\\COM1",       //ComPort
    9600,                //Baudrate
    9600,                //BaudrateSafe
    115200,              //BaudrateUpd
    TRUE,                //DoSafeBoot
    TRUE,                //DoReset
    0,                   //Verbose
    FALSE,               //Autobaud
    TRUE,                //EraseWholeFlash
    FALSE,               //EraseOnly
    TRUE,                //Send training sequence
    FALSE,               //FisOnly
    FALSE,               //ChipErase
    FALSE,               //NoFisMerging
    FALSE,               //UpdateRam
    FALSE,               //usbAltMode
};

//! known arguments and according identifier
static ARG_ASSOC_t knownArgs[] =
{
    {"-b",          BAUDRATE       },
    {"-p",          PORT           },
    {"-f",          FLASHFILE      },
    {"-F",          FISFILE        },
    {"-h",          HELP           },
    {"-s",          DOSAFEBOOT     },
    {"-v",          VERBOSITY      },
    {"-R",          RESET          },
    {"-a",          AUTOBAUD       },
    {"-e",          ERASEALL       },
    {"-E",          ERASEONLY      },
    {"-t",          TRAINING       },
    {"--help",      HELP           },
    {"--version",   VERSION        },
    {"--fis-only",  FIS_ONLY       },
    {"-C",          CHIP_ERASE     },
    {"--no-fis",    NO_FIS_MERGING },
    {"--up-ram",    UPDATE_RAM     },
    {"--usb-alt",   USB_ALT_MODE   },
};

//! Set program options
/*!
    \param option    option identifier
    \param value    value for the option
    \param clargs    options structures receiving parsed options
    \return TRUE if the arguments are valid, FALSE else
*/
BOOL SetArgument(IN ARG_t option, IN char const * value, OUT CL_ARGUMENTS_pt clargs)
{
    static BOOL binaryFileGiven = FALSE;
    switch (option)
    {
    case BAUDRATE:
        clargs->Baudrate    = atol(value);
        value = strchr(value,':');

        if (value)
        {
            clargs->BaudrateSafe = atol(value+1);
            value = strchr(value+1, ':');
            if (value)
            {
                clargs->BaudrateUpd = atol(value+1);
            }
            else
            {
                clargs->BaudrateUpd = clargs->BaudrateSafe;
            }
        }
        else
        {
            clargs->BaudrateSafe = clargs->Baudrate;
            clargs->BaudrateUpd  = clargs->Baudrate;
        }
        break;

    case PORT:
        clargs->ComPort = value;

        // do not send training sequence over SPI/I2C
        if((strncmp(clargs->ComPort, "I2C", 3) == 0) ||
           (strncmp(clargs->ComPort, "U2C", 3) == 0) ||
           (strncmp(clargs->ComPort, "SPI", 3) == 0) ||
           (strncmp(clargs->ComPort, "SPU", 3) == 0))
        {
            // set the 0
            clargs->TrainingSequence = 0;
        }

        // check for I2C port
        if((strncmp(clargs->ComPort, "I2C", 3) == 0) ||
           (strncmp(clargs->ComPort, "U2C", 3) == 0) )
        {
            // set the default baudrates
            clargs->Baudrate     = BaudrateDefaultI2C;
            clargs->BaudrateSafe = BaudrateDefaultI2C;
            clargs->BaudrateUpd  = BaudrateDefaultI2C;
        }
        break;

    case HELP:
        Usage();
        return FALSE;
    case FILENAME:
        if (!binaryFileGiven)
        {
            clargs->BinaryFileName = value;
            binaryFileGiven = TRUE;
        }
        break;
    case FLASHFILE:
        clargs->FlashDefFileName = value;
        break;
    case FISFILE:
        clargs->FisFileName = value;
        break;
    case DOSAFEBOOT:
        clargs->DoSafeBoot = (atoi(value) != 0);
        break;
    case VERBOSITY:
        clargs->Verbose = atoi(value);
        break;
    case AUTOBAUD:
        clargs->Autobaud = (atoi(value) != 0);
        break;
    case RESET:
        clargs->DoReset = (atoi(value) != 0);
        break;
    case ERASEALL:
        // always set this value to 1
        clargs->EraseWholeFlash = 1;
        break;
    case ERASEONLY:
        clargs->EraseOnly = (atoi(value) != 0);
        break;
    case TRAINING:
        // do not send training sequence over SPI/I2C
        if((strncmp(clargs->ComPort, "I2C", 3) == 0) ||
           (strncmp(clargs->ComPort, "U2C", 3) == 0) ||
           (strncmp(clargs->ComPort, "SPI", 3) == 0) ||
           (strncmp(clargs->ComPort, "SPU", 3) == 0))
        {
            // set the 0
            clargs->TrainingSequence = 0;
        }
        // evaluate the command line option
        else
        {
            clargs->TrainingSequence = (atoi(value) != 0);
        }
        break;
    case FIS_ONLY:
        clargs->fisOnly = atoi(value) != 0;
        break;
    case CHIP_ERASE:
        clargs->chipErase = (atoi(value) != 0);
        break;
    case NO_FIS_MERGING:
        clargs->noFisMerging = (atoi(value) != 0);
        break;
    case UPDATE_RAM:
        clargs->updateRam = (U4)atoi(value);
        break;
    case USB_ALT_MODE:
        clargs->usbAltMode = (atoi(value) != 0);
        break;
    default:
        Usage();
        break;
    }
    return TRUE;
}

void Usage(void)
{
        MESSAGE_PLAIN("\n");
        MESSAGE_PLAIN("NAME\n");
        MESSAGE_PLAIN("    %s - u-blox GNSS receiver firmware update tool v%s\n", exename, PRODUCTVERSTR);
        MESSAGE_PLAIN("\n");
        MESSAGE_PLAIN("DESCRIPTION\n");
        MESSAGE_PLAIN("    Perform flash firmware update of u-blox GNSS receivers (u-blox 5 and later).\n");
        MESSAGE_PLAIN("    The tool can be also used to erase the whole flash content \n");
        MESSAGE_PLAIN("    for reverting to factory firmware / defaults.\n");
        MESSAGE_PLAIN("\n");
        MESSAGE_PLAIN("SYNOPSIS\n");
        MESSAGE_PLAIN("    %s [--help] [--version] [-F flash.xml] [-f flash.txt] [--fis-only] [-p port]\n", exename);
        MESSAGE_PLAIN("    [-b baudcur[:baudsafe[:baudupd]]] [-s 1] [-v 0] [-a 0] [-E 1] [-R 0] [-t 1] [-C 0] [--no-fis 0]\n");
        MESSAGE_PLAIN("    firmware.bin\n");
        MESSAGE_PLAIN("\n");
        MESSAGE_PLAIN("EXIT STATUS\n");
        MESSAGE_PLAIN("    0 on success\n");
        MESSAGE_PLAIN("    1 if invalid arguments provided\n");
        MESSAGE_PLAIN("    2 if update failed\n");
        MESSAGE_PLAIN("\n");
        MESSAGE_PLAIN("OPTIONS\n");
        MESSAGE_PLAIN("    --help     display help\n");
        MESSAGE_PLAIN("    --version  display version\n");
        MESSAGE_PLAIN("    -f         define flash definition filename.\n");
        MESSAGE_PLAIN("                 this parameter is not needed anymore because all the information\n");
        MESSAGE_PLAIN("                 is compiled in. Use for custom flash definition files only.\n");
        MESSAGE_PLAIN("    -F         define FIS (Flash Information Structure) filename\n");
        MESSAGE_PLAIN("                 (default: %s)\n", defaultargs.FisFileName);
        MESSAGE_PLAIN("    --fis-only download only the FIS information without firmware (1).\n");
        MESSAGE_PLAIN("                 (default: %i)\n", defaultargs.fisOnly);
        MESSAGE_PLAIN("    -b         switch baudrate to specified value [bit/s]\n");
        MESSAGE_PLAIN("                 first baudrate defines current baudrate\n");
        MESSAGE_PLAIN("                 second baudrate after colon is used after safeboot\n");
        MESSAGE_PLAIN("                 third baudrate after second colon is used during update\n");
        MESSAGE_PLAIN("                  (defaults: %u:%u:%u\n", defaultargs.Baudrate, defaultargs.BaudrateSafe, defaultargs.BaudrateUpd);
        MESSAGE_PLAIN("    -p         choose port (default: %s)\n", defaultargs.ComPort);
        MESSAGE_PLAIN("                 \\\\.\\COMy      - serial (RS232) port y (Windows)\n");
        MESSAGE_PLAIN("                 /dev/ttySy    - serial (RS232) port y (Linux)\n");
        MESSAGE_PLAIN("                   remark: has to be a valid system device name,\n");
        MESSAGE_PLAIN("                   windows maps names 'COM1..9' to '\\\\.\\COM1..9', \n");
        MESSAGE_PLAIN("                   for ports > 9 use \\\\.\\COMy for device name\n");
        MESSAGE_PLAIN("                 I2C[y[:0xaa]]   - Aardvark I2C device y (default 0),\n");
        MESSAGE_PLAIN("                                   slave address aa (default 0x42)\n");
        MESSAGE_PLAIN("                 SPI[y]          - Aardvark SPI device y (default 0),\n");
        MESSAGE_PLAIN("                 U2C[y[:0xaa]]   - Diolan I2C device y (default 0),\n");
        MESSAGE_PLAIN("                                   slave address aa (default 0x42)\n");
        MESSAGE_PLAIN("                 SPU[y]          - Diolan SPI device y (default 0)\n");
        MESSAGE_PLAIN("                 host:port       - network, e.g. through comtrol devicemaster,\n");
#ifdef WIN32
        MESSAGE_PLAIN("                 STDIO           - communicate with receiver over stdin and stdout\n");
#endif //ifdef WIN32
        MESSAGE_PLAIN("    -s         enter safeboot before updating\n");
        MESSAGE_PLAIN("                 (default: %i)\n", defaultargs.DoSafeBoot);
        MESSAGE_PLAIN("    -v         verbose mode on (1), including packet dump (2) or off (0)\n");
        MESSAGE_PLAIN("                 (default: %i)\n", defaultargs.Verbose);
        MESSAGE_PLAIN("    -a         perform autobauding (1), no autobauding (0) \n");
        MESSAGE_PLAIN("                 (default: %i)\n", defaultargs.Autobaud);
        MESSAGE_PLAIN("    -E         erase only, don't write anything to flash (erases the first sector)\n");
        MESSAGE_PLAIN("                 (default: %i)\n", defaultargs.EraseOnly);
        MESSAGE_PLAIN("    -R         reset after update completion \n");
        MESSAGE_PLAIN("                 (default: %i)\n", defaultargs.DoReset);
        MESSAGE_PLAIN("    -t         send training sequence (when in safeboot) (not applicable over I2C or SPI)\n");
        MESSAGE_PLAIN("                 (default: %i)\n", defaultargs.TrainingSequence);
        MESSAGE_PLAIN("    -C         do chip erase instead of sector erases\n");
        MESSAGE_PLAIN("                 (default: %i)\n", defaultargs.chipErase);
        MESSAGE_PLAIN("    --no-fis   don't merge the image with anything (1) or merge the FIS into the image (0)\n");
        MESSAGE_PLAIN("                 (default: %i)\n", defaultargs.noFisMerging);
        MESSAGE_PLAIN("    --up-ram   download image to CODE-RAM (1) or flash (0)\n");
        MESSAGE_PLAIN("                 (default: %i)\n", defaultargs.updateRam);
        MESSAGE_PLAIN("    --usb-alt  use USB alternative mode for firmware update\n");
        MESSAGE_PLAIN("                 (default: %i)\n", defaultargs.usbAltMode);
        MESSAGE_PLAIN("\n");
        MESSAGE_PLAIN("EXAMPLES\n");
        MESSAGE_PLAIN("    erase whole flash content:\n");
        MESSAGE_PLAIN("      %s -p \\\\.\\COM1 -b 9600 -E 1\n", exename);
        MESSAGE_PLAIN("\n");
        MESSAGE_PLAIN("    program image to flash (on first serial port under Linux):\n");
        MESSAGE_PLAIN("      %s -p /dev/ttyS0 -b 9600:9600:115200 -F flash.xml\n", exename);
        MESSAGE_PLAIN("      <firmware.bin>\n");
        MESSAGE_PLAIN("\n");
        MESSAGE_PLAIN("    program image to flash (using I2C interface):\n");
        MESSAGE_PLAIN("      %s -p I2C0 -b 100000 \n", exename);
        MESSAGE_PLAIN("      <firmware.bin>\n");
        MESSAGE_PLAIN("\n");
        MESSAGE_PLAIN("AUTHOR\n");
        MESSAGE_PLAIN("    u-blox software development team (www.u-blox.com)\n");
        MESSAGE_PLAIN("\n");
}

//! Map the option to an enumeration type
/*!
    \param  arg         option from command line
    \return enumeration type of argument if available, #UNKNOWN_ARG else
*/
ARG_t GetArgAssociation(char const * arg)
{
    unsigned int idx = 0;
    while (idx < sizeof(knownArgs)/sizeof(*knownArgs))
    {
        if (strcmp(arg, knownArgs[idx].argstr) == 0)
        {
            return knownArgs[idx].argass;
        }
        idx++;
    }
    return UNKNOWN_ARG;
}

//! Parse the command line arguments
BOOL ParseArguments(IN int argc, IN char const * const argv[], OUT CL_ARGUMENTS_pt clargs)
{
    ARG_t option = UNKNOWN_ARG;
    //initialize command line arguments structure
    memcpy(clargs, (void*)&defaultargs, sizeof(*clargs));

    //parse the arguments
    int arg;
    for (arg=1; arg < argc; arg++)
    {
        if (argv[arg][0] == '-' && argv[arg][1] != '\0')
        {
            option = GetArgAssociation(argv[arg]) ;
            if (option == UNKNOWN_ARG)
            {
                MESSAGE_PLAIN("Unknown Argument: %s\n", argv[arg]);
                Usage();
                return FALSE;
            }
            else if (option == HELP)
            {
                Usage();
                return FALSE;
            }
            else if (option == VERSION)
            {
                MESSAGE_PLAIN("ubxfwupdate %s\n", PRODUCTVERSTR);
                return FALSE;
            }
        }
        else
        {
            if (option != UNKNOWN_ARG)
            {
                //this is the argument of an option
                //cout<<"Option: "<<option<<" Argument: "<<argv[arg]<<endl;
                SetArgument(option, argv[arg], clargs);
                option = UNKNOWN_ARG;
            }
            else
            {
                //this is a standalone argument
                //cout<<"Standalone Argument: "<<argv[arg]<<endl;
                SetArgument(FILENAME, argv[arg], clargs);
            }
        }
    }
    // either eraseOnly operation or the binary firmware image name must be set
    if (!clargs->EraseOnly && (!clargs->BinaryFileName || !*clargs->BinaryFileName) && !clargs->fisOnly)
    {
        Usage();
        return FALSE;
    }

    return TRUE;
}

//! Program entry
int main(int argc, char* argv[])
{

    CL_ARGUMENTS_t clArgs;
    BOOL success = FALSE;
    exename = argv[0];

    if(ParseArguments(argc, (const char * const*) argv, &clArgs))
    {
        CONSOLE_INIT();


        MESSAGE_PLAIN("----------CMD line arguments-----------\n"                                                          );
        MESSAGE_PLAIN("Image file:        %s\n", clArgs.BinaryFileName                                                     );
        MESSAGE_PLAIN("Flash:             %s\n", (strlen(clArgs.FlashDefFileName)?clArgs.FlashDefFileName:"<compiled-in>") );
        MESSAGE_PLAIN("Fis:               %s\n", clArgs.FisFileName                                                        );
        MESSAGE_PLAIN("Port:              %s\n", clArgs.ComPort                                                            );
        MESSAGE_PLAIN("Baudrates:         %u/%u/%u\n", clArgs.Baudrate, clArgs.BaudrateSafe, clArgs.BaudrateUpd            );
        MESSAGE_PLAIN("Safeboot:          %i\n", clArgs.DoSafeBoot                                                         );
        MESSAGE_PLAIN("Reset:             %i\n", clArgs.DoReset                                                            );
        MESSAGE_PLAIN("AutoBaud:          %i\n", clArgs.Autobaud                                                           );
        MESSAGE_PLAIN("Verbose:           %i\n", clArgs.Verbose                                                            );
        MESSAGE_PLAIN("Erase all:         %i\n", clArgs.EraseWholeFlash                                                    );
        MESSAGE_PLAIN("Erase only:        %i\n", clArgs.EraseOnly                                                          );
        MESSAGE_PLAIN("Training sequence: %i\n", clArgs.TrainingSequence                                                   );
        MESSAGE_PLAIN("Chip erase:        %i\n", clArgs.chipErase                                                          );
        MESSAGE_PLAIN("Merging FIS:       %i\n", clArgs.noFisMerging                                                       );
        MESSAGE_PLAIN("Update RAM:        %u\n", clArgs.updateRam                                                          );
        MESSAGE_PLAIN("Use USB alt:       %i\n", clArgs.usbAltMode);
        MESSAGE_PLAIN("---------------------------------------\n");

        success = UpdateFirmware(clArgs.BinaryFileName,
                                 clArgs.FlashDefFileName,
                                 clArgs.FisFileName,
                                 clArgs.ComPort,
                                 clArgs.Baudrate,
                                 clArgs.BaudrateSafe,
                                 clArgs.BaudrateUpd,
                                 clArgs.DoSafeBoot,
                                 clArgs.DoReset,
                                 clArgs.Autobaud,
                                 clArgs.EraseWholeFlash,
                                 clArgs.EraseOnly,
                                 clArgs.TrainingSequence,
                                 clArgs.chipErase,
                                 clArgs.noFisMerging,
                                 clArgs.updateRam,
                                 clArgs.usbAltMode,
                                 clArgs.Verbose,
                                 clArgs.fisOnly);

        MESSAGE(MSG_LEV2, "Firmware Update %s", (success) ? "SUCCESS\n" :"FAILED\n");
        CONSOLE_DONE();
    }
    else
    {
        // parsing of arguments failed
        return ERROR_ARGUMENTS;
    }

    // indicate the outcome of the firmware update process
    return (success) ? SUCCESS : ERROR_UPDATE;
}
