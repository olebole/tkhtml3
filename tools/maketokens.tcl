#!/bin/sh
# This script is a replacement for the maketokens.sh shell script.
# The shell script required GNU awk.  This script should work with
# any old version of tclsh.
# \
exec tclsh "$0" ${1+"$@"}

if {$argc!=1} {
  puts stderr "Usage: $argv0 tokenlist.txt"
  exit 1
}
if {[catch {open [lindex $argv 0] r} f]} {
  puts stderr "$argv0: can not open \"[lindex $argv 0]\": $f"
  exit 1
}
set tokenlist {}
while {![eof $f]} {
  set line [string trim [gets $f]]
  if {$line==""} continue
  if {[string index $line 0]=="#"} continue
  if {[llength $line]!=2 && [llength $line]!=3}  continue
  lappend tokenlist [lindex $line 0]
  lappend tokenlist [lindex $line 1]
  lappend tokenlist [lindex $line 2]
}
close $f

# Open the two files that will be generated.
set h_file [open htmltokens2.h w]
set c_file [open htmltokens.c w]

set warning {
/* 
 * DO NOT EDIT!
 *
 * The code in this file was automatically generated. See the files
 * src/tokenlist.txt and tools/maketokens.tcl from the tkhtml source
 * distribution.
 */
}
puts $h_file $warning
puts $c_file $warning

puts $h_file {
#define Html_Text    1
#define Html_Space   2
#define Html_Unknown 3
#define Html_Block   4
#define HtmlIsMarkup(X) ((X)->base.type>Html_Block)
}

set count 5
set fmt {#define %-20s %d}

foreach {name start end} $tokenlist {
  set upr [string toupper $name]
  puts $h_file [format $fmt Html_$upr $count]
  incr count
  if {$end!=""} {
    puts $h_file [format $fmt Html_End$upr $count]
    incr count
  }
}

puts $h_file [format $fmt Html_TypeCount [expr $count-1]]
puts $h_file "#define HTML_MARKUP_HASH_SIZE [expr $count+11]"
puts $h_file "#define HTML_MARKUP_COUNT [expr $count-5]"
puts $c_file "HtmlTokenMap HtmlMarkupMap\[\] = {"

set fmt "  { %-15s %-25s %-30s },"

foreach {name start end} $tokenlist {
  set upr [string toupper $name]
  set nm "\"$name\","
  set val Html_$upr,
  if {$start=="0"} {
    set size "0,"
  } else {
    set size "sizeof($start),"
  }
  puts $c_file [format $fmt $nm $val $size]
  if {$end==""} continue
  set nm "\"/$name\","
  set val Html_End$upr,
  if {$end=="0"} {
    set size "0,"
  } else {
    set size "sizeof($end),"
  }
  puts $c_file [format $fmt $nm $val $size]
}

puts $c_file "};"

close $c_file
close $h_file
