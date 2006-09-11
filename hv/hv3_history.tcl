
package require snit

# History state for a single browser frame. TODO: This should eventually
# be extended to support history for frameset documents.
#
snit::type ::hv3::history_state {

  # Variable $myUri stores the document URI to fetch. This is the value
  # that will be displayed in the "location" entry field when the 
  # history state is loaded.
  #
  # List variable $myDeps stores a list of URI's corresponding to 
  # resources (i.e. stylesheets and images) that were required last time 
  # the resource at $myUri was loaded. 
  #
  # The idea is that when the user "visits" a history-state, all dependencies
  # can be requested before the actual document. If, as is likely, responses
  # are returned in the order requested, there will be no "flicker" as the
  # document is displayed first without stylesheets and images, and then 
  # with.
  #
  variable myUri     ""
  variable myDeps [list]

  # Title of the page last seen at location $myUri.
  variable myTitle   ""

  # Values to use with [pathName xscroll|yscroll moveto] to restore
  # the previous scrollbar positions.
  #
  variable myXscroll ""
  variable myYscroll ""

  method Getset {var arglist} {
    if {[llength $arglist] > 0} {
      set $var [lindex $arglist 0]
    }
    return [set $var]
  }

  method uri {args} { return [$self Getset myUri $args] }
  method title {args} { return [$self Getset myTitle $args] }
  method xscroll {args} { return [$self Getset myXscroll $args] }
  method yscroll {args} { return [$self Getset myYscroll $args] }
  method deps {args} { return [$self Getset myDeps $args] }
}

# class ::hv3::history
#
# Options:
#     -gotocmd
#     -historymenu
#     -backbutton
#     -forwardbutton
#
# Methods:
#     locationvar
# 
snit::type ::hv3::history {
  # corresponding option exported by this class.
  #
  variable myLocationVar ""
  variable myTitleVarName ""

  variable myHv3 ""
  variable myProtocol ""

  # The following two variables store the history list
  variable myStateList [list]
  variable myStateIdx -1

  # Variables used when loading a history-state.
  variable myHistorySeek -1
  variable myIgnoreGotoHandler 0

  # Configuration options to attach this history object to a set of
  # widgets - back and forward buttons and a menu widget.
  #
  option -historymenu   -default "" -configuremethod setoption
  option -backbutton    -default "" -configuremethod setoption
  option -forwardbutton -default "" -configuremethod setoption

  # An option to set the script to invoke to goto a URI. The script is
  # evaluated with a single value appended - the URI to load.
  #
  option -gotocmd -default ""

  constructor {hv3 protocol args} {
    $hv3 configure -locationvar [myvar myLocationVar]
    $self configurelist $args

    trace add variable [$hv3 titlevar] write [mymethod Titlevarcmd $hv3]
    trace add variable [myvar myLocationVar] write [mymethod Locvarcmd $hv3]
    set myTitleVarName [$hv3 titlevar]
    set myHv3 $hv3
    set myProtocol $protocol

    bind $hv3 <<Goto>>     +[mymethod GotoHandler]
    bind $hv3 <<Complete>> +[mymethod CompleteHandler]
    bind $hv3 <<Reset>>    +[mymethod ResetHandler]
  }

  destructor {
    trace remove variable $myTitleVarName write [mymethod Titlevarcmd $myHv3]
    trace remove variable [myvar myLocationVar] write \
        [mymethod Locvarcmd $myHv3]

    foreach state $myStateList {
      $state destroy
    }
  }

  # Return the name of the variable configured as the -locationvar option
  # of the hv3 widget. This is provided so that other code can add
  # [trace] callbacks to the variable.
  method locationvar {} {return [myvar myLocationVar]}

  # This method is bound to the <<Goto>> event of the ::hv3::hv3 
  # widget associated with this history-list.
  #
  method GotoHandler {} {
    if {$myStateIdx >= 0} {
      set state [lindex $myStateList $myStateIdx]
      $state xscroll [lindex [$myHv3 xview] 0]
      $state yscroll [lindex [$myHv3 yview] 0]
      $state deps [$myHv3 dependencies]
    }
    if {!$myIgnoreGotoHandler} {
      set myHistorySeek -1
      $myProtocol configure -relaxtransparency 0
    }
  }

  # This method is bound to the <<Complete>> event of the ::hv3::hv3 
  # widget associated with this history-list.
  #
  method CompleteHandler {} {
    if {$myHistorySeek >= 0} {
      set state [lindex $myStateList $myStateIdx]
      after idle [list $myHv3 yview moveto [$state yscroll]]
      after idle [list $myHv3 xview moveto [$state xscroll]]
    }
  }

  # Invoked whenever our hv3 widget is reset (i.e. just before a new
  # document is loaded). The current state of the widget should be
  # copied into the history list.
  method ResetHandler {} {

    # Update the current history-state record with the current scrollbar
    # and dependency settings.
    if {$myStateIdx >= 0} {
      set state [lindex $myStateList $myStateIdx]
      $state xscroll [lindex [$myHv3 xview] 0]
      $state yscroll [lindex [$myHv3 yview] 0]
      $state deps [$myHv3 dependencies]
    }

    if 0 {
      puts "RESET $myHv3"
      puts "History list is:"
      foreach state $myStateList {
        puts "uri=[$state uri] title=[$state title]"
        puts "xscroll=[$state xscroll] yscroll=[$state yscroll]"
        puts "deps=[$state deps]"
      }
      puts ""
    }
  }

  # Invoked when the [$hv3 titlevar] or [$hv3 locationvar] variables
  # are modified. Update the current history-state record according
  # to the new values.
  method Titlevarcmd {hv3 args} {
    if {$myStateIdx >= 0} {
      set state [lindex $myStateList $myStateIdx]
      set t [set [$myHv3 titlevar]]
      if {$t ne ""} { $state title $t }
    }

    $self populatehistorymenu
  }

  method Locvarcmd {hv3 args} {
    if {$myHistorySeek >= 0} {
      set myStateIdx $myHistorySeek
    } else {
      set myStateList [lrange $myStateList 0 $myStateIdx]
      incr myStateIdx
      lappend myStateList [::hv3::history_state %AUTO%]
    }

    if {$myStateIdx >= 0} {
      set state [lindex $myStateList $myStateIdx]
      $state uri $myLocationVar
      $self populatehistorymenu
    }
  }

  method setoption {option value} {
    set options($option) $value
    switch -- $option {
      -historymenu   { $self populatehistorymenu }
      -forwardbutton { $self populatehistorymenu }
      -backbutton    { $self populatehistorymenu }
    }
  }

  method gotohistory {idx} {
    set myHistorySeek $idx
    set state [lindex $myStateList $idx]

    set myIgnoreGotoHandler 1
    $myProtocol configure -relaxtransparency 1
    eval [linsert $options(-gotocmd) end [$state uri]]
    set myIgnoreGotoHandler 0
  }

  # This method reconfigures the state of the -historymenu, -backbutton
  # and -forwardbutton to match the internal state of this object. To
  # summarize, it:
  #
  #     * Enables or disabled the -backbutton button
  #     * Enables or disabled the -forward button
  #     * Clears and repopulates the -historymenu menu
  #
  # This should be called whenever some element of internal state changes.
  # Possibly as an [after idle] background job though...
  #
  method populatehistorymenu {} {

    # Handles for the three widgets this object is controlling.
    set menu $options(-historymenu)
    set back $options(-backbutton)
    set forward $options(-forwardbutton)

    if {$menu ne ""} {
      $menu delete 0 end
      $menu add command -label Back
      $menu add command -label Forward
      $menu add separator

      set idx [expr [llength $myStateList] - 15]
      if {$idx < 0} {set idx 0}
      for {} {$idx < [llength $myStateList]} {incr idx} {
        set state [lindex $myStateList $idx]

        # Try to use the history-state "title" as the menu item label, 
        # but if this is an empty string, fall back to the URI.
        set caption [$state title]
        if {$caption eq ""} {set caption [$state uri]}

        $menu add radiobutton                       \
          -label $caption                           \
          -variable [myvar myStateIdx]              \
          -value    $idx                            \
          -command [mymethod gotohistory $idx]
      }
    }

    set backidx [expr $myStateIdx - 1]
    set backcmd [mymethod gotohistory $backidx]
    if {$backidx >= 0} {
        if {$back ne ""} { $back configure -state normal -command $backcmd }
        if {$menu ne ""} { $menu entryconfigure 0 -command $backcmd }
    } else {
        if {$back ne ""} { $back configure -state disabled }
        if {$menu ne ""} { $menu entryconfigure 0 -state disabled }
    }

    set fwdidx [expr $myStateIdx + 1]
    set fwdcmd [mymethod gotohistory $fwdidx]
    if {$fwdidx < [llength $myStateList]} {
        if {$forward ne ""} { $forward configure -state normal -command $fwdcmd}
        if {$menu ne ""} { $menu entryconfigure 1 -command $fwdcmd }
    } else {
        if {$forward ne ""} { $forward configure -state disabled }
        if {$menu ne ""} { $menu entryconfigure 1 -state disabled }
    }

  }
}

# Class ::hv3::visiteddb
#
# There is a single instance of the following type used by all browser
# frames created in the lifetime of the application. It's job is to
# store a list of URI's that are considered "visited". Links to these
# URI's should be styled using the ":visited" pseudo-class, not ":link".
#
# TODO: At present the database of visited URIs can grow without bound.
# This is probably not a huge problem in the short-term.
#
# TODO2: This, like cookies, needs to be put in an SQLite database.
#
snit::type ::hv3::visiteddb {

  # This method is called whenever the application constructs a new 
  # ::hv3::hv3 mega-widget. Argument $hv3 is the new mega-widget.
  #
  method init {hv3} {
    bind $hv3 <<Location>> [mymethod LocationHandler %W]
    $hv3 configure -isvisitedcmd [mymethod LocationQuery $hv3]
  }

  method LocationHandler {hv3} {
    set location [$hv3 location]
    set myDatabase($location) 1
  }

  method LocationQuery {hv3 node} {

    set rel [$node attr href]
    set obj [::hv3::uri %AUTO% [$hv3 location]]
    $obj load $rel
    set full [$obj get]
    $obj destroy

    set rc [info exists myDatabase($full)] 
    return $rc
  }

  variable myDatabase -array [list]
}

# Create the single, application-wide instance of ::hv3::visiteddb.
#
::hv3::visiteddb ::hv3::the_visited_db

