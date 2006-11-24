
##### Top of the Tkhtml source tree - the directory with this file in it.
#
TOP = /home/dan/work/tkhtml/htmlwidget/

##### BUILD can be DEBUG or RELEASE.
#
# BUILD = DEBUG
BUILD = RELEASE

HV3_POLIPO = /home/dan/work/polipo/hv3_polipo.exe

##### Version of and path to the Tcl installation to use.
#
TCLVERSION = 85
TCL = /home/dan/work/tkhtml/mingwtcl/install

##### Flags passed to the C-compiler to link to Tcl.
#
# TCLLIB = -L$(TCL)/lib -ltclstub$(TCLVERSION) -ltkstub$(TCLVERSION)
TCLLIB = -L$(TCL)/lib -ltcl$(TCLVERSION) -ltk$(TCLVERSION) -ltclstub$(TCLVERSION) -ltkstub$(TCLVERSION)

##### Extra libraries used by Tcl on Linux. These flags are only required to
#     staticly link Tcl into an executable
#
# TCLLIB_DEBUG += -L/usr/X11R6/lib/ -lX11 -ldl -lm

BCC = gcc
CC = i386-mingw32-gcc

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
TCLSTUBSLIB =  "/home/dan/work/tkhtml/mingwtcl/install/lib/libtclstub85.a" 
TCLSTUBSLIB += "/home/dan/work/tkhtml/mingwtcl/install/lib/libtkstub85.a" 
TCLSTUBSLIB += -LC:/Tcl/lib

##### Commands to run tclsh and wish.
#
TCLSH = tclsh
WISH = wish

##### Strip the shared library
#
STRIP = i386-mingw32-strip

MKSTARKIT = tclkit /home/dan/bin/sdx.kit wrap
STARKITRT = /home/dan/work/tclkit-win32.upx.exe

#
# End of configuration section.
###########################################################################

default: binaries

hv3-win32.exe: hv3_img.kit
	cp $(STARKITRT) starkit_runtime
	$(MKSTARKIT) hv3_img.bin -runtime ./starkit_runtime
	mv hv3_img.bin hv3-win32.exe
	chmod 644 hv3-win32.exe

###############################################################################
include $(TOP)/main.mk

