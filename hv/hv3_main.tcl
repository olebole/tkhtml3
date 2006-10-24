namespace eval hv3 { set {version($Id: hv3_main.tcl,v 1.88 2006/10/24 13:08:51 danielk1977 Exp $)} 1 }

catch {memory init on}

package require Tk
package require Tkhtml 3.0

# option add *TButton.compound left

# If possible, load package "Img". Without it the script can still run,
# but won't be able to load many image formats. Similarly, try to load
# sqlite3. If sqlite3 is present cookies, auto-completion and 
# coloring of visited URIs work.
#
if {[catch { package require Img } errmsg]} {
  puts stderr "WARNING: $errmsg"
}
if {[catch { package require sqlite3 } errmsg]} {
  puts stderr "WARNING: $errmsg"
}

# Source the other script files that are part of this application.
#
proc sourcefile {file} { return [file join [file dirname [info script]] $file] }
source [sourcefile snit.tcl]
source [sourcefile hv3.tcl]
source [sourcefile hv3_prop.tcl]
source [sourcefile hv3_log.tcl]
source [sourcefile hv3_http.tcl]
source [sourcefile hv3_home.tcl]
source [sourcefile hv3_frameset.tcl]
source [sourcefile hv3_polipo.tcl]
source [sourcefile hv3_icons.tcl]
source [sourcefile hv3_history.tcl]
source [sourcefile hv3_db.tcl]

proc ::hv3::returnX {val args} {return $val}

#--------------------------------------------------------------------------
# Widget ::hv3::browser_frame
#
#     This mega widget is instantiated for each browser frame (a regular
#     html document has one frame, a <frameset> document may have more
#     than one). This widget is not considered reusable - it is designed
#     for the demo web browser only. The following application-specific
#     functionality is added to ::hv3::hv3:
#
#         * The -statusvar option
#         * The right-click menu
#         * Overrides the default -targetcmd supplied by ::hv3::hv3
#           to respect the "target" attribute of <a> and <form> elements.
#
#     For more detail on handling the "target" attribute, see HTML 4.01. 
#     In particular the following from appendix B.8:
# 
#         1. If the target name is a reserved word as described in the
#            normative text, apply it as described.
#         2. Otherwise, perform a depth-first search of the frame hierarchy 
#            in the window that contained the link. Use the first frame whose 
#            name is an exact match.
#         3. If no such frame was found in (2), apply step 2 to each window,
#            in a front-to-back ordering. Stop as soon as you encounter a frame
#            with exactly the same name.
#         4. If no such frame was found in (3), create a new window and 
#            assign it the target name.
#
#     Hv3 currently only implements steps 1 and 2.
#
snit::widget ::hv3::browser_frame {

  component myHv3

  variable myNodeList ""                  ;# Current nodes under the pointer
  variable myX 0                          ;# Current location of pointer
  variable myY 0                          ;# Current location of pointer
  variable myHyperlinkNode ""             ;# Current node for hyper-link menu

  variable myBrowser ""                   ;# ::hv3::browser_toplevel widget
  variable myPositionId ""                ;# See sub-command [positionid]

  # If "Copy Link Location" has been selected, store the selected text
  # (a URI) in variable $myCopiedLinkLocation.
  variable myCopiedLinkLocation ""

  constructor {browser args} {
    set myBrowser $browser
    $self configurelist $args
 
    set myHv3      [::hv3::hv3 $win.hv3]
    pack $myHv3 -expand true -fill both

    ::hv3::the_visited_db init $myHv3

    catch {$myHv3 configure -fonttable $::hv3::fontsize_table}


    # Click to focus (so that this frame accepts keyboard input).

    # Create bindings for motion, right-click and middle-click.
    bind $myHv3 <Motion> +[mymethod motion %x %y]
    bind $myHv3 <3>       [mymethod rightclick %x %y %X %Y]
    bind $myHv3 <2>       [mymethod goto_selection]

    # Create the hyper-link menu (right-click on hyper-link to access)
    set m [::hv3::menu ${win}.hyperlinkmenu]
    foreach {l c} [list                       \
      "Open Link"            openlink         \
      "Open Link in Bg Tab"  opentablink      \
      "Download Link"        downloadlink     \
      "Copy Link Location"   copylink         \
      "Open Tree Browser"    browselink       \
    ] {
      $m add command -label $l -command [mymethod hyperlinkmenu_select $c]
    }

    # When the hyperlink menu "owns" the selection (happens after 
    # "Copy Link Location" is selected), invoke method 
    # [GetCopiedLinkLocation] with no arguments to retrieve it.
    selection handle ${win}.hyperlinkmenu [mymethod GetCopiedLinkLocation]

    # Register a handler command to handle <frameset>.
    set html [$myHv3 html]
    $html handler node frameset [list ::hv3::frameset_handler $self]

    # Add this object to the browsers frames list. It will be removed by
    # the destructor proc. Also override the default -targetcmd
    # option of the ::hv3::hv3 widget with our own version.
    $myBrowser add_frame $self
    $myHv3 configure -targetcmd [mymethod Targetcmd]
  }

  method browser {} {return $myBrowser}

  # The name of this frame (as specified by the "name" attribute of 
  # the <frame> element).
  option -name -default ""

  method Targetcmd {node} {
    set target [$node attr -default "" target]
    if {$target eq ""} {
      # If there is no target frame specified, see if a default
      # target was specified in a <base> tag i.e. <base target="_top">.
      set n [lindex [[$myHv3 html] search base] 0]
      if {$n ne ""} { set target [$n attr -default "" target] }
    }

    set theTopFrame [[lindex [$myBrowser get_frames] 0] hv3]

    # Find the target frame widget.
    set widget $myHv3
    switch -- $target {
      ""        { set widget $myHv3 }
      "_self"   { set widget $myHv3 }
      "_top"    { set widget $theTopFrame }

      "_parent" { 
        set w [winfo parent $myHv3]
        while {$w ne "" && [lsearch [$myBrowser get_frames] $w] < 0} {
          set w [winfo parent $w]
        }
        if {$w ne ""} {
          set widget [$w hv3]
        } else {
          set widget $theTopFrame
        }
      }

      # This is incorrect. The correct behaviour is to open a new
      # top-level window. But hv3 doesn't support this (and because 
      # reasonable people don't like new top-level windows) we load
      # the resource into the "_top" frame instead.
      "_blank"  { set widget $theTopFrame }

      default {
        # In html 4.01, an unknown frame should be handled the same
        # way as "_blank". So this next line of code implements the
        # same bug as described for "_blank" above.
        set widget $theTopFrame

        # TODO: The following should be a depth first search through the
        # frames in the list returned by [get_frames].
        #
        foreach f [$myBrowser get_frames] {
          set n [$f cget -name]
          if {$n eq $target} {
            set widget [$f hv3]
            break
          }
        }
      }
    }

    return $widget
  }

  # This method returns the "position-id" of a frame, an id that is
  # used by the history sub-system when loading a historical state of
  # a frameset document.
  #
  method positionid {} {
    if {$myPositionId eq ""} {
      set w $win
      while {[set p [winfo parent $w]] ne ""} {
        set class [winfo class $p]
        if {$class eq "Panedwindow"} {
          set myPositionId [linsert $myPositionId 0 [lsearch [$p panes] $w]]
        }
        set w $p
      }
      set myPositionId [linsert $myPositionId 0 0 0]
    }
    return $myPositionId
  }

  destructor {
    # Remove this object from the $theFrames list.
    $myBrowser del_frame $self
  }

  # This callback is invoked when the user right-clicks on this 
  # widget. If the mouse cursor is currently hovering over a hyperlink, 
  # popup the hyperlink menu. Otherwise launch the tree browser.
  #
  # Arguments $x and $y are the the current cursor position relative to
  # this widget's window. $X and $Y are the same position relative to
  # the root window.
  #
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

    # No hyper-link. Launch the tree browser.
    ::HtmlDebug::browse $myHv3 [lindex $nodelist 0]
  }

  # Popup the hyper-link menu. Argument $node is the hyperlink node to
  # configure the menu for. Arguments $x and $y are the current position 
  # of the mouse cursor relative to the root window.
  method hyperlinkmenu {node x y} {
    set myHyperlinkNode $node
    # puts [::hv3::resolve_uri [$myHv3 location] [$node attr href]]
    tk_popup ${win}.hyperlinkmenu $x $y
  }

   # Called when an option has been selected on the hyper-link menu. The
   # argument identifies the specific option. May be one of:
   #
   #     openlink
   #     downloadlink
   #     copylink
   #     browselink
   #
  method hyperlinkmenu_select {option} {
    set uri [$myHv3 resolve_uri [string trim [$myHyperlinkNode attr href]]]
    set theTopFrame [lindex [$myBrowser get_frames] 0]
    switch -- $option {
      openlink {
        $theTopFrame goto $uri
      }
      opentablink {
        set new [.notebook addbg $uri]
      }
      downloadlink {
        $myHv3 download $uri
      }
      copylink {
        set myCopiedLinkLocation $uri
        selection own ${win}.hyperlinkmenu
      }
      browselink {
        ::HtmlDebug::browse $myHv3 $myHyperlinkNode
      }
      default {
        error "Internal error"
      }
    }
  }

  method GetCopiedLinkLocation {args} {
    return $myCopiedLinkLocation
  }

  # Called when the user middle-clicks on the widget
  method goto_selection {} {
    set theTopFrame [lindex [$myBrowser get_frames] 0]
    $theTopFrame goto [selection get]
  }

  method motion {x y} {
    set myX $x
    set myY $y
    set myNodeList [$myHv3 node $x $y]
    $self update_statusvar
  }

  method node_to_string {node {hyperlink 1}} {
    set value ""
    for {set n $node} {$n ne ""} {set n [$n parent]} {
      if {[info commands $n] eq ""} break
      set tag [$n tag]
      if {$tag eq ""} {
        set value [$n text]
      } elseif {$hyperlink && $tag eq "a" && [$n attr -default "" href] ne ""} {
        set value "hyper-link: [string trim [$n attr href]]"
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
      set value [$self node_to_string [lindex $myNodeList end]]
      set str "($myX $myY) $value"
      uplevel #0 [list set $options(-statusvar) $str]
    }
  }
 
  #--------------------------------------------------------------------------
  # PUBLIC INTERFACE
  #--------------------------------------------------------------------------

  method goto {args} {
    eval [concat $myHv3 goto $args]
    $self update_statusvar
  }

  # Launch the tree browser
  method browse {} {
    ::HtmlDebug::browse $myHv3 [$myHv3 node]
  }

  method hv3 {} {
    return $myHv3
  }

  option -statusvar        -default ""

  delegate option -doublebuffer     to myHv3
  delegate option -forcefontmetrics to myHv3
  delegate option -fonttable        to myHv3
  delegate option -fontscale        to myHv3
  delegate option -enableimages     to myHv3

  delegate method dumpforms         to myHv3

  delegate option -width         to myHv3
  delegate option -height        to myHv3

  delegate option -requestcmd         to myHv3
  delegate option -resetcmd           to myHv3
  delegate option -cancelrequestcmd   to myHv3
  delegate option -pendingvar         to myHv3

  delegate method stop to myHv3
  delegate method titlevar to myHv3
}

# An instance of this widget represents a top-level browser frame (not
# a toplevel window - an html frame not contained in any frameset 
# document).
#
snit::widget ::hv3::browser_toplevel {

  component myHistory                ;# The undo/redo system
  component myProtocol               ;# The ::hv3::protocol
  component myMainFrame              ;# The browser_frame widget

  # Variables passed to [$myProtocol configure -statusvar] and
  # the same option of $myMainFrame. Used to create the value for 
  # $myStatusVar.
  variable myProtocolStatus ""
  variable myFrameStatus ""

  variable myStatusVar ""
  variable myLocationVar ""

  # List of all ::hv3::browser_frame objects using this object as
  # their toplevel browser. 
  variable myFrames [list]

  method statusvar {}   {return [myvar myStatusVar]}
  delegate method titlevar to myMainFrame

  # Variable passed to the -pendingvar option of the ::hv3::hv3 widget
  # associated with the $myMainFrame frame. Set to true when the 
  # "Stop" button should be enabled, else false.
  #
  # TODO: Frames bug.
  variable myPendingVar 0

  constructor {args} {
    # Create the main browser frame (always present)
    set myMainFrame [::hv3::browser_frame $win.browser_frame $self]
    pack $myMainFrame -expand true -fill both -side top

    # Create the protocol
    set myProtocol [::hv3::protocol %AUTO%]
    $myMainFrame configure -requestcmd       [list $myProtocol requestcmd]
    $myMainFrame configure -cancelrequestcmd [list $myProtocol cancelrequestcmd]
    $myMainFrame configure -pendingvar       [myvar myPendingVar]

    trace add variable [myvar myPendingVar] write [mymethod Setstopbutton]

    $myProtocol configure -statusvar [myvar myProtocolStatus]
    $myMainFrame configure -statusvar [myvar myFrameStatus]
    trace add variable [myvar myProtocolStatus] write [mymethod Writestatus]
    trace add variable [myvar myFrameStatus] write    [mymethod Writestatus]

    # Link in the "home:" and "about:" scheme handlers (from hv3_home.tcl)
    ::hv3::home_scheme_init [$myMainFrame hv3] $myProtocol
    ::hv3::about_scheme_init $myProtocol
    ::hv3::cookies_scheme_init $myProtocol
    ::hv3::download_scheme_init [$myMainFrame hv3] $myProtocol

    # Create the history sub-system
    set myHistory [::hv3::history %AUTO% [$myMainFrame hv3] $myProtocol $self]
    $myHistory configure -gotocmd [mymethod goto]

    $self configurelist $args
  }

  destructor {
    if {$myProtocol ne ""} { $myProtocol destroy }
    if {$myHistory ne ""}  { $myHistory destroy }
  }

  # Interface used by code in class ::hv3::browser_frame for frame management.
  #
  method add_frame {frame} {
    lappend myFrames $frame
    if {$myHistory ne ""} {
      $myHistory add_hv3 [$frame hv3]
    }

    set HTML [[$frame hv3] html]
    bind $HTML <1>               [list focus %W]
    bind $HTML <Escape>          [mymethod Escape]
    bind $HTML <KeyPress-slash>  [mymethod Find]
    bindtags $HTML [concat Hv3HotKeys $self [bindtags $HTML]]
  }
  method del_frame {frame} {
    set idx [lsearch $myFrames $frame]
    if {$idx >= 0} {
      set myFrames [lreplace $myFrames $idx $idx]
    }
  }
  method get_frames {} {return $myFrames}

  method debug_style {} {
    set path ${win}.stylereport
    if {![winfo exists $path]} {
        ::hv3::stylereport $path [[$myMainFrame hv3] html]
    }
    $path update
  }

  # This method is called by a [trace variable ... write] hook attached
  # to the myProtocolStatus variable. Set myStatusVar.
  method Writestatus {args} {
    set myStatusVar "$myProtocolStatus    $myFrameStatus"
  }

  method Setstopbutton {args} {
    if {$options(-stopbutton) ne ""} {
      if {$myPendingVar} { 
        $options(-stopbutton) configure -state normal
      } else {
        $options(-stopbutton) configure -state disabled
      }
      $options(-stopbutton) configure -command [list $myMainFrame stop]
    }
  }
  method Configurestopbutton {option value} {
    set options(-stopbutton) $value
    $self Setstopbutton
  }

  # Escape --
  #
  #     This method is called when the <Escape> key sequence is seen.
  #     Get rid of the "find-text" widget, if it is currently visible.
  #
  method Escape {} {
    catch {
      destroy ${win}.findwidget
    }
  }

  method packwidget {w} {
    pack $w -before $myMainFrame -side bottom -fill x -expand false
    bind $w <Destroy> [list focus [[$myMainFrame hv3] html]]
  }

  # Find --
  #
  #     This method is called when the "find-text" widget is summoned.
  #     Currently this can happen when the users:
  #
  #         * Presses "control-f",
  #         * Presses "/", or
  #         * Selects the "Edit->Find Text" pull-down menu command.
  #
  method Find {} {

    set fdname ${win}.findwidget
    set initval ""
    if {[llength [info commands $fdname]] > 0} {
      set initval [${fdname}.entry get]
      destroy $fdname
    }
  
    ::hv3::findwidget $fdname $self

    $self packwidget $fdname
    $fdname configure -borderwidth 1 -relief raised

    # When the findwidget is destroyed, return focus to the html widget. 
    bind $fdname <Escape>  [list destroy $fdname]

    ${fdname}.entry insert 0 $initval
    focus ${fdname}.entry
  }

  method history {} {
    return $myHistory
  }

  method reload {} {
    $myHistory reload
  }

  option -stopbutton -default "" -configuremethod Configurestopbutton

  delegate option -historymenu   to myHistory
  delegate option -backbutton    to myHistory
  delegate option -forwardbutton to myHistory

  delegate method locationvar to myHistory

  delegate method debug_cookies  to myProtocol

  delegate option * to myMainFrame
  delegate method * to myMainFrame
}

# ::hv3::config
#
#     An instance of this class manages the application "View" menu, which
#     contains all the runtime configuration options (font size, image loading
#     etc.).
#
snit::type ::hv3::config {

  variable myGuiFontSize 11

  variable myFontTable [list 8 9 10 11 13 15 17]
  variable myFontScale 100%
  variable myForceFontMetrics 1
  variable myEnableImages 1
  variable myDoubleBuffer 0

  variable myMenu ""
  
  constructor {menu_path} {
    set myMenu $menu_path
    ::hv3::menu $myMenu

    # Add the 'Gui Font' menu
    create_guifont_menu ${myMenu}.font3 [myvar myGuiFontSize]
    $myMenu add cascade -label {GUI Font} -menu ${myMenu}.font3

    # Add the 'Font Size Table' menu
    create_fontsize_menu ${myMenu}.font [myvar myFontTable]
    $myMenu add cascade -label {Browser Font Size Table} -menu ${myMenu}.font

    # Add the 'Font Scale' menu
    create_fontscale_menu ${myMenu}.font2 [myvar myFontScale]
    $myMenu add cascade -label {Browser Font Scale} -menu ${myMenu}.font2

    $myMenu add checkbutton \
        -label {Force CSS Font Metrics}                 \
        -variable [myvar myForceFontMetrics]            

    $myMenu add checkbutton \
        -label {Enable Images} \
        -variable [myvar myEnableImages]

    if {$::tcl_platform(platform) eq "windows"} {
      set myDoubleBuffer 1
    }
    $myMenu add checkbutton \
        -label {Double-buffer} \
        -variable [myvar myDoubleBuffer]

    trace add variable myGuiFontSize      write [mymethod ConfigureGui]
    trace add variable myFontTable        write [mymethod ConfigureCurrent]
    trace add variable myFontScale        write [mymethod ConfigureCurrent]
    trace add variable myForceFontMetrics write [mymethod ConfigureCurrent]
    trace add variable myEnableImages     write [mymethod ConfigureCurrent]
    trace add variable myDoubleBuffer     write [mymethod ConfigureCurrent]
  }

  method ConfigureGui {name1 name2 op} {
    ::hv3::SetFont [list -size $myGuiFontSize]
  }

  method ConfigureCurrent {name1 name2 op} {
    $self configurebrowser [.notebook current]
  }

  method configurebrowser {b} {
    foreach {option var} [list                      \
        -fonttable myFontTable                      \
        -fontscale myFontScale                      \
        -forcefontmetrics myForceFontMetrics        \
        -enableimages myEnableImages                \
        -doublebuffer myDoubleBuffer                \
    ] {
      if {[$b cget $option] ne [set $var]} {
        $b configure $option [set $var]
      }
    }
  }

  # Return the created menu widget
  method menu {} {
    return $myMenu
  }
}

snit::type ::hv3::search {

  variable myHotKeys [list  \
      {Google}    g         \
      {Tcl Wiki}  w         \
  ]
  
  variable mySearchEngines [list \
      ----------- -                                                        \
      {Google}    "http://www.google.com/search?q=%s"                      \
      {Tcl Wiki}  "http://wiki.tcl.tk/2?Q=%s"                              \
      ----------- -                                                        \
      {Ask.com}   "http://www.ask.com/web?q=%s"                            \
      {MSN}       "http://search.msn.com/results.aspx?q=%s"                \
      {Wikipedia} "http://en.wikipedia.org/wiki/Special:Search?search=%s"  \
      {Yahoo}     "http://search.yahoo.com/search?p=%s"                    \
  ]
  variable myDefaultEngine Google

  variable myMenu
  constructor {menu_path} {
    set myMenu $menu_path
    ::hv3::menu $myMenu

    set findcmd [list gui_current Find] 
    $myMenu add command \
        -label {Find in page...} -command $findcmd -accelerator (Ctrl-F)
    bind Hv3HotKeys <Control-f>  [list gui_current Find]
    bind Hv3HotKeys <Control-F>  [list gui_current Find]

    array set hotkeys $myHotKeys

    foreach {label uri} $mySearchEngines {

      if {[string match ---* $label]} {
        $myMenu add separator
        continue
      }

      $myMenu add command -label $label -command [mymethod search $label]

      if {[info exists hotkeys($label)]} {
        set lc $hotkeys($label)
        set uc [string toupper $hotkeys($label)]
        $myMenu entryconfigure end -accelerator "(Ctrl-$uc)"
        bind Hv3HotKeys <Control-$lc> [mymethod search $label]
        bind Hv3HotKeys <Control-$uc> [mymethod search $label]
      }
    }
  }

  # Return the created menu widget
  method menu {} {
    return $myMenu
  }

  method search {{default ""}} {
    if {$default eq ""} {set default $myDefaultEngine}

    # The currently visible ::hv3::browser_toplevel widget.
    set btl [.notebook current]

    set fdname ${btl}.findwidget
    set initval ""
    if {[llength [info commands $fdname]] > 0} {
      set initval [${fdname}.entry get]
      destroy $fdname
    }

    set conf [list]
    foreach {label uri} $mySearchEngines {
      if {![string match ---* $label]} {
        lappend conf $label $uri
      }
    }
  
    ::hv3::googlewidget $fdname  \
        -getcmd [list $btl goto] \
        -config $conf            \
        -initial $default

    $btl packwidget $fdname
    $fdname configure -borderwidth 1 -relief raised

    # Pressing <Escape> dismisses the search widget.
    bind $fdname <Escape>  [list destroy $fdname]

    ${fdname}.entry insert 0 $initval
    focus ${fdname}.entry
  }
}


#--------------------------------------------------------------------------
# The following functions are all called during startup to construct the
# static components of the web browser gui:
#
#     gui_build
#     gui_menu
#       gui_load_tkcon
#       create_fontsize_menu
#       create_fontscale_menu
#

# gui_build --
#
#     This procedure is called once at the start of the script to build
#     the GUI used by the application. It creates all the widgets for
#     the main window. 
#
#     The argument is the name of an array variable in the parent context
#     into which widget names are written, according to the following 
#     table:
#
#         Array Key            Widget
#     ------------------------------------------------------------
#         stop_button          The "stop" button
#         back_button          The "back" button
#         forward_button       The "forward" button
#         location_entry       The location bar
#         notebook             The ::hv3::notebook instance
#         status_label         The label used for a status bar
#         history_menu         The pulldown menu used for history
#
proc gui_build {widget_array} {
  upvar $widget_array G
  global HTML

  # Create the top bit of the GUI - the URI entry and buttons.
  frame .entry
  ::hv3::locationentry .entry.entry
  ::hv3::toolbutton .entry.back    -text {Back} -tooltip    "Go Back"
  ::hv3::toolbutton .entry.stop    -text {Stop} -tooltip    "Stop"
  ::hv3::toolbutton .entry.forward -text {Forward} -tooltip "Go Forward"

  ::hv3::toolbutton .entry.new -text {New Tab} -command [list .notebook add]
  ::hv3::toolbutton .entry.home -text Home -command {gui_current goto home:}
  ::hv3::toolbutton .entry.reload -text Reload -command {gui_current reload}

  .entry.new configure -tooltip "Open New Tab"
  .entry.home configure -tooltip "Go Home"
  .entry.reload configure -tooltip "Reload Current Document"

  catch {
    set backimg [image create photo -data $::hv3::back_icon]
    .entry.back configure -image $backimg
    set forwardimg [image create photo -data $::hv3::forward_icon]
    .entry.forward configure -image $forwardimg
    set stopimg [image create photo -data $::hv3::stop_icon]
    .entry.stop configure -image $stopimg
    set newimg [image create photo -data $::hv3::new_icon]
    .entry.new configure -image $newimg
    set homeimg [image create photo -data $::hv3::home_icon]
    .entry.home configure -image $homeimg
    set reloadimg [image create photo -data $::hv3::reload_icon]
    .entry.reload configure -image $reloadimg
  }

  # Create the middle bit - the browser window
  #
  ::hv3::notebook .notebook              \
      -newcmd    gui_new                 \
      -switchcmd gui_switch

  # And the bottom bit - the status bar
  ::hv3::label .status -anchor w -width 1

  # Set the widget-array variables
  set G(new_button)     .entry.new
  set G(stop_button)    .entry.stop
  set G(back_button)    .entry.back
  set G(forward_button) .entry.forward
  set G(home_button)    .entry.home
  set G(reload_button)  .entry.reload
  set G(location_entry) .entry.entry
  set G(status_label)   .status
  set G(notebook)       .notebook

  # Pack the elements of the "top bit" into the .entry frame
  pack .entry.new -side left
  pack .entry.back -side left
  pack .entry.forward -side left
  pack .entry.reload -side left
  pack .entry.stop -side left
  pack .entry.home -side left
  pack [frame .entry.spacer -width 2 -height 1] -side left
  pack .entry.entry -fill x -expand true

  # Pack the top, bottom and middle, in that order. The middle must be 
  # packed last, as it is the bit we want to shrink if the size of the 
  # main window is reduced.
  pack .entry -fill x -side top 
  pack .status -fill x -side bottom
  pack .notebook -fill both -expand true
}

proc goto_gui_location {browser entry args} {
  set location [$entry get]
  $browser goto $location
}

# A helper function for gui_menu.
#
# This procedure attempts to load the tkcon package. An error is raised
# if the package cannot be loaded. On success, an empty string is returned.
#
proc gui_load_tkcon {} {
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

# A helper function for gui_menu.
#
# Create a menu widget named $menupath and populate it with entries
# to set the font-size table of the hv3 widget at $hv3path to various
# values (i.e. normal, large etc.).
#
# Return the name of the new menu widget.
#
proc create_fontsize_menu {menupath varname} {
  ::hv3::menu $menupath
  foreach {label table} [list \
    Normal {7 8 9 10 12 14 16} \
    Medium {8 9 10 11 13 15 17} \
    Large  {9 10 11 12 14 16 18} \
    {Very Large}  {11 12 13 14 16 18 20} \
    {Extra Large}  {13 14 15 16 18 20 22} \
    {Recklessly Large}  {15 16 17 18 20 22 24}
  ] {
    $menupath add radiobutton       \
      -variable $varname            \
      -value $table                 \
      -command [list gui_setfontsize $varname] \
      -label $label
  }
  set $varname [list 8 9 10 11 13 15 17]
  return $menupath
}

proc create_guifont_menu {menupath varname} {
  ::hv3::menu $menupath
  foreach val [list 8 9 10 11 12 14 16] {
    $menupath add radiobutton                  \
      -variable $varname                       \
      -value $val                              \
      -label "$val pts"
  }
  return $menupath
}

proc create_fontscale_menu {menupath varname} {
  ::hv3::menu $menupath
  foreach val [list 0.8 0.9 1.0 1.2 1.4 2.0] {
    $menupath add radiobutton                  \
      -variable $varname                       \
      -value $val                              \
      -label [format "%d%%" [expr int($val * 100)]]
  }
  set $varname 1.0
  return $menupath
}

# Invoked when an entry in the font-size menu is selected.
#
proc gui_setfontsize {varname} {
  gui_current configure -fonttable [set $varname]
}

proc gui_setfontscale {varname} {
  gui_current configure -fontscale [set $varname]
}

# Invoked when an entry in the font-size menu is selected.
#
proc gui_setforcefontmetrics {varname} {
  gui_current configure -forcefontmetrics [set $varname]
}

proc gui_openlocation {location_entry} {
  $location_entry selection range 0 end
  $location_entry OpenDropdown *
  focus ${location_entry}.entry
}

# gui_menu
#
proc gui_menu {widget_array} {
  upvar $widget_array G

  # Attach a menu widget - .m - to the toplevel application window.
  . config -menu [::hv3::menu .m]

  # Add the 'File menu'
  .m add cascade -label {File} -menu [::hv3::menu .m.file]
  foreach {label command key} [list \
      "Open File..."  [list guiOpenFile $G(notebook)]            o \
      "Open Tab"      [list $G(notebook) add]                    t \
      "Open Location" [list gui_openlocation $G(location_entry)] l \
  ] {
    set uc [string toupper $key]
    .m.file add command -label $label -command $command -accelerator (Ctrl-$uc)
    bind Hv3HotKeys <Control-$key> $command
    bind Hv3HotKeys <Control-$uc> $command
  }

  .m.file add separator
  .m.file add command -label Downloads -command [
    list ::hv3::the_download_manager show
  ]

  # Add a separator the "Close Tab" command and the inevitable 
  # Exit item to the File menu.
  .m.file add separator
  .m.file add command                      \
      -label "Close Tab"                   \
      -command [list $G(notebook) close]
  set cmd [list .m.file entryconfigure [.m.file index end] -state]
  $G(notebook) configure -delstatecmd $cmd
  .m.file add command -label Exit -accelerator (Ctrl-Q) -command exit
  bind Hv3HotKeys <Control-q>  exit
  bind Hv3HotKeys <Control-Q>  exit

  # Add the 'Search' menu
  set G(search) [::hv3::search %AUTO% .m.search]
  .m add cascade -label {Search} -menu [$G(search) menu]

  # Add the 'Config' menu
  set G(config) [::hv3::config %AUTO% .m.config]
  .m add cascade -label {View} -menu [$G(config) menu]

  # The 'Debug' menu (contains the little tools used to debug hv3/tkhtml3).
  .m add cascade -label Debug -menu [::hv3::menu .m.tools]

  .m.tools add command -label Cookies -command [list $G(notebook) add cookies:]
  .m.tools add command -label Version -command [list $G(notebook) add about:]
  .m.tools add command -label Polipo -command ::hv3::polipo::popup
  catch {
    # If the [gui_load_tkcon] proc cannot find the Tkcon package, it
    # throws an exception. No menu item will be added in this case.
    gui_load_tkcon
    .m.tools add command -label Tkcon -command {tkcon show}
  }
  .m.tools add separator
  .m.tools add command -label Events -command [list gui_log_window $G(notebook)]
  .m.tools add command -label Browser -command [list gui_current browse]
  .m.tools add command -label Style   -command [list gui_current debug_style]

  # Add the 'History' menu
  .m add cascade -label {History} -menu [::hv3::menu .m.history]
  set G(history_menu) .m.history

}
#--------------------------------------------------------------------------

proc gui_current {args} {
  eval [linsert $args 0 [.notebook current]]
}

proc gui_switch {new} {
  upvar #0 ::hv3::G G

  # Loop through *all* tabs and detach them from the history
  # related controls. This is so that when the state of a background
  # tab is updated, the history menu is not updated (only the data
  # structures in the corresponding ::hv3::history object).
  #
  foreach browser [.notebook tabs] {
    $browser configure -historymenu   ""
    $browser configure -backbutton    ""
    $browser configure -stopbutton    ""
    $browser configure -forwardbutton ""
  }

  # Configure the new current tab to control the history controls.
  #
  set new [.notebook current]
  $new configure -historymenu   $G(history_menu)
  $new configure -backbutton    $G(back_button)
  $new configure -stopbutton    $G(stop_button)
  $new configure -forwardbutton $G(forward_button)

  # Attach some other GUI elements to the new current tab.
  #
  set gotocmd [list goto_gui_location $new $G(location_entry)]
  $G(location_entry) configure -command $gotocmd
  $G(status_label) configure -textvar [$new statusvar]
  $G(location_entry) configure -textvar [$new locationvar]

  # Configure the new current tab with the contents of the drop-down
  # config menu (i.e. font-size, are images enabled etc.).
  #
  $G(config) configurebrowser $new

  # Set the top-level window title to the title of the new current tab.
  #
  wm title . [.notebook get_title $new]
}

proc gui_new {path args} {
  set new [::hv3::browser_toplevel $path]

  set var [$new titlevar]
  trace add variable $var write [list gui_settitle $new $var]

  set var [$new locationvar]
  trace add variable $var write [list gui_settitle $new $var]

  if {[llength $args] == 0} {
    $new goto home:
  } else {
    $new goto [lindex $args 0]
  }
  
  # This black magic is required to initialise the history system.
  # A <<Location>> event will be generated from within the [$new goto]
  # command above, but the history system won't see it, because 
  # events are not generated until the window is mapped. So generate
  # an extra <<Location>> when the window is mapped.
  #
  bind [$new hv3] <Map>  [list event generate [$new hv3] <<Location>>]
  bind [$new hv3] <Map> +[list bind <Map> [$new hv3] ""]

  return $new
}

proc gui_settitle {browser var args} {
  if {[.notebook current] eq $browser} {
    wm title . [set $var]
  }
  .notebook set_title $browser [set $var]
}

# This procedure is invoked when the user selects the File->Open menu
# option. It launches the standard Tcl file-selector GUI. If the user
# selects a file, then the corresponding URI is passed to [.hv3 goto]
#
proc guiOpenFile {notebook} {
  set browser [$notebook current]
  set f [tk_getOpenFile -filetypes [list \
      {{Html Files} {.html}} \
      {{Html Files} {.htm}}  \
      {{All Files} *}
  ]]
  if {$f != ""} {
    if {$::tcl_platform(platform) eq "windows"} {
      set f [string map {: {}} $f]
    }
    $browser goto file://$f 
  }
}

proc gui_log_window {notebook} {
  set browser [$notebook current]
  ::hv3::log_window [[$browser hv3] html]
}

# Override the [exit] command to check if the widget code leaked memory
# or not before exiting.
#
rename exit tcl_exit
proc exit {args} {
  destroy .notebook
  catch {destroy .prop.hv3}
  catch {::tkhtml::htmlalloc}
  eval [concat tcl_exit $args]
}

proc ::hv3::scroll {r} {
  set html [[gui_current hv3] html]
  set region [$html yview]
  set max [expr 1.0 - ([lindex $region 1] - [lindex $region 0])]
  ::hv3::scrollcb idle $max 0 60 $r
}
proc ::hv3::scrollcb {delay max ii maxii r} {
  set html [[gui_current hv3] html]
  $html yview moveto [expr {double($ii) * (double($max) / double($maxii))}]
  if {$ii < $maxii} {
    after $delay [list ::hv3::scrollcb $delay $max [expr $ii + 1] $maxii $r] 
  } elseif {$r > 0} {
    after $delay [list ::hv3::scrollcb $delay $max 0 $maxii [expr $r - 1]] 
  }
}

proc ::hv3::nOverflow {} {
  ::hv3::walkTree [[gui_current hv3] node] N {
    if {[$N tag] ne ""} {
      set overflow [$N property overflow]
      if {$overflow ne "visible"} {
        puts "$N: $overflow"
      }
    }
  }
}

#--------------------------------------------------------------------------
# main URI
#
#     The main() program for the application. This proc handles
#     parsing of command line arguments.
#
proc main {args} {
  # Build the GUI
  gui_build     ::hv3::G
  gui_menu      ::hv3::G

  # Default startup page is "home:///"
  set doc ""

  for {set ii 0} {$ii < [llength $args]} {incr ii} {
    set val [lindex $args $ii]
    switch -glob -- $val {
      -s* {                  # -statefile <file-name>
        if {$ii == [llength $args] - 1} ::hv3::usage
        incr ii
        set ::hv3::statefile [lindex $args $ii]
      }
      default {
        if {$doc ne ""} ::hv3::usage
        set doc $val
      }
    }
  }

  if {$doc eq ""} {set doc home:///}

  ::hv3::downloadmanager ::hv3::the_download_manager
  ::hv3::dbinit

  # After the event loop has run to create the GUI, run [main2]
  # to load the startup document. It's better if the GUI is created first,
  # because otherwise if an error occurs Tcl deems it to be fatal.
  after idle [list main2 $doc]
}
proc main2 {doc} {
  set tab [$::hv3::G(notebook) add $doc]
  focus $tab
}
proc ::hv3::usage {} {
  puts stderr "Usage:"
  puts stderr "    $::argv0 ?-statefile <file-name>? ?<uri>?"
  puts stderr ""
  tcl_exit
}

set ::hv3::statefile ":memory:"

# Set variable $::hv3::maindir to the directory containing the 
# application files. Then run the [main] command with the command line
# arguments passed to the application.
set ::hv3::maindir [file dirname [info script]] 
eval [concat main $argv]

#--------------------------------------------------------------------------

