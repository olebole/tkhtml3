#! /bin/sh
#
# This script builds "tkhtml.dll" for Win95/NT and Tcl/Tk8.1.1-stubs.
# First do "make srcdir; cd srcdir".  Then run this script.
#

LIBHOME=/home/drh/tcltk/win81
TCLBASE=/home/drh/tcltk/tcl8.1.1
TKBASE=/home/drh/tcltk/tk8.1.1

PATH=$PATH:/opt/cygwin20/bin

CC='i586-cygwin32-gcc -DUSE_TCL_STUBS=1 -DUSE_TK_STUBS -O2'
INC="-I. -I$TCLBASE/generic -I$TKBASE/generic"
TKLIB="$LIBHOME/tkstub81.lib"
TCLLIB="$LIBHOME/tclstub81.lib"

CMD="rm *.o"
echo $CMD
$CMD
for i in *.c; do
  CMD="$CC $INC -c $i"
  echo $CMD
  $CMD
done
echo 'EXPORTS' >tkhtml.def
echo 'Tkhtml_Init' >>tkhtml.def
CMD="i586-cygwin32-dllwrap \
     --def tkhtml.def -v \
     --driver-name i586-cygwin32-gcc \
     --dlltool-name i586-cygwin32-dlltool \
     -o tkhtml.dll *.o $TKLIB $TCLLIB"
echo $CMD
$CMD
