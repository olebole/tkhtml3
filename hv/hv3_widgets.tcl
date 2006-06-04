
package require snit
package require Tk

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
    set myVsb  [scrollbar ${win}.vsb -orient vertical -highlightthickness 0]
    set myHsb  [scrollbar ${win}.hsb -orient horizontal -highlightthickness 0]

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

