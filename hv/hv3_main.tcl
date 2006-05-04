
catch { memory init on }

# Load packages.
if {[info exists auto_path]} {
    # set auto_path [concat . $auto_path]
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

# A cookie manager manages cookies. It supports the following 
# operations:
#
#     * Add cookie to database,
#     * Retrieve applicable cookies for an http request, and
#     * Delete the contents of the cookie database.
#
# Also, by invoking [pathName debug] a GUI to inspect and manipulate the
# database is created in a new top-level window.
#
snit::type ::hv3_browser::cookiemanager {

  variable myDebugWindow

  # The cookie data is stored in the following array variable. The
  # array keys are authority names. The array values are the list of cookies
  # associated with the authority. Each list element (a single cookie) is 
  # stored as a list of two elements, the cookie name and value. For
  # example, a two cookies from tkhtml.tcl.tk might be added to the database
  # using code such as:
  #
  #     set myCookies(tkhtml.tcl.tk) [list {login qwertyuio} {prefs 1234567}
  # 
  variable myCookies -array [list]

  constructor {} {
    set myDebugWindow [string map {: _} ".${self}_toplevel"]
  }

  method add_cookie {authority name value} {
    if {0 == [info exists myCookies($authority)]} {
      set myCookies($authority) [list]
    }

    array set cookies $myCookies($authority)
    set cookies($name) $value
    set myCookies($authority) [array get cookies]

    if {[winfo exists $myDebugWindow]} {$self debug}
  }

  # Retrieve the cookies that should be sent to the specified authority.
  # The cookies are returned as a string of the following form:
  #
  #     "NAME1=OPAQUE_STRING1; NAME2=OPAQUE_STRING2 ..."
  #
  method get_cookies {authority} {
    set ret ""
    if {[info exists myCookies($authority)]} {
      foreach {name value} $myCookies($authority) {
        append ret [format "%s=%s; " $name $value]
      }
    }
    return $ret
  }

  method get_report {} {
    set Template {
      <html><head><style>$Style</style></head>
      <body>
        <h1>Hv3 Cookies</h1>
        <p>
	  <b>Note:</b> This window is automatically updated when Hv3's 
	  internal cookies database is modified in any way. There is no need to
          close and reopen the window to refresh it's contents.
        </p>
        <div id="clear"/>
        <br clear=all>
        $Content
      </body>
      <html>
    }

    set Style {
      .authority { margin-top: 2em; font-weight: bold; }
      .name      { padding-right: 5ex; }
      #clear { 
        float: left; 
        margin: 1em; 
        margin-top: 0px; 
      }
    }

    set Content ""
    if {[llength [array names myCookies]] > 0} {
      append Content "<table>"
      foreach authority [array names myCookies] { 
        append Content "<tr><td><div class=authority>$authority</div>"
        foreach {name value} $myCookies($authority) {
          append Content [subst {
            <tr>
              <td><span class=name>$name</span>
              <td><span class=value>$value</span>
          }]
        }
      }
      append Content "</table>"
    } else {
      set Content {
        <p>The cookies database is currently empty.
      }
    }

    return [subst $Template]
  }

  method download_report {downloadHandle} {
    $downloadHandle append [$self get_report]
    $downloadHandle finish
  }

  method debug {} {
    set path $myDebugWindow
    if {![winfo exists $path]} {
      toplevel $path
      ::hv3::scrolled hv3 ${path}.hv3
      ${path}.hv3 configure -width 400 -height 400
      pack ${path}.hv3 -expand true -fill both
      set HTML [${path}.hv3 html]

      # The "clear database button"
      button ${HTML}.clear   -text "Clear Database" -command [subst {
        array unset [myvar myCookies]
        [mymethod debug]
      }]
    }
    raise $path
    ${path}.hv3 protocol report [mymethod download_report]
    ${path}.hv3 postdata POSTME!
    ${path}.hv3 goto report://

    set HTML [${path}.hv3 html]
    [lindex [${path}.hv3 search #clear] 0] replace ${HTML}.clear
  }
}

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

  variable myHttpHandles [list]
  variable myNodeList ""                  ;# Current nodes under the pointer

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

    # Create the right-click behaviour - launch the property browser:
    bind $myHv3 <3> [
      subst -nocommands {
        ::HtmlDebug::browse [$myHv3 html] [lindex [$myHv3 node %x %y] 0]
      }
    ]

    # Create the middle-click behaviour - launch the property browser:
    bind $myHv3 <2> [mymethod goto_selection]

    focus $myHv3
  }

  method goto_selection {} {
    $self goto [selection get]
  }

  destructor {
    if {$myHttp ne ""} { $myHttp destroy }
  }

  method motion {nodelist x y} {
    set myNodeList $nodelist
    $self update_statusvar
  }

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
      uplevel #0 [subst {set $options(-statusvar) {$requests    $value}}]
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

  catch {
    # If the [load_tkcon] proc cannot find the Tkcon package, it
    # throws an exception. No menu item will be added in this case.
    load_tkcon
    .m.file add command -label Tkcon -command {tkcon show}
  }

  .m.file add command -label Browser -command [list .hv3 browse]
  .m.file add command -label "Debug Forms" -command [list .hv3 dumpforms]
  .m.file add command -label "Debug Cookies" -command [list .hv3 debug_cookies]
  .m.file add separator
  .m.file add command -label Exit -command exit

  # Add the 'Font Size Table' menu
  set fontsize_menu [create_fontsize_menu .m.font .hv3]
  .m add cascade -label {Font Size Table} -menu $fontsize_menu
  $fontsize_menu invoke 1

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

snit::type Hv3HttpProtcol {

  option -proxyport -default 8123      -configuremethod _ConfigureProxy
  option -proxyhost -default localhost -configuremethod _ConfigureProxy

  # variable myCookies -array [list]

  variable myCookieManager ""

  constructor {args} {
    package require http
    $self configurelist $args
    $self _ConfigureProxy proxyport $options(-proxyport)
    set myCookieManager [::hv3_browser::cookiemanager %AUTO%]
  }

  destructor {
    if {$myCookieManager ne ""} {$myCookieManager destroy}
  }

  method download {downloadHandle} {
    set uri [$downloadHandle uri]
    set finish [mymethod _DownloadCallback $downloadHandle]
    set append [mymethod _AppendCallback $downloadHandle]

    set headers ""
    set authority [$downloadHandle authority]
#    if {[info exists myCookies($authority)]} {
#      set headers "Cookie "
#      foreach cookie $myCookies($authority) {
#        lappend headers $cookie
#      }
#    }

    set cookies [$myCookieManager get_cookies $authority]
    if {$cookies ne ""} {
      set headers [list Cookie $cookies]
    }

    set postdata [$downloadHandle postdata]

    if {$postdata ne ""} {
      ::http::geturl $uri     \
          -command $finish    \
          -handler $append    \
          -headers $headers   \
          -query   $postdata
    } else {
      ::http::geturl $uri -command $finish -handler $append -headers $headers
    }
  }

  # Configure the http package to use a proxy as specified by
  # the -proxyhost and -proxyport options on this object.
  #
  method _ConfigureProxy {option value} {
    set options($option) $value
    ::http::config -proxyhost $options(-proxyhost)
    ::http::config -proxyport $options(-proxyport)
    ::http::config -useragent {Mozilla/5.0 Gecko/20050513}
    set ::http::defaultCharset utf-8
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
          regexp {^([^= ]*)=([^ ;]*)} $value dummy name value
          $myCookieManager add_cookie [$downloadHandle authority] $name $value
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

  method debug_cookies {} {
    $myCookieManager debug
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
