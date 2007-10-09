
#export VERSION=nightly-`date +"%y_%m%d"`
export VERSION=alpha-16

HERE=`pwd`

export SRC=/home/dan/work/tkhtml/htmlwidget/
BLD1=/tmp/bld1
BLD2=/tmp/bld2
BLD3=/tmp/bld3

rm -rf $BLD1 $BLD2 $BLD3
mkdir $BLD1
mkdir $BLD2
mkdir $BLD3

# Make the linux build.
#
cd ${BLD1}
make -f ${SRC}/linux-gcc.mk tclsee hv3-linux-x86.gz BUILD=RELEASE

# Make the win32 build.
#
cd ${BLD2}
make -f ${SRC}/mingw.mk tclsee hv3-win32.exe BUILD=RELEASE

# Create the puppy package.
#
export BUILD=${BLD1}
cd ${BLD3}
sh ${SRC}/puppy.sh

cd ${HERE}
mv ${BLD1}/hv3_img.kit ./hv3-linux-${VERSION}.kit
mv ${BLD1}/hv3-linux-x86.gz ./hv3-linux-${VERSION}.gz
mv ${BLD2}/hv3-win32.exe ./hv3-win32-${VERSION}.exe
mv ${BLD2}/hv3_img.kit ./hv3-win32-${VERSION}.kit
mv ${BLD3}/hv3-${VERSION}.pet .

chmod 644 ./hv3-linux-${VERSION}.kit
chmod 644 ./hv3-win32-${VERSION}.kit
chmod 644 ./hv3-win32-${VERSION}.exe
chmod 644 ./hv3-${VERSION}.pet
chmod 644 ./hv3-linux-${VERSION}.gz

tclsh ${SRC}/webpage/mkhv3page.tcl > hv3.html

