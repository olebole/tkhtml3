
catch { memory init on }

# Load packages.
if {[info exists auto_path]} {
    # set auto_path [concat . $auto_path]
}
package require Tk
package require Tkhtml 3.0

option add *borderWidth 1
option add *tearOff 0
# option add *font {Arial 9 normal}

# If possible, load package "Img". Without it the script can still run,
# but won't be able to load many image formats.
if {[catch { package require Img }]} {
  puts "WARNING: Failed to load package Img"
}

# Source the other script files that are part of this application.
#
proc sourcefile {file} {
  set fname [file join [file dirname [info script]] $file] 
  uplevel #0 [list source $fname]
}

sourcefile snit.tcl
sourcefile hv3.tcl
sourcefile hv3_log.tcl
sourcefile hv3_prop.tcl
sourcefile hv3_http.tcl

snit::type ::hv3_browser::history {

  variable myHistoryList [list]
  variable myCurrentPosition -1
  variable myHv3 ""
  variable myGotoCommand ""

  variable myLocationVar ""
  option -locationvar -default ""

  option -historymenu   -default "" -configuremethod setoption
  option -backbutton    -default "" -configuremethod setoption
  option -forwardbutton -default "" -configuremethod setoption

  constructor {hv3widget gotocommand} {
    $hv3widget configure -locationvar [myvar myLocationVar]
    set myGotoCommand $gotocommand
    set cmd [mymethod locationvarcmd]
    trace add variable [myvar myLocationVar] write [mymethod locationvarcmd]
  }

  # Add a new (initially empty) URI to the current position in the 
  # history list.
  method add {} {
    set c $myCurrentPosition
    set myHistoryList [linsert [lrange $myHistoryList 0 $c] end {}]
    incr myCurrentPosition
    $self populatehistorymenu
  }

  method locationvarcmd {args} {
    lset myHistoryList $myCurrentPosition $myLocationVar
    if {$options(-locationvar) ne ""} {
      uplevel #0 [list set $options(-locationvar) $myLocationVar]
    }
    $self populatehistorymenu
  }

  method setoption {option value} {
    switch -- $option {
      -locationvar   { $self locationvarcmd }
      -historymenu   { $self populatehistorymenu }
      -forwardbutton { $self populatehistorymenu }
      -backbutton    { $self populatehistorymenu }
    }
    set options($option) $value
  }

  method gotohistory {idx} {
    set myCurrentPosition $idx
    eval [linsert $myGotoCommand end [lindex $myHistoryList $idx]]
    $self populatehistorymenu
  }

  method populatehistorymenu {} {
    set menu $options(-historymenu)
    set back $options(-backbutton)
    set forward $options(-forwardbutton)
    if {$menu ne ""} {
      $menu delete 0 end
      set idx 0
      foreach item $myHistoryList {
        $menu add radiobutton                       \
          -label $item                              \
          -variable [myvar myCurrentPosition]       \
          -value    $idx                            \
          -command [mymethod gotohistory $idx]
        incr idx
      }
    }
    if {$back ne ""} {
      if {$myCurrentPosition > 0} {
        set idx [expr $myCurrentPosition - 1]
        $back configure -state normal -command [mymethod gotohistory $idx]
      } else {
        $back configure -state disabled 
      }
    } 
    if {$forward ne ""} {
      if {$myCurrentPosition < ([llength $myHistoryList] - 1)} {
        set idx [expr $myCurrentPosition + 1]
        $forward configure -state normal -command [mymethod gotohistory $idx]
      } else {
        $forward configure -state disabled 
      }
    } 
  }
}

proc ::hv3::returnX {val args} {return $val}

#--------------------------------------------------------------------------
# Class hv3_browser
#
#     * Support for http:// protocol (see class Hv3HttpProtcol).
#     * Support for back, forward and history-list operations.
#     * Support for a message to display in a status bar
#
snit::widget hv3_browser {
  component myHv3
  component myHttp
  component myHistory

  variable myHttpHandles [list]
  variable myNodeList ""                  ;# Current nodes under the pointer
  variable myX 0                          ;# Current location of pointer
  variable myY 0                          ;# Current location of pointer
  variable myHyperlinkNode ""             ;# Current node for hyper-link menu

  constructor {args} {
    set myHv3 [::hv3::scrolled hv3 $win.hv3]
    set myHttp [Hv3HttpProtcol %AUTO%]
    set myHistory [::hv3_browser::history %AUTO% $myHv3 [mymethod gotohistory]]
    pack $myHv3 -expand true -fill both

    $myHv3 protocol http [mymethod http]
    $myHv3 protocol home [list Hv3HomeProtocol $myHttp $myHv3 \
        [file normalize [file dirname [info script]]]
    ]
    $myHv3 configure -hyperlinkcmd [mymethod goto]
    $myHv3 configure -getcmd       [mymethod Getcmd]
    $myHv3 configure -postcmd      [mymethod Postcmd]
    $myHv3 configure -motioncmd    [mymethod motion]

    # Set up a binding to press "Q" to exit the application
    bind $myHv3 <KeyPress-q> exit
    bind $myHv3 <KeyPress-Q> exit
    bind $myHv3 <Control-f> [mymethod find]

    # Create the right-click behaviour.
    bind $myHv3 <3> [mymethod rightclick %x %y %X %Y]

    # Create the middle-click behaviour.
    bind $myHv3 <2> [mymethod goto_selection]

    focus $myHv3

    # Create the hyper-link menu
    set m [menu ${win}.hyperlinkmenu]
    $m add command -label "Follow Link" -command [mymethod followlink]
    $m add command -label "Download Link" -command [mymethod downloadlink]
    $m add command -label "Copy Link Location" -command [mymethod copylink]
    $m add command -label "Open Tree Browser" -command [mymethod browselink]
  }

  method rightclick {x y X Y} {
    set nodelist [$myHv3 node $x $y]
    foreach leaf $nodelist {
      for {set N $leaf} {$N ne ""} {set N [$N parent]} {
        if {[$N tag] eq "a" && 0 == [catch {$N attr href}]} {
          # Right click on a hyper-link. Pop up the hyper-link menu.
          $self hyperlinkmenu $N $X $Y
          return
        }
      }
    }
    ::HtmlDebug::browse [$myHv3 html] [lindex $nodelist 0]
  }

  method hyperlinkmenu {node x y} {
    set myHyperlinkNode $node
    puts [::hv3::resolve_uri [$myHv3 location] [$node attr href]]
    tk_popup ${win}.hyperlinkmenu $x $y
  }
  method followlink {} {
    $self goto [$myHyperlinkNode attr href]
  }
  method downloadlink {} {
    set uri [::hv3::resolve_uri [$myHv3 location] [$myHyperlinkNode attr href]]
    $myHttp saveFile $uri
  }
  method copylink {} {
    selection own ${win}.hyperlinkmenu
    selection handle ${win}.hyperlinkmenu [list \
        ::hv3::returnX \
        [::hv3::resolve_uri [$myHv3 location] [$myHyperlinkNode attr href]] \
    ]
  }
  method browselink {} {
    ::HtmlDebug::browse [$myHv3 html] $myHyperlinkNode
  }

  method goto_selection {} {
    $self goto [selection get]
  }

  destructor {
    if {$myHttp ne ""} { $myHttp destroy }
  }

  method motion {nodelist x y} {
    set myX $x
    set myY $y
    set myNodeList $nodelist
    $self update_statusvar
  }

  # Handle an http: get or post request.
  method http {downloadHandle} {
    lappend myHttpHandles $downloadHandle
    trace add command $downloadHandle delete [mymethod http_done]
    $myHttp download $downloadHandle
    $self update_statusvar
  }

  method http_done {handle args} {
    set i [lsearch -exact $myHttpHandles $handle]
    if {$i >= 0} {
      set myHttpHandles [lreplace $myHttpHandles $i $i]
      $self update_statusvar
    }
  }

  method node_to_string {node {hyperlink 1}} {
    set value ""
    for {set n $node} {$n ne ""} {set n [$n parent]} {
      if {[info commands $n] eq ""} break
      set tag [$n tag]
      if {$tag eq ""} {
        set value [$n text]
      } elseif {$hyperlink && $tag eq "a" && [$n attr -default "" href] ne ""} {
        set value "hyper-link: [$n attr href]"
        break
      } elseif {[set nid [$n attr -default "" id]] ne ""} {
        set value "<$tag id=$nid>$value"
      } else {
        set value "<$tag>$value"
      }
    }
    return $value
  }

  method update_statusvar {} {
    if {$options(-statusvar) ne ""} {
      set N [llength $myHttpHandles]
      set requests "$N http requests outstanding   "
      set value [$self node_to_string [lindex $myNodeList end]]
      set str "$requests    ($myX $myY) $value"
      uplevel #0 [list set $options(-statusvar) $str]
    }

    if {$options(-stopbutton) ne ""} {
      set s normal
      if {[llength $myHttpHandles] == 0} {set s disabled}
      $options(-stopbutton) configure -command [mymethod stop] -state $s
    }
  }
 
  method stop {} {
    set myHttpHandles [list]
    $myHv3 stop
    $self update_statusvar
  }

  method gotohistory {uri} {
    set myHttpHandles [list]
    $myHv3 goto $uri
    $self update_statusvar
  }

  method Getcmd {action encdata} {
    set uri "${action}?${encdata}"
    $self goto $uri
  }
  method Postcmd {action encdata} {
    set uri "${action}"
    $myHv3  postdata $encdata
    $self   goto $uri
  }

  #--------------------------------------------------------------------------
  # PUBLIC INTERFACE
  #--------------------------------------------------------------------------

  method goto {uri} {
    set myHttpHandles [list]
    $myHistory add
    $myHv3 goto $uri
    $self update_statusvar
  }

  method browse {} {
    ::HtmlDebug::browse [$myHv3 html] [$myHv3 node]
  }

  # Launch the find dialog.
  method find {} {
    if {[llength [info commands ${win}_finddialog]] == 0} {
      ::hv3::finddialog ${win}_finddialog $myHv3
    }
    raise ${win}_finddialog
  }

  option -statusvar -default ""
  option -stopbutton -default ""

  delegate option -locationvar   to myHistory
  delegate option -historymenu   to myHistory
  delegate option -backbutton    to myHistory
  delegate option -forwardbutton to myHistory

  delegate option -fonttable     to myHv3
  delegate method dumpforms      to myHv3
  delegate method debug_cookies  to myHttp

  delegate option -width         to myHv3
  delegate option -height        to myHv3
}

# This procedure attempts to load the tkcon package. An error is raised
# if the package cannot be loaded. On success, an empty string is returned.
#
proc load_tkcon {} {
  foreach f [list \
    [file join $::tcl_library .. .. bin tkcon] \
    [file join $::tcl_library .. .. bin tkcon.tcl]
  ] {
    if {[file exists $f]} {
      uplevel #0 "source $f"
      package require tkcon
      return 
    }
  }
  error "Failed to load Tkcon"
  return ""
}

proc create_fontsize_menu {menupath hv3path} {
  menu $menupath
  foreach {label table} [list \
    Normal {7 8 9 10 12 14 16} \
    Large  {9 10 11 12 14 16 18} \
    {Very Large}  {11 12 13 14 16 18 20} \
    {Extra Large}  {13 14 15 16 18 20 22} \
    {Recklessly Large}  {15 16 17 18 20 22 24}
  ] {
    $menupath add radiobutton       \
      -variable ::hv3_fonttable_var \
      -value $table               \
      -label $label               \
      -command [list $hv3path configure -fonttable $table]
  }
  return $menupath
}

#--------------------------------------------------------------------------
# gui_build --
#
#     This procedure is called once at the start of the script to build
#     the GUI used by the application. It creates all the widgets for
#     the main window.
#
proc gui_build {} {
  global HTML

  # Create the top bit of the GUI - the URI entry and buttons.
  frame .entry
  entry .entry.entry
  button .entry.back    -text {Back} 
  button .entry.stop    -text {Stop} 
  button .entry.forward -text {Forward}
  pack .entry.back -side left
  pack .entry.stop -side left
  pack .entry.forward -side left
  pack .entry.entry -fill both -expand true
  bind .entry.entry <KeyPress-Return> {.hv3 goto [.entry.entry get]}

  # Create the middle bit - the browser window
  hv3_browser .hv3

  # And the bottom bit - the status bar
  label .status -anchor w -width 1

  # Pack the top, bottom and middle, in that order. The middle must be 
  # packed last, as it is the bit we want to shrink if the size of the 
  # main window is reduced.
  pack .entry -fill x -side top 
  pack .status -fill x -side bottom
  pack .hv3 -fill both -expand true
  focus .hv3

  # Connect the -statusvar and -locationvar values provided by the browser
  # widget to the status-bar and URI entry widgets respectively.
  .hv3 configure -statusvar hv3_status_var
  .hv3 configure -locationvar hv3_location_var
  .entry.entry configure -textvar hv3_location_var
  .status configure -textvar hv3_status_var

  # Attach a menu widget - .m - to the toplevel application window.
  . config -menu [menu .m]

  # Add the 'File menu'
  .m add cascade -label {File} -menu [menu .m.file]
  .m.file add command -label "Open File..." -command guiOpenFile
  .m.file add separator

  # Add the Tkcon, Browser and Cookies entries to the File menu.
  catch {
    # If the [load_tkcon] proc cannot find the Tkcon package, it
    # throws an exception. No menu item will be added in this case.
    load_tkcon
    .m.file add command -label Tkcon -command {tkcon show}
  }
  .m.file add command -label Browser -command [list .hv3 browse]
  .m.file add command -label Cookies -command [list .hv3 debug_cookies]

  # Add a separator and the inevitable Exit item to the File menu.
  .m.file add separator
  .m.file add command -label Exit -command exit

  .m add cascade -label {Edit} -menu [menu .m.edit]
  .m.edit add command -label {Find in page...} -command [list .hv3 find]

  # Add the 'Font Size Table' menu
  set fontsize_menu [create_fontsize_menu .m.edit.font .hv3]
  .m.edit add cascade -label {Font Size Table} -menu $fontsize_menu
  $fontsize_menu invoke 0

  # Add the 'History' menu
  .m add cascade -label {History} -menu [menu .m.history]
  .hv3 configure -historymenu .m.history
  .hv3 configure -backbutton .entry.back
  .hv3 configure -stopbutton .entry.stop
  .hv3 configure -forwardbutton .entry.forward
}

# This procedure is invoked when the user selects the File->Open menu
# option. It launches the standard Tcl file-selector GUI. If the user
# selects a file, then the corresponding URI is passed to [.hv3 goto]
#
proc guiOpenFile {} {
  set f [tk_getOpenFile -filetypes [list \
      {{Html Files} {.html}} \
      {{Html Files} {.htm}}  \
      {{All Files} *}
  ]]
  if {$f != ""} {
    .hv3 goto file://$f 
  }
}

proc Hv3HomeProtocol {http hv3 dir downloadHandle} {
  set fname [string range [$downloadHandle uri] 8 end]
  if {$fname eq ""} {
      set fname index.html
      after idle [list Hv3HomeAfterIdle $http $hv3]
  }
  set fd [open [file join $dir $fname]]
  if {[$downloadHandle binary]} {
    fconfigure $fd -encoding binary -translation binary
  } 
  set data [read $fd]
  close $fd
  $downloadHandle append $data
  $downloadHandle finish
}

proc Hv3HomeAfterIdle {http hv3} {
  trace remove variable ::hv3_home_radio write [list Hv3HomeSetProxy $http $hv3]
  trace remove variable ::hv3_home_port  write [list Hv3HomeSetProxy $http $hv3]
  trace remove variable ::hv3_home_proxy write [list Hv3HomeSetProxy $http $hv3]

  set html [$hv3 html]
  set ::hv3_home_host [$http cget -proxyhost]
  set ::hv3_home_port [$http cget -proxyport]

  foreach node [$html search {span[widget]}] {
    switch [$node attr widget] {
      radio_noproxy {
        set widget [radiobutton ${html}.radio_noproxy]
        $widget configure -variable ::hv3_home_radio -value 1
      }
      radio_configured_proxy {
        set widget [radiobutton ${html}.radio_configured_proxy]
        $widget configure -variable ::hv3_home_radio -value 2
      }
      entry_host {
        set widget [entry ${html}.entry_host -textvar ::hv3_home_host]
      }
      entry_port {
        set widget [entry ${html}.entry_port -textvar ::hv3_home_port]
      }
    }
    $node replace $widget                               \
        -deletecmd [list destroy $widget]               \
        -configurecmd [list Hv3HomeConfigure $widget]
  }
 
  set val 2
  if {$::hv3_home_host eq "" && $::hv3_home_port eq ""} {
    set val 1
    set ::hv3_home_host localhost
    set ::hv3_home_port 8123
  }

  trace add variable ::hv3_home_radio write [list Hv3HomeSetProxy $http $hv3]
  trace add variable ::hv3_home_port  write [list Hv3HomeSetProxy $http $hv3]
  trace add variable ::hv3_home_proxy write [list Hv3HomeSetProxy $http $hv3]

  set ::hv3_home_radio $val
}

proc Hv3HomeConfigure {widget values} {
  array set v $values
  set class [winfo class $widget]

  if {$class eq "Checkbutton" || $class eq "Radiobutton"} {
    catch { $widget configure -background          $v(background-color) }
    catch { $widget configure -highlightbackground $v(background-color) }
    catch { $widget configure -activebackground    $v(background-color) }
    catch { $widget configure -highlightcolor      $v(background-color) }
    $widget configure -padx 0 -pady 0
  }
  catch { $widget configure -font $v(font) }

  $widget configure -borderwidth 0
  $widget configure -highlightthickness 0
  catch { $widget configure -selectborderwidth 0 } 

  set font [$widget cget -font]
  set descent [font metrics $font -descent]
  set ascent  [font metrics $font -ascent]
  set drop [expr ([winfo reqheight $widget] + $descent - $ascent) / 2]
  return $drop
}

proc Hv3HomeSetProxy {http hv3 args} {
  switch $::hv3_home_radio {
    1 {
      $http configure -proxyhost "" -proxyport ""
      set val disabled
    }
    2 {
      $http configure -proxyhost $::hv3_home_host -proxyport $::hv3_home_port
      set val normal
    }
  }

  set html [$hv3 html]
  foreach widget [list ${html}.entry_host ${html}.entry_port] {
    $widget configure -state $val
  }
}


# Override the [exit] command to check if the widget code leaked memory
# or not before exiting.
rename exit tcl_exit
proc exit {args} {
  destroy .hv3
  catch {destroy .prop.hv3}
  catch {::tkhtml::htmlalloc}
  eval [concat tcl_exit $args]
}

# goto uri
#
proc goto {uri} {
  set uri [.hv3 goto $uri]
  #.entry.entry delete 0 end
  #.entry.entry insert 0 $uri
}

# main URL
#
proc main {{doc home:}} {
  # Build the GUI
  gui_build

  # Goto the first document
  goto $doc
}
eval [concat main $argv]
