namespace eval hv3 { set {version($Id: hv3_widgets.tcl,v 1.40 2007/01/08 09:56:16 danielk1977 Exp $)} 1 }

package require snit
package require Tk

set ::hv3::toolkit Tk
catch {
#  package require tile
#  set ::hv3::toolkit Tile
}

#-------------------------------------------------------------------
# Font control:
#
#     Most of the ::hv3::** widgets use the named font 
#     "Hv3DefaultFont". The following two procs, [::hv3::UseHv3Font]
#     and [::hv3::SetFont] deal with configuring the widgets and
#     dynamically modifying the font when required.
#
proc ::hv3::UseHv3Font {widget} {
  $widget configure -font Hv3DefaultFont
}
proc ::hv3::SetFont {font} {
  catch {font delete Hv3DefaultFont}
  eval [linsert $font 0 font create Hv3DefaultFont]

  # WARNING: Horrible, horrible action at a distance...
  catch {.notebook.notebook Redraw}
}
::hv3::SetFont {-size 10}

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
    $w configure -pady 0 -borderwidth 1
    ::hv3::UseHv3Font $w
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
    ::hv3::UseHv3Font $w
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
    ::hv3::UseHv3Font $w
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
    ::label $myPopupLabel -fg black -bg white
    ::hv3::UseHv3Font $myPopupLabel

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
    $w configure -borderwidth 1 -tearoff 0 -activeborderwidth 1
    ::hv3::UseHv3Font $w
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

    $myVsb configure -cursor "top_left_arrow"
    $myHsb configure -cursor "top_left_arrow"

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

  variable myWidgets [list]
  variable myTitles  [list]
  variable myCurrent 0

  # Height of the tabs part of the window (when visible), in pixels.
  variable myTabHeight

  # Font used for tabs.
  variable myFont Hv3DefaultFont

  # True if an [after idle] callback on the RedrawCallback method is
  # pending. This variable is set by [$self Redraw] and cleared
  # by RedrawCallback.
  variable myRedrawScheduled 0

  # The two images to use for the small "close-tab" buttons 
  # placed on the tabs themselves.
  variable myCloseTabImage ""
  variable myCloseTabImage2 ""

  delegate option * to hull
  
  constructor {args} {
  
    # Create a canvas widget to paint the tabs in
    canvas ${win}.tabs
    ${win}.tabs configure -borderwidth 0 
    ${win}.tabs configure -highlightthickness 0 
    ${win}.tabs configure -selectborderwidth 0

    bind ${win}.tabs <Configure> [mymethod Redraw]
    $self configurelist $args

    # Set up the two images used for the "close tab" buttons positioned
    # on the tabs themselves. The image data was created by me using an 
    # archaic tool called "bitmap" that was installed with fedora.
    #
    set BitmapData {
      #define closetab_width 14
      #define closetab_height 14
      static unsigned char closetab_bits[] = {
        0xff, 0x3f, 0x01, 0x20, 0x0d, 0x2c, 0x1d, 0x2e, 0x39, 0x27, 0xf1, 0x23,
        0xe1, 0x21, 0xe1, 0x21, 0xf1, 0x23, 0x39, 0x27, 0x1d, 0x2e, 0x0d, 0x2c,
        0x01, 0x20, 0xff, 0x3f
      };
    }
    set myCloseTabImage  [image create bitmap -data $BitmapData -background ""]
    set myCloseTabImage2 [image create bitmap -data $BitmapData -background red]
  }

  destructor {
    image delete $myCloseTabImage
    image delete $myCloseTabImage2
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
    # bind $widget <Destroy> [mymethod forget $widget]
    bind $widget <Destroy> [mymethod HandleDestroy $widget %W]
  }

  method HandleDestroy {widget w} {
    if {$widget eq $w} {$self forget $widget}
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

  method ButtonRelease {tag idx x y} {
    foreach {x1 y1 x2 y2} [${win}.tabs bbox $tag] {}
    if {$x1 <= $x && $x2 >= $x && $y1 <= $y && $y2 >= $y} {
      destroy [lindex $myWidgets $idx]
    }
  }
  method CreateButton {idx x y size} {
    set c ${win}.tabs              ;# Canvas widget to draw on
    set tag [$c create image $x $y -anchor nw]
    $c itemconfigure $tag -image $myCloseTabImage
    $c bind $tag <Enter> [list $c itemconfigure $tag -image $myCloseTabImage2]
    $c bind $tag <Leave> [list $c itemconfigure $tag -image $myCloseTabImage]
    $c bind $tag <ButtonRelease-1> [mymethod ButtonRelease $tag $idx %x %y]
  }

  method RedrawCallback {} {

    set iPadding  2
    set iDiagonal 2
    set iButton   14
    set iCanvasWidth [expr [winfo width ${win}.tabs] - 2]
    set iThreeDots [font measure $myFont "..."]

    set myTabHeight [expr [font metrics $myFont -linespace] * 1.5]
    ${win}.tabs configure -height $myTabHeight -width 100

    # "place" the tabs canvas widget at the top of the parent frame
    place ${win}.tabs -anchor nw -x 0 -y 0 -relwidth 1.0 -height $myTabHeight

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

    # Variable $iAggWidth stores the aggregate requested width of all the 
    # tabs. This loop loops through all the tabs to determine $iAggWidth
    #
    set iAggWidth 0
    foreach t $myTitles {
      incr iAggWidth [expr {
          [font measure $myFont $t] + 
          $iPadding * 2 + $iDiagonal * 2 + $iButton + 1
      }]
    }

    set iRemainingTabs [llength $myTitles]
    set iRemainingPixels $iCanvasWidth

    set idx 0
    set yt [expr 0.5 * ($myTabHeight + [font metrics $myFont -linespace])]
    set x 1
    foreach title $myTitles {

      set  iTabWidth [expr $iRemainingPixels / $iRemainingTabs]
      incr iRemainingTabs -1
      incr iRemainingPixels [expr $iTabWidth * -1]

      set iTextWidth [expr                                            \
          $iTabWidth - $iButton - $iDiagonal * 2 - $iPadding * 2 - 1  \
          - $iThreeDots
      ]
      set zTitle $title
      for {set n 0} {$n <= [string length $zTitle]} {incr n} {
        if {[font measure $myFont [string range $zTitle 0 $n]] > $iTextWidth} {
          break;
        }
      }
      if {$n <= [string length $zTitle]} {
        set zTitle "[string range $zTitle 0 [expr $n - 1]]..."
      }

      set x2 [expr $x + $iDiagonal]
      set x3 [expr $x + $iTabWidth - $iDiagonal - 1]
      set x4 [expr $x + $iTabWidth - 1]

      set y1 [expr $myTabHeight - 0]
      set y2 [expr $iDiagonal + 1]
      set y3 1

      set ximg [expr $x + $iTabWidth - $iDiagonal - $iButton - 1]
      set yimg [expr 1 + ($myTabHeight - $iButton) / 2]

      set id [${win}.tabs create polygon \
          $x $y1 $x $y2 $x2 $y3 $x3 $y3 $x4 $y2 $x4 $y1]

      set id2 [${win}.tabs create text [expr $x2 + $iPadding] $yt]
      ${win}.tabs itemconfigure $id2 -anchor sw -text $zTitle -font $myFont

      $self CreateButton $idx $ximg $yimg $iButton

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
#
# WIDGET COMMAND
#
#     $notebook add ARGS
#     $notebook addbg ARGS
#     $notebook close
#     $notebook current
#     $notebook set_title WIDGET TITLE
#     $notebook tabs
#
snit::widget ::hv3::notebook {

  option -newcmd      -default ""
  option -switchcmd   -default ""
  option -delcmd      -default ""

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
    }
  }

  method close {} {
    if {$myOnlyTab eq ""} {
      destroy [$self current]
    }
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

  method get_title {widget} {
    if {$widget eq $myOnlyTab} {
      set title $myOnlyTitle
    } elseif {[catch {set title [${win}.notebook tab $widget -text]}]} {
      set title $myPendingTitle
    }
    return $title
  }

  method current {} {
    if {$myOnlyTab ne ""} {return $myOnlyTab}

    if {0 == [catch {${win}.notebook select} current]} {
      return $current
    }
    return [lindex [${win}.notebook tabs] [${win}.notebook index current]]
  }

  method tabs {} {
    if {$myOnlyTab ne ""} {return $myOnlyTab}
    return [${win}.notebook tabs]
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
    $w configure -borderwidth 1 -relief raised -pady 2
    ::hv3::UseHv3Font $w
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

proc ::hv3::ComparePositionId {frame1 frame2} {
  return [string compare [$frame1 positionid] [$frame2 positionid]]
}

#-------------------------------------------------------------------------
# ::hv3::findwidget
#
#     This snit widget encapsulates the "Find in page..." functionality.
#
#     Two tags may be added to the html widget(s):
#
#         findwidget                      (all search hits)
#         findwidgetcurrent               (the current search hit)
#
#
snit::widget ::hv3::findwidget {
  variable myBrowser          ;# The ::hv3::browser_toplevel widget

  variable myNocaseVar 1      ;# Variable for the "Case insensitive" checkbox 
  variable myEntryVar  ""     ;# Variable for the entry widget
  variable myCaptionVar ""    ;# Variable for the label widget
  
  variable myCurrentHit -1
  variable myCurrentList ""

  delegate option -borderwidth to hull
  delegate option -relief      to hull

  constructor {browser args} {
    set myBrowser $browser

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

  method Hv3List {} {
    if {[catch {$myBrowser get_frames} msg]} {
      return $myBrowser
    } else {
      set frames [$myBrowser get_frames]
    }

    # Filter the $frames list to exclude frameset documents.
    set frames2 ""
    foreach f $frames {
      if {![$f isframeset]} {
        lappend frames2 $f
      }
    }

    # Sort the frames list in [positionid] order
    set frames3 [lsort -command ::hv3::ComparePositionId $frames2]

    set ret [list]
    foreach f $frames3 {
      lappend ret [$f hv3]
    }
    return $ret
  }

  method lazymoveto {hv3 n1 i1 n2 i2} {
    set nodebbox [$hv3 text bbox $n1 $i1 $n2 $i2]
    set docbbox  [$hv3 bbox]

    set docheight "[lindex $docbbox 3].0"

    set ntop    [expr ([lindex $nodebbox 1].0 - 30.0) / $docheight]
    set nbottom [expr ([lindex $nodebbox 3].0 + 30.0) / $docheight]
 
    set sheight [expr [winfo height $hv3].0 / $docheight]
    set stop    [lindex [$hv3 yview] 0]
    set sbottom [expr $stop + $sheight]


    if {$ntop < $stop} {
      $hv3 yview moveto $ntop
    } elseif {$nbottom > $sbottom} {
      $hv3 yview moveto [expr $nbottom - $sheight]
    }
  }

  # Dynamic update proc.
  method UpdateDisplay {nMaxHighlight} {

    set nMatch 0      ;# Total number of matches
    set nHighlight 0  ;# Total number of highlighted matches
    set matches [list]

    # Get the list of hv3 widgets that (currently) make up this browser
    # display. There is usually only 1, but may be more in the case of
    # frameset documents.
    #
    set hv3list [$self Hv3List]

    # Delete any instances of our two tags - "findwidget" and
    # "findwidgetcurrent". Clear the caption.
    #
    foreach hv3 $hv3list {
      $hv3 tag delete findwidget
      $hv3 tag delete findwidgetcurrent
    }
    set myCaptionVar ""

    # Figure out what we're looking for. If there is nothing entered 
    # in the entry field, return early.
    set searchtext $myEntryVar
    if {$myNocaseVar} {
      set searchtext [string tolower $searchtext]
    }
    if {[string length $searchtext] == 0} return

    foreach hv3 $hv3list {
      set doctext [$hv3 text text]
      if {$myNocaseVar} {
        set doctext [string tolower $doctext]
      }

      set iFin 0
      set lMatch [list]

      while {[set iStart [string first $searchtext $doctext $iFin]] >= 0} {
        set iFin [expr $iStart + [string length $searchtext]]
        lappend lMatch $iStart $iFin
        incr nMatch
        if {$nMatch == $nMaxHighlight} { set nMatch "many" ; break }
      }

      set lMatch [lrange $lMatch 0 [expr ($nMaxHighlight - $nHighlight)*2 - 1]]
      incr nHighlight [expr [llength $lMatch] / 2]
      if {[llength $lMatch] > 0} {
        lappend matches $hv3 [eval [concat $hv3 text index $lMatch]]
      }
    }

    set myCaptionVar "(highlighted $nHighlight of $nMatch hits)"

    foreach {hv3 matchlist} $matches {
      foreach {n1 i1 n2 i2} $matchlist {
        $hv3 tag add findwidget $n1 $i1 $n2 $i2
      }
      $hv3 tag configure findwidget -bg purple -fg white
      $self lazymoveto $hv3                         \
            [lindex $matchlist 0] [lindex $matchlist 1] \
            [lindex $matchlist 2] [lindex $matchlist 3]
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

    set previousHit $myCurrentHit
    if {$myCurrentHit < 0} {
      $self UpdateDisplay 100000
    } 
    incr myCurrentHit $dir

    set nTotalHit 0
    foreach {hv3 matchlist} $myCurrentList {
      incr nTotalHit [expr [llength $matchlist] / 4]
    }

    if {$myCurrentHit < 0 || $nTotalHit <= $myCurrentHit} {
      tk_messageBox -message "The text you entered was not found" -type ok
      incr myCurrentHit [expr -1 * $dir]
      return
    }
    set myCaptionVar "Hit [expr $myCurrentHit + 1] / $nTotalHit"

    set hv3 ""
    foreach {hv3 n1 i1 n2 i2} [$self GetHit $previousHit] { }
    catch {$hv3 tag delete findwidgetcurrent}

    set hv3 ""
    foreach {hv3 n1 i1 n2 i2} [$self GetHit $myCurrentHit] { }
    $self lazymoveto $hv3 $n1 $i1 $n2 $i2
    $hv3 tag add findwidgetcurrent $n1 $i1 $n2 $i2
    $hv3 tag configure findwidgetcurrent -bg black -fg yellow
  }

  method GetHit {iIdx} {
    set nSofar 0
    foreach {hv3 matchlist} $myCurrentList {
      set nThis [expr [llength $matchlist] / 4]
      if {($nThis + $nSofar) > $iIdx} {
        return [concat $hv3 [lrange $matchlist \
                [expr ($iIdx-$nSofar)*4] [expr ($iIdx-$nSofar)*4+3]
        ]]
      }
      incr nSofar $nThis
    }
    return ""
  }

  destructor {
    # Delete any tags added to the hv3 widget. Do this inside a [catch]
    # block, as it may be that the hv3 widget has itself already been
    # destroyed.
    foreach hv3 [$self Hv3List] {
      catch {
        $hv3 tag delete findwidget
        $hv3 tag delete findwidgetcurrent
      }
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

