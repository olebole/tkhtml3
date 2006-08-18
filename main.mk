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
# MKSTARKIT           Command to transform a *.vfs directory to *.kit file.
# 

CFLAGS += -I$(TCL)/include -I. -I$(TOP)/src/
STUBSFLAGS = -DUSE_TCL_STUBS -DUSE_TK_STUBS

SRC = htmlparse.c htmldraw.c htmltcl.c htmlimage.c htmltree.c htmltagdb.c \
      cssparse.c css.c cssprop.c htmlstyle.c htmllayout.c htmlprop.c \
      htmlfloat.c htmlhash.c swproc.c htmlinline.c htmltable.c restrack.c \
      cssdynamic.c htmldecode.c htmltext.c

SRCHDR = $(TOP)/src/html.h $(TOP)/src/cssInt.h $(TOP)/src/css.h
GENHDR = cssprop.h htmltokens.h cssparse.h

HDR = $(GENHDR) $(SRCHDR)

OBJS = $(SRC:.c=.o)

LEMON = lemon
BINARIES = $(SHARED_LIB) pkgIndex.tcl

# How to run the C compiler:
COMPILE = $(CC) $(CFLAGS) $(STUBSFLAGS)

compile_announce:
	@echo ""
	@echo "Building Tkhtml 3"
	@echo ""
	@echo "COMPILE = $(COMPILE)"
	@echo "TCLSH   = $(TCLSH)"
	@echo ""
	@echo ""

binaries: compile_announce $(BINARIES)

pkgIndex.tcl: $(SHARED_LIB)
	echo 'pkg_mkIndex -load Tk . ; exit;' | $(WISH)


# (echo pkg_mkIndex -load Tk . \; exit;) | $(WISH)

$(SHARED_LIB): $(OBJS)
	$(MKSHLIB) $(OBJS) $(TCLSTUBSLIB) -o $@

%.o: $(TOP)/src/%.c $(HDR)
	@echo '$$(COMPILE) -c $< -o $@'
	@$(COMPILE) -c $< -o $@

htmltcl.o: $(TOP)/src/htmltcl.c $(HDR) htmldefaultstyle.c
	@echo '$$(COMPILE) -c $< htmldefaultstyle.c -o $@'
	@$(COMPILE) -c $(TOP)/src/htmltcl.c -o $@

%.o: %.c $(HDR)
	@echo '$$(COMPILE) -c $< -o $@'
	@$(COMPILE) -c $< -o $@

cssprop.h: $(TOP)/src/cssprop.tcl
	@echo '$$(TCLSH) $<'
	@$(TCLSH) $<

htmldefaultstyle.c: $(TOP)/src/tkhtml.tcl  $(TOP)/src/html.css $(TOP)/src/mkdefaultstyle.tcl 
	@echo '$$(TCLSH) $(TOP)/src/mkdefaultstyle.tcl > htmldefaultstyle.c'
	@$(TCLSH) $(TOP)/src/mkdefaultstyle.tcl > htmldefaultstyle.c

htmltokens.h:	$(TOP)/src/tokenlist.txt
	@echo '$$(TCLSH) $<'
	@$(TCLSH) $<

cssprop.c: cssprop.h

htmltokens.c: htmltokens.h

$(LEMON): $(TOP)/tools/lemon.c
	@echo '$$(COMPILE) $< htmldefaultstyle.c -o $@'
	@$(COMPILE) $(TOP)/tools/lemon.c -o $(LEMON)

cssparse.c: $(TOP)/src/cssparse.y $(LEMON)
	cp $(TOP)/src/cssparse.y .
	cp $(TOP)/tools/lempar.c .
	@echo '$$(LEMON) cssparse.y'
	@./$(LEMON) cssparse.y

cssparse.h: cssparse.c

# hwish: $(OBJS) $(TOP)/src/main.c
# $(CC) $(CFLAGS) $^ $(TCLLIB) -o $@

hwish: $(OBJS) $(TOP)/src/main.c
	$(COMPILE) $^ $(TCLLIB) -o $@

hv3_img.vfs: binaries
	mkdir -p ./hv3_img.vfs
	mkdir -p ./hv3_img.vfs/lib
	cp $(BINARIES) ./hv3_img.vfs/lib
	cp $(TOP)/hv/hv*.tcl ./hv3_img.vfs/
	cp $(TOP)/hv/main.tcl ./hv3_img.vfs/
	cp $(TOP)/hv/combobox.tcl ./hv3_img.vfs/
	cp $(TOP)/hv/snit.tcl ./hv3_img.vfs/
	cp $(TOP)/hv/index.html ./hv3_img.vfs/
	if test -d $(TCL)/lib/Img*/ ; then \
		cp -R $(TCL)/lib/Img*/ ./hv3_img.vfs/lib ; \
	fi
	# if test -d $(TCL)/lib/tile*/ ; then \
	# 	cp -R $(TCL)/lib/tile*/ ./hv3_img.vfs/lib ; \
	# fi
	if test -d $(TCL)/lib/*tls*/ ; then \
	  cp -R $(TCL)/lib/*tls* ./hv3_img.vfs/lib ; \
	fi
	touch hv3_img.vfs

hv3.vfs: binaries
	mkdir -p ./hv3.vfs
	mkdir -p ./hv3.vfs/lib
	cp $(BINARIES) ./hv3.vfs/lib
	cp $(TOP)/hv/hv*.tcl ./hv3.vfs/
	cp $(TOP)/hv/combobox.tcl ./hv3.vfs/
	cp $(TOP)/hv/main.tcl ./hv3.vfs/
	cp $(TOP)/hv/snit.tcl ./hv3.vfs/
	cp $(TOP)/hv/index.html ./hv3.vfs/
	if test -d $(TCL)/lib/*tls*/ ; then \
	  cp -R $(TCL)/lib/*tls* ./hv3.vfs/lib ; \
	fi
	touch hv3.vfs

hv3_img.kit: hv3_img.vfs
	$(MKSTARKIT) hv3_img.kit

hv3.kit: hv3.vfs
	$(MKSTARKIT) hv3.kit

website: hv3.kit hv3_img.kit
	mkdir -p www
	$(TCLSH) $(TOP)/webpage/mkwebpage.tcl > www/index.html
	$(TCLSH) $(TOP)/webpage/mksupportpage.tcl > www/support.html
	$(TCLSH) $(TOP)/webpage/mkhv3page.tcl > www/hv3.html
	$(TCLSH) $(TOP)/doc/macros.tcl -html $(TOP)/doc/html.man > www/tkhtml.html
	$(TCLSH) $(TOP)/doc/tkhtml_requirements.tcl > www/requirements.html
	cp $(TOP)/doc/tree.gif www/tree.gif
	cp $(TOP)/webpage/tkhtml_tcl_tk.css www/tkhtml_tcl_tk.css
	cp hv3.kit www/
	chmod 644 www/hv3.kit
	cp hv3_img.kit www/
	chmod 644 www/hv3_img.kit

test: hwish
	./hwish $(TOP)/tests/all.tcl

