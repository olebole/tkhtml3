# This Tcl script generates two files, cssprop.h and cssprop.c, that 
# implement a way to resolve CSS property names to symbols.

set properties [list \
azimuth background-attachment background-color background-image \
background-position background-repeat border-collapse border-spacing \
border-top-color border-right-color border-bottom-color border-left-color \
border-top-style border-right-style border-bottom-style border-left-style \
border-top-width border-right-width border-bottom-width border-left-width \
bottom caption-side clear clip color content counter-increment counter-reset \
cue-after cue-before cursor direction display elevation empty-cells float \
font-family font-size font-size-adjust font-stretch font-style font-variant \
font-weight height left letter-spacing line-height list-style list-style-image \
list-style-position list-style-type margin margin-top margin-right \
margin-bottom margin-left marker-offset marks max-height max-width \
min-height min-width orphans outline-color outline-style outline-width \
overflow padding padding-top padding-right padding-bottom padding-left \
page page-break-after page-break-before page-break-inside pause pause-after \
pause-before pitch pitch-range play-during position quotes richness right \
size speak speak-header speak-numeral speak-punctuation speech-rate stress \
table-layout text-align text-decoration text-indent text-shadow text-transform \
top unicode-bidi vertical-align visibility voice-family volume white-space \
widows width word-spacing z-index \
]

set shortcut_properties [list \
background border border-top border-right border-bottom border-left \
border-color border-style border-width cue font outline \
]

set fd [open cssprop.h w]
puts $fd "#ifndef __CSSPROP_H__"
puts $fd "#define __CSSPROP_H__"
set i 0
foreach p $properties {
  set str [string map {- _} [string toupper $p]]
  puts $fd "#define CSS_PROPERTY_$str $i"
  incr i
}
foreach s $shortcut_properties {
  set str [string map {- _} [string toupper $s]]
  puts $fd "#define CSS_SHORTCUTPROPERTY_$str $i"
  incr i
}
puts $fd "const char *tkhtmlCssPropertyToString(int i);"
puts $fd "int tkhtmlCssPropertyFromString(int n, const char *z);"
puts $fd "#endif"
set property_count $i
close $fd

set fd [open cssprop.c w]
puts $fd {#include "cssprop.h"}
puts $fd {#include <tcl.h>}
puts $fd "const char *tkhtmlCssPropertyToString(int i){"
puts $fd "    static const char *property_names\[\] = {"
foreach p $properties {
  puts $fd "        \"$p\","
}
foreach s $shortcut_properties {
  puts $fd "        \"$s\","
}
puts $fd "    };"
puts $fd {    return property_names[i];}
puts $fd "}"
puts $fd ""
puts $fd ""

puts $fd [subst -nocommands -nobackslashes {
int tkhtmlCssPropertyFromString(int n, const char *z){
    static int isInit = 0;
    static Tcl_HashTable h;
    Tcl_HashEntry *pEntry;
    char *zTerm;

    if( !isInit ){
        int i;
        isInit = 1;
        Tcl_InitHashTable(&h, TCL_STRING_KEYS);
        for(i=0; i<$i; i++){
            int newEntry;
            char const *zProp = tkhtmlCssPropertyToString(i);
            pEntry = Tcl_CreateHashEntry(&h, zProp, &newEntry);
            Tcl_SetHashValue(pEntry, i);
        }
    }

    if( n<0 ){
      zTerm = (char *)z;
    }else{
      zTerm = ckalloc(n+1);
      memcpy(zTerm, z, n);
      zTerm[n] = '\0';
    }

    pEntry = Tcl_FindHashEntry(&h, zTerm);

    if( zTerm!=z ){
      ckfree(zTerm);
    }

    if( pEntry ){
         return (int)Tcl_GetHashValue(pEntry);
    }
    return -1;
}
}]

close $fd

