#! /bin/sh
#
# This script builds "libtkhtml.so" for Linux and Tcl/Tk8.1.1-stubs.
# First do "make srcdir; cd srcdir".  Then run this script.
#
# $Revision: 1.5 $
#

TCLBASE=/home/drh/tcltk/tcl8.3.0
TKBASE=/home/drh/tcltk/tk8.3.0

TKLIB="/home/drh/tcltk/linux/lib/libtkstub8.3g.a -L/usr/X11R6/lib -lX11"
TCLLIB="/home/drh/tcltk/linux/lib/libtclstub8.3g.a -lm -ldl"

CC='gcc -fPIC -DUSE_TCL_STUBS=1 -DUSE_TK_STUBS=1 -O2'
INC="-I. -I$TCLBASE/generic -I$TKBASE/generic"

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
