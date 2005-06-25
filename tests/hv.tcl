
set auto_path [concat . $auto_path]
package require Tkhtml
source [file join [file dirname [info script]] tkhtml.tcl]

# Global symbols:
set ::HTML {}            ;# The HTML widget command
set ::DOCUMENT {}        ;# Name of html file to load

# This procedure is called once at the start of the script to build
# the GUI used by the application.
#
proc build_gui {} {
    set ::HTML [html .h]
    pack $::HTML
}

# This procedure parses the command line arguments
#
proc parse_args {argv} {
    set ::DOCUMENT [lindex $argv 0]
}

proc load_document {} {
    set fd [open $::DOCUMENT]
    set doc [read $fd]
    close $fd

    $::HTML clear
    $::HTML default_style html
    $::HTML parse $doc
}

parse_args $argv
build_gui
load_document

