#! /bin/sh
#
# This script extracts version information from all the source files.
# Run this script in the top-level directory of the source tree.
#

grep Revision: `find . -type f -print` |
  sed -e 's,^./,,' -e 's,:[^ ]*, ,' |
  awk '/[1-9]\.[1-9]/ { printf "%-20s %s\n",$1,$3 }'
