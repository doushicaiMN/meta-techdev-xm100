###############################################################################2
#
# Copyright (C) u-blox AG
# u-blox AG, Thalwil, Switzerland
#
# All rights reserved.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose without fee is hereby granted, provided that this entire notice is
# included in all copies of any software which is or includes a copy or
# modification of this software and in all copies of the supporting
# documentation for such software.
#
# THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
# WARRANTY. IN PARTICULAR, NEITHER THE AUTHOR NOR U-BLOX MAKES ANY
# REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY OF
# THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
#
###############################################################################
#
# Project: firmwareUpdateTool v21.05
#
###############################################################################
# $Id$
# $HeadURL$
###############################################################################

# define the verbosity level
ifeq ($(V),1)
VERBOSE :=1
else
VERBOSE:=0
endif

# define if debug build required
DEBUG  :=0


# define standard commands
RM    := rm
SED   := sed
CAT   := cat
MAKE  := make
PERL  := perl
GREP  := grep
MKDIR := mkdir

# CC  = gcc
# LD  = gcc
LD = $(CC)

# library and compiler flags
LIBS   = -ldl
CFLAGS +=  -Wall -Wextra -Wno-unused-parameter $(DEFINES) -I. -Isrc/

# check if verbose output requested
ifeq ($(VERBOSE),1)
P =
else
P = @
MAKE += --no-print-directory
endif

# check if 32-bit version requested
ifeq ($(M32),1)
CFLAGS  += -m32
LDFLAGS += -m32
endif

# check if debug build required
ifneq ($(DEBUG),0)
CFLAGS  += -g -O0
LDFLAGS += -g
else
CFLAGS += -O2
endif


CFLAGS += -std=gnu90

# define the program name
OUTNAME=ubxfwupdate$(VERSION)


# get the version
PRODUCTVERSION := ${shell $(GREP) "\#define PRODUCTVERSTR" ./src/version.h | $(SED) -r 's/\#define PRODUCTVERSTR\s+"([^"]+)"\s+\/\/!< Product version string/\1/'}

# define the source directory
SRCDIR=src

# define the binary directory
BINDIR=bin

# define the object directory
ODIR = obj_$(PLATFORM)_$(MACHINE)$(VERSION)$(EXT)

MAIN_OBJ = $(ODIR)/main.o
FUNC_OBJ = $(ODIR)/update.o $(ODIR)/image.o $(ODIR)/checksum.o $(ODIR)/platform.o $(ODIR)/ubxmsg.o $(ODIR)/flash.o $(ODIR)/aardvark.o $(ODIR)/yxml.o $(ODIR)/mergefis.o $(ODIR)/receiver.o $(ODIR)/updateCore.o

ALL_OBJ = $(MAIN_OBJ) $(FUNC_OBJ)


# default make target
all: external

# listing of all available targets
help:
	@echo "Available targets:"
	@echo ""
	@echo "external   -- Customer 32-bit version of firmware update utility"
	@echo "external64 -- Customer 64-bit version of firmware update utility"
	@echo ""

# customer version (32-bit) (default)
external:
	@echo "* Building firmware update tool (32-bit) v$(PRODUCTVERSION)"
	$(P)$(MAKE) -C . PLATFORM=$(OECORE_TARGET_ARCH) MACHINE=$(OECORE_TARGET_ARCH) M32="0" program VERBOSE=$(VERBOSE)

external64:
	@echo "* Building firmware update tool (64-bit) v$(PRODUCTVERSION)"
	$(P)$(MAKE) -C . PLATFORM="`uname -s`" MACHINE="`uname -m | $(SED) 's/ /_/g'`" VERSION="_64" M32="0" program VERBOSE=$(VERBOSE)


program: $(ODIR) $(BINDIR) $(BINDIR)/$(OUTNAME)

# create the object directory
$(ODIR):
	${P}$(MKDIR) -p $(ODIR)

# create the binary directory
$(BINDIR):
	${P}$(MKDIR) -p $(BINDIR)

# link the objects
$(BINDIR)/$(OUTNAME): $(ALL_OBJ)
	${P}$(LD) -o $@ $^ $(LIBS)

# compile the C source files
$(ODIR)/%.o: $(SRCDIR)/%.c
	${P}$(CC) -c $(CFLAGS) $< -o $@

clean:
	${P}$(RM) -rf obj*
	${P}$(RM) -rf bin/

.PHONY: clean

# eof
