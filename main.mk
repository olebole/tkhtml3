# This file is included by linux-gcc.mk or linux-mingw.mk or possible
# some other makefiles.  This file contains the rules that are common
# to building regardless of the target.
#

XTCC = $(TCC) $(CFLAGS) -I. -I$(SRCDIR)


SRC = \
  $(SRCDIR)/src/htmlPs.c \
  $(SRCDIR)/src/htmlPsImg.c \
  $(SRCDIR)/src/htmlcmd.c \
  $(SRCDIR)/src/htmldraw.c \
  $(SRCDIR)/src/htmlexts.c \
  $(SRCDIR)/src/htmlform.c \
  $(SRCDIR)/src/htmlimage.c \
  $(SRCDIR)/src/htmlindex.c \
  $(SRCDIR)/src/htmllayout.c \
  $(SRCDIR)/src/htmlparse.c \
  $(SRCDIR)/src/htmlsizer.c \
  $(SRCDIR)/src/htmltable.c \
  $(SRCDIR)/src/htmltcl.c \
  $(SRCDIR)/src/htmltest.c \
  $(SRCDIR)/src/htmlurl.c \
  $(SRCDIR)/src/htmlwidget.c

OBJ = \
  htmlPs.o \
  htmlPsImg.o \
  htmlcmd.o \
  htmldraw.o \
  htmlexts.o \
  htmlform.o \
  htmlimage.o \
  htmlindex.o \
  htmllayout.o \
  htmlparse.o \
  htmlsizer.o \
  htmltable.o \
  htmltcl.o \
  htmltest.o \
  htmltokens.o \
  htmlurl.o \
  htmlwidget.o


all:	$(LIBNAME) 

makeheaders:	$(SRCDIR)/tools/makeheaders.c
	$(BCC) -o makeheaders $(SRCDIR)/tools/makeheaders.c

$(LIBNAME):	headers $(OBJ)
	$(AR) $(LIBNAME) $(OBJ)

htmltokens.c:	$(SRCDIR)/src/tokenlist.txt $(SRCDIR)/tools/maketokens.tcl
	$(TCLSH) $(SRCDIR)/tools/maketokens.tcl  $(SRCDIR)/src/tokenlist.txt >htmltokents.c

headers:	makeheaders htmltokens.c $(SRC)
	./makeheaders $(SRCDIR)/src/html.h htmltokens.c $(SRC)
	touch headers

srcdir:	headers htmltokens.c
	mkdir -p srcdir
	rm -f srcdir/*
	cp $(SRC) htmltokens.c *.h srcdir

clean:	
	rm -f *.o $(LIBNAME)
	rm -f makeheaders headers
	rm -f htmlPs.h htmlPsImg.h htmlcmd.h htmldraw.h htmlexts.h htmlform.h htmlimage.h htmlindex.h htmllayout.h htmlparse.h htmlsizer.h htmltable.h htmltcl.h htmltest.h htmltokens.h htmlurl.h htmlwidget.h

htmlPs.o:	$(SRCDIR)/src/htmlPs.c htmlPs.h
	$(XTCC) -o htmlPs.o -c $(SRCDIR)/src/htmlPs.c

htmlPsImg.o:	$(SRCDIR)/src/htmlPsImg.c htmlPsImg.h
	$(XTCC) -o htmlPsImg.o -c $(SRCDIR)/src/htmlPsImg.c

htmlcmd.o:	$(SRCDIR)/src/htmlcmd.c htmlcmd.h
	$(XTCC) -o htmlcmd.o -c $(SRCDIR)/src/htmlcmd.c

htmldraw.o:	$(SRCDIR)/src/htmldraw.c htmldraw.h
	$(XTCC) -o htmldraw.o -c $(SRCDIR)/src/htmldraw.c

htmlexts.o:	$(SRCDIR)/src/htmlexts.c htmlexts.h
	$(XTCC) -o htmlexts.o -c $(SRCDIR)/src/htmlexts.c

htmlform.o:	$(SRCDIR)/src/htmlform.c htmlform.h
	$(XTCC) -o htmlform.o -c $(SRCDIR)/src/htmlform.c

htmlimage.o:	$(SRCDIR)/src/htmlimage.c htmlimage.h
	$(XTCC) -o htmlimage.o -c $(SRCDIR)/src/htmlimage.c

htmlindex.o:	$(SRCDIR)/src/htmlindex.c htmlindex.h
	$(XTCC) -o htmlindex.o -c $(SRCDIR)/src/htmlindex.c

htmllayout.o:	$(SRCDIR)/src/htmllayout.c htmllayout.h
	$(XTCC) -o htmllayout.o -c $(SRCDIR)/src/htmllayout.c

htmlparse.o:	$(SRCDIR)/src/htmlparse.c htmlparse.h
	$(XTCC) -o htmlparse.o -c $(SRCDIR)/src/htmlparse.c

htmlsizer.o:	$(SRCDIR)/src/htmlsizer.c htmlsizer.h
	$(XTCC) -o htmlsizer.o -c $(SRCDIR)/src/htmlsizer.c

htmltable.o:	$(SRCDIR)/src/htmltable.c htmltable.h
	$(XTCC) -o htmltable.o -c $(SRCDIR)/src/htmltable.c

htmltcl.o:	$(SRCDIR)/src/htmltcl.c htmltcl.h
	$(XTCC) -o htmltcl.o -c $(SRCDIR)/src/htmltcl.c

htmltest.o:	$(SRCDIR)/src/htmltest.c htmltest.h
	$(XTCC) -o htmltest.o -c $(SRCDIR)/src/htmltest.c

htmlurl.o:	$(SRCDIR)/src/htmlurl.c htmlurl.h
	$(XTCC) -o htmlurl.o -c $(SRCDIR)/src/htmlurl.c

htmlwidget.o:	$(SRCDIR)/src/htmlwidget.c htmlwidget.h
	$(XTCC) -o htmlwidget.o -c $(SRCDIR)/src/htmlwidget.c

htmltokens.o:	htmltokens.c htmltokens.h
	$(XTCC) -o htmltokens.o -c htmltokens.c

