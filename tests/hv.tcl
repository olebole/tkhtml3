
catch {
  memory init on
}

set auto_path [concat . $auto_path]
package require Tkhtml 3.0
# source [file join [file dirname [info script]] tkhtml.tcl]

# Global symbols:
set ::HTML {}                ;# The HTML widget command
set ::DOCUMENT {}            ;# Name of html file to load on startup.
set ::EXIT 0                 ;# True if -exit switch specified 
set ::NODE {}                ;# Name of node under the cursor
set ::WIDGET 1               ;# Counter used to generate unique widget names
set ::MEMARRAY {}            ;# Used by proc layout_engine_report.
set ::TIMEARRAY {}           ;# Used by proc layout_engine_report.
array set ::ANCHORTONODE {}  ;# Map from anchor name to node command

# If possible, load package "Img". Without it the script can still run,
# but won't be able to load many image formats.
catch {
  package require Img
}

proc nodePrint {indent node} {
    set type [$node tag]
    set istr [string repeat " " $indent]
    set ret {}

    if {$type == "text"} {
        append ret $istr
        append ret [$node text]
        append ret "\n"
    } else {
        append ret $istr
        append ret "<[$node tag]>\n"
        for {set i 0} {$i < [$node nChildren]} {incr i} {
            append ret [nodePrint [expr $indent + 2] [$node child $i]]
        }
        append ret $istr
        append ret "</[$node tag]>\n"
    }

    return $ret
}

proc report_dialog {report} {
    if {![winfo exists .report]} {
        toplevel .report

        text .report.text
        scrollbar .report.scroll
        .report.text configure -width 100
        .report.text configure -yscrollcommand {.report.scroll set}
        .report.scroll configure -command {.report.text yview}

        pack .report.text -fill both -expand true -side left
        pack .report.scroll -fill y -expand true
    }

    .report.text delete 0.0 end
    .report.text insert 0.0 $report
}

proc layout_primitives_report {} {
    report_dialog [join [$::HTML layout primitives] "\n"]
}

proc document_tree_report {} {
    report_dialog [nodePrint 0 [$::HTML node]]
}

proc document_summary_report {} {
    set report {}
    set node [$::HTML node]
    set count [count_nodes $node]
    set primitives [llength [$::HTML layout primitives]]
    set layout_time [lindex [$::HTML var layout_time] 0]
    set report    "Layout time: $layout_time us\n"
    append report "Document nodes: [lindex $count 0]"
    append report " ([lindex $count 1] text)\n"
    append report "Layout primitives: $primitives\n"
    report_dialog $report
}

proc layout_engine_report {} {
    lappend ::MEMARRAY [string trim [memory info]]
    lappend ::TIMEARRAY [$::HTML var layout_time]

    $::HTML reset
    lappend ::MEMARRAY [string trim [memory info]]

    load_document $::DOCUMENT {}

    if {[llength $::MEMARRAY] < 6} {
        after idle layout_engine_report
    } else {
        set report_lines [split [lindex $::MEMARRAY 0] "\n"]
        foreach mem [lrange $::MEMARRAY 1 end] {
            set l 0
            foreach line [split $mem "\n"] {
                set number [format {% 8s} [lindex $line end]]
                lset report_lines $l "[lindex $report_lines $l] $number" 
                incr l
            }
        }
        lappend report_lines {}
        lappend report_lines "Layout times (us): $::TIMEARRAY"
        set report [join $report_lines "\n"]
        report_dialog $report
        set ::MEMARRAY {}
        set ::TIMEARRAY {}
    }
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

# This procedure is called when the user clicks on the html widget. If the
# mouse is over a hyper-link we load the new document.
#
proc click {x y} {
    set link ""
    for {set node [$::HTML node $x $y]} {$node!=""} {set node [$node parent]} {
        if {[$node tag] == "a" && [$node attr href] != ""} {
            set link [$node attr href]
            break
        }
    }

    if {$link != ""} {
        set parts [split $link #]
        set doc [lindex $parts 0]
        set anchor [lindex $parts 1]
        if {$doc == "" || [file join $::BASE $doc] == $::DOCUMENT} {
            if {[info exists ::ANCHORTONODE($anchor)]} {
                set node $::ANCHORTONODE($anchor)
                $::HTML yview moveto $node
            }
        } else {
            set ::DOCUMENT [file join $::BASE $doc]
            load_document $::DOCUMENT $anchor
        }
    }
}

# This procedure is called when a <style> tag is encountered during
# parsing. The $script parameter holds the entire contents of the node.
#
proc handle_style_node {script} {
    $::HTML style parse author.0 $script
}

# This procedure is called when a <link> node is encountered while building
# the document tree. It loads a stylesheet from a file on disk if required.
#
proc handle_link_node {node} {
    if {[$node attr rel] == "stylesheet"} {
        set fd [open [file join $::BASE [$node attr href]]]
        set script [read $fd]
        close $fd
    }
    $::HTML style parse author.1 $script
}

# This procedure is called when a <a> node is encountered while building
# the document tree. If the <a> has a name attribute, put an entry in the
# ::ANCHORTONODE map.
#
proc handle_a_node {node} {
    set name [$node attr name]
    if {$name != ""} {
        set ::ANCHORTONODE($name) $node
    }
}

# Analyse the tree with node $node at it's head and return a two element
# list. The first element of the list is the total number of nodes in the
# tree. The second element is the number of "text" nodes in the tree.
#
proc count_nodes {node} {
    if {[$node tag] == "text"} {
        set ret {1 1}
    } else {
        set ret {1 0}
    }
    
    for {set i 0} {$i < [$node nChildren]} {incr i} {
        set c [count_nodes [$node child $i]]
        lset ret 0 [expr [lindex $ret 0] + [lindex $c 0]]
        lset ret 1 [expr [lindex $ret 1] + [lindex $c 1]]
    }

    return $ret
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

    . config -menu [menu .m]
    foreach cascade [list Reports] {
        set newmenu [string tolower .m.$cascade]
        .m add cascade -label $cascade -menu [menu $newmenu]
        $newmenu configure -tearoff 0
    }

    .m.reports add command -label {Document Summary} \
             -command document_summary_report
    .m.reports add command -label {Document Tree} -command document_tree_report
    .m.reports add command -label {Layout Primitives} \
            -command layout_primitives_report
    .m.reports add command -label {Layout Engine} -command layout_engine_report

    pack .vscroll -fill y -side right
    pack .status -fill x -side bottom 
    pack .hscroll -fill x -side bottom
    pack $::HTML -fill both -expand true

    $::HTML configure -yscrollcommand {.vscroll set}
    $::HTML configure -xscrollcommand {.hscroll set}
    .vscroll configure -command "$::HTML yview"
    .hscroll configure -command "$::HTML xview"

    bind $::HTML <Motion> "update_status %x %y"
    bind $::HTML <KeyPress-q> "$::HTML reset ; exit"
    bind $::HTML <ButtonPress> "click %x %y"

    $::HTML handler script style "handle_style_node"
    $::HTML handler node link "handle_link_node"
    $::HTML handler node a "handle_a_node"

    focus $::HTML
}

# This procedure parses the command line arguments
#
proc parse_args {argv} {
    for {set i 0} {$i < [llength $argv]} {incr i} {
        if {[lindex $argv $i] == "-exit"} {
            set ::EXIT 1
        } else {
            set ::DOCUMENT [lindex $argv $i]
        }
    }
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

proc load_document {document anchor} {
    set fd [open $document]
    set doc [read $fd]
    close $fd

    set base [file dirname $document]
    set ::BASE $base

    array set ::ANCHORTONODE {}
    $::HTML reset
    $::HTML default_style html
    $::HTML style parse agent.1 [subst -nocommands {
        IMG[src] {-tkhtml-replace:tcl(replace_img_node $base)}
        INPUT    {-tkhtml-replace:tcl(replace_input_node $base)}
        SELECT   {-tkhtml-replace:tcl(replace_select_node $base)}
    }]
    $::HTML parse $doc

    if {$anchor != "" && [info exists ::ANCHORTONODE($anchor)]} {
        update
        $::HTML yview moveto $::ANCHORTONODE($anchor)
    }
}

parse_args $argv
build_gui
load_document $::DOCUMENT {}

if {$::EXIT} {
    update
    catch {
      memory active mem.out
      puts [memory info]
    }
    exit
}

