#! /bin/sh
#
# This script builds "libtkhtml.so" for Linux and Tcl/Tk8.1.1-stubs.
# First do "make srcdir; cd srcdir".  Then run this script.
#
# $Revision: 1.3 $
#

TCLBASE=/home/drh/tcltk/tcl8.1.1
TKBASE=/home/drh/tcltk/tk8.1.1

CC='gcc -fPIC -DUSE_TCL_STUBS=1 -DUSE_TK_STUBS=1 -O2'
INC="-I. -I$TCLBASE/generic -I$TKBASE/generic"
set TKLIB="$TKBASE/unix/libtkstub8.1.a -L/usr/X11R6/lib -lX11"
set TCLLIB="$TCLBASE/unix/libtclstub8.1.a -lm -ldl"

CMD="rm *.o"
echo $CMD
$CMD
for i in *.c; do
  CMD="$CC $INC -c $i"
  echo $CMD
  $CMD
done
CMD="gcc -o libtkhtml.so -shared *.o $TKLIB $TCLLIB"
echo $CMD
$CMD
