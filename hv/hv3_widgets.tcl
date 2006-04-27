
package require snit
package require Tk

snit::widget ::hv3::scrolledwidget {
  component myWidget
  variable  myVsb
  variable  myHsb

  constructor {widget args} {
    # Create the three widgets - one user widget and two scrollbars.
    set myWidget [eval [linsert $widget 1 ${win}.widget]]
    set myVsb  [scrollbar ${win}.vsb -orient vertical]
    set myHsb  [scrollbar ${win}.hsb -orient horizontal]

    grid configure $myWidget -column 0 -row 0 -sticky nsew
    grid columnconfigure $win 0 -weight 1
    grid rowconfigure    $win 0 -weight 1
    grid propagate       $win 0

    # First, set the values of -width and -height to the defaults for 
    # the scrolled widget class. Then configure this widget with the
    # arguments provided.
    $self configure -width [$myWidget cget -width] 
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
    set isrequired [expr ($first == 0.0 && $last == 1.0) ? 0 : 1]
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

snit::widget ::hv3::framepair {

  component pannedwindow 

  constructor {left right args} {
    set pannedwindow [panedwindow ${win}.pannedwindow -opaque 1]
    set w1           [eval [linsert $left 1 ${win}.pannedwindow.left]] 
    set w2           [eval [linsert $right 1 ${win}.pannedwindow.right]]
    $self configurelist $args
    $pannedwindow add $w1 
    $pannedwindow add $w2
    pack $pannedwindow -expand true -fill both
  }

  delegate option * to pannedwindow

  method left   {} {return [lindex [$pannedwindow panes] 0]}
  method right  {} {return [lindex [$pannedwindow panes] 1]}
  method top    {} {return [$self left]}
  method bottom {} {return [$self right]}
}

# Example:
#
#     frameset .frames \
#         hv3    -variable myTopWidget    -side top  \
#         canvas -variable myTreeWidget   -side left \
#         hv3    -variable myReportWidget
# 
proc frameset {win args} {
  set idx 0
  set frames [list]
  while {$idx < [llength $args]} {
    set cmd [lindex $args $idx]
    incr idx

    unset -nocomplain variable
    set side left

    set options [list -variable -side]
    while {                                                        \
      $idx < ([llength $args]-1) &&                                \
      [set opt [lsearch $options [lindex $args $idx]]] >= 0        \
    } {
      incr idx 
      set [string range [lindex $options $opt] 1 end] [lindex $args $idx]
      incr idx
    }

    if {![info exists variable]} {
      error "No -variable option supplied for widget $cmd"
    }
    if {$side ne "top" && $side ne "left"} {
      error "Bad value for -side option: should be \"left\" or \"top\""
    }

    lappend frames [list $cmd $variable $side]
  }

  unset -nocomplain cmd
  for {set ii [expr [llength $frames] - 1]} {$ii >= 0} {incr ii -1} {
    foreach {wid variable side} [lindex $frames $ii] {}
    if {[info exists cmd]} { 
      switch -- $side {
        top  {set o vertical}
        left {set o horizontal}
      }
      set cmd [list ::hv3::framepair $wid $cmd -orient $o]
    } else {
      set cmd $wid
    }
  }
  eval [linsert $cmd 1 $win]

  set framepair $win
  for {set ii 0} {$ii < [llength $frames]} {incr ii} {
    foreach {wid variable side} [lindex $frames $ii] {}
    if {$ii == ([llength $frames]-1)} {
      uplevel [list set $variable $framepair]
    } else {
      uplevel [list set $variable [$framepair left]]
      set framepair [$framepair right]
    }
  }

  return $win
}

