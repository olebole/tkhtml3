
###########################################################################
# hv3_prop.tcl --
#
#     This file contains code to implement the node browser window (the thing
#     with the tree on the left and the node info on the right). The public
#     interface to this file are the commands:
#
#         prop_browse HTML ?-node NODE?
#
set ::hv3_prop_selected ""

swproc prop_browse {HTML {node ""}} {
    if {[info commands .prop] == ""} {
        toplevel .prop
        bind .prop <KeyPress-q> {wm withdraw .prop}
        bind .prop <KeyPress-Q> {wm withdraw .prop}

        html .prop.html -width 400
        canvas .prop.tree -width 400 -background white -borderwidth 10

        scrollbar .prop.html_sb -orient vertical
        scrollbar .prop.tree_sb -orient vertical
    
        .prop.html configure -yscrollcommand {.prop.html_sb set}
        .prop.tree configure -yscrollcommand {.prop.tree_sb set}
        .prop.html_sb configure -command ".prop.html yview"
        .prop.tree_sb configure -command ".prop.tree yview"

        pack .prop.tree -side left -fill both -expand true
        pack .prop.html_sb -side right -fill y
        pack .prop.html -side right -fill both -expand true
        pack .prop.tree_sb -side right -fill y 
    } 
    wm state .prop normal

    if {$node != "" && [info commands $node] != ""} {
        set ::hv3_prop_selected $node
        for {set n [$node parent]} {$n != ""} {set n [$n parent]} {
            set ::hv3_prop_expanded($n) 1
        }
    }

    .prop.tree delete all
    prop_drawTree $HTML [$HTML node] 10 30
    if {[info commands $::hv3_prop_selected] == ""} {
        set ::hv3_prop_selected [$HTML node]
    }
    prop_displayNode $::hv3_prop_selected
    .prop.tree configure -scrollregion [.prop.tree bbox all]
}

image create photo idir -data {
    R0lGODdhEAAQAPIAAAAAAHh4eLi4uPj4APj4+P///wAAAAAAACwAAAAAEAAQAAADPVi63P4w
    LkKCtTTnUsXwQqBtAfh910UU4ugGAEucpgnLNY3Gop7folwNOBOeiEYQ0acDpp6pGAFArVqt
    hQQAO///
}
image create photo ifile -data {
    R0lGODdhEAAQAPIAAAAAAHh4eLi4uPj4+P///wAAAAAAAAAAACwAAAAAEAAQAAADPkixzPOD
    yADrWE8qC8WN0+BZAmBq1GMOqwigXFXCrGk/cxjjr27fLtout6n9eMIYMTXsFZsogXRKJf6u
    P0kCADv/
}

proc prop_nodeToLabel {node} {
    if {[$node tag] == ""} {
        return [string range [string trim [$node text]] 0 20]
    }
    set d "<[$node tag]"
    foreach {a v} [$node attr] {
        append d " $a=\"$v\""
    }
    append d ">"
}


proc prop_displayNode {node} {

    if {[$node tag] == ""} {
        append doc "<h1>Text</h1>"
        set text [string map {< &lt; > &gt;} [$node text]]
        set tokens [string map {< &lt; > &gt;} [$node text -tokens]]
        set doc [subst {
            <html><head></head><body>
            <h1>Text</h1>
            <p>$text
            <h1>Tokens</h1>
            <p>$tokens
        }]
    } else {
        set property_rows ""
        foreach {p v} [prop_compress [$node prop]] {
            append property_rows "<tr><td>$p<td>$v"
        }
        set attribute_rows ""
        foreach {p v} [$node attr] {
            append attribute_rows "<tr><td>$p<td>$v"
        }
        
        set doc [subst {
            <html><head></head><body>
            <h1>&lt;[$node tag]&gt;</h1>
            <p>Tcl command: <span class="code">\[$node\]</span>
            <p>Replacement: <span class="code">\[[$node replace]\]</span>
            <table class=computed>
                <tr><th colspan=2>Computed Properties
                $property_rows
            </table>
            <table class=attributes>
                <tr><th colspan=2>Attributes
                $attribute_rows

            </table>
        }]
    }

    if {[info exists ::hv3_log_layoutengine($node)]} {
        append doc "<h>Layout Engine:</h1>\n"
        append doc "<ul>\n"
        foreach entry $::hv3_log_layoutengine($node) {
            append doc "    <li>$entry\n"
        }
        append doc "</ul>\n"
    }
    if {[info exists ::hv3_log_styleengine($node)]} {
        append doc "<h>Style Engine:</h1>\n"
        append doc "<ul>\n"
        foreach entry $::hv3_log_styleengine($node) {
            append doc "    <li>$entry\n"
        }
        append doc "</ul>\n"
    }

    append doc "</body></html>\n"

    .prop.html reset
    .prop.html parse -final $doc
    .prop.html style {
        table,th,td {
          border: 1px solid;
        }
        td {
          padding:0px 15px;
        }
        table {
          margin: 20px;
        }
        .code {
          font-family: fixed;
        }
    }
}

proc prop_drawTree {HTML node x y} {
    set IWIDTH [image width idir]
    set IHEIGHT [image width idir]

    set XINCR [expr $IWIDTH + 2]
    set YINCR [expr $IHEIGHT + 5]

    set label [prop_nodeToLabel $node]
    if {$label == ""} {return 0}

    set leaf 1
    for {set i 0} {$i < [$node nChild]} {incr i} {
        if {"" != [prop_nodeToLabel $node]} {
            set leaf 0
            break
        }
    }

    if {$leaf} {
        .prop.tree create image $x $y -image ifile -anchor sw -tags ${node}_img
    } else {
        .prop.tree create image $x $y -image idir -anchor sw -tags ${node}_img
    }

    set tid [.prop.tree create text [expr $x+$XINCR] $y -tags ${node}_text]
    .prop.tree itemconfigure $tid -text $label -anchor sw
    if {$::hv3_prop_selected == $node} {
        set bbox [.prop.tree bbox $tid]
        set rid [
            .prop.tree create rectangle $bbox -fill skyblue -outline skyblue
        ]
        .prop.tree lower $rid $tid
    }

    .prop.tree bind ${node}_text <1> [subst -nocommands {
        prop_browse $HTML -node $node
    }]

    .prop.tree bind ${node}_img <1> [subst -nocommands {
        if {[info exists ::hv3_prop_expanded($node)]} {
          unset ::hv3_prop_expanded($node)
        } else {
          set ::hv3_prop_expanded($node) 1 
        }
        prop_browse $HTML
    }]
    
    set ret 1
    if {[info exists ::hv3_prop_expanded($node)]} {
        set xnew [expr $x+$XINCR]
        for {set i 0} {$i < [$node nChild]} {incr i} {
            set ynew [expr $y + $YINCR*$ret]
            set child [$node child $i]
            set incrret [prop_drawTree $HTML $child $xnew $ynew]
            incr ret $incrret
            if {$incrret > 0} {
                set y1 [expr $ynew - $IHEIGHT * 0.5]
                set x1 [expr $x + $XINCR * 0.5]
                set x2 [expr $x + $XINCR]
                .prop.tree create line $x1 $y1 $x2 $y1 -fill black
            }
        }

        catch {
          .prop.tree create line $x1 $y $x1 $y1 -fill black
        }
    }
    return $ret
}

proc prop_compress {props} {
    array set p $props

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

    set keys [lsort [array names p]]
    foreach r $keys {
      lappend ret $r $p($r)
    }
    return $ret
}

