#! /bin/sh
#
# This script builds "tkhtml.dll" for Win95/NT and Tcl/Tk8.1.1-stubs.
# First do "make srcdir; cd srcdir".  Then run this script.
#
# Notes:
#
# The tclstub.o and tkstub.o files were obtained by compiling the
# tclStubLib.c and tkStubLib.c files from the Tk8.1.1 distribution
# using cygwin/mingw.  Do not use the tclstub81.lib and tkstub81.lib
# files that come with Tcl/Tk from scripts.  They won't work.
#
# $Revision: 1.9 $
#

LIBHOME=/u/pcmacdon/Tcl8.3/win32/lib
TCLBASE=/u/pcmacdon/Tcl8.3/tcl8.3
TKBASE=/u/pcmacdon/Tcl8.3/tk8.3
TKLIB="$LIBHOME/tkStubLib.a"
TCLLIB="$LIBHOME/tclStubLib.a"

PATH=$PATH:/usr/local/cygb20/bin

CC='i586-cygwin32-gcc -DUSE_TCL_STUBS=1 -DUSE_TK_STUBS -mno-cygwin -O2'
INC="-I. -I$TCLBASE/generic -I$TKBASE/xlib -I$TKBASE/generic"

CMD="rm *.o"
echo $CMD
#$CMD
for i in *.c; do
  CMD="$CC $INC -c $i"
  echo $CMD
  $CMD
done
echo 'EXPORTS' >tkhtml.def
echo 'Tkhtml_Init' >>tkhtml.def
#-lcomdlg32 -luser32 -ladvapi32 -lgdi32. 

CMD="i586-cygwin32-dllwrap \
     --def tkhtml.def -v --export-all \
     --driver-name i586-cygwin32-gcc \
     --dlltool-name i586-cygwin32-dlltool \
     --as i586-cygwin32-as \
     --target i386-mingw32 -mno-cygwin \
     -dllname tkhtml.dll *.o $TKLIB $TCLLIB -luser32"
echo $CMD
$CMD
