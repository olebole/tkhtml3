
catch { memory init on }

# Load packages.
if {[info exists auto_path]} {
    set auto_path [concat . $auto_path]
}
package require Tk
package require Tkhtml 3.0

option add *borderWidth 1
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

  variable myHttpOutstanding 0
  variable myNodeList ""                  ;# Current nodes under the pointer

  constructor {args} {
    set myHv3 [hv3 $win.hv3]
    set myHttp [Hv3HttpProtcol %AUTO%]
    set myHistory [::hv3_browser::history %AUTO% $myHv3 [mymethod gotohistory]]
    pack $myHv3 -expand true -fill both

    $myHv3 protocol http [mymethod http]
    $myHv3 configure -hyperlinkcmd [mymethod goto]
    $myHv3 configure -motioncmd [mymethod motion]

    # Set up a binding to press "Q" to exit the application
    bind $myHv3 <KeyPress-q> exit
    bind $myHv3 <KeyPress-Q> exit

    # Create the right-click behaviour - launch the property browser:
    bind $myHv3 <3> [
      subst -nocommands {
        ::HtmlDebug::browse [$myHv3 html] [lindex [$myHv3 node %x %y] 0]
      }
    ]

    focus $myHv3
  }

  destructor {
    if {$myHttp ne ""} { $myHttp destroy }
  }

  method motion {nodelist x y} {
    set myNodeList $nodelist
    $self update_statusvar
  }

  method http {downloadHandle} {
    incr myHttpOutstanding 1
    trace add command $downloadHandle delete [mymethod http_done]
    $myHttp download $downloadHandle
    $self update_statusvar
  }

  method http_done {args} {
    incr myHttpOutstanding -1
    $self update_statusvar
  }

  method update_statusvar {} {
    if {$options(-statusvar) ne ""} {
    set requests "$myHttpOutstanding http requests outstanding   "
    set value ""
      for {set n [lindex $myNodeList 0]} {$n ne ""} {set n [$n parent]} {
        if {[info commands $n] eq ""} break
        set tag [$n tag]
        if {$tag eq ""} {
          set value [$n text]
        } elseif {$tag eq "a" && [$n attr -default "" href] ne ""} {
          set value "hyper-link: [$n attr href]"
          break
        } else {
          set value "<$tag>$value"
        }
      }
      uplevel #0 [subst {set $options(-statusvar) {$requests    $value}}]
    }
  }

  method gotohistory {uri} {
    set myHttpOutstanding 0
    $myHv3 goto $uri
    $self update_statusvar
  }

  #--------------------------------------------------------------------------
  # PUBLIC INTERFACE
  #--------------------------------------------------------------------------

  method goto {uri} {
    set myHttpOutstanding 0
    $myHistory add
    $myHv3 goto $uri
    $self update_statusvar
  }

  method browse {} {
    ::HtmlDebug::browse $myHv3 [$myHv3 node]
  }

  option -statusvar -default ""

  delegate option -locationvar   to myHistory
  delegate option -historymenu   to myHistory
  delegate option -backbutton    to myHistory
  delegate option -forwardbutton to myHistory

  delegate option -fonttable     to myHv3
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
  button .entry.clear   -text {Clear ->} -command {.entry.entry delete 0 end}
  button .entry.back    -text {Back} 
  button .entry.forward -text {Forward}
  pack .entry.back -side left
  pack .entry.forward -side left
  pack .entry.clear -side left
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

  # Build the main window menu.
  . config -menu [menu .m]

  .m add cascade -label {File} -menu [menu .m.file]
  .m.file add command -label "Open File..." -command guiOpenFile
  .m.file add separator
  foreach f [list \
    [file join $::tcl_library .. .. bin tkcon] \
    [file join $::tcl_library .. .. bin tkcon.tcl]
  ] {
    if {[file exists $f]} {
      catch {
        uplevel #0 "source $f"
        package require tkcon
        .m.file add command -label Tkcon -command {tkcon show}
      }
      break
    }
  }
  .m.file add command -label Browser -command [list .hv3 browse]
  .m.file add separator
  .m.file add command -label Exit -command exit

  # Add the 'Font Size Table' menu
  .m add cascade -label {Font Size Table} -menu [menu .m.font]
  foreach {label table} [list \
    Normal {7 8 9 10 12 14 16} \
    Large  {9 10 11 12 14 16 18} \
    {Very Large}  {11 12 13 14 16 18 20} \
    {Extra Large}  {13 14 15 16 18 20 22} \
    {Recklessly Large}  {15 16 17 18 20 22 24}
  ] {
    .m.font add radiobutton       \
      -variable ::hv3_fonttable_var \
      -value $table               \
      -label $label               \
      -command [list .hv3 configure -fonttable $table]
  }
  .m.font invoke 1

  # Add the 'History' menu
  .m add cascade -label {History} -menu [menu .m.history]
  .hv3 configure -historymenu .m.history
  .hv3 configure -backbutton .entry.back
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

snit::type Hv3HttpProtcol {

  option -proxyport -default 3128      -configuremethod _ConfigureProxy
  option -proxyhost -default localhost -configuremethod _ConfigureProxy

  variable myCookies -array [list]

  constructor {} {
    package require http
    $self _ConfigureProxy
  }

  method download {downloadHandle} {
    set uri [$downloadHandle uri]
puts "DOWNLOAD: $uri"
    set finish [mymethod _DownloadCallback $downloadHandle]
    set append [mymethod _AppendCallback $downloadHandle]

    set headers ""
    set authority [$downloadHandle authority]
    if {[info exists myCookies($authority)]} {
      set headers "Cookie "
      foreach cookie $myCookies($authority) {
        lappend headers $cookie
      }
    }

    ::http::geturl $uri -command $finish -handler $append -headers $headers
  }

  # Configure the http package to use a proxy as specified by
  # the -proxyhost and -proxyport options on this object.
  #
  method _ConfigureProxy {} {
    ::http::config -proxyhost $options(-proxyhost)
    ::http::config -proxyport $options(-proxyport)
  }

  # Invoked when data is available from an http request. Pass the data
  # along to hv3 via the downloadHandle.
  #
  method _AppendCallback {downloadHandle socket token} {
    upvar \#0 $token state 
    set data [read $socket 2048]
    $downloadHandle append $data
    set nbytes [string length $data]
    return $nbytes
  }

  # Invoked when an http request has concluded.
  #
  method _DownloadCallback {downloadHandle token} {
    upvar \#0 $token state 

    if {[info exists state(meta)]} {
      foreach {name value} $state(meta) {
        if {$name eq "Set-Cookie"} {
          puts "COOKIE: $value"
          regexp {^[^ ]*} $value nv_pair
          lappend myCookies([$downloadHandle authority]) $nv_pair
        }
      }
      foreach {name value} $state(meta) {
        if {$name eq "Location"} {
          puts "REDIRECT: $value"
          $downloadHandle redirect $value
          $self download $downloadHandle
          return
        }
      }
    } 

    $downloadHandle append $state(body)
    $downloadHandle finish
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
proc main {{doc index.html}} {
  # Build the GUI
  gui_build

  # Goto the first document
  goto $doc
}
eval [concat main $argv]

