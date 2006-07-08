namespace eval hv3 { set {version($Id: hv3_widgets.tcl,v 1.12 2006/07/08 09:54:54 danielk1977 Exp $)} 1 }

package require snit
package require Tk

set ::hv3::toolkit Tk
catch {
  package require tile
  set ::hv3::toolkit Tile
}

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
  }
  return $w
}

proc ::hv3::button {args} {
  if {$::hv3::toolkit eq "Tile"} {
    set w [eval [linsert $args 0 ::ttk::button]]
  } else {
    set w [eval [linsert $args 0 ::button]]
    $w configure -highlightthickness 0
  }
  return $w
}

proc ::hv3::entry {args} {
  if {$::hv3::toolkit eq "Tile"} {
    set w [eval [linsert $args 0 ::ttk::entry]]
  } else {
    set w [eval [linsert $args 0 ::entry]]
    $w configure -highlightthickness 0
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
      set myButton [::button ${win}.button -highlightthickness 0]
    }
    set top [winfo toplevel $myButton]
    set myPopup ${top}[string map {. _} $myButton]
    set myPopupLabel ${myPopup}.label
    frame $myPopup -bg black
    ::label $myPopupLabel -fg black -bg white -font TkDefaultFont

    pack $myButton -expand true -fill both
    pack  $myPopup.label -padx 1 -pady 1 -fill both -expand true

    $self configurelist $args

    bind $myButton <Enter> [mymethod Enter]
    bind $myButton <Leave> [mymethod Leave]
    bind $myButton <ButtonPress-1> +[mymethod Leave]
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
    $w configure -borderwidth 1 -tearoff 0 -font TkDefaultFont
    lappend ::hv3::menu_list $w
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



snit::widget ::hv3::buttontab {

  component myButton

  constructor {args} {
    # set myButton [::hv3::button ${win}.button]
    set myButton [::hv3::label ${win}.button]
    catch { $myButton configure -padding 2 }

    bind $myButton <1>     [mymethod invoke]
    # bind $myButton <Enter> [list $myButton state active]
    # bind $myButton <Leave> [list $myButton state !active]

    $self configurelist $args
    pack $myButton -fill both -expand true

    if {$::hv3::toolkit eq "Tile"} {
        $myButton configure -style TNotebook.Tab
    } else {
    }
  }

  method tabstate {state} {
    if {$::hv3::toolkit eq "Tile"} {
      $myButton configure -style TNotebook.Tab
      if {$state} { $myButton state {selected} } \
      else        { $myButton state {!selected} }
    } else {
      if {$state} { $myButton configure -relief solid } \
      else        { $myButton configure -relief ridge }
    }
  }

  method invoke {} {
    if {$options(-command) ne ""} {
      eval $options(-command)
    }
  }

  option -command -default ""

  delegate option * to myButton
  delegate method * to myButton
}

# Widget to add automatic scrollbars to a widget supporting the
# [xview], [yview], -xscrollcommand and -yscrollcommand interface (e.g.
# html, canvas or text).
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
    catch {
      # bindtags [$myWidget html] [concat [bindtags [$myWidget html]] $win]
    }
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

#
# Tabbed notebook widget for hv3.
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
#     $widget add
#     $widget close
#     $widget current
#     $widget set_title
#
proc ::hv3::notebook {args} {
  if {$::hv3::toolkit eq "Tile"} {
    return [eval [linsert $args 0 ::hv3::tile_notebook]]
  }  
  return [eval [linsert $args 0 ::hv3::tk_notebook]]
}

snit::widget ::hv3::tile_notebook {

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
      eval [linsert $options(-switchcmd) 1 [$self current]]
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
        ${win}.notebook select $tab1
      }
      $options(-delbutton) configure -state normal
    } else {
      if {1 && $myOnlyTab eq ""} {
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
    ::ttk::notebook ${win}.notebook  -width 700 -height 500 
    bind ${win}.notebook <<NotebookTabChanged>> [list $self Switchcmd]
    # place ${win}.notebook -relheight 1.0 -relwidth 1.0
    pack ${win}.notebook -fill both -expand true

  }

  method add {args} {

    set widget ${win}.notebook.tab_[incr myNextId]

    set myPendingTitle ""
    eval [concat [linsert $options(-newcmd) 1 $widget] $args]
    ${win}.notebook add $widget -sticky ewns -text Blank
    ${win}.notebook select $widget

    if {$myPendingTitle ne ""} {$self set_title $widget $myPendingTitle}

    $self Switchcmd
    return $widget
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

    # In new versions of Tile you can do [${win}.notebook select] to
    # get the currently visible widget. But the following works in old
    # versions too.
    return [lindex [${win}.notebook tabs] [${win}.notebook index current]]
  }
}

snit::widget ::hv3::tk_notebook {

  option -newcmd    -default ""
  option -switchcmd -default ""
  option -delcmd    -default ""
  option -delbutton -default ""

  variable myNextId       0
  variable myPendingTitle ""

  variable myCurrent      ""
  variable myWidgets      ""

  method close {} {
    destroy [$self current]
  }

  constructor {args} {
    $self configurelist $args

    frame ${win}.frame
    frame ${win}.tabs 

    pack ${win}.tabs  -side top -fill x
    pack ${win}.frame -side top -fill both -expand true
  }

  method add {args} {
    set widget ${win}.frame.widget_${myNextId}
    set button [$self WidgetToButton $widget]
    incr myNextId

    set myPendingTitle "Blank"
    eval [concat [linsert $options(-newcmd) 1 $widget] $args]
    ::button $button -text $myPendingTitle -command [mymethod Switchto $widget]

    lappend myWidgets $widget
    bind $widget <Destroy> [mymethod Destroy $widget]

    $self Switchto $widget

    return $widget
  }

  method Destroy {widget} {
    destroy [$self WidgetToButton $widget]
    set idx [lsearch $myWidgets $widget]
    set myWidgets [lreplace $myWidgets $idx $idx]
    if {$widget eq $myCurrent} {
      set new [lindex $myWidgets $idx]
      if {$new eq ""} {set new [lindex $myWidgets end]}
      $self Switchto $new
    } else {
      $self Switchto $myCurrent
    }
  }

  method current {} {
    return $myCurrent
  }

  method set_title {widget title} {
    if {0 > [lsearch $myWidgets $widget]} {
      set myPendingTitle $title
    } else {
      [$self WidgetToButton $widget] configure -text $title
    }
  }

  method Switchto {widget} {

    if {$widget ne $myCurrent} {
      pack forget $myCurrent
      set myCurrent $widget
      pack $myCurrent -fill both -expand true
      if {$options(-switchcmd) ne ""} { 
        eval [linsert $options(-switchcmd) 1 [$self current]]
      }
    }

    set height 0
    set i 0
    set fraction [expr 1.0 / double([llength $myWidgets])]
    foreach w $myWidgets {
      set button [$self WidgetToButton $w]
      place configure $button                   \
          -relwidth $fraction                   \
          -relx [expr $i * $fraction]           \
          -anchor nw

      set h [winfo reqheight $button]
      if {$h > $height} {set height $h}
      incr i

      if {$w eq $myCurrent} {
        $button configure -relief solid
      } else {
        $button configure -relief ridge
      }
    }

    if {[llength $myWidgets] == 1} {
      $options(-delbutton) configure -state disabled
      # place forget [$self WidgetToButton $myCurrent]
      # set height 0
    } else {
      $options(-delbutton) configure -state normal
    }

    ${win}.tabs configure -height $height
  }

  method WidgetToButton {widget} {
    set id [regexp {_[0-9]+} $widget match]
    set button ${win}.tabs.widget${match}
    return $button
  }
}


# Wrapper around the ::hv3::scrolledwidget constructor. 
#
# Example usage to create a 400x400 canvas widget named ".c" with 
# automatic scrollbars:
#
#     ::hv3::scrolled canvas .c -width 400 -height 400
#
proc ::hv3::scrolled {widget name args} {
  return [eval [concat \
    [list ::hv3::scrolledwidget $name $widget] $args
  ]]
}

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


snit::type ::hv3::textdocument {

  variable myHtml                      ;# Html widget
  variable myText                      ;# Text rep of the document

  variable myIndex                     ;# Mapping from node to text indices.

  constructor {html} {
    set space_pending 0
    set myText ""
    set myIndex [list]
    set myHtml $html
    ::hv3::walkTree [$html node] N {
      set idx 0
      foreach token [$N text -tokens] {
        foreach {type arg} $token break
        switch $type {
          text    {
            if {$space_pending} {append myText " "}
            set space_pending 0
            lappend myIndex [list [string length $myText] $N $idx]
            append myText $arg
            incr idx [string length $arg]
          }
          space   {
            set space_pending 1
            incr idx
          }
          newline {
            set space_pending 1
            incr idx
          }
        }
      }
    }
  }

  method text {} {return $myText}

  method stringToNode {idx} {
    set best 0
    for {set ii 0} {$ii < [llength $myIndex]} {incr ii} {
      foreach {stridx node nodeidx} [lindex $myIndex $ii] {}
      if {$stridx <= $idx} {
        set retnode $node
        set retnodeidx [expr $nodeidx + ($idx - $stridx)]
      }
    }
    return [list $retnode $retnodeidx]
  }

  method nodeToString {the_node the_node_idx} {
    set ii 0
    set ret -1
    ::hv3::walkTree [$myHtml node] N {
      foreach {stridx node nodeidx} [lindex $myIndex $ii] {}

      if {$N eq $the_node} {
        set ret $stridx
        while {$node eq $N} {
          if {$the_node_idx >= $nodeidx} {
            set ret [expr $stridx + $the_node_idx - $nodeidx]
          }
          incr ii
          set node ""
          foreach {stridx node nodeidx} [lindex $myIndex $ii] {}
        }
      }

      while {$node eq $N} {
        incr ii
        set node ""
        foreach {stridx node nodeidx} [lindex $myIndex $ii] {}
      }
    }
    return $ret
  }
}


# This class implements a "find text" dialog box for hv3.
#
snit::widget ::hv3::finddialog {
  hulltype toplevel

  # The html widget.
  variable myHtml 

  # Index to start searching the document text representation from.
  variable myIndex 0 

  # These three variables are connected to the three checkbox widgets
  # in the GUI via the -variable option. i.e. they will be set to 1
  # when the corresponding checkbox is checked, and zero otherwise.
  variable myNocase     0 
  variable myWraparound 0 
  variable myBackward   0 

  constructor {htmlwidget args} {
    set myHtml $htmlwidget

    label $win.label -text "Search for text:"
    entry $win.entry -width 60
    checkbutton $win.check_backward -text "Search Backwards"
    checkbutton $win.check_nocase   -text "Case Insensitive"
    checkbutton $win.check_wrap     -text "Wrap Around"

    $win.check_nocase configure   -variable [myvar myNocase]
    $win.check_wrap configure     -variable [myvar myWraparound]
    $win.check_backward configure -variable [myvar myBackward]

    frame $win.buttons
    button $win.buttons.findnext -text Find    -command [mymethod findnext]
    button $win.buttons.cancel   -text Dismiss -command [mymethod cancel]

    bind $win.entry <Return> [list $win.buttons.findnext invoke]
    bind $win.entry <Escape> [list $win.buttons.cancel invoke]
    focus $win.entry

    grid configure $win.buttons.findnext -column 0 -row 0 -sticky ew
    grid configure $win.buttons.cancel   -column 1 -row 0 -sticky ew
    grid columnconfigure $win.buttons 0 -weight 1
    grid columnconfigure $win.buttons 1 -weight 1

    grid configure $win.label          -column 0 -row 0
    grid configure $win.entry          -column 1 -row 0 -sticky ew
    grid configure $win.check_backward -column 1 -row 1 -sticky w
    grid configure $win.check_nocase   -column 1 -row 2 -sticky w
    grid configure $win.check_wrap     -column 1 -row 3 -sticky w
    grid configure $win.buttons        -column 0 -row 4 -columnspan 2 -sticky ew

    grid columnconfigure $win 1 -weight 1
    $hull configure -pady 2 -padx 2
  }

  # Vertically scroll the html widget the minimum distance (which may be 0)
  # required so that document node $node is visible.
  #
  method lazymoveto {node} {
    set nodebbox [$myHtml bbox $node]
    set docbbox  [$myHtml bbox]

    set docheight "[lindex $docbbox 3].0"

    set ntop    [expr [lindex $nodebbox 1].0 / $docheight]
    set nbottom [expr [lindex $nodebbox 3].0 / $docheight]
 
    set sheight [expr [winfo height $myHtml].0 / $docheight]
    set stop    [lindex [$myHtml yview] 0]
    set sbottom [expr $stop + $sheight]

    if {$ntop < $stop} {
      $myHtml yview moveto $ntop
    } elseif {$nbottom > $sbottom} {
      $myHtml yview moveto [expr $nbottom - $sheight]
    }
  }

  method findnext {} {
    # The text to search for
    set searchtext [${win}.entry get]

    # Prepare the textdocument representation
    set td [$myHtml textdocument]

    # Retrieve the raw text from the textdocument object
    set doctext [$td text]
  
    # If the search is to be case independent, fold everything to lower case
    if {$myNocase} { 
      set doctext [string tolower $doctext]
      set searchtext [string tolower $searchtext]
    }

    # Search the text representation using [string first] or [string last].
    # Variable $myIndex stores the string-index to start searching at (this
    # applies regardless of whether the search direction is forward or
    # backward).
    set op first
    if {$myBackward} { set op last }
    set ii [string $op $searchtext $doctext $myIndex]

    if {$ii >= 0} {
      # A search-hit.
      set ii2 [expr $ii + [string length $searchtext]]
      set myIndex [expr $ii + 1]

      set from [$td stringToNode $ii]
      set to [$td stringToNode $ii2]

      eval [concat [list $myHtml select from] $from]
      eval [concat [list $myHtml select to] $to]

      $self lazymoveto [lindex $from 0]
    } elseif {$myIndex > 0 && $myWraparound} {
      # Text not found. But the search began part way through the document
      # and the "wrap around" checkbox is set. Repeat the search, starting
      # at the beginning of the document.
      set myIndex 0
      if {$myBackward} {set myIndex [string length $doctext]}
      $self findnext
    } else {
      # Text not found. Pop up a dialog to inform the user.
      set myIndex 0
      $myHtml select clear
      tk_messageBox -message "The text you entered was not found" -type ok
    }

  #  $td destroy
    return
  }

  method cancel {} {
    destroy $win
  }
}

