

proc prop_click {HTML x y} {
    catch { .prop_menu unpost }
    set node [$HTML node $x $y]
    if {$node == ""} return

    if {[info commands .prop_menu]==""} {
        menu .prop_menu -tearoff 0
    }

    .prop_menu delete 0 end

    for {set n $node} {$n != ""} {set n [$n parent]} {
        if {[$n tag] != ""} {
            set d "<[$n tag]"
            foreach {a v} [$n attr] {
                append d " $a=\"$v\""
            }
            append d ">"
            .prop_menu add command -label "$d" -command [list prop_print $d $n]
        }
    }

    incr x [winfo x .]
    incr y [winfo y .]
    incr x [winfo x $HTML]
    incr y [winfo y $HTML]
    .prop_menu post $x $y
}

proc prop_print {description node} {
    puts "Computed values for node {$description}:"
    array set p [$node prop]

    set p(padding) $p(padding-top)
    if {
            $p(padding-left) != $p(padding) ||
            $p(padding-right) != $p(padding) ||
            $p(padding-bottom) != $p(padding)
    } {
        lappend p(padding) $p(padding-right) $p(padding-bottom) $p(padding-left)
    }
    unset p(padding-left)
    unset p(padding-right)
    unset p(padding-bottom)
    unset p(padding-top)

    set p(margin) $p(margin-top)
    if {
            $p(margin-left) != $p(margin) ||
            $p(margin-right) != $p(margin) ||
            $p(margin-bottom) != $p(margin)
    } {
        lappend p(margin) $p(margin-right) $p(margin-bottom) $p(margin-left)
    }
    unset p(margin-left)
    unset p(margin-right)
    unset p(margin-bottom)
    unset p(margin-top)

    foreach edge {top right bottom left} { 
        if {
            $p(border-$edge-width) != "inherit" &&
            $p(border-$edge-style) != "inherit" &&
            $p(border-$edge-color) != "inherit"
        } {
            set p(border-$edge) [list \
                $p(border-$edge-width) \
                $p(border-$edge-style) \
                $p(border-$edge-color) \
            ]
            unset p(border-$edge-width) 
            unset p(border-$edge-style) 
            unset p(border-$edge-color) 
        }
    }

    if {
        [info exists p(border-top)] &&
        [info exists p(border-bottom)] &&
        [info exists p(border-right)] &&
        [info exists p(border-left)] &&
        $p(border-top) == $p(border-right) &&
        $p(border-right) == $p(border-bottom) &&
        $p(border-bottom) == $p(border-left)
    } {
        set p(border) $p(border-top)
        unset p(border-top)
        unset p(border-left)
        unset p(border-right)
        unset p(border-bottom)
    }

    foreach key [lsort [array names p]] {
        puts "    $key: $p($key)"
    }
}



