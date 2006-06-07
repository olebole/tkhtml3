
##### Top of the Tkhtml source tree - the directory with this file in it.
#
TOP = $(HOME)/work/tkhtml/htmlwidget

##### BUILD can be DEBUG, RELEASE or MEMDEBUG.
#
BUILD = DEBUG
# BUILD = RELEASE
# BUILD = MEMDEBUG
# BUILD = PROFILE

##### Version of and path to the Tcl installation to use.
#
# TCL_DEBUG   = $(HOME)/profiletcl
# TCL_DEBUG   = $(HOME)/tcl

TCLVERSION = 8.5
TCL_RELEASE = /home/dan/tcl

TCL_DEBUG    = /home/dan/tcl
TCL_PROFILE  = /home/dan/tcl
TCL_MEMDEBUG = $(TCL_DEBUG)
TCL = $(TCL_$(BUILD))

MKSTARKIT = ~/tcl/bin/tclkit-linux-x86-xft ~/bin/sdx.kit wrap

##### Flags passed to the C-compiler to link to Tcl.
#
# TCLLIB_DEBUG   = -L$(TCL)/lib -ltcl$(TCLVERSION)g -ltk$(TCLVERSION)g 
TCLLIB_RELEASE  = -L$(TCL)/lib -ltcl$(TCLVERSION) -ltk$(TCLVERSION)   
TCLLIB_DEBUG    = -L$(TCL)/lib -ltcl$(TCLVERSION) -ltk$(TCLVERSION)   
TCLLIB_PROFILE  = -L$(TCL)/lib -ltcl$(TCLVERSION) -ltk$(TCLVERSION)   
TCLLIB_MEMDEBUG = $(TCLLIB_DEBUG)

TCLLIB = -L/usr/X11R6/lib/ -lX11 -ldl -lm $(TCLLIB_$(BUILD))

##### The C-compiler to use and the flags to pass to it.
#
CC_RELEASE  = gcc
CC_DEBUG    = gcc
CC_MEMDEBUG = $(CC_DEBUG)
CC_PROFILE  = $(CC_DEBUG)
CC = $(CC_$(BUILD))

CFLAGS_RELEASE = -O2 -Wall -DNDEBUG -DHTML_MACROS
# CFLAGS_DEBUG   = -g -pg -DHTML_MACROS
#CFLAGS_DEBUG   = -g -Wall -DHTML_MACROS -DHTML_RES_DEBUG
CFLAGS_DEBUG    = -g -Wall -DHTML_DEBUG
CFLAGS_PROFILE  = -g -pg -Wall -DNDEBUG
CFLAGS_MEMDEBUG = -g -Wall -DRES_DEBUG -DHTML_DEBUG
CFLAGS = $(CFLAGS_$(BUILD))

##### The name of the shared library file to build.
#
SHARED_LIB_DEBUG    = libTkhtml3g.so
SHARED_LIB_PROFILE  = libTkhtml3pg.so
SHARED_LIB_MEMDEBUG = $(SHARED_LIB_DEBUG)
SHARED_LIB_RELEASE  = libTkhtml3.so
SHARED_LIB = $(SHARED_LIB_$(BUILD))

##### Command to build a shared library from a set of object files. The
#     command executed will be:
# 
#         $(MKSHLIB) $(OBJS) -o $(SHARED_LIB)
#
MKSHLIB = $(CC) -shared

##### Commands to run tclsh and wish.
#
TCLSH = $(TCL)/bin/tclsh$(TCLVERSION)
WISH = $(TCL)/bin/wish$(TCLVERSION)

##### Installation directories used by the 'install' target.
#
INSTALLDIR = $(TCL)/lib/Tkhtml3.0
MANINSTALLDIR = $(TCL)/man/mann

#
# End of configuration section.
# You should not need to change anything below this line
###########################################################################

default: binaries hwish

install: binaries
	mkdir -p $(INSTALLDIR)
	mkdir -p $(MANINSTALLDIR)
	cp -f $(BINARIES) $(INSTALLDIR)

###############################################################################
include $(TOP)/main.mk
