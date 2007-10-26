
##### Top of the Tkhtml source tree - the directory with this file in it.
#
TOP = $(HOME)/work/tkhtml/htmlwidget

##### BUILD can be DEBUG, RELEASE or MEMDEBUG.
#
BUILD = DEBUG
# BUILD = RELEASE
# BUILD = MEMDEBUG
# BUILD = PROFILE

##### Path to hv3_polipo binary to include in starkit builds.
#
HV3_POLIPO = $(HOME)/bin/hv3_polipo

##### Version of and path to the Tcl installation to use.
#
TCLVERSION = 8.5

TCL_RELEASE  = $(HOME)/tcl
TCL_DEBUG    = $(HOME)/tcl
TCL_PROFILE  = $(HOME)/tcl_profile
#TCL_PROFILE  = $(HOME)/tcl
TCL_MEMDEBUG = $(HOME)/memtcl
TCL = $(TCL_$(BUILD))

# MKSTARKIT = ~/tcl/bin/tclkit-linux-x86-xft ~/bin/sdx.kit wrap
MKSTARKIT = ~/bin/tclkit ~/bin/sdx.kit wrap
STARKITRT = ~/bin/tclkit

##### Javascript libaries - libgc.a and libsee.a
#
JS_SHARED_LIB = libTclsee.so

JSLIB   = $(HOME)/javascript/install/lib/libgc.a
JSLIB  += $(HOME)/javascript/install/lib/libsee.a -lpthread
JSFLAGS = -I$(HOME)/javascript/install/include

#JSLIB  = $(HOME)/javascript/install_nogc/lib/libsee.a
#JSFLAGS = -I$(HOME)/javascript/install_nogc/include -DNO_HAVE_GC

##### Flags passed to the C-compiler to link to Tcl.
#
# TCLLIB_DEBUG   = -L$(TCL)/lib -ltcl$(TCLVERSION)g -ltk$(TCLVERSION)g 
TCLLIB_RELEASE  = -L$(TCL)/lib -ltcl$(TCLVERSION) -ltk$(TCLVERSION)   
TCLLIB_DEBUG    = -L$(TCL)/lib -ltcl$(TCLVERSION) -ltk$(TCLVERSION)   
#TCLLIB_PROFILE    = -L$(TCL)/lib -ltcl$(TCLVERSION) -ltk$(TCLVERSION)   
TCLLIB_PROFILE  = $(TCL)/lib/libtcl8.5.a $(TCL)/lib/libtk8.5.a -lXft -lXss

TCLLIB_MEMDEBUG = $(TCLLIB_DEBUG)
TCLLIB_MEMDEBUG += $(TCL)/lib/libtclstub$(TCLVERSION).a
TCLLIB_MEMDEBUG += $(TCL)/lib/libtkstub$(TCLVERSION).a

TCLLIB = -L/usr/X11R6/lib/ -lX11 -ldl -lm $(TCLLIB_$(BUILD))

##### The C-compiler to use and the flags to pass to it.
#
CC_RELEASE  = gcc
CC_DEBUG    = gcc
CC_MEMDEBUG = $(CC_DEBUG)
CC_PROFILE  = $(CC_DEBUG)
CC = $(CC_$(BUILD))
BCC = $(CC_$(BUILD))

CFLAGS_RELEASE = -O2 -Wall -DNDEBUG -DHTML_MACROS -DTKHTML_ENABLE_PROFILE
CFLAGS_DEBUG    = -g -Wall -DHTML_DEBUG -DTKHTML_ENABLE_PROFILE
CFLAGS_PROFILE  = -g -pg -Wall -DNDEBUG
CFLAGS_MEMDEBUG = -g -Wall -DRES_DEBUG -DHTML_DEBUG -DTCL_MEM_DEBUG=1
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
TCLSTUBSLIB_MEMDEBUG  =  "/home/dan/memtcl/lib/libtclstub8.5.a" 
TCLSTUBSLIB_MEMDEBUG += "/home/dan/memtcl/lib/libtkstub8.5.a" 

TCLSTUBSLIB = $(TCLSTUBSLIB_$(BUILD))

STRIP_RELEASE = strip
STRIP_DEBUG = true
STRIP_MEMDEBUG = $(STRIP_DEBUG)
STRIP_PROFILE = $(STRIP_DEBUG)
STRIP = $(STRIP_$(BUILD))

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

hv3-linux-x86: hv3_img.kit
	cp $(STARKITRT) starkit_runtime
	$(MKSTARKIT) hv3_img.bin -runtime ./starkit_runtime
	mv hv3_img.bin hv3-linux-x86

hv3-linux-x86.gz: hv3-linux-x86
	gzip hv3-linux-x86
	chmod 644 hv3-linux-x86.gz

###############################################################################
include $(TOP)/main.mk
