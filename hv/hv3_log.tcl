

rename puts real_puts
proc puts {args} {
  eval [concat real_puts $args]
}

proc log_init {HTML} {
    $HTML configure -logcmd ""
    .m add cascade -label {Log} -menu [menu .m.log]
   
    set modes [list CALLBACK EVENT]
    set setlogcmd [list log_setlogcmd $HTML $modes]
 
    foreach mode $modes {
        .m.log add checkbutton -label $mode -variable ::log_$mode
        set ::log_$mode 0
        trace add variable ::log_$mode write $setlogcmd
    }

    eval $setlogcmd
}

proc log_setlogcmd {HTML modes args} {
    foreach mode $modes {
        if {[set ::log_$mode]} {
            $HTML configure -logcmd log_puts
            return 
        }
    }
    $HTML configure -logcmd ""
}

proc log_puts {topic body} {
    if {[info exists ::log_$topic] && [set ::log_$topic]} {
        real_puts stdout "$topic: $body"
    }
}


