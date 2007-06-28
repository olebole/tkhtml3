
# hv3_encodings.tcl
#
#     This file contains wrappers around the Tcl built-in commands 
#     [fconfigure] and [encoding]. The purpose is to support identifiers 
#     like "windows-1257" as an alias for "cp1257". We need to replace
#     the original commands so that the http package sees our encoding
#     database.
#
rename encoding encoding_orig
rename fconfigure fconfigure_orig

# encoding convertfrom ?encoding? data
# encoding convertto ?encoding? string
# encoding names
#
proc encoding {args} {
  set argv $args

  # Handle [encoding names]
  #
  if {[llength $argv] == 1 && [lindex $argv 0] eq "names"} {
    return [concat [array names ::Hv3EncodingMap] [encoding_orig names]]
  }

  # Map any explicitly specified encoding.
  #
  if {[llength $argv] == 3} {
    set enc [lindex $argv 1]
    if {[info exists ::Hv3EncodingMap($enc)]} {
      lset argv 1 $::Hv3EncodingMap($enc)
    }
  }

  # Call the real [encoding] command.
  eval encoding_orig $argv
}

# fconfigure channelId name value ?name value ...?
#
proc fconfigure {args} {
  set argv $args
  for {set ii 1} {($ii+1) < [llength $argv]} {incr ii 2} {
    if {[lindex $argv $ii] eq "-encoding"} {
      set enc [lindex $argv [expr {$ii+1}]]
      if {[info exists ::Hv3EncodingMap($enc)]} {
        lset argv [expr {$ii+1}] $::Hv3EncodingMap($enc)
      }
    }
  }

  # Call the real [fconfigure] command.
  eval fconfigure_orig $argv
}

# Build the mappings "database".
#
foreach name [encoding_orig names] {
  set ::Hv3EncodingMap($name) $name
  if {[string match cp* $name]} {
    set    name2 "windows-"
    append name2 [string range $name 2 end]
    set ::Hv3EncodingMap($name2) $name
  } 
}

