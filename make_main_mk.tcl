#!/usr/bin/tclsh
#
# Run this TCL script to generate the "main.mk" makefile.
#

# Basenames of all source files:
#
set src {
  htmlcmd
  htmldraw
  htmlexts
  htmlform
  htmlimage
  htmlindex
  htmllayout
  htmlparse
  htmlPs
  htmlPsImg
  htmlsizer
  htmltable
  htmltcl
  htmltest
  htmlurl
  htmlwidget
}

# Generated files
#
set gen {
  htmltokens
}

puts {# This file is included by linux-gcc.mk or linux-mingw.mk or possible
# some other makefiles.  This file contains the rules that are common
# to building regardless of the target.
#

XTCC = $(TCC) $(CFLAGS) -I. -I$(SRCDIR)

}
puts -nonewline "SRC ="
foreach s [lsort $src] {
  puts -nonewline " \\\n  \$(SRCDIR)/src/$s.c"
}
puts "\n"
puts -nonewline "OBJ ="
foreach s [lsort [concat $src $gen]] {
  puts -nonewline " \\\n  $s.o"
}
puts "\n"

puts {
all:	$(LIBNAME) 

makeheaders:	$(SRCDIR)/tools/makeheaders.c
	$(BCC) -o makeheaders $(SRCDIR)/tools/makeheaders.c

$(LIBNAME):	headers $(OBJ)
	$(AR) $(LIBNAME) $(OBJ)

htmltokens.c:	$(SRCDIR)/src/tokenlist.txt $(SRCDIR)/tools/maketokens.tcl
	$(TCLSH) $(SRCDIR)/tools/maketokens.tcl \
		$(SRCDIR)/src/tokenlist.txt >htmltokents.c

headers:	makeheaders htmltokens.c $(SRC)
	./makeheaders $(SRCDIR)/src/html.h htmltokens.c $(SRC)
	touch headers

srcdir:	headers htmltokens.c
	mkdir -p srcdir
	rm -f srcdir/*
	cp $(SRC) htmltokens.c *.h srcdir

clean:	
	rm -f *.o $(LIBNAME)
	rm -f makeheaders headers}

set hfiles {}
foreach s [lsort [concat $src $gen]] {lappend hfiles $s.h}
puts "\trm -f $hfiles\n"

foreach s [lsort $src] {
  puts "$s.o:\t\$(SRCDIR)/src/${s}.c $s.h"
  puts "\t\$(XTCC) -o $s.o -c \$(SRCDIR)/src/${s}.c\n"
}
foreach s [lsort $gen] {
  puts "$s.o:\t${s}.c $s.h"
  puts "\t\$(XTCC) -o $s.o -c ${s}.c\n"
}
