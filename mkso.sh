#! /bin/sh
#
# This script builds "libtkhtml.so" for Linux and Tcl/Tk8.3-stubs.
# First do "make srcdir; cd srcdir".  Then run this script.
#
# $Revision: 1.6 $
#

BASE=/home/drh/tcltk/8.3linux
VERS=8.3

TKLIB="$BASE/libtkstub${VERS}g.a -L/usr/X11R6/lib -lX11"
TCLLIB="$BASE/libtclstub${VERS}g.a -lm -ldl"

CC='gcc -fPIC -DUSE_TCL_STUBS=1 -DUSE_TK_STUBS=1 -O2'
INC="-I. -I$BASE"

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
