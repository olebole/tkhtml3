
###########################################################################
# hv3_prop.tcl --
#
#     This file contains code to implement the Tkhtml debugging interface.
#

package require Itcl
package require Tk

#--------------------------------------------------------------------------
# class HtmlDebug --
#
#     This class encapsulates code used for debugging the Html widget. 
#     It manages a gui which can be used to investigate the structure and
#     Tkhtml's handling of a currently loaded Html document.
#
#
itcl::class HtmlDebug {

  # Public interface
  public {
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
    proc browse {HTML {node ""}}
  }

  # Class internals
  private {
    common Template

    common myCommonWidgets       ;# Map from <html-widget> -> HtmlDebug obj
    variable mySelected ""       ;# Currently selected node
    variable myExpanded          ;# Array. Entries are present for exp. nodes
    variable myHtml              ;# Name of html widget being debugged

    # Debugging window widgets:
    #
    #   $myTopLevel.tree_frame              ;# frame
    #     $myTopLevel.tree_frame.canvas     ;# canvas
    #     $myTopLevel.tree_frame.vsb        ;# scrollbar
    #     $myTopLevel.tree_frame.hsb        ;# scrollbar
    #   $myTopLevel.header                  ;# Hv3 mega-widget
    #     $myTopLevel.header.html.relayout  ;# button
    #     $myTopLevel.header.html.search    ;# entry
    #   $myTopLevel.report                  ;# Hv3 mega-widget
    variable myTopLevel          ;# Name of top-level window for debugger

    variable myStyleEngineLog
    variable myLayoutEngineLog

    variable mySearchResults ""

    method drawSubTree    {node x y}
    method redrawCanvas {}
    method browseNode {node}
    constructor           {HTML} {}
    destructor            {}
  }

  # These are not actually part of the public interface, but they must
  # be declared public because they are invoked by widget callback scripts
  # and so on. They are really private methods.
  public {
    method rerender       {}
    method searchNode     {{idx 0}}
    method toggleExpanded {node}
    method report         {{node ""}}
    method configureTree  {}
    method logcmd         {args}
  }
}
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Document template for the debugging window report.
#
#--------------------------------------------------------------------------

# HtmlDebug::browse + HtmlDebug::browseNode
#
itcl::body HtmlDebug::browse {HTML {node ""}} {
  if {![info exists myCommonWidgets($HTML)]} {
    HtmlDebug #auto $HTML
  }
  return [$myCommonWidgets($HTML) browseNode $node]
}

itcl::body HtmlDebug::browseNode {node} {
  wm state $myTopLevel normal
  wm deiconify $myTopLevel
  hv3Goto $myTopLevel.report "tcl:///:$this report $node" -noresolve
}

proc tclProtocol {downloadHandle} {
  set cmd [string range [$downloadHandle uri] 7 end]
  $downloadHandle append [eval $cmd]
  $downloadHandle finish
}

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

itcl::body HtmlDebug::logcmd {subject message} {
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
      unset -nocomplain $arrayvar
    } else {
      set idx [string first " " $message]
      set node [string range $message 0 [expr $idx-1]]
      set msg  [string range $message [expr $idx+1] end]
      lappend ${arrayvar}($node) $msg
    }
  }
}

itcl::body HtmlDebug::searchNode {{idx 0}} {
  set Template {
    <html><head>
      <style>
        td { padding-left: 10px ; padding-right: 10px }
      </style>
    </head><body><center>
      <h1>Tkhtml Document Tree Browser</h1>
      <input widget="relayout">
      <p>
        Search for node: <input widget="search">
      </p>
      $search_results
    </center></body></html>
  }

  set search_results {}
  set nodelist [list]
  set selector [$myTopLevel.header.html.search get]
  if {$selector != ""} {
    set search_results <table><tr>
    set nodelist [$myHtml search $selector]
    set ii 0
    foreach node $nodelist {
      # set script "after idle {::HtmlDebug::browse $myHtml $node}"
      # set script "::HtmlDebug::browse $myHtml $node"
      set script "$this searchNode $ii"
      append search_results "<td><a href=\"tcl:///:$script\">$node</a>"
      incr ii
      if {($ii % 5)==0} {
        append search_results "</tr><tr>"
      }
    }
    append search_results </table>
  }

  $myTopLevel.header.html reset
  $myTopLevel.header.html parse -final [subst $Template]
  foreach node [$myTopLevel.header.html search {input[widget]}] {
    set widget [$node attr widget]
    $node replace $myTopLevel.header.html.$widget -deletecmd {}
  }

  set h [lindex [$myTopLevel.header.html bbox] 3]
  if {$selector != ""} {
    set mh [expr [winfo height $myTopLevel]/2]
    if {$h > $mh} {set h $mh}
  }
  $myTopLevel.header.html configure -height $h

  if {[llength $nodelist]>$idx} {
    HtmlDebug::browse $myHtml [lindex $nodelist $idx]
  }

  return ""
}

# HtmlDebug::rerender
#
#     This method is called to force the attached html widget to rerun the
#     style and layout engines.
#
itcl::body HtmlDebug::rerender {} {
  $myHtml configure -logcmd [list $this logcmd]
  $myHtml style ""
  after idle [list ::HtmlDebug::browse $myHtml $mySelected]
  after idle [list $myHtml configure -logcmd {}]
}

proc wireup_scrollbar {x_or_y widget scrollbar} {
  set scrollcommand "-${x_or_y}scrollcommand"
  set view "${x_or_y}view"
  $widget configure $scrollcommand [list $scrollbar set]
  $scrollbar configure -command [list $widget $view]
}

# HtmlDebug constructor
#
#     Create a new html widget debugger for the widget $HTML.
#
itcl::body HtmlDebug::constructor {HTML} {
  set myHtml $HTML
  set myTopLevel [string map {: _} ".${this}_toplevel"]

  # Top level window
  toplevel $myTopLevel -height 600
  bind $myTopLevel <KeyPress-q>  [list destroy $myTopLevel]
  bind $myTopLevel <KeyPress-Q>  [list destroy $myTopLevel]

  # Header html widget
  hv3Init  $myTopLevel.header
  hv3RegisterProtocol $myTopLevel.header tcl tclProtocol
  # $myTopLevel.header.html configure -height 100
  pack forget $myTopLevel.header.status 
  set b [button $myTopLevel.header.html.relayout]
  $b configure -text "Re-Render Document With Logging" 
  $b configure -command [list $this rerender]
  set e [entry $myTopLevel.header.html.search]
  bind $e <Return> [list $this searchNode]
  $this searchNode

  # Report html widget
  hv3Init  $myTopLevel.report
  hv3RegisterProtocol $myTopLevel.report tcl tclProtocol
  $myTopLevel.report.html configure -width 300 -height 500
  pack forget $myTopLevel.report.status 

  # Tree canvas widget and scrollbars
  frame $myTopLevel.tree_frame
  canvas $myTopLevel.tree_frame.canvas -background white -borderwidth 10
  $myTopLevel.tree_frame.canvas configure -width 350
  scrollbar $myTopLevel.tree_frame.hsb -orient horizontal
  scrollbar $myTopLevel.tree_frame.vsb
  pack $myTopLevel.tree_frame.hsb    -fill x -side bottom
  pack $myTopLevel.tree_frame.canvas -fill both -expand true -side left
  pack $myTopLevel.tree_frame.vsb    -fill y -side right

  wireup_scrollbar x $myTopLevel.tree_frame.canvas $myTopLevel.tree_frame.hsb
  wireup_scrollbar y $myTopLevel.tree_frame.canvas $myTopLevel.tree_frame.vsb

  pack $myTopLevel.header     -side top -fill x
  pack $myTopLevel.report     -side right -fill both -expand true
  pack $myTopLevel.tree_frame -side left -fill both -expand true

  bind $myTopLevel.report <Destroy> [list itcl::delete object $this]
  set myCommonWidgets($HTML) $this
}

# HtmlDebug Destructor
#
itcl::body HtmlDebug::destructor {} {
  unset myCommonWidgets($myHtml)
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
itcl::body HtmlDebug::report {{node ""}} {

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

  if {$node != "" && [info commands $node] != ""} {
    # The second argument to this proc is a valid node.
    set mySelected $node
    for {set n [$node parent]} {$n != ""} {set n [$n parent]} {
      set myExpanded($n) 1
    }
    redrawCanvas

  }
  if {$mySelected == "" || [info commands $mySelected] == ""} {
    set mySelected [$myHtml node]
    set mySearchResults {}
  }
  set node $mySelected
  if {$node eq ""} return ""

  set doc {}

    if {[$node tag] == ""} {
        append doc "<h1>Text</h1>"
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
        set attribute_rows ""
        foreach {p v} [$node attr] {
            append attribute_rows "<tr><td>$p<td>$v"
        }
        
        append doc [subst {
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

    append doc {<table><tr><th>CSS Dynamic Conditions}
    foreach condition [$node dynamic conditions] {
        set c [string map {< &lt; > &gt;} $condition]
        append doc "<tr><td>$c"
    }
    append doc {</table>}

    if {[info exists myLayoutEngineLog($node)]} {
        append doc {<table class=layout_engine><tr><th>Layout Engine}
        foreach entry $myLayoutEngineLog($node) {
            append doc "    <tr><td>$entry\n"
        }
        append doc "</table>\n"
    }
    if {[info exists myStyleEngineLog($node)]} {
        append doc {<table class=style_engine><tr><th>Style Engine}
        foreach entry $myStyleEngineLog($node) {
            append doc "    <tr><td>$entry\n"
        }
        append doc "</table>\n"
    }

    set CONTENT $doc
    set doc [subst $Template]
    return $doc
}

itcl::body HtmlDebug::toggleExpanded {node} {
  if {[info exists myExpanded($node)]} {
    unset myExpanded($node)
  } else {
    set myExpanded($node) 1 
  }
  redrawCanvas
}

itcl::body HtmlDebug::redrawCanvas {} {
  set canvas $myTopLevel.tree_frame.canvas
  $canvas delete all
  drawSubTree [$myHtml node] 15 30
  $canvas configure -scrollregion [$canvas bbox all]
}

itcl::body HtmlDebug::drawSubTree {node x y} {
    set IWIDTH [image width idir]
    set IHEIGHT [image width idir]

    set XINCR [expr $IWIDTH + 2]
    set YINCR [expr $IHEIGHT + 5]

    set tree $myTopLevel.tree_frame.canvas

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
    $tree bind $iid <1> [list $this toggleExpanded $node]
    
    set ret 1
    if {[info exists myExpanded($node)]} {
        set xnew [expr $x+$XINCR]
        for {set i 0} {$i < [$node nChild]} {incr i} {
            set ynew [expr $y + $YINCR*$ret]
            set child [$node child $i]
            set incrret [drawSubTree $child $xnew $ynew]
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
