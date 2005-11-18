#
# cssprop.tcl --
#
#     This Tcl script generates two files, cssprop.h and cssprop.c, that
#     implement a way to resolve CSS property names and constants to
#     symbols.
#
#----------------------------------------------------------------------------
# Copyright (c) 2005 Eolas Technologies Inc.
# All rights reserved.
#
# This Open Source project was made possible through the financial support
# of Eolas Technologies Inc.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the <ORGANIZATION> nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

proc C {args} {foreach a $args {lappend ::constants $a}}
proc P {args} {foreach a $args {lappend ::properties $a}}
proc S {args} {foreach a $args {lappend ::shortcut_properties $a}}

C inherit
C table-header-group table-footer-group table-row 
C block inline list-item none                     ;# 'display'
C run-in compact marker table inline-table 
C table-caption table-row-group table-cell
C table-header-group table-footer-group table-row 
C table-column-group table-column
C left right none                                 ;# 'float'
C left right none both                            ;# 'clear'
C left right center justify                       ;# 'text-align'
C auto                                            ;# 'margin'
C square disc circle none                         ;# 'list-style-type'
C decimal lower-alpha upper-alpha lower-roman     ;# 'list-style-type'
C upper-roman                                     ;# 'list-style-type'
C outside inside                                  ;# 'list-style-position'
C italic oblique                                  ;# 'font-style'
C bold bolder lighter                             ;# 'font-weight'
C small-caps                                      ;# 'font-variant'
C top middle bottom baseline sub super            ;# 'vertical-align'
C pre nowrap normal                               ;# 'white-space'
C xx-small x-small small medium large x-large     ;# 'font-size'
C xx-large larger smaller
C thin medium thick                               ;# 'border-width'
C none hidden dotted dashed solid double groove   ;# 'border-style'
C ridge outset inset
C scroll fixed                                    ;# 'background-attachment'
C repeat no-repeat repeat-x repeat-y              ;# 'background-repeat'
C top left right bottom center                    ;# 'background-position'
C black silver gray white maroon red purple aqua  ;# Standard web colors
C fuchsia green lime olive yellow navy blue teal
C transparent                                     ;# 'background-color'
C underline overline line-through none            ;# 'text-decoration'
C text-top text-bottom 

P azimuth background-attachment background-color background-image 
P background-repeat border-collapse border-spacing 
P border-top-color border-right-color border-bottom-color border-left-color 
P border-top-style border-right-style border-bottom-style border-left-style 
P border-top-width border-right-width border-bottom-width border-left-width 
P bottom caption-side clear clip color content counter-increment counter-reset 
P cue-after cue-before cursor direction display elevation empty-cells float 
P font-family font-size font-size-adjust font-stretch font-style font-variant 
P font-weight height left letter-spacing line-height list-style-image
P list-style-position list-style-type margin-top margin-right 
P margin-bottom margin-left marker-offset marks max-height max-width 
P min-height min-width orphans outline-color outline-style outline-width 
P overflow padding-top padding-right padding-bottom padding-left 
P page page-break-after page-break-before page-break-inside pause pause-after 
P pause-before pitch pitch-range play-during position quotes richness right 
P size speak speak-header speak-numeral speak-punctuation speech-rate stress 
P table-layout text-align text-decoration text-indent text-shadow text-transform
P top unicode-bidi vertical-align visibility voice-family volume white-space 
P widows width word-spacing z-index 

S background border border-top border-right border-bottom border-left
S border-color border-style border-width cue font padding outline margin
S list-style

# We treat property 'background-position' as a composite property that sets
# invented properties background-position-x and background-position-y.
#
P background-position-x background-position-y
S background-position

#########################################################################
#########################################################################
# End of configuration, start of code.
#########################################################################
#########################################################################

set ::cssprop_h {}          ;# Contents of cssprop_h file
set ::cssprop_c {}          ;# Contents of cssprop_c file

proc CodeInfrastructure {} {
    append ::cssprop_c [regsub -all {\n        } {

        typedef struct HashEntry HashEntry;
        struct HashEntry {
            const char *zString;       /* String for this entry */
            int iNext;                 /* Next entry in hash-chain, or -1 */
        };
        
        /*
         * Return the hash of zString. The hash is guaranteed to be between 0
         * and 127, inclusive.
         */
        static int 
        Hash(nString, zString) 
            int nString;
            const char *zString;
        {
            unsigned int result = 0;
            const char *string = zString;
            const char *end = &zString[nString];
            int c;
            for (c=*string; string != end; c=*++string) {
                result += (result<<3) + tolower(c);
            }
            if (result & 0x00000080) { 
                result = ~result;
            }
            return (result & 0x000000FF);
        }

        static int
        Lookup(nString, zString, aTable, aHashTable) 
            int nString;
            const char *zString;
            int *aTable;
            HashEntry *aHashTable;
        {
            int t;

            if (nString < 0) {
                nString = strlen(zString);
            }

            for (
                 t = aTable[Hash(nString, zString)]; 
                 t >= 0 && (
                     strlen(aHashTable[t].zString) != nString || 
                     strncasecmp(zString, aHashTable[t].zString, nString)
                 );
                 t = aHashTable[t].iNext
            );

            return t;
        }
    } "\n"]
}

proc Hash {string} {
    set result 0
    binary scan [string tolower $string] c* bytes
    foreach b $bytes {
        incr result [expr ($result<<3) + $b]
    }
    if {$result & 0x00000080} {
        set r [expr ($result & 0x0000007F) ^ 0x0000007F]
    } else {
        set r [expr $result & 0x0000007F]
    }
    return $r
}

#
# CodeLookup --
#
#     Write C code for a set of strings. The two function prototypes will be:
#
#         int          ${prefix}Lookup(int n, const char *z);
#         const char * ${prefix}ToString(int e);
#     
#     The argument $entries is a list containing the data used by the Lookup()
#     function. Each entry of the list is itself a list of length two, the
#     string followed by it's numeric symbol. For example:
#
#         set entries [list                                              \
#                 {display CSS_PROPERTY_DISPLAY}                         \
#                 {border-width-right CSS_PROPERTY_BORDER_WIDTH_RIGHT}
#         ]
#
#     The entries list should be in the order that the numerical constants
#     should be assigned. The constant allocated is $firstconstant.
#
proc CodeLookup {prefix entries firstconstant} {

    append ::cssprop_h "int ${prefix}Lookup(int, const char *);\n"
    append ::cssprop_h "const char * ${prefix}ToString(int);\n"

    # Setup array $hashtable. This array maps from 7-bit hash value to a list
    # of constants that correspond to it.
    foreach e $entries {
        set s [lindex $e 0]
        set c [lindex $e 1]
        set h [Hash $s]
        lappend hashtable($h) $c
    }

    foreach k [array names hashtable] {
        set l $hashtable($k)
        set inexttable([lindex $l 0]) -1
        set ifirsttable($k) [lindex $l end]
        for {set i [expr [llength $l] - 1]} {$i > 0} {incr i -1} {
            set a [lindex $l $i]
            set b [lindex $l [expr $i - 1]]
            set inexttable($a) $b
        }
    }

    set negval [expr -1 * ($firstconstant + 1)]

    append ::cssprop_c "\n"
    append ::cssprop_c "static const HashEntry a${prefix}\[\] = \{\n"
    set constant $firstconstant
    foreach e $entries {
        set s [lindex $e 0]
        set c [lindex $e 1]
        append ::cssprop_h "#define $c $constant\n"
        set iNext $inexttable($c)
        if {$iNext == "-1"} {
            append ::cssprop_c "    \{\"$s\", $negval\},\n"
        } else {
            append ::cssprop_c "    \{\"$s\", $iNext - $firstconstant\},\n"
        }
        incr constant
    }
    append ::cssprop_c "\};\n"

    append ::cssprop_c "\n"
    append ::cssprop_c "\n"

    append ::cssprop_c "int\n"
    append ::cssprop_c "${prefix}Lookup(n, z)\n"
    append ::cssprop_c "    int n;\n"
    append ::cssprop_c "    const char *z;\n"
    append ::cssprop_c "\{\n"
    append ::cssprop_c "    int aTable\[\] = \{\n"

    set acc 0
    for {set i 0} {$i < 128} {incr i} {
        if {[info exists ifirsttable($i)]} {
            set nextbit "$ifirsttable($i) - $firstconstant, "
        } else {
            set nextbit "$negval, "
        }
        if {($acc + [string length $nextbit]) > 70} {
            append ::cssprop_c "\n"
            set acc 0
        }
        if {$acc == 0} {
            append ::cssprop_c "        "
        }
        append ::cssprop_c $nextbit
        incr acc [string length $nextbit]
    }
    if {$acc > 0} {
        append ::cssprop_c "\n"
    }
    append ::cssprop_c "    \};"
    append ::cssprop_c "\n"
    append ::cssprop_c "    return Lookup(n, z, aTable, a${prefix})"
    append ::cssprop_c " + $firstconstant;\n"
    append ::cssprop_c "\}\n"
    append ::cssprop_c "\n"
    append ::cssprop_c "\n"
    append ::cssprop_c "const char *\n"
    append ::cssprop_c "${prefix}ToString(e)\n"
    append ::cssprop_c "    int e;\n"
    append ::cssprop_c "\{\n"
    append ::cssprop_c "    return a${prefix}\[e - $firstconstant\].zString;\n"
    append ::cssprop_c "\}\n"
}

proc writefile {filename text} {
    set fd [open $filename w]
    puts $fd $text
    close $fd
}

foreach a [lsort -unique $constants] {
    set b "CSS_CONST_[string map {- _} [string toupper $a]]"
    lappend ::constant_map [list $a $b]
}
foreach a [lsort -unique $properties] {
    set b "CSS_PROPERTY_[string map {- _} [string toupper $a]]"
    lappend ::property_map [list $a $b]
}
foreach a [lsort -unique $shortcut_properties] {
    set b "CSS_SHORTCUTPROPERTY_[string map {- _} [string toupper $a]]"
    lappend ::property_map [list $a $b]
}

append ::cssprop_c "#include \"cssprop.h\"\n"
append ::cssprop_c "#include <string.h>        /* strlen() */\n"
append ::cssprop_c "#include <ctype.h>         /* tolower() */\n"
CodeInfrastructure
CodeLookup HtmlCssConstant $constant_map  100
CodeLookup HtmlCssProperty $property_map  0

set max_constant [expr [llength $constant_map] + 100 - 1]
set max_property [expr [llength [lsort -unique $properties]] - 1]

append ::cssprop_h "#define CSS_CONST_MIN_CONSTANT 100\n"
append ::cssprop_h "#define CSS_PROPERTY_MIN_PROPERTY 0\n"
append ::cssprop_h "#define CSS_CONST_MAX_CONSTANT $max_constant\n"
append ::cssprop_h "#define CSS_PROPERTY_MAX_PROPERTY $max_property\n"

writefile cssprop.h $::cssprop_h
writefile cssprop.c $::cssprop_c

