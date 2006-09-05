namespace eval hv3 { set {version($Id: hv3_prop.tcl,v 1.41 2006/09/05 16:06:02 danielk1977 Exp $)} 1 }

###########################################################################
# hv3_prop.tcl --
#
#     This file contains code to implement the Tkhtml debugging interface.
#     Sometimes I call it the "tree browser".
#

package require Tk

source [file join [file dirname [info script]] hv3_widgets.tcl]

# The first argument, $HTML, must be the name of an html widget that
# exists somewhere in the application. If one does not already exist,
# invoking this proc instantiates an HtmlDebug object associated with
# the specified html widget. This in turn creates a top-level 
# debugging window exclusively associated with the specified widget.
# The HtmlDebug instance is deleted when the top-level window is 
# destroyed, either by hitting 'q' or 'Q' while the window has focus,
# or by closing the window using a window-manager interface.
#
# If the second argument is not "", then it is the name of a node 
# within the document currently loaded by the specified html widget.
# If this node exists, the debugging window presents information 
# related to it.
#
namespace eval ::HtmlDebug {
  proc browse {HTML {node ""}} {
    set name ${HTML}.debug
    if {![winfo exists $name]} {
      HtmlDebug $name $HTML
    }
    return [$name browseNode $node]
  }
}

#--------------------------------------------------------------------------
# class HtmlDebug --
#
#     This class encapsulates code used for debugging the Html widget. 
#     It manages a gui which can be used to investigate the structure and
#     Tkhtml's handling of the currently loaded Html document.
#
snit::widget HtmlDebug {
  hulltype toplevel

  # Class internals
  typevariable Template

  variable mySelected ""         ;# Currently selected node
  variable myExpanded -array ""  ;# Array. Entries are present for exp. nodes
  variable myHtml                ;# Name of html widget being debugged

    # Debugging window widgets. The top 3 are managed by panedwindow
    # widgets.
    #
    #   $myTreeCanvas                  ;# [::hv3::scrolled canvas] mega-widget
    #   $mySearchHtml                  ;# Hv3 mega-widget
    #     $mySearchHtml.html.relayout  ;# button
    #     $mySearchHtml.html.outline   ;# button
    #     $mySearchHtml.html.search    ;# entry
    #   $myReportHtml                  ;# Hv3 mega-widget

  variable myStyleEngineLog
  variable myLayoutEngineLog
  variable mySearchResults ""

  variable myTreeCanvas         ;# The canvas widget with the tree
  variable myReportHtml         ;# The hv3 widget with the report
  variable mySearchHtml         ;# The hv3 widget with the search field

  constructor {HTML} {
    set myHtml $HTML
  
    # Top level window
    bind $win <KeyPress-q>  [list destroy $win]
    bind $win <KeyPress-Q>  [list destroy $win]
  
    set mySearchHtml $win.hpan.search
    set myTreeCanvas $win.hpan.vpan.tree
    set myReportHtml $win.hpan.vpan.report
  
    panedwindow $win.hpan -orient vertical
    ::hv3::hv3 $mySearchHtml
    panedwindow $win.hpan.vpan -orient horizontal
    ::hv3::scrolled canvas $myTreeCanvas -propagate 1
    ::hv3::hv3 $myReportHtml
  
    $win.hpan add $mySearchHtml
    $win.hpan add $win.hpan.vpan
    $win.hpan.vpan add $myTreeCanvas
    $win.hpan.vpan add $myReportHtml 
    catch {
      $win.hpan.vpan paneconfigure $myTreeCanvas -stretch always
      $win.hpan.vpan paneconfigure $myReportHtml -stretch always
    }
    $win.hpan sash place 0 200 200
  
    ::hv3::use_tclprotocol $mySearchHtml 
    $mySearchHtml configure -height 200
  
    set b [button [$mySearchHtml html].relayout]
    $b configure -text "Re-Render Document With Logging" 
    $b configure -command [mymethod rerender]
    set b2 [button [$mySearchHtml html].outline]
    $b2 configure -text "Add \":focus {outline: solid ...}\""
    $b2 configure -command [list $myHtml style {
      :focus {outline-style: solid; outline-color: blue ; outline-width: 1px}
    }]
    set e [entry [$mySearchHtml html].search]
    bind $e <Return> [mymethod searchNode]
    $self searchNode
  
    ::hv3::use_tclprotocol $myReportHtml 
    [$myReportHtml html] configure -width 5 -height 5
  
    $myTreeCanvas configure -background white -borderwidth 10
    $myTreeCanvas configure -width 5
  
    pack ${win}.hpan -expand true -fill both
    ${win}.hpan configure -width 800 -height 600
  
    bind ${win}.hpan <Destroy> [list destroy $win]
  }

  method drawSubTree {node x y} {
    set IWIDTH [image width idir]
    set IHEIGHT [image width idir]

    set XINCR [expr $IWIDTH + 2]
    set YINCR [expr $IHEIGHT + 5]

    set tree $myTreeCanvas

    set label [prop_nodeToLabel $node]
    if {$label == ""} {return 0}

    set leaf 1
    foreach child [$node children] {
        if {"" != [prop_nodeToLabel $child]} {
            set leaf 0
            break
        }
    }

    if {$leaf} {
      set iid [$tree create image $x $y -image ifile -anchor sw]
    } else {
      set iid [$tree create image $x $y -image idir -anchor sw]
    }

    set tid [$tree create text [expr $x+$XINCR] $y]
    $tree itemconfigure $tid -text $label -anchor sw
    if {$mySelected == $node} {
        set bbox [$tree bbox $tid]
        set rid [
            $tree create rectangle $bbox -fill skyblue -outline skyblue
        ]
        $tree lower $rid $tid
    }

    $tree bind $tid <1> [list HtmlDebug::browse $myHtml $node]
    $tree bind $iid <1> [mymethod toggleExpanded $node]
    
    set ret 1
    if {[info exists myExpanded($node)]} {
        set xnew [expr $x+$XINCR]
        foreach child [$node children] {
            set ynew [expr $y + $YINCR*$ret]
            set incrret [$self drawSubTree $child $xnew $ynew]
            incr ret $incrret
            if {$incrret > 0} {
                set y1 [expr $ynew - $IHEIGHT * 0.5]
                set x1 [expr $x + $XINCR * 0.5]
                set x2 [expr $x + $XINCR]
                $tree create line $x1 $y1 $x2 $y1 -fill black
            }
        }

        catch {
          $tree create line $x1 $y $x1 $y1 -fill black
        }
    }
    return $ret
  }

  method redrawCanvas {} {
    set canvas $myTreeCanvas
    $canvas delete all
    $self drawSubTree [$myHtml node] 15 30
    $canvas configure -scrollregion [$canvas bbox all]
  }

  method browseNode {node} {
    wm state $win normal
    raise $win

    $myReportHtml goto "tcl:///$self report $node"
  }

  # These are not actually part of the public interface, but they must
  # be declared public because they are invoked by widget callback scripts
  # and so on. They are really private methods.

  # HtmlDebug::rerender
  #
  #     This method is called to force the attached html widget to rerun the
  #     style and layout engines.
  #
  method rerender {} {
    set html [$myHtml html]
    set logcmd [$html cget -logcmd]
    $html configure -logcmd [mymethod Logcmd]
    unset -nocomplain myLayoutEngineLog
    unset -nocomplain myStyleEngineLog
    $html relayout
    $html force
    after idle [list ::HtmlDebug::browse $myHtml $mySelected]
    # after idle [list $html configure -logcmd $logcmd]
    $html configure -logcmd $logcmd
  }

  method searchNodeBg {idx} {
    after idle [mymethod searchNode $idx]
  }

  method searchNode {{idx 0}} {

    # The template document for the $myReportHtml widget.
    #
    set Template {
      <html><head>
        <style>
          td { padding-left: 10px ; padding-right: 10px }
        </style>
      </head><body><center>
        <h1>Tkhtml Document Tree Browser</h1>
        <span widget="relayout"></span>
        <span widget="outline"></span>
        <p>
          Search for node: <span widget="search"></span>
        </p>
        $search_results
      </center></body></html>
    }
  
    set search_results {}
    set nodelist [list]
    set selector [[$mySearchHtml html].search get]
    if {$selector != ""} {
      set search_results <table><tr>
      set nodelist [$myHtml search $selector]
      set ii 0
      foreach node $nodelist {
        # set script "after idle {::HtmlDebug::browse $myHtml $node}"
        # set script "::HtmlDebug::browse $myHtml $node"
        set script [mymethod searchNodeBg $ii]
        append search_results "<td><a href=\"$script\">$node</a>"
        incr ii
        if {($ii % 5)==0} {
          append search_results "</tr><tr>"
        }
      }
      append search_results </table>
    }
  
    set ::subbed_template [subst $Template]
    $mySearchHtml goto "tcl:///list"
    $mySearchHtml goto "tcl:///set ::subbed_template"
    foreach node [$mySearchHtml search {span[widget]}] {
      set widget [$node attr widget]
      $node replace [$mySearchHtml html].$widget -deletecmd {}
    }
  
    set h [lindex [[$mySearchHtml html] bbox] 3]
    if {$selector != ""} {
      set mh [expr [winfo height $win]/2]
      if {$h > $mh} {set h $mh}
    }
    # [$mySearchHtml html] configure -height $h
  
    if {[llength $nodelist]>$idx} {
      HtmlDebug::browse $myHtml [lindex $nodelist $idx]
    }
  
    return ""
  }

  method toggleExpanded {node} {
    if {[info exists myExpanded($node)]} {
      unset myExpanded($node)
    } else {
      set myExpanded($node) 1 
    }
    $self redrawCanvas
  }

# HtmlDebug::report node
#
#     This method generates and returns an html formated report about 
#     document node $node. It also sets the tree widget so that node 
#     $node is the selected node and it's ancestor nodes are all 
#     expanded.
#
#     If $node is an empty string or is not a node of the current document
#     the currently selected node is used instead. If there is no currently 
#     selected node, or if it is not a node of the current document, the
#     root node of the current document is used instead.
#
  method report {{node ""}} {

  # Template for the node report. The $CONTENT variable is replaced by
  # some content generated by the Tcl code below.
  set Template {
    <html>
      <head>
        <style>
          /* Table display parameters */
          table,th,td { border: 1px solid; }
          td          { padding:0px 15px; }
          table       { margin: 20px; }

          /* Elements of class "code" are rendered in fixed font */
          .code       { font-family: fixed; }

        </style>
      </head>
      <body>
        $CONTENT
      </body>
    </html>
  }

  catch {$mySelected dynamic clear focus}
  if {$node != "" && [info commands $node] != ""} {
    # The second argument to this proc is a valid node.
    set mySelected $node
    for {set n [$node parent]} {$n != ""} {set n [$n parent]} {
      set myExpanded($n) 1
    }
    $self redrawCanvas

  }
  if {$mySelected == "" || [info commands $mySelected] == ""} {
    set mySelected [$myHtml node]
    set mySearchResults {}
  }
  set node $mySelected
  catch {$mySelected dynamic set focus}
  if {$node eq ""} return ""

  set doc {}

    if {[$node tag] == ""} {
        append doc "<h1>Text</h1>"
        append doc "<p>Tcl command: <span class=\"code\">\[$node\]</span>"
        set text [string map {< &lt; > &gt;} [$node text]]
        set tokens [string map {< &lt; > &gt;} [$node text -tokens]]
        append doc [subst {
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

        set after_tbl ""
        catch {
            set rows ""
            foreach {p v} [prop_compress [$node prop after]] {
                append rows "<tr><td>$p<td>$v"
            }
            set after_tbl "
              <table class=computed>
                <tr><th colspan=2>:after Element Properties
                $rows
              </table>
            "
        }

        set attribute_rows ""
        foreach {p v} [$node attr] {
            append attribute_rows "<tr><td>$p<td>$v"
        }
        
        append doc [subst {
            <h1>&lt;[$node tag]&gt;</h1>
            <p>Tcl command: <span class="code">\[$node\]</span>
            <p>Replacement: <span class="code">\[[$node replace]\]</span>
            <p>Note: Fields 'margin', 'padding' and sometimes 'position' 
               contain either one or four length values. If there is only
	       one value, then this is the value for the associated top,
               right, bottom and left lengths. If there are four values, they
               are respectively the top, right, bottom, and left lengths.
            </p>

            <table class=computed>
                <tr><th colspan=2>Computed Properties
                $property_rows
            </table>

            $after_tbl

            <table class=attributes>
                <tr><th colspan=2>Attributes
                $attribute_rows

            </table>
        }]
    }

    append doc {<table><tr><th>CSS Dynamic Conditions}
    foreach condition [$node dynamic conditions] {
        set c [string map {< &lt; > &gt;} $condition]
        append doc "<tr><td>$c"
    }
    append doc {</table>}

    if {[info exists myLayoutEngineLog($node)]} {
        append doc {<table class=layout_engine><tr><th>Layout Engine}
        foreach entry $myLayoutEngineLog($node) {
            set entry [regsub {[A-Za-z]+\(\)} $entry <b>&</b>]
            append doc "    <tr><td>$entry\n"
        }
        append doc "</table>\n"
    }
    if {[info exists myStyleEngineLog($node)]} {
        append doc {<table class=style_engine><tr><th>Style Engine}
        foreach entry $myStyleEngineLog($node) {
            if {[string match matches* $entry]} {
                append doc "    <tr><td><b>$entry<b>\n"
            } else {
                append doc "    <tr><td>$entry\n"
            }
        }
        append doc "</table>\n"
    }

    set CONTENT $doc
    set doc [subst $Template]
    return $doc
  }

  # Logcmd
  #
  #     This method is registered as the -logcmd callback with the widget
  #     when it is running the "Rerender with logging" function.
  #
  method Logcmd {subject message} {
    set arrayvar ""
    switch -- $subject {
      STYLEENGINE {
        set arrayvar myStyleEngineLog
      }
      LAYOUTENGINE {
        set arrayvar myLayoutEngineLog
      }
    }
    if {$arrayvar != ""} {
      if {$message == "START"} {
        #unset -nocomplain $arrayvar
        unset -nocomplain $arrayvar
      } else {
        set idx [string first " " $message]
        set node [string range $message 0 [expr $idx-1]]
        set msg  [string range $message [expr $idx+1] end]
        lappend ${arrayvar}($node) $msg
      }
    }
  }

}
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Document template for the debugging window report.
#
#--------------------------------------------------------------------------

proc tclInputHandler {node} {
  set widget [$node attr -default "" widget]
  if {$widget != ""} {
    $node replace $widget -configurecmd [list tclInputConfigure $widget]
  }
  set tcl [$node attr -default "" tcl]
  if {$tcl != ""} {
    eval $tcl
  }
}
proc tclInputConfigure {widget args} {
  if {[catch {set font [$widget cget -font]}]} {return 0}
  set descent [font metrics $font -descent]
  set ascent  [font metrics $font -ascent]
  set drop [expr ([winfo reqheight $widget] + $descent - $ascent) / 2]
  return $drop
}


# ::hv3::use_tclprotocol
#
# Configure the -requestcmd option of the hv3 widget to interpret
# URI's as tcl scripts.
#
proc ::hv3::use_tclprotocol {hv3} {
  $hv3 configure -requestcmd ::hv3::tclprotocol -cancelrequestcmd ""
}
proc ::hv3::tclprotocol {handle} {
  set uri [$handle uri]
  set cmd [string range [$handle uri] 7 end]
  $handle append [eval $cmd]
  $handle finish
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

    if {$p(position) ne "static"} {
        lappend p(position) $p(top)
        set v $p(top)
        if {$v ne $p(right) || $v eq $p(bottom) || $v eq $p(left)} {
            lappend p(position) $p(right)
            lappend p(position) $p(bottom)
            lappend p(position) $p(left)
        }
    }
    unset p(top) p(right) p(bottom) p(left)

    set ret [list               \
        {<b>Handling} ""        \
        display  $p(display)    \
        float    $p(float)      \
        clear    $p(clear)      \
        position $p(position)   \
        {<b>Dimensions} ""      \
        width $p(width)         \
        height $p(height)       \
        margin $p(margin)       \
        padding $p(padding)     \
        {<b>Other} ""           \
    ]

    foreach {a b} $ret { 
      unset -nocomplain p($a)
    }

    set keys [lsort [array names p]]
    foreach r $keys {
      lappend ret $r $p($r)
    }
    return $ret
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
