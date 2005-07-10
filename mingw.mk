
##### Top of the Tkhtml source tree - the directory with this file in it.
#
TOP = $(HOME)/work/htmlwidget

##### BUILD can be DEBUG or RELEASE.
#
# BUILD = DEBUG
BUILD = RELEASE

##### Version of and path to the Tcl installation to use.
#
TCLVERSION = 8.4
TCL = /c/tcl

##### Flags passed to the C-compiler to link to Tcl.
#
TCLLIB = -L$(TCL)/lib -ltcl$(TCLVERSION) -ltk$(TCLVERSION)

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

#
# End of configuration section.
###########################################################################

default: headers binaries

headers:
	cp $(TOP)/src/*.h .

###########################################################################

###########################################################################
#
# Generic part of Makefile for Tkhtml. The following variables should be
# defined when this is sourced:
#
# CC                  Command to invoke C compiler.
# CFLAGS              Flags to pass to C compiler.
# SHARED_LIB          Name of shared-library to build.
# MKSHLIB             Command to build shared library.
# TCLSTUBSLIB         Libraries to link the shared library against.
# TCLSH               Command to execute a Tcl shell.
# TCLLIB              Options to pass to CC to link with Tcl.
# TOP                 Top of source tree (directory with this file).
# 

CFLAGS += -I$(TCL)/include -I. -I$(TOP)/src/

SRC = htmlparse.c htmldraw.c htmltcl.c htmlimage.c htmltree.c htmltagdb.c \
      cssparse.c css.c cssprop.c htmlstyle.c htmllayout.c htmlprop.c \
      htmlfloat.c htmlhash.c 

SRCHDR = $(TOP)/src/html.h $(TOP)/src/cssInt.h $(TOP)/src/css.h
GENHDR = cssprop.h htmltokens.h cssparse.h

HDR = $(GENHDR) $(SRCHDR)

OBJS = $(SRC:.c=.o)

LEMON = lemon
BINARIES = html.css tkhtml.tcl $(SHARED_LIB) pkgIndex.tcl

binaries: $(BINARIES)

html.css: $(TOP)/tests/html.css
	cp $< .

tkhtml.tcl: $(TOP)/tests/tkhtml.tcl
	cp $< .

pkgIndex.tcl: tkhtml.tcl $(SHARED_LIB)
	(echo pkg_mkIndex -load Tk . \; exit;) | $(WISH)

$(SHARED_LIB): $(OBJS)
	$(MKSHLIB) $(OBJS) $(TCLSTUBSLIB) -o $@

%.o: $(TOP)/src/%.c $(HDR)
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.c $(HDR)
	$(CC) -c $(CFLAGS) $< -o $@

cssprop.h: $(TOP)/src/cssprop.tcl
	$(TCLSH) $<

htmltokens.h:	$(TOP)/src/tokenlist.txt
	$(TCLSH) $<

cssprop.c: cssprop.h

htmltokens.c: htmltokens.h

$(LEMON): $(TOP)/tools/lemon.c
	$(CC) $(CFLAGS) `echo $(TOP)/tools/lemon.c` -o $(LEMON)

cssparse.c: $(TOP)/src/cssparse.y $(LEMON)
	cp $(TOP)/src/cssparse.y .
	cp $(TOP)/tools/lempar.c .
	./$(LEMON) cssparse.y

cssparse.h: cssparse.c

hwish: $(OBJS) $(TOP)/src/main.c
	$(CC) $(CFLAGS) -DTCL_USE_STUBS=0 $^ $(TCLLIB) -o $@

hv3.vfs: binaries 
	mkdir -p ./hv3.vfs
	mkdir -p ./hv3.vfs/lib
	cp $(BINARIES) ./hv3.vfs/lib
	cp $(TOP)/tests/hv.tcl ./hv3.vfs/lib
	cp $(TOP)/tests/main.tcl ./hv3.vfs
	(echo pkg_mkIndex -load Tk ./hv3.vfs/lib \; exit;) | $(WISH)
	if test -d $(TCL)/lib/Img1.3/ ; then \
		cp -R $(TCL)/lib/Img1.3/ ./hv3.vfs/lib ; \
	fi


