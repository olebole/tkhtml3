#!/bin/sh
#
# This script is used to turn the "token.list" file into the
# "htmltokens.c" source file.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# http://www.gnu.ai.mit.edu/
#
# Author contact information:
#   drh@acm.org
#   http://www.hwaci.com/drh/


file=htmltokens.c
cat <<\END >$file
/* Automatically generated from token.list.  Do not edit */
#include <tk.h>
#include "htmltokens.h"
#if INTERFACE
struct HtmlTokenMap {
  char *zName;                /* Name of a markup */
  Html_16 type;               /* Markup type code */
  Html_16 extra;              /* Extra space needed above HtmlBaseElement */
  HtmlTokenMap *pCollide;     /* Hash table collision chain */
};
#define Html_Text    1
#define Html_Space   2
#define Html_Unknown 3
#define Html_Block   4
#define HtmlIsMarkup(X) ((X)->base.type>Html_Block)
END

############

sed -e '/^#/d' -e '/^ *$/d' $1 | sort | awk '
BEGIN {
  count = 5
}
{
  name = "Html_" toupper($1) 
  printf "#define %-20s %d\n", name, count++
  if ($3!="") {
    name = "Html_End" toupper($1)
    printf "#define %-20s %d\n", name, count++
  }
}
END {
  printf "#define %-20s %d\n", "Html_TypeCount", count-1
  printf "#define HTML_MARKUP_HASH_SIZE %d\n", count+11
  printf "#define HTML_MARKUP_COUNT %d\n", count-5;
}' >>$file

#############

cat <<\END >>$file
#endif /* INTERFACE */
HtmlTokenMap HtmlMarkupMap[] = {
END

#############

sed -e '/^#/d' -e '/^ *$/d' $1 | sort | awk '
/^ *[^#]/ {
  name = "\"" $1 "\","
  value = "Html_" toupper($1) ","
  if ( $2=="0" ){
    size = "0,"
  }else{
    size = "sizeof(" $2 "),"
  }
  printf "  { %-15s %-25s %-30s },\n",name,value,size
  if ($3!="") {
    name = "\"/" $1 "\","
    value = "Html_End" toupper($1) ","
    if ( $3=="0" ){
      size = "0,"
    }else{
      size = "sizeof(" $3 "),"
    }
    printf "  { %-15s %-25s %-30s },\n",name,value,size
  }
}' >>$file

#############

cat <<\END >>$file
};
END
