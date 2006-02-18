
#--------------------------------------------------------------------------
# Global variables section
 
array unset ::hv3_log_styleengine
array unset ::hv3_log_layoutengine
array set ::hv3_log_layoutengine [list]
array set ::hv3_log_styleengine [list]
#--------------------------------------------------------------------------

rename puts real_puts
proc puts {args} {
  eval [concat real_puts $args]
}

proc log_init {HTML} {
    $HTML configure -logcmd ""
    .m add cascade -label {Log} -menu [menu .m.log]
   
    set modes [list CALLBACK EVENT ENGINES]
    set timermodes [list LAYOUT STYLE]

    # Command to run to make sure -logcmd and -timercmd are set as per the
    # configuration in array variables ::html_log_log and ::html_log_timer
    #
    set setlogcmd [list log_setlogcmd $HTML $modes]
 
    foreach mode $modes {
        .m.log add checkbutton -label $mode -variable ::html_log_log($mode)
        set ::html_log_log($mode) 0
        trace add variable ::html_log_log($mode) write $setlogcmd
    }
    .m.log add separator
    foreach mode $timermodes {
        .m.log add checkbutton -label $mode -variable ::html_log_timer($mode)
        set ::html_log_timer($mode) 0
        trace add variable ::html_log_timer($mode) write $setlogcmd
    }
    .m.log add separator
    .m.log add command -label "Layout Primitives" \
        -command [list log_primitives $HTML]
    .m.log add command -label "Tree" -command [list log_tree $HTML]

    eval $setlogcmd
}

proc log_setlogcmd {HTML modes args} {
    $HTML configure -logcmd ""
    $HTML configure -timercmd ""

    foreach key [array names ::html_log_log] {
        if {$::html_log_log($key)} {
            $HTML configure -logcmd log_puts
            break 
        }
    }
    foreach key [array names ::html_log_timer] {
        if {$::html_log_timer($key)} {
            $HTML configure -timercmd timer_puts
            break 
        }
    }

}

proc log_puts {topic body} {
    if {$topic == "LAYOUTENGINE"} {
        if {$body == "START"} {
            array unset ::hv3_log_layoutengine
        } else {
            set idx [string first " " $body]
            set node [string range $body 0 [expr $idx - 1]]
            set msg [string range $body [expr $idx + 1] end]
            lappend ::hv3_log_layoutengine($node) $msg
        }
        return
    }

    if {$topic == "STYLEENGINE"} {
        if {$body == "START"} {
            array unset ::hv3_log_styleengine
        } else {
            set idx [string first " " $body]
            set node [string range $body 0 [expr $idx - 1]]
            set msg [string range $body [expr $idx + 1] end]
            lappend ::hv3_log_styleengine($node) $msg
        }
        return
    }

    if {[info exists ::html_log_log($topic)] && $::html_log_log($topic)} {
        real_puts stdout "$topic: $body"
    }
}
proc timer_puts {topic body} {
    if {[info exists ::html_log_timer($topic)] && $::html_log_timer($topic)} {
        real_puts stdout "TIMER: $topic: $body"
    }
}

proc log_primitives {HTML} {
    foreach p [$HTML primitives] {
        set type [lindex $p 0]
        if {[catch {incr hist($type)}]} {set hist($type) 1}
        real_puts stdout $p
    }
    puts ""
    foreach t [array names hist] {
      puts "$t -> $hist($t)"
    }
}

proc log_node {n indent} {
    if {[$n tag] == ""} {
        if {[string trim [$n text]] != ""} {  
            puts -nonewline [string repeat " " $indent]
            puts "\"[$n text]\""
        }
    } else {
        puts -nonewline [string repeat " " $indent]
        incr indent 4
        set out "<[$n tag]"
        foreach {a v} [$n attr] {
            append out " $a=\"$v\""
        }
        append out ">"
        puts $out
        for {set ii 0} {$ii < [$n nChild]} {incr ii} {
            log_node [$n child $ii] $indent
        }
    }
}

proc log_tree {HTML} {
    set n [$HTML node]
    log_node $n 0
}

