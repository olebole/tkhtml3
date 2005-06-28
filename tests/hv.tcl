
set auto_path [concat . $auto_path]
package require Tkhtml
source [file join [file dirname [info script]] tkhtml.tcl]

# Global symbols:
set ::HTML {}            ;# The HTML widget command
set ::DOCUMENT {}        ;# Name of html file to load on startup.

# If possible, load package "Img". Without it the script can still run,
# but won't be able to load many image formats.
catch {
  package require Img
}

# Update the status bar. The mouse is at screen coordinates (x, y).
# This proc is tied to a <Motion> event on the main Html widget.
#
proc update_status {x y} {
    set status ""
    set linkto ""
    for {set n [$::HTML node $x $y]} {$n != ""} {set n [$n parent]} {
        if {[$n tag] == "text"} {
            set status "[$n text]"
        } else {
            set status "<[$n tag]>$status"
        }
        if {$linkto == "" && [$n tag] == "a" && [$n attr href] != ""} {
            set linkto [$n attr href]
        }
    }

    if {$linkto != ""} {
        set status "link: $linkto"
        . configure -cursor hand2
    } else {
        . configure -cursor {}
        set status "[string range $status 0 60]"
    }

    .status configure -text $status
}

# This procedure is called once at the start of the script to build
# the GUI used by the application. It also sets up the callbacks
# supplied by this script to help the widget render html.
#
proc build_gui {} {
    set ::HTML [html .h]
    scrollbar .vscroll -orient vertical
    scrollbar .hscroll -orient horizontal
    label .status -height 1 -anchor w -background white

    pack .vscroll -fill y -side right
    pack .status -fill x -side bottom 
    pack .hscroll -fill x -side bottom
    pack $::HTML -fill both -expand true

    $::HTML configure -yscrollcommand {.vscroll set}
    $::HTML configure -xscrollcommand {.hscroll set}
    .vscroll configure -command "$::HTML yview"
    .hscroll configure -command "$::HTML xview"

    bind $::HTML <Motion> "update_status %x %y"
     
    focus $::HTML
    bind $::HTML <KeyPress-q> exit
}

# This procedure parses the command line arguments
#
proc parse_args {argv} {
    set ::DOCUMENT [lindex $argv 0]
}

# This proc is called to get the replacement image for a node of type <IMG>
# with a "src" attribute defined. 
#
proc replace_img_node {base node} {
    set imgfile [file join $base [$node attr src]]
    image create photo -file $imgfile
}

proc load_document {document} {
    set fd [open $document]
    set doc [read $fd]
    close $fd

    set base [file dirname $document]

    $::HTML clear
    $::HTML default_style html
    $::HTML style parse agent.1 [subst -nocommands {
        IMG[src] {-tkhtml-replace:tcl(replace_img_node $base)}
    }]
    $::HTML parse $doc
}

parse_args $argv
build_gui
load_document $::DOCUMENT

