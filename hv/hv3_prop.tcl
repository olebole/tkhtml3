
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
    variable myHtml              ;# Name of html widget

    # Debugging window widgets:
    variable myTopLevel          ;# Name of top-level window for debugger
    variable myTree              ;# Name of canvas widget
    variable myRelayout          ;# Name of re-layout button
    variable mySearchField       ;# Name of "search for node" entry widget

    variable myStyleEngineLog
    variable myLayoutEngineLog

    variable mySearchResults ""

    method drawSubTree    {node x y}
    method browseNode {node}
    constructor           {HTML} {}
    destructor            {}
  }

  # These are not actually part of the public interface, but they must
  # be declared public because they are invoked by widget callback scripts
  # and so on. They are really private methods.
  public {
    method rerender       {}
    method searchNode     {}
    method toggleExpanded {node}
    method report         {{node ""}}
    method configureTree  {}
    method logcmd         {args}
  }
}
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Document template for the debugging window document.
#
set HtmlDebug::Template {
  <html>
    <head>
      <style>

        /* Table display parameters */
        table,th,td { border: 1px solid; }
        td          { padding:0px 15px; }
        table       { margin: 20px; }

        /* Do not display borders for <table class="noborder"> elements */
        .noborder, .noborder>tr>td, .noborder>tr>th { border: none; }

        /* Elements of class "code" are rendered in fixed font */
        .code       { font-family: fixed; }
  
        /* Border for elements of class "border" */
        .border {
            border: solid 2px;
            border-color: grey60 grey25 grey25 grey60;
            margin: 0 0.25;
        }

      </style>
    </head>
    <body>
      <h1><center>Tkhtml Debugging Interface</center></h1>

      <!-- Apart from the heading, the entire document is encapsulated 
           within a borderless table. The table consists of two cells 
           in a single row. The left cell contains the "tree" widget. The
           right-hand cell contains everything else.  
      -->
      <table class="noborder">
        <tr>

          <td>    <!-- Left-hand cell - tree widget only -->
            <input 
                class="border" 
                widget="$myTree" 
                tcl="$this configureTree" 
                style="float:left"
            />

          <td>    <!-- Right-hand cell - everything else -->

            <!-- The "Re-Render With Logging" and "Search for node" widgets -->
            <div class="border" style="padding: 20px">
              <input widget="$myRelayout"/>
              <p style="margin:20px 0px 0px">
                Search For Node: <input widget="$mySearchField"/>
              </p>
$mySearchResults
            </div>
$CONTENT

          </td>
        </tr>
      </table>
    </body>
  </html>
}
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
  hv3Goto $myTopLevel.hv3 "tcl:///$this report $node"
  wm state $myTopLevel normal
  wm deiconify $myTopLevel
}

swproc tclProtocol {url {script ""} {binary 0}} {
  if {$url=="-reset"} {
    return
  }
  if {$script != ""} {
    set cmd [string range $url 7 end]
    set result [eval $cmd]
    lappend script $result
    eval $script
  }
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

itcl::body HtmlDebug::searchNode {} {
  set selector [$mySearchField get]
  set mySearchResults <ul>
  set nodelist [$myHtml search $selector]
  foreach node $nodelist {
    # set script "after idle {::HtmlDebug::browse $myHtml $node}"
    # set script "::HtmlDebug::browse $myHtml $node"
    set script "$this report $node"
    append mySearchResults "<li><a href=\"tcl:///$script\">"
    append mySearchResults "$node"
    append mySearchResults "</li>"
  }
  append mySearchResults </ul>
  HtmlDebug::browse $myHtml [lindex $nodelist 0]
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

# HtmlDebug constructor
#
#     Create a new html widget debugger for the widget $HTML.
#
itcl::body HtmlDebug::constructor {HTML} {
  set myHtml $HTML
  set myTopLevel [string map {: _} ".${this}_toplevel"]

  set myTree $myTopLevel.hv3.html.tree
  set myRelayout $myTopLevel.hv3.html.relayout
  set mySearchField $myTopLevel.hv3.html.search

  toplevel $myTopLevel

  bind $myTopLevel <KeyPress-q> [list destroy $myTopLevel]
  bind $myTopLevel <KeyPress-Q> [list destroy $myTopLevel]

  hv3Init $myTopLevel.hv3
  hv3RegisterProtocol $myTopLevel.hv3 tcl tclProtocol
  $myTopLevel.hv3.html handler node input tclInputHandler
  $myTopLevel.hv3.html configure -width 800
  pack $myTopLevel.hv3 -side right -fill both -expand true

  canvas $myTree -background white -borderwidth 10
  button $myRelayout -text "Re-Render Document With Logging" \
     -command [list $this rerender]

  entry $mySearchField
  bind $mySearchField <Return> [list $this searchNode]

  bind $myTree <4> [list event generate $myTopLevel.hv3.html <4> ]
  bind $myTree <5> [list event generate $myTopLevel.hv3.html <5> ]

  bind $myTopLevel.hv3 <Destroy>    [list itcl::delete object $this]
  set myCommonWidgets($HTML) $this
}

# HtmlDebug Destructor
#
itcl::body HtmlDebug::destructor {} {
  unset myCommonWidgets($myHtml)
}

itcl::body HtmlDebug::configureTree {} {
  $myTree delete all
  if {$mySelected != ""} {
    drawSubTree [$myHtml node] 15 30
    set box [$myTree bbox all]
    set width [lindex $box 2]
    set height [lindex $box 3]
    set width [expr $width < 250 ? 250 : $width]
    set height [expr $height < 250 ? 250 : $height]
    $myTree configure -height $height -width $width
  }
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
#     This method generates an html formated report about node $node.
#
itcl::body HtmlDebug::report {{node ""}} {

    if {$node != "" && [info commands $node] != ""} {
      set mySelected $node
      for {set n [$node parent]} {$n != ""} {set n [$n parent]} {
        set myExpanded($n) 1
      }
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
  HtmlDebug::browse $myHtml
}

itcl::body HtmlDebug::drawSubTree {node x y} {
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
      set iid [$myTree create image $x $y -image ifile -anchor sw]
    } else {
      set iid [$myTree create image $x $y -image idir -anchor sw]
    }

    set tid [$myTree create text [expr $x+$XINCR] $y]
    $myTree itemconfigure $tid -text $label -anchor sw
    if {$mySelected == $node} {
        set bbox [$myTree bbox $tid]
        set rid [
            $myTree create rectangle $bbox -fill skyblue -outline skyblue
        ]
        $myTree lower $rid $tid
    }

    $myTree bind $tid <1> [list HtmlDebug::browse $myHtml $node]
    $myTree bind $iid <1> [list $this toggleExpanded $node]
    
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
                $myTree create line $x1 $y1 $x2 $y1 -fill black
            }
        }

        catch {
          $myTree create line $x1 $y $x1 $y1 -fill black
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
