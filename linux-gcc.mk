#!/usr/bin/make
#
#### The toplevel directory of the source tree.
#
SRCDIR = /home/dan/work/tkhtml_cvs/htmlwidget

#### C Compiler and options for use in building executables that
#    will run on the platform that is doing the build.
#
BCC = gcc -g -O2

#### Name of the generated static library file
#
LIBNAME = libtkhtml.a

#### The suffix to add to executable files.  ".exe" for windows.
#    Nothing for unix.
#
E =

#### C Compile and options for use in building executables that 
#    will run on the target platform.  This is usually the same
#    as BCC, unless you are cross-compiling.
#
TCC = gcc -O6
#TCC = gcc -g -O0 -Wall
#TCC = gcc -g -O0 -Wall -fprofile-arcs -ftest-coverage
#TCC = /opt/mingw/bin/i386-mingw32msvc-gcc -O6 -DSTATIC_BUILD=1

#### Include file directories for Tcl and Tk.
#
INC = -I/home/dan/tcl/include

#### Extra arguments for linking 
#
LIBS = 

#### Command used to build a static library
#
#AR = /opt/mingw/bin/i386-mingw32msvc-ar r
AR = ar r
#RANLIB = /opt/mingw/bin/i386-mingw32msvc-ranlib
RANLIB = ranlib

#### The TCLSH command
#
TCLSH = tclsh

# You should not need to change anything below this line
###############################################################################
include $(SRCDIR)/main.mk
