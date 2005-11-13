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

CFLAGS += -I$(TCL)/include -I. -I$(TOP)/src/

SRC = htmlparse.c htmldraw.c htmltcl.c htmlimage.c htmltree.c htmltagdb.c \
      cssparse.c css.c cssprop.c htmlstyle.c htmllayout.c htmlprop.c \
      htmlfloat.c htmlhash.c swproc.c htmlinline.c htmltable.c restrack.c

SRCHDR = $(TOP)/src/html.h $(TOP)/src/cssInt.h $(TOP)/src/css.h
GENHDR = cssprop.h htmltokens.h cssparse.h

HDR = $(GENHDR) $(SRCHDR)

OBJS = $(SRC:.c=.o)

LEMON = lemon
BINARIES = $(SHARED_LIB) pkgIndex.tcl

binaries: $(BINARIES)

pkgIndex.tcl: $(SHARED_LIB)
	(echo pkg_mkIndex -load Tk . \; exit;) | $(WISH)

$(SHARED_LIB): $(OBJS)
	$(MKSHLIB) $(OBJS) -o $@

%.o: $(TOP)/src/%.c $(HDR)
	$(CC) -c $(CFLAGS) $< -o $@

htmltcl.o: $(TOP)/src/htmltcl.c $(HDR) htmldefaultstyle.c
	$(CC) -c $(CFLAGS) $(TOP)/src/htmltcl.c -o $@

%.o: %.c $(HDR)
	$(CC) -c $(CFLAGS) $< -o $@

cssprop.h: $(TOP)/src/cssprop.tcl
	$(TCLSH) $<

htmldefaultstyle.c: $(TOP)/tests/tkhtml.tcl  $(TOP)/tests/html.css \
                    $(TOP)/src/mkdefaultstyle.tcl 
	$(TCLSH) $(TOP)/src/mkdefaultstyle.tcl > htmldefaultstyle.c

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
	if test -f $(TOP)/doc/tkhtml.html ; then \
		cp $(TOP)/doc/tkhtml.html ./hv3.vfs/index.html ; \
	fi


