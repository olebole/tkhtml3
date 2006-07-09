
##### Top of the Tkhtml source tree - the directory with this file in it.
#
TOP = /z/htmlwidget/

##### BUILD can be DEBUG or RELEASE.
#
# BUILD = DEBUG
BUILD = RELEASE

##### Version of and path to the Tcl installation to use.
#
TCLVERSION = 84
TCL = /c/tcl

##### Flags passed to the C-compiler to link to Tcl.
#
TCLLIB = -L$(TCL)/lib -ltclstub$(TCLVERSION) -ltkstub$(TCLVERSION)
# TCLLIB = -L$(TCL)/lib -ltcl$(TCLVERSION) -ltk$(TCLVERSION) -ltclstub$(TCLVERSION) -ltkstub$(TCLVERSION)

##### Extra libraries used by Tcl on Linux. These flags are only required to
#     staticly link Tcl into an executable
#
# TCLLIB_DEBUG += -L/usr/X11R6/lib/ -lX11 -ldl -lm

CC = gcc

CFLAGS_RELEASE = -O2 -DNDEBUG 
CFLAGS_DEBUG   = -g
CFLAGS = $(CFLAGS_$(BUILD))
CFLAGS += -DUSE_TCL_STUBS=1 -DUSE_TK_STUBS=1

##### The name of the shared library file to build.
#
SHARED_LIB_DEBUG = libTkhtml3g.dll
SHARED_LIB_RELEASE = libTkhtml3.dll
SHARED_LIB = $(SHARED_LIB_$(BUILD))

##### Command to build a shared library from a set of object files. The
#     command executed will be:
# 
#         $(MKSHLIB) $(OBJS) $(TCLSTUBSLIB) -o $(SHARED_LIB)
#
MKSHLIB = $(CC) -shared 
TCLSTUBSLIB =  "/c/tcl/lib/tclstub84.lib" "/c/tcl/lib/tkstub84.lib" 
TCLSTUBSLIB += -LC:/Tcl/lib

##### Commands to run tclsh and wish.
#
TCLSH = $(TCL)/bin/tclsh
WISH = $(TCL)/bin/wish

MKSTARKIT = $(TCL)/bin/tclkit $(TCL)/bin/sdx.kit wrap

#
# End of configuration section.
###########################################################################

default: binaries hwish

###############################################################################
include $(TOP)/main.mk

