
set auto_path [concat . $auto_path]
package require Tkhtml
source [file join [file dirname [info script]] tkhtml.tcl]

# Global symbols:
set ::HTML {}            ;# The HTML widget command
set ::DOCUMENT {}        ;# Name of html file to load on startup.
set ::NODE {}            ;# Name of node under the cursor
set ::WIDGET 1           ;# Counter used to generate unique widget names

# If possible, load package "Img". Without it the script can still run,
# but won't be able to load many image formats.
catch {
  package require Img
}

# Update the status bar. The mouse is at screen coordinates (x, y).
# This proc is tied to a <Motion> event on the main Html widget.
#
proc update_status {x y} {
    # Global variable ::NODE stores the node that the cursor was over last
    # time this proc was called. If we are still hovering over the same
    # node, then return early as the status bar is already correct.
    #
    set n [$::HTML node $x $y]
    if {$n == $::NODE} {
        return
    }
    set ::NODE $n

    set status ""
    set linkto ""
    for {} {$n != ""} {set n [$n parent]} {
        if {[$n tag] == "text"} {
            set status "[$n text]"
        } else {
            set status "<[$n tag]>$status"
        }
        if {$linkto == "" && [$n tag] == "a" && [$n attr href] != ""} {
            set linkto [$n attr href]
        }
    }

    # If the cursor is hovering over a hyperlink, then set the status bar
    # to display "link: <url>" and set the cursor to "hand2". Otherwise,
    # set the status bar to display the node chain and set the cursor to
    # the default.
    #
    if {$linkto != ""} {
        . configure -cursor hand2
        set status "link: $linkto"
    } else {
        . configure -cursor {}
    }

    # Trim the status bar string so that it does not cause the GUI window
    # to grow.
    #
    set pixels [expr [winfo width .status] - 30]
    while {$status != "" && 
           [font measure [.status cget -font] $status] > $pixels} {
        set status [string range $status 0 end-10]
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

# This proc is called to get the replacement window for a node of type
# <INPUT>.
#
proc replace_input_node {base node} {
    set type [string tolower [$node attr type]]
    if {$type == ""} { set type text }
    set win ""
    set winname "$::HTML.formcontrol[incr ::WIDGET]"
    switch -- $type {
        text {
            set win [entry $winname]
        }
        password {
            set win [entry $winname -show]
        }
        submit {
            set win [button $winname -text [$node attr value]] 
        }
        button {
            set win [button $winname -text [$node attr value]] 
        }
    }
    return $win
}

# This proc is called to get the replacement window for a node of type
# <SELECT>.
#
proc replace_select_node {base node} {
    set options [list]
    set maxlen 0
    set win ""

    set winname "$::HTML.formcontrol[incr ::WIDGET]"
    set menuname "$winname.menu"
    set radiogroupname "::radio$::WIDGET"

    set menubutton [menubutton $winname]
    set menu [menu $menuname]

    for {set i 0} {$i < [$node nChildren]} {incr i} {
        set child [$node child $i]
        if {[$child tag] == "option"} {
            set label [$child attr label]
            if {$label == "" && [$child nChildren] == 1} {
                set label [[$child child 0] text]
            }
            $menu add radiobutton -label $label -variable $radiogroupname
            if {[string length $label]>$maxlen} {
                set maxlen [string length $label]
                set $radiogroupname $label
            }
        }
    }

    $menubutton configure -menu $menu 
    $menubutton configure -textvariable $radiogroupname 
    $menubutton configure -width $maxlen
    $menubutton configure -relief raised

    return $menubutton
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
        INPUT    {-tkhtml-replace:tcl(replace_input_node $base)}
        SELECT   {-tkhtml-replace:tcl(replace_select_node $base)}
    }]
    $::HTML parse $doc
}

parse_args $argv
build_gui
load_document $::DOCUMENT
