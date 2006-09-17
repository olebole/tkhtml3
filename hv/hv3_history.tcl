
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
  variable myStateIdx 0 

  variable myRadioVar 0 

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

  # Events:
  #     <<Goto>>
  #     <<Complete>>
  #     <<Reset>>
  #     <<Location>>
  #
  #     Also there is a trace on "titlevar" (set whenever a <title> node is
  #     parsed)
  #

  constructor {hv3 protocol args} {
    $hv3 configure -locationvar [myvar myLocationVar]
    $self configurelist $args

    trace add variable [$hv3 titlevar] write [mymethod Titlevarcmd $hv3]
#    trace add variable [myvar myLocationVar] write [mymethod Locvarcmd $hv3]

    bind $hv3 <<Location>> +[mymethod Locvarcmd $hv3]

    set myTitleVarName [$hv3 titlevar]
    set myHv3 $hv3
    set myProtocol $protocol

    bind $hv3 <<Goto>>     +[mymethod GotoHandler]
    bind $hv3 <<Complete>> +[mymethod CompleteHandler]
    bind $hv3 <<Reset>>    +[mymethod ResetHandler]

    # Initialise the state-list to contain a single, unconfigured state.
    set myStateList [::hv3::history_state %AUTO%]
    set myStateIdx 0
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

    # Set the xscroll and yscroll of the current state object.
    set state [lindex $myStateList $myStateIdx]
    $state xscroll [lindex [$myHv3 xview] 0]
    $state yscroll [lindex [$myHv3 yview] 0]
    $state deps [$myHv3 dependencies]

    if {!$myIgnoreGotoHandler} {
      # We are not in "history" mode.
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
    set state [lindex $myStateList $myStateIdx]
    $state xscroll [lindex [$myHv3 xview] 0]
    $state yscroll [lindex [$myHv3 yview] 0]
    $state deps [$myHv3 dependencies]

    if {$myHistorySeek >= 0} return

    # Exception - if the state-list contains a single state with no
    # "location" or "title" attribute set, then re-use it. This
    # occurs when starting the application with a remote document
    # as the first URI.
    if { [llength $myStateList] == 1 && [[lindex $myStateList 0] uri] eq "" } {
      return
    }
    set myStateList [lrange $myStateList 0 $myStateIdx]
    incr myStateIdx
    set myRadioVar $myStateIdx
    lappend myStateList [::hv3::history_state %AUTO%]

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
    set state [lindex $myStateList $myStateIdx]
    set t [set [$myHv3 titlevar]]
    if {$myHistorySeek < 0} {$state title $t}

    $self populatehistorymenu
  }

  proc StripFragment {uri} {
    set obj [::hv3::uri %AUTO% $uri]
    set ret [$obj get -nofragment]
    $obj destroy
    return $ret
  }

  method Locvarcmd {hv3} {
    if {$myHistorySeek >= 0} {
      set myStateIdx $myHistorySeek
    }

    set state [lindex $myStateList $myStateIdx]
    if {$myHistorySeek < 0} {
      set new [StripFragment $myLocationVar]
      set old [StripFragment [$state uri]]
      if {$old eq $new && $myLocationVar != [$state uri]} {
        set myStateList [lrange $myStateList 0 $myStateIdx]
        incr myStateIdx
        set myRadioVar $myStateIdx
        set newstate [::hv3::history_state %AUTO%]
        lappend myStateList $newstate
        $newstate title [$state title]
        set state $newstate
      }
    }

    $state uri $myLocationVar
    $self populatehistorymenu
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

    set myRadioVar $myStateIdx
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
          -variable [myvar myRadioVar]              \
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
# Object sub-commands:
#
#     init HV3-WIDGET
#
#         Configure the specified hv3 mega-widget to use the object
#         as a database of visited URIs (i.e. set the value of
#         the -isvisitedcmd option).
#
snit::type ::hv3::visiteddb {

  # This method is called whenever the application constructs a new 
  # ::hv3::hv3 mega-widget. Argument $hv3 is the new mega-widget.
  #
  method init {hv3} {
    bind $hv3 <<Location>> +[mymethod LocationHandler %W]
    $hv3 configure -isvisitedcmd [mymethod LocationQuery $hv3]
  }

  method LocationHandler {hv3} {
    set location [$hv3 location]
    $self addkey $location
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

  method keys {} {return [array names myDatabase]}
  method addkey {key} {
    if {[info exists myDatabase($key)]} {
      incr myDatabase($key)
    } else {

      if {[llength [array names myDatabase]] > ($myMaxEntries - 1)} {
        set pairs [list]
        foreach key [array names myDatabase] {
          lappend pairs [list $key $myDatabase($key)]
        }
        set new [
           lrange [lsort -decreasing -index 1 $pairs] 0 [expr $myMaxEntries - 2]
        ]
        array unset myDatabase
        foreach pair $new {
          set myDatabase([lindex $pair 0]) [lindex $pair 1]
        }
      }
      set myDatabase($key) 1
    }
  }

  method getdata {} {return [array get myDatabase]}
  method loaddata {data} {array set myDatabase $data}

  variable myMaxEntries 200
  variable myDatabase -array [list]
}

snit::widget ::hv3::scrolledlistbox {
  component myVsb
  component myListbox

  delegate method * to myListbox
  delegate option * to myListbox

  constructor {args} {

    set myVsb [::hv3::scrollbar ${win}.scrollbar]
    set myListbox [listbox ${win}.listbox]
    $myVsb configure -command [list $myListbox yview]
    $myListbox configure -yscrollcommand [list $myVsb set]

    pack $myVsb -side right -fill y
    pack $myListbox -fill both -expand yes

    $self configurelist $args
  }
}

snit::widget ::hv3::locationentry {

  component myEntry
  component myButton

  delegate option * to myEntry
  delegate method * to myEntry

  variable myListbox
  variable myListboxVar [list]

  option -command -default ""

  constructor {args} {
    set myEntry [entry ${win}.entry]
    $myEntry configure -background white
 
    set myButtonImage [image create bitmap -data {
      #define v_width 8
      #define v_height 4
      static unsigned char v_bits[] = { 0xff, 0x7e, 0x3c, 0x18 };
    }]

    set myButton [button ${win}.button -image $myButtonImage]
    $myButton configure -command [mymethod ButtonPress]

    pack $myButton -side right -fill y
    pack $myEntry -fill both -expand true

    $myEntry configure -borderwidth 0 -highlightthickness 0
    $myButton configure -borderwidth 1 -highlightthickness 0
    $hull configure -borderwidth 1 -relief sunken -background white

    # Create the listbox for the drop-down list. This is a child of
    # the same top-level as the ::hv3::locationentry widget...
    #
    set myListbox [winfo toplevel $win][string map {. _} ${win}]
    ::hv3::scrolledlistbox $myListbox

    # Any button-press anywhere in the GUI folds up the drop-down menu.
    bind [winfo toplevel $win] <ButtonPress> +[mymethod AnyButtonPress %W]

    bind $myEntry <KeyPress> +[mymethod KeyPress]
    bind $myEntry <KeyPress-Return> +[mymethod KeyPressReturn]
    bind $myEntry <KeyPress-Down> +[mymethod KeyPressDown]

    $myListbox configure -listvariable [myvar myListboxVar]
    $myListbox configure -background white
    $myListbox configure -font [$myEntry cget -font]
    $myListbox configure -highlightthickness 0
    $myListbox configure -borderwidth 1

    # bind $myListbox.listbox <<ListboxSelect>> [mymethod ListboxSelect]
    bind $myListbox.listbox <KeyPress-Return> [mymethod ListboxReturn]
    bind $myListbox.listbox <1>   [mymethod ListboxPress %y]

    $self configurelist $args
  }

  method AnyButtonPress {w} {
    if {
      [winfo ismapped $myListbox] &&
      $w ne $myButton && 
      $w ne $myEntry && 
      0 == [string match ${myListbox}* $w]
    } {
      place forget $myListbox
    }
  }

  # Configured -command callback for the button widget.
  #
  method ButtonPress {} {
    if {[winfo ismapped $myListbox]} {
      $self CloseDropdown
    } else {
      $self OpenDropdown *
    }
  }

  # Bindings for KeyPress events that occur in the entry widget:
  #
  method KeyPressReturn {} {

    set current [$myEntry get]
    if {![string match *:/* $current] && ![string match *: $current]} {
      if {[string range $current 0 0] eq "/"} {
        set final "file://${current}"
      } else {
        set final "http://${current}"
      }
      $myEntry delete 0 end
      $myEntry insert 0 $final
    }

    if {$options(-command) ne ""} {
      eval $options(-command)
    }
    $self CloseDropdown
  }
  method KeyPressDown {} {
    if {[winfo ismapped $myListbox]} {
      focus $myListbox.listbox
      $myListbox activate 0
      $myListbox selection set 0 0
    }
  }
  method KeyPress {} {
    after idle [mymethod AfterKeyPress]
  }
  method AfterKeyPress {} {
    $self OpenDropdown [$myEntry get]
  }

  method TransformSearch {str} {
    if {[regexp "Search the web for \"(.*)\"" $str -> newval]} {
      set newval [::hv3::escape_string $newval]
      set str "http://www.google.com/search?q=$newval"
    }
    return $str
  }

  method ListboxReturn {} {
    set str [$myListbox get active]
    $myEntry delete 0 end
    $myEntry insert 0 [$self TransformSearch $str]
    $self KeyPressReturn
  }
  method ListboxPress {y} {
    set str [$myListbox get [$myListbox nearest $y]]
    $myEntry delete 0 end
    $myEntry insert 0 [$self TransformSearch $str]
    $self KeyPressReturn
  }

  method CloseDropdown {} {
    place forget $myListbox
  }

  method OpenDropdown {pattern} {

    set matchpattern *
    if {$pattern ne "*"} {
      set matchpattern *${pattern}*
    }

    set myListboxVar ""
    foreach entry [::hv3::the_visited_db keys] {
      if {[string match $matchpattern $entry]} {
        lappend myListboxVar $entry
      }
    }

    set myListboxVar [lsort $myListboxVar] 
    if {$pattern ne "*" && ![string match *.* $pattern] } {
      set search "Search the web for \"$pattern\""
      set myListboxVar [linsert $myListboxVar 0 $search]
    }

    if {[llength $myListboxVar] == 0 && $pattern ne "*"} {
      $self CloseDropdown
      return
    }

    if {[llength $myListboxVar] > 4} {
      $myListbox configure -height 5
    } else {
      $myListbox configure -height -1
    }

    set t [winfo toplevel $win]
    set x [expr [winfo rootx $win] - [winfo rootx $t]]
    set y [expr [winfo rooty $win] + [winfo height $win] - [winfo rooty $t]]
    set w [winfo width $win]

    place $myListbox -x $x -y $y -width $w
    raise $myListbox
  }
}

