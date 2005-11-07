

rename puts real_puts
proc puts {args} {
  eval [concat real_puts $args]
}

proc log_init {HTML} {
    $HTML configure -logcmd ""
    .m add cascade -label {Log} -menu [menu .m.log]
   
    set modes [list CALLBACK EVENT]
    set timermodes [list DAMAGE LAYOUT STYLE]

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
    if {[info exists ::html_log_log($topic)] && $::html_log_log($topic)} {
        real_puts stdout "$topic: $body"
    }
}
proc timer_puts {topic body} {
    if {[info exists ::html_log_timer($topic)] && $::html_log_timer($topic)} {
        real_puts stdout "TIMER: $topic: $body"
    }
}


