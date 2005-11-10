

swproc nav_init {HTML {doclist ""}} {
    .m add cascade -label {Navigation} -menu [menu .m.nav]

    .m.nav add command -label {Forward} -command [list nav_forward $HTML]
    .m.nav add command -label {Back}    -command [list nav_back $HTML]
    .m.nav add separator

    set ::html_nav_doclist $doclist
    set ::html_nav_where -1

    if {[llength $::html_nav_doclist]} {
        nav_goto 0
    } else {
        nav_enabledisable
    }
}

proc nav_forward {HTML} {
    nav_goto [expr $::html_nav_where + 1]
}

proc nav_back {HTML} {
    nav_goto [expr $::html_nav_where - 1]
}

proc nav_goto {where} {
    set ::html_nav_where $where
    gui_goto [lindex $::html_nav_doclist $::html_nav_where]
    nav_enabledisable
}

proc nav_enabledisable {} {
    if {$::html_nav_where < 1} {
        .m.nav entryconfigure Back -state disabled
    } else {
        .m.nav entryconfigure Back -state normal
    }
    if {$::html_nav_where == [llength $::html_nav_doclist]} {
        .m.nav entryconfigure Forward -state disabled
    } else {
        .m.nav entryconfigure Forward -state normal
    }

    .m.nav delete 3 end
    for {set ii 0} {$ii < [llength $::html_nav_doclist]} {incr ii} {
        set doc [lindex $::html_nav_doclist $ii]
        .m.nav add command -label $doc -command [list nav_goto $ii]
    }

    .m.nav entryconfigure [expr $::html_nav_where + 3] -background white -state disabled
}

