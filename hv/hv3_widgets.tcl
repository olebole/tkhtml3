namespace eval hv3 { set {version($Id: hv3_widgets.tcl,v 1.26 2006/09/17 08:35:21 danielk1977 Exp $)} 1 }

package require snit
package require Tk

set ::hv3::toolkit Tk
catch {
#  package require tile
#  set ::hv3::toolkit Tile
}

catch { font create Hv3DefaultFont -size 9 -weight normal }

# Basic wrapper widget-names used to abstract Tk and Tile:
#
#    ::hv3::scrollbar
#    ::hv3::button
#    ::hv3::entry
#    ::hv3::text
#    ::hv3::label
#    ::hv3::toolbutton

proc ::hv3::scrollbar {args} {
  if {$::hv3::toolkit eq "Tile"} {
    set w [eval [linsert $args 0 ::ttk::scrollbar]]
  } else {
    set w [eval [linsert $args 0 ::scrollbar]]
    $w configure -highlightthickness 0
    $w configure -borderwidth 1
  }
  return $w
}

proc ::hv3::button {args} {
  if {$::hv3::toolkit eq "Tile"} {
    set w [eval [linsert $args 0 ::ttk::button]]
  } else {
    set w [eval [linsert $args 0 ::button]]
    $w configure -highlightthickness 0
    $w configure -font Hv3DefaultFont -pady 0 -borderwidth 1
  }
  return $w
}

proc ::hv3::entry {args} {
  if {$::hv3::toolkit eq "Tile"} {
    set w [eval [linsert $args 0 ::ttk::entry]]
  } else {
    set w [eval [linsert $args 0 ::entry]]
    $w configure -highlightthickness 0
    $w configure -borderwidth 1
    $w configure -background white
    $w configure -font Hv3DefaultFont
  }
  return $w
}

proc ::hv3::text {args} {
  set w [eval [linsert $args 0 ::text]]
  $w configure -highlightthickness 0
  return $w
}

proc ::hv3::label {args} {
  if {$::hv3::toolkit eq "Tile"} {
    set w [eval [linsert $args 0 ::ttk::label]]
  } else {
    set w [eval [linsert $args 0 ::label]]
    $w configure -highlightthickness 0
    $w configure -font Hv3DefaultFont
  }
  return $w
}

::snit::widget ::hv3::toolbutton {

  component myButton
  component myPopupLabel
  variable  myPopup

  variable myCallback ""

  constructor {args} {

    if {$::hv3::toolkit eq "Tile"} {
      set myButton [::ttk::button ${win}.button -style Toolbutton]
    } else {
      set myButton [::button ${win}.button]

      # Configure Tk presentation options not required for Tile here.
      $myButton configure -highlightthickness 0
      $myButton configure -borderwidth 1
      $myButton configure -relief flat -overrelief raised
    }
    set top [winfo toplevel $myButton]
    set myPopup ${top}[string map {. _} $myButton]
    set myPopupLabel ${myPopup}.label
    frame $myPopup -bg black
    ::label $myPopupLabel -fg black -bg white -font Hv3DefaultFont

    pack $myButton -expand true -fill both
    pack $myPopup.label -padx 1 -pady 1 -fill both -expand true

    $self configurelist $args

    bind $myButton <Enter> [mymethod Enter]
    bind $myButton <Leave> [mymethod Leave]
    bind $myButton <ButtonPress-1> +[mymethod Leave]
  }

  destructor {
    destroy $myPopup
  }

  method Enter {} {
    after 600 [mymethod Popup]
  }

  method Leave {} {
    after cancel [mymethod Popup]
    place forget $myPopup
  }

  method Popup {} {
    set top [winfo toplevel $myButton]
    set x [expr [winfo rootx $myButton] - [winfo rootx $top]]
    set y [expr [winfo rooty $myButton] - [winfo rooty $top]]
    incr y [expr [winfo height $myButton]  / 2]
    if {$x < ([winfo width $top] / 2)} {
      incr x [expr [winfo width $myButton]]
      place $myPopup -anchor w -x $x -y $y
    } else {
      place $myPopup -anchor e -x $x -y $y
    }
  }

  delegate method * to myButton
  delegate option * to myButton

  delegate option -tooltip to myPopupLabel as -text
}

# List of menu widgets used by ::hv3::menu and ::hv3::menu_color
#
set ::hv3::menu_list  [list]
set ::hv3::menu_style [list]

proc ::hv3::menu {args} {
  set w [eval [linsert $args 0 ::menu]]
  if {$::hv3::toolkit eq "Tile"} {
    lappend ::hv3::menu_list $w
    $w configure -borderwidth 1 -tearoff 0 -font TkDefaultFont
  } else {
    $w configure -borderwidth 1 -tearoff 0 -font Hv3DefaultFont
  }
  return $w
}

proc ::hv3::menu_color {} {
  set fg  [style lookup Toolbutton -foreground]
  set afg [style lookup Toolbutton -foreground active]
  set bg  [style lookup Toolbutton -background]
  set abg [style lookup Toolbutton -background active]

  foreach w $::hv3::menu_list {
    catch {
      $w configure -fg $fg -bg $bg -activebackground $abg -activeforeground $afg
    }
  }
}

#---------------------------------------------------------------------------
# ::hv3::scrolledwidget
#
#     Widget to add automatic scrollbars to a widget supporting the
#     [xview], [yview], -xscrollcommand and -yscrollcommand interface (e.g.
#     html, canvas or text).
#
snit::widget ::hv3::scrolledwidget {
  component myWidget
  variable  myVsb
  variable  myHsb

  option -propagate -default 0 -configuremethod set_propagate
  option -scrollbarpolicy -default auto

  method set_propagate {option value} {
    grid propagate $win $value
    set options(-propagate) $value
  }

  constructor {widget args} {
    # Create the three widgets - one user widget and two scrollbars.
    set myWidget [eval [linsert $widget 1 ${win}.widget]]

    set myVsb [::hv3::scrollbar ${win}.vsb -orient vertical] 
    set myHsb [::hv3::scrollbar ${win}.hsb -orient horizontal] 

    grid configure $myWidget -column 0 -row 0 -sticky nsew
    grid columnconfigure $win 0 -weight 1
    grid rowconfigure    $win 0 -weight 1
    grid propagate       $win $options(-propagate)

    # First, set the values of -width and -height to the defaults for 
    # the scrolled widget class. Then configure this widget with the
    # arguments provided.
    $self configure -width  [$myWidget cget -width] 
    $self configure -height [$myWidget cget -height]
    $self configurelist $args

    # Wire up the scrollbars using the standard Tk idiom.
    $myWidget configure -yscrollcommand [mymethod scrollcallback $myVsb]
    $myWidget configure -xscrollcommand [mymethod scrollcallback $myHsb]
    $myVsb    configure -command        [mymethod yview]
    $myHsb    configure -command        [mymethod xview]

    # Propagate events from the scrolled widget to this one.
    bindtags $myWidget [concat [bindtags $myWidget] $win]
  }

  method scrollcallback {scrollbar first last} {
    $scrollbar set $first $last
    set ismapped   [expr [winfo ismapped $scrollbar] ? 1 : 0]

    if {$options(-scrollbarpolicy) eq "auto"} {
      set isrequired [expr ($first == 0.0 && $last == 1.0) ? 0 : 1]
    } else {
      set isrequired $options(-scrollbarpolicy)
    }

    if {$isrequired && !$ismapped} {
      switch [$scrollbar cget -orient] {
        vertical   {grid configure $scrollbar  -column 1 -row 0 -sticky ns}
        horizontal {grid configure $scrollbar  -column 0 -row 1 -sticky ew}
      }
    } elseif {$ismapped && !$isrequired} {
      grid forget $scrollbar
    }
  }

  method widget {} {return $myWidget}

  delegate option -width  to hull
  delegate option -height to hull
  delegate option *       to myWidget
  delegate method *       to myWidget
}

# Wrapper around the ::hv3::scrolledwidget constructor. 
#
# Example usage to create a 400x400 canvas widget named ".c" with 
# automatic scrollbars:
#
#     ::hv3::scrolled canvas .c -width 400 -height 400
#
proc ::hv3::scrolled {widget name args} {
  return [eval [concat ::hv3::scrolledwidget $name $widget $args]]
}
#
# End of "scrolled" implementation
#---------------------------------------------------------------------------

#---------------------------------------------------------------------------
# ::hv3::notebook
#
#     Tabbed notebook widget for hv3 based on the Tile notebook widget. If
#     Tile is not available, ::hv3::pretend_tile_notebook is used instead.
#
# OPTIONS
#
#     -newcmd
#     -switchcmd
#     -delcmd
#     -delbutton
#
# WIDGET COMMAND
#
#     $notebook add ARGS
#     $notebook addbg ARGS
#     $notebook close
#     $notebook current
#     $notebook set_title WIDGET TITLE
#


#
# This class uses vanilla Tk widgets to implement the following subset of
# the Tile ttk::notebook API.
#
#     $notebook select
#     $notebook select WIDGET
#     $notebook forget WIDGET
#     $notebook tabs
#     $notebook add WIDGET -sticky nsew -text TEXT
#     $notebook tab WIDGET -text 
#     $notebook tab WIDGET -text TEXT
#     <<NotebookTabChanged>>
#
snit::widget ::hv3::pretend_tile_notebook {

  variable myWidgets
  variable myTitles
  variable myCurrent 0

  # Height of the tabs part of the window (when visible), in pixels.
  variable myTabHeight

  # Font used for tabs.
  variable myFont Hv3DefaultFont

  # True if an [after idle] callback on the RedrawCallback method is
  # pending. This variable is set by [$self Redraw] and cleared
  # by RedrawCallback.
  variable myRedrawScheduled 0

  delegate option * to hull
  
  constructor {args} {
    set myTabHeight [expr [font metrics $myFont -linespace] * 1.5]
  
    # Create a canvas widget to paint the tabs in
    canvas ${win}.tabs -height $myTabHeight -width 100
    ${win}.tabs configure -borderwidth 0 
    ${win}.tabs configure -highlightthickness 0 
    ${win}.tabs configure -selectborderwidth 0

    # "place" the tabs canvas widget at the top of the parent frame
    place ${win}.tabs -anchor nw -x 0 -y 0 -relwidth 1.0 -height $myTabHeight

    bind ${win}.tabs <Configure> [mymethod Redraw]

    $self configurelist $args
  }

  # add WIDGET
  # 
  #     Add a new widget to the set of tabbed windows.
  method add {widget args} {
    array set A $args
    lappend myWidgets $widget
    lappend myTitles ""
    if {[info exists A(-text)]} {
      lset myTitles end $A(-text)
    }
    $self Redraw
    bind $widget <Destroy> [mymethod forget $widget]
  }

  # forget WIDGET
  # 
  #     Remove $widget from the set of tabbed windows. Regardless of
  #     whether or not $widget is the current tab, a <<NotebookTabChanged>>
  #     event is generated.
  method forget {widget} {
    set idx [lsearch $myWidgets $widget]
    if {$idx < 0} { error "$widget is not managed by $self" }

    place forget $widget
    bind $widget <Destroy> ""

    set myWidgets [lreplace $myWidgets $idx $idx]
    set myTitles  [lreplace $myTitles $idx $idx]

    if {$myCurrent == [llength $myWidgets]} {
      incr myCurrent -1
    }
    after idle [list event generate $self <<NotebookTabChanged>>]
    $self Redraw
  }

  # select ?WIDGET?
  # 
  #     If an argument is provided, make that widget the current tab.
  #     Return the current tab widget (a copy of the argument if one
  #     was provided).
  method select {{widget ""}} {
    if {$widget ne ""} {
      set idx [lsearch $myWidgets $widget]
      if {$idx < 0} { error "$widget is not managed by $self" }
      if {$myCurrent != $idx} {
        set myCurrent $idx
        $self Redraw
        after idle [list event generate $self <<NotebookTabChanged>>]
      }
    }
    return [lindex $myWidgets $myCurrent]
  }

  # tab WIDGET ?options?
  #
  #     The only option recognized is the -text option. It sets the
  #     title for the specified tabbed widget.
  # 
  method tab {widget -text args} {
    set idx [lsearch $myWidgets $widget]
    if {$idx < 0} { error "$widget is not managed by $self" }

    if {[llength $args] ne 0} {
      lset myTitles $idx [lindex $args 0]
      $self Redraw
    }
    return [lindex $myTitles $idx]
  }

  method tabs {} {
    return $myWidgets
  }

  method Redraw {} {
    if {$myRedrawScheduled == 0} {
      set myRedrawScheduled 1
      after idle [mymethod RedrawCallback]
    }
  }

  method RedrawCallback {} {

    set iPadding  2
    set iDiagonal 2

    set iCanvasWidth [winfo width ${win}.tabs]

    # Delete the existing canvas items. This proc draws everything 
    # from scratch.
    ${win}.tabs delete all

    # If the myCurrent variable is less than 0, the notebook widget is
    # empty. There are no tabs to draw in this case.
    if {$myCurrent < 0} return

    # Make sure the $myCurrent widget is the displayed tab.
    set c [lindex $myWidgets $myCurrent]
    place $c                              \
        -x 0 -y [expr $myTabHeight - 0]   \
        -relwidth 1.0 -relheight 1.0      \
        -height [expr -1 * $myTabHeight]  \
        -anchor nw

    # And unmap all other tab windows.
    foreach w $myWidgets {
      if {$w ne $c} {place forget $w}
    }

    # Variable $iAggWidth stores the aggregate width of all the tabs.
    set iAggWidth 0
    foreach t $myTitles {
      incr iAggWidth [
          expr [font measure $myFont $t] + ($iPadding + $iDiagonal) * 2
      ]
    }

    if {$iCanvasWidth < $iAggWidth} {
        set fBudget [expr double($iCanvasWidth) / double($iAggWidth)]
    } else {
        set fBudget 1.0
    }

    set idx 0
    set yt [expr 0.5 * ($myTabHeight + [font metrics $myFont -linespace])]
    set x 1
    foreach title $myTitles {

      set zTitle $title

      set width    [font measure $myFont $title]
      set iTabWidth [expr $iPadding * 2 + $iDiagonal * 2 + $width + 1]
      if {$fBudget < 1.0} {
        set iTabWidth [expr int(double($iTabWidth) * $fBudget)]
        set width [expr $iTabWidth - ($iPadding + $iDiagonal) * 2 - 1]
        if {$width < 0} {
          set zTitle ""
        } else {
          set w [expr $width + $iPadding]
          for {set n 1} {$n < [string length $zTitle]} {incr n} {
            if {[font measure $myFont [string range $zTitle 0 $n]] > $w} {
              break;
            }
          }
          set zTitle [string range $zTitle 0 [expr $n - 1]]
        }
      }

      set x2 [expr $x + $iDiagonal]
      set x3 [expr $x2 + $width + ($iPadding * 2)]
      set x4 [expr $x3 + $iDiagonal]

      set y1 [expr $myTabHeight - 0]
      set y2 [expr $iDiagonal + 1]
      set y3 1

      set id [${win}.tabs create polygon \
          $x $y1 $x $y2 $x2 $y3 $x3 $y3 $x4 $y2 $x4 $y1]

      set id2 [${win}.tabs create text [expr $x2 + $iPadding] $yt]
      ${win}.tabs itemconfigure $id2 -anchor sw -text $zTitle -font $myFont

      if {$idx == $myCurrent} {
        set yb [expr $y1 - 1]
        ${win}.tabs itemconfigure $id -fill #d9d9d9
        ${win}.tabs create line 0 $yb $x $yb -fill white -tags whiteline
        ${win}.tabs create line $x4 $yb $iCanvasWidth $yb -tags whiteline
        ${win}.tabs itemconfigure whiteline -fill white
      } else {
        ${win}.tabs itemconfigure $id -fill #c3c3c3
        set cmd [list ${win}.tabs itemconfigure $id -fill]
        foreach i [list $id $id2] {
          ${win}.tabs bind $i <Enter> [concat $cmd #ececec]
          ${win}.tabs bind $i <Leave> [concat $cmd #c3c3c3]
          ${win}.tabs bind $i <1> [mymethod select [lindex $myWidgets $idx]]
        }
      }

      ${win}.tabs create line $x $y1 $x $y2 $x2 $y3 $x3 $y3 -fill white
      ${win}.tabs create line $x3 $y3 $x4 $y2 $x4 $y1 -fill black

      incr x $iTabWidth
      incr idx
    }

    ${win}.tabs raise whiteline
    set myRedrawScheduled 0
  }

}

snit::widget ::hv3::notebook {

  option -newcmd    -default ""
  option -switchcmd -default ""
  option -delcmd    -default ""
  option -delbutton -default ""

  variable myNextId 0
  variable myPendingTitle ""

  variable myOnlyTab ""
  variable myOnlyTitle ""

  method Switchcmd {} {
    if {$options(-switchcmd) ne ""} {
      eval [linsert $options(-switchcmd) end [$self current]]
      $self WorldChanged
    }
  }

  method WorldChanged {} {
    set dummy ${win}.notebook.dummy

    set nTab [llength [${win}.notebook tabs]]
    if {$myOnlyTab ne ""} { incr nTab }

    if {[lsearch [${win}.notebook tabs] $dummy] >= 0} {
      incr nTab -1
    }

    if {$nTab > 1} {
      if {$myOnlyTab ne ""} {
        place forget $myOnlyTab
        ${win}.notebook add $myOnlyTab -sticky ewns -text $myOnlyTitle
        catch { ${win}.notebook forget $dummy }
        set myOnlyTab ""
        set myOnlyTitle ""

        set tab1 [lindex [${win}.notebook tabs] 0]
        set text1 [${win}.notebook tab $tab1 -text]
        ${win}.notebook forget $tab1
        ${win}.notebook add $tab1
        ${win}.notebook tab $tab1 -text $text1
      }
      $options(-delbutton) configure -state normal
    } else {
      if {$myOnlyTab eq ""} {
        set myOnlyTab [${win} current]
       
        catch { canvas $dummy -width 0 -height 0 -bg blue }
        catch { ${win}.notebook add ${win}.notebook.dummy -state hidden }

        set myOnlyTitle [${win}.notebook tab $myOnlyTab -text]
        ${win}.notebook forget $myOnlyTab
        raise $myOnlyTab
        place $myOnlyTab -relheight 1.0 -relwidth 1.0
      }
      $options(-delbutton) configure -state disabled
    }
  }

  method close {} {
    destroy [$self current]
  }

  constructor {args} {
    $self configurelist $args
    if {$::hv3::toolkit eq "Tile"} {
      ::ttk::notebook ${win}.notebook -width 700 -height 500 
    } else {
      ::hv3::pretend_tile_notebook ${win}.notebook -width 700 -height 500
    }
    bind ${win}.notebook <<NotebookTabChanged>> [list $self Switchcmd]
    pack ${win}.notebook -fill both -expand true
  }

  method Addcommon {switchto args} {
    set widget ${win}.notebook.tab_[incr myNextId]

    set myPendingTitle ""
    eval [concat [linsert $options(-newcmd) 1 $widget] $args]
    ${win}.notebook add $widget -sticky ewns -text Blank
    if {$myPendingTitle ne ""} {$self set_title $widget $myPendingTitle}

    if {$switchto} {
      ${win}.notebook select $widget
      $self Switchcmd
      catch {${win}.notebook select $widget}
    } else {
      $self WorldChanged
    }

    return $widget
  }

  method addbg {args} {
      eval [concat $self Addcommon 0 $args]
  }

  method add {args} {
      eval [concat $self Addcommon 1 $args]
  }

  method set_title {widget title} {
    if {$widget eq $myOnlyTab} {
      set myOnlyTitle $title
    } elseif {[catch {${win}.notebook tab $widget -text $title}]} {
      set myPendingTitle $title
    }
  }

  method current {} {
    if {$myOnlyTab ne ""} {return $myOnlyTab}

    if {0 == [catch {${win}.notebook select} current]} {
      return $current
    }
    return [lindex [${win}.notebook tabs] [${win}.notebook index current]]
  }
}
# End of notebook implementation.
#---------------------------------------------------------------------------

#---------------------------------------------------------------------------
# ::hv3::walkTree
# 
#     This proc is used for depth first traversal of the document tree 
#     headed by the argument node. 
#
#     Example:
#
#         ::hv3::walkTree [.html node] N {
#           puts "Type of node: [$N tag]"
#         }
#
#     If the body of the loop executes a [continue], then the current iteration
#     is terminated and the body is not executed for any of the current nodes
#     children. i.e. [continue] prevents descent of the tree.
#
proc ::hv3::walkTree {N varname body} {
  set level "#[expr [info level] - 1]"
  ::hv3::walkTree2 $N $body $varname $level
}
proc ::hv3::walkTree2 {N body varname level} {
  uplevel $level [list set $varname $N]
  set rc [catch {uplevel $level $body} msg] 
  switch $rc {
    0 {           ;# OK
      foreach n [$N children] {
        ::hv3::walkTree2 $n $body $varname $level
      }
    }
    1 {           ;# ERROR
      error $msg
    }
    2 {           ;# RETURN
      return $msg
    }
    3 {           ;# BREAK
      error "break from within ::hv3::walkTree"
    }
    4 {           ;# CONTINUE
      # Do nothing. Do not descend the tree.
    }
  }
}
#---------------------------------------------------------------------------

#---------------------------------------------------------------------------
# ::hv3::resolve_uri
#
proc ::hv3::resolve_uri {base relative} {
  set obj [::hv3::uri %AUTO% $base]
  $obj load $relative
  set ret [$obj get]
  $obj destroy
  return $ret
}
#---------------------------------------------------------------------------

snit::widget ::hv3::googlewidget {

  option -getcmd  -default ""
  option -config  -default ""
  option -initial -default Google

  delegate option -borderwidth to hull
  delegate option -relief      to hull

  variable myEngine 

  constructor {args} {
    $self configurelist $args

    set myEngine $options(-initial)

    ::hv3::label $win.label -text "Search:"
    ::hv3::entry $win.entry -width 30
    ::hv3::button $win.close -text dismiss -command [list destroy $win]

    set w ${win}.menubutton
    menubutton $w -textvar [myvar myEngine] -indicatoron 1 -menu $w.menu
    ::hv3::menu $w.menu
    $w configure -borderwidth 1 -font Hv3DefaultFont -relief raised -pady 2
    foreach {label uri} $options(-config) {
      $w.menu add radiobutton -label $label -variable [myvar myEngine]
    }

    pack $win.label -side left
    pack $win.entry -side left
    pack $w -side left
    pack $win.close -side right

    bind $win.entry <Return>       [mymethod Search]

    # Propagate events that occur in the entry widget to the 
    # ::hv3::findwidget widget itself. This allows the calling script
    # to bind events without knowing the internal mega-widget structure.
    # For example, the hv3 app binds the <Escape> key to delete the
    # findwidget widget.
    #
    bindtags $win.entry [concat [bindtags $win.entry] $win]
  }

  method Search {} {
    array set a $options(-config)

    set search [::hv3::escape_string [${win}.entry get]]
    set query [format $a($myEngine) $search]

    if {$options(-getcmd) ne ""} {
      set script [linsert $options(-getcmd) end $query]
      eval $script
    }
  }
}

snit::widget ::hv3::findwidget {
  variable myHv3              ;# The HTML widget

  variable myNocaseVar 1      ;# Variable for the "Case insensitive" checkbox 
  variable myEntryVar  ""     ;# Variable for the entry widget
  variable myCaptionVar ""    ;# Variable for the label widget
  
  variable myCurrentHit -1
  variable myCurrentList ""

  delegate option -borderwidth to hull
  delegate option -relief      to hull

  constructor {hv3 args} {
    set myHv3 $hv3

    ::hv3::label $win.label -text "Search for text:"
    ::hv3::entry $win.entry -width 30
    ::hv3::label $win.num_results -textvar [myvar myCaptionVar]

    checkbutton $win.check_nocase -variable [myvar myNocaseVar] -pady 0
    ::hv3::label $win.check_nocase_label -text "Case Insensitive"

    ::hv3::button $win.close -text dismiss -command [list destroy $win]
 
    $win.entry configure -textvar [myvar myEntryVar]
    trace add variable [myvar myEntryVar] write [mymethod DynamicUpdate]
    trace add variable [myvar myNocaseVar] write [mymethod DynamicUpdate]

    bind $win.entry <Return>       [mymethod Return 1]
    bind $win.entry <Shift-Return> [mymethod Return -1]
    focus $win.entry

    # Propagate events that occur in the entry widget to the 
    # ::hv3::findwidget widget itself. This allows the calling script
    # to bind events without knowing the internal mega-widget structure.
    # For example, the hv3 app binds the <Escape> key to delete the
    # findwidget widget.
    #
    bindtags $win.entry [concat [bindtags $win.entry] $win]

    pack $win.label -side left
    pack $win.entry -side left
    pack $win.check_nocase -side left
    pack $win.check_nocase_label -side left
    pack $win.close -side right
    pack $win.num_results -side right -fill x
  }

  method lazymoveto {n1 i1 n2 i2} {
    set nodebbox [$myHv3 text bbox $n1 $i1 $n2 $i2]
    set docbbox  [$myHv3 bbox]

    set docheight "[lindex $docbbox 3].0"

    set ntop    [expr ([lindex $nodebbox 1].0 - 30.0) / $docheight]
    set nbottom [expr ([lindex $nodebbox 3].0 + 30.0) / $docheight]
 
    set sheight [expr [winfo height $myHv3].0 / $docheight]
    set stop    [lindex [$myHv3 yview] 0]
    set sbottom [expr $stop + $sheight]


    if {$ntop < $stop} {
      $myHv3 yview moveto $ntop
    } elseif {$nbottom > $sbottom} {
      $myHv3 yview moveto [expr $nbottom - $sheight]
    }
  }

  # Dynamic update proc.
  method UpdateDisplay {nMaxHighlight} {
    $myHv3 tag delete findwidget
    $myHv3 tag delete findwidgetcurrent
    set myCaptionVar ""

    set searchtext $myEntryVar
    if {[string length $searchtext] == 0} return
    set doctext [$myHv3 text text]

    if {$myNocaseVar} {
      set doctext [string tolower $doctext]
      set searchtext [string tolower $searchtext]
    }

    set iFin 0
    set lMatch [list]

    while {[set iStart [string first $searchtext $doctext $iFin]] >= 0} {
      set iFin [expr $iStart + [string length $searchtext]]
      lappend lMatch $iStart $iFin
    }
    set nMatch [expr [llength $lMatch] / 2]
    set lMatch [lrange $lMatch 0 [expr $nMaxHighlight * 2 - 1]]
    set nHighlight [expr [llength $lMatch] / 2]
    set myCaptionVar "(highlighted $nHighlight of $nMatch hits)"

    set matches [list]
    if {[llength $lMatch] > 0} {
      set matches [eval [concat $myHv3 text index $lMatch]]
      foreach {n1 i1 n2 i2} $matches {
        $myHv3 tag add findwidget $n1 $i1 $n2 $i2
      }
  
      $myHv3 tag configure findwidget -bg purple -fg white
        $self lazymoveto                            \
            [lindex $matches 0] [lindex $matches 1] \
            [lindex $matches 2] [lindex $matches 3]
    }

    set myCurrentList $matches
  }

  method DynamicUpdate {args} {
    set myCurrentHit -1
    $self UpdateDisplay 42
  }
  
  method Escape {} {
    # destroy $win
  }
  method Return {dir} {
    if {$myCurrentHit < 0} {
      $self UpdateDisplay 100000
    }
    incr myCurrentHit $dir

    set nHit [expr [llength $myCurrentList] / 4]
    if {$myCurrentHit < 0 || $nHit <= $myCurrentHit} {
      tk_messageBox -message "The text you entered was not found" -type ok
      incr myCurrentHit [expr -1 * $dir]
      return
    }
    set myCaptionVar "Hit [expr $myCurrentHit + 1] / $nHit"

    set n1 [lindex $myCurrentList [expr $myCurrentHit * 4]]
    set i1 [lindex $myCurrentList [expr $myCurrentHit * 4 + 1]]
    set n2 [lindex $myCurrentList [expr $myCurrentHit * 4 + 2]]
    set i2 [lindex $myCurrentList [expr $myCurrentHit * 4 + 3]]

    $self lazymoveto $n1 $i1 $n2 $i2
    $myHv3 tag delete findwidgetcurrent
    $myHv3 tag add findwidgetcurrent $n1 $i1 $n2 $i2
    $myHv3 tag configure findwidgetcurrent -bg black -fg yellow
  }

  destructor {
    # Delete any tags added to the hv3 widget. Do this inside a [catch]
    # block, as it may be that the hv3 widget has itself already been
    # destroyed.
    catch {
      $myHv3 tag delete findwidget
      $myHv3 tag delete findwidgetcurrent
    }
    trace remove variable [myvar myEntryVar] write [mymethod UpdateDisplay]
    trace remove variable [myvar myNocaseVar] write [mymethod UpdateDisplay]
  }
}

snit::widget ::hv3::stylereport {
  hulltype toplevel

  variable myHtml ""

  constructor {html} {
    set hv3 ${win}.hv3
    set myHtml $html
    set hv3 ${win}.hv3

    ::hv3::hv3 $hv3
    $hv3 configure -requestcmd [mymethod Requestcmd] -width 600 -height 400

    # Create an ::hv3::findwidget so that the report is searchable.
    # In this case the findwidget should always be visible, so remove
    # the <Escape> binding that normally destroys the widget.
    ::hv3::findwidget ${win}.find $hv3
    bind ${win} <Escape> [list destroy $win]
    
    pack ${win}.find -side bottom -fill x
    pack $hv3 -fill both -expand true
  }
  
  method update {} {
    set hv3 ${win}.hv3
    $hv3 reset
    $hv3 goto report:
  }

  method Requestcmd {downloadHandle} {
    $downloadHandle append "<html><body>[$myHtml stylereport]"
    $downloadHandle finish
  }
}

