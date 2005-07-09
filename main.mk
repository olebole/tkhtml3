
TOP = $(HOME)/work/tkhtml_cvs/htmlwidget

TCL = $(HOME)/tcl
TCLVERSION = 8.5

TCLSH = $(TCL)/bin/tclsh$(TCLVERSION)
TCLLIB = -L$(TCL)/lib -ltcl$(TCLVERSION) -ltk$(TCLVERSION)

CC = gcc
# CFLAGS = -O2 -DNDEBUG
CFLAGS = -g
CFLAGS += -I$(TCL)/include -I. -I$(TOP)/src/

SHARED_LIB = libTkhtml3.so
MKSHLIB = gcc -shared

INSTALLDIR = $(TCL)/lib/Tkhtml3.0
MANINSTALLDIR = $(TCL)/man/mann

install: binaries
	mkdir -p $(INSTALLDIR)
	mkdir -p $(MANINSTALLDIR)
	cp -f $(BINARIES) $(INSTALLDIR)
	cp -f $(TOP)/doc/tkhtml.n $(MANINSTALLDIR)

#
# End of configuration section.
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
# TCLSH               Command to execute a Tcl shell.
# TCLLIB              Options to pass to CC to link with Tcl.
# TOP                 Top of source tree (directory with this file).
# 

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
	(echo package require Tk \; pkg_mkIndex -load Tk . \; exit;) | $(TCLSH)

$(SHARED_LIB): $(OBJS)
	$(MKSHLIB) $(OBJS) -o $@

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
