#! /bin/sh
#
# This script builds "libtkhtml.so" for Linux and Tcl/Tk8.3.3-stubs.
# First do "make srcdir; cd srcdir; ../tkhtml/configure; make headers"
# Then run this script.
#
# $Revision: 1.8 $
#

SVER=8.3
VER=8.3.2

TCLBASE=../tcl$VER
TKBASE=../tk$VER
TKHTML=../tkhtml
TKLIB="/usr/lib/libtkstub$SVER.a -L/usr/X11R6/lib -lX11"
TCLLIB="/usr/lib/libtclstub$SVER.a -lm -ldl"

CC='gcc -g -fPIC -O2'
STUBS='-DUSE_TCL_STUBS=1 -DUSE_TK_STUBS=1'
INC="-I. -I$TCLBASE/generic -I$TKBASE/generic"

CMD="rm *.o"
echo $CMD
$CMD
for i in $TKHTML/src/html[a-z]*.c htmltokens.c; do
  if [ `basename $i` != htmlwish.c ]; then
    CMD="$CC $STUBS $INC -c $i"
    echo $CMD
    $CMD
  fi
done
CMD="gcc -g -o tkhtml.so -shared *.o $TKLIB $TCLLIB"
echo $CMD
$CMD

CMD="$CC $INC -c $TKHTML/src/htmlPs.c"
echo $CMD
$CMD
CMD="gcc -g -o tkhtmlpr.so -shared htmlPs.o $TKLIB $TCLLIB"
echo $CMD
$CMD
