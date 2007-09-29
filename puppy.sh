
# Script to build puppy linux package.
#

if test "${VERSION}" = "" ; then VERSION=15 ; fi
if test "${BUILD}" = ""   ; then BUILD=/home/dan/work/tkhtml/bld ; fi
if test "${SRC}" = ""     ; then SRC=/home/dan/work/tkhtml/htmlwidget ; fi

TCL=/home/dan/tcl/lib

rm -rf hv3-$VERSION
mkdir -p hv3-$VERSION/usr/lib/hv3/
mkdir -p hv3-$VERSION/usr/bin

cp -R $TCL/*tls* hv3-$VERSION/usr/lib/

strip $BUILD/libTkhtml3.so
cp $BUILD/libTkhtml3.so hv3-$VERSION/usr/lib/hv3/
cp $BUILD/pkgIndex.tcl hv3-$VERSION/usr/lib/hv3/

strip $BUILD/tclsee0.1/libTclsee.so
cp $BUILD/tclsee0.1/libTclsee.so hv3-$VERSION/usr/lib/hv3/
cat $BUILD/tclsee0.1/pkgIndex.tcl >> hv3-$VERSION/usr/lib/hv3/pkgIndex.tcl

cp $SRC/hv/*.tcl hv3-$VERSION/usr/lib/hv3/
rm hv3-$VERSION/usr/lib/hv3/tst_main.tcl
rm hv3-$VERSION/usr/lib/hv3/main.tcl

cp /home/dan/bin/hv3_polipo hv3-$VERSION/usr/bin

HV3=hv3-$VERSION/usr/bin/hv3
echo '#!/bin/sh' > $HV3
echo 'exec wish /usr/lib/hv3/hv3_main.tcl "$@"' >> $HV3
chmod 755 $HV3

tar -czf hv3-$VERSION.tgz hv3-$VERSION
tgz2pet hv3-$VERSION.tgz

