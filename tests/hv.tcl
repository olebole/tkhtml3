
set auto_path [concat . $auto_path]
package require Tkhtml
source [file join [file dirname [info script]] tkhtml.tcl]

# Global symbols:
set ::HTML {}            ;# The HTML widget command
set ::DOCUMENT {}        ;# Name of html file to load

# This procedure is called once at the start of the script to build
# the GUI used by the application. It also sets up the callbacks
# supplied by this script to help the widget render html.
#
proc build_gui {} {
    set ::HTML [html .h]
    scrollbar .vscroll -orient vertical
    scrollbar .hscroll -orient horizontal

    pack .vscroll -fill y -expand true -side right
    pack .hscroll -fill x -expand true -side bottom
    pack $::HTML -fill both -expand true

    $::HTML configure -yscrollcommand {.vscroll set}
    $::HTML configure -xscrollcommand {.hscroll set}
    .vscroll configure -command "$::HTML yview"
    .hscroll configure -command "$::HTML xview"
     
    focus $::HTML
    bind $::HTML <KeyPress-q> exit
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

after 5000 {
  puts [$::HTML xview]
}
