namespace eval hv3 { set {version($Id: hv3_main.tcl,v 1.66 2006/09/07 11:03:02 danielk1977 Exp $)} 1 }

catch {memory init on}

package require Tk
package require Tkhtml 3.0

# option add *TButton.compound left

# If possible, load package "Img". Without it the script can still run,
# but won't be able to load many image formats.
if {[catch { package require Img } errmsg]} {
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

snit::type ::hv3::history {

  # The following two variables store the history list
  variable myHistoryList [list]
  variable myCurrentPosition -1

  # The variable passed to [$hv3 configure -locationvar] and the
  # corresponding option exported by this class.
  #
  variable myLocationVar ""
  option -locationvar -default ""

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

  constructor {hv3widget args} {
    $hv3widget configure -locationvar [myvar myLocationVar]
    $self configurelist $args

    set cmd [mymethod locationvarcmd]
    trace add variable [myvar myLocationVar] write [mymethod locationvarcmd]
  }

  # Invoked whenever the [::hv3::hv3 configure -locationvar] variable
  # is modified. This is the point where entries may be added to the 
  # history list.
  # 
  method locationvarcmd {args} {
    # If one exists, modify the -locationvar option variable
    if {$options(-locationvar) ne ""} {
      uplevel #0 [list set $options(-locationvar) $myLocationVar]
    }

    # Do nothing if the new URI is the same as the current history
    # list entry.
    set newuri $myLocationVar
    if {[lindex $myHistoryList $myCurrentPosition] eq $newuri} return

    # Otherwise, if the new URI is not the same as the next entry
    # in the history list (which may not even exist, if $myCurrentPosition
    # points to the last entry in the history list) truncat the history
    # list at the current point and append the new URI.
    #
    # Whether we do this or not, increment myCurrentPosition, so that
    # myCurrentPosition points at the history list entry that contains
    # the same as $myLocationVar.
    if {[lindex $myHistoryList [expr $myCurrentPosition+1]] ne $newuri} {
      set myHistoryList [lrange $myHistoryList 0 $myCurrentPosition]
      lappend myHistoryList $myLocationVar
    }
    incr myCurrentPosition

    $self populatehistorymenu
  }

  method setoption {option value} {
    set options($option) $value
    switch -- $option {
      -locationvar   { uplevel #0 [list set $value $myLocationVar] }
      -historymenu   { $self populatehistorymenu }
      -forwardbutton { $self populatehistorymenu }
      -backbutton    { $self populatehistorymenu }
    }
  }

  method gotohistory {idx} {
    set myCurrentPosition $idx
    eval [linsert $options(-gotocmd) end [lindex $myHistoryList $idx]]
    $self populatehistorymenu
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
    set menu $options(-historymenu)
    set back $options(-backbutton)
    set forward $options(-forwardbutton)
    if {$menu ne ""} {
      $menu delete 0 end
      set idx [expr [llength $myHistoryList] - 15]
      if {$idx < 0} {set idx 0}
      for {} {$idx < [llength $myHistoryList]} {incr idx} {
        set item [lindex $myHistoryList $idx]
        $menu add radiobutton                       \
          -label $item                              \
          -variable [myvar myCurrentPosition]       \
          -value    $idx                            \
          -command [mymethod gotohistory $idx]
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
#         * Overrides the default -hyperlinkcmd supplied by ::hv3::hv3
#           to respect the "target" attribute of <a> elements.
#
snit::widget ::hv3::browser_frame {

  component myHv3

  variable myNodeList ""                  ;# Current nodes under the pointer
  variable myX 0                          ;# Current location of pointer
  variable myY 0                          ;# Current location of pointer
  variable myHyperlinkNode ""             ;# Current node for hyper-link menu

  variable myBrowser ""                   ;# ::hv3::browser_toplevel widget

  constructor {browser args} {
    set myBrowser $browser
    $self configurelist $args
 
    set myHv3      [::hv3::hv3 $win.hv3]
    pack $myHv3 -expand true -fill both

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

    # Register a handler command to handle <frameset>.
    set html [$myHv3 html]
    $html handler node frameset [list ::hv3::frameset_handler $self]

    # Register a handler command for <title> elements.
    $html handler node title [mymethod TitleNodeHandler]

    # Add this object to the browsers frames list. It will be removed by
    # the destructor proc. Also override the default -hyperlinkcmd
    # option of the ::hv3::hv3 widget with our own version.
    $myBrowser add_frame $self

    $myHv3 configure -hyperlinkcmd [mymethod Hyperlinkcmd]
  }

  method browser {} {return $myBrowser}

  # System for handling <title> elements. This object exports
  # a method [titlevar] that returns a globally valid variable name
  # to a variable used to store the string that should be displayed as the
  # "title" of this document. The idea is that the caller add a trace
  # to that variable.
  #
  variable myTitleVar ""
  method titlevar {}    {return [myvar myTitleVar]}
  method TitleNodeHandler {node} {
    set val ""
    foreach child [$node children] {
      append val [$child text]
    }
    set myTitleVar $val
  }

  # The name of this frame (as specified by the "name" attribute of 
  # the <frame> element).
  option -name -default ""

  method Hyperlinkcmd {node} {
    set href   [string trim [$node attr -default "" href]]
    set target [$node attr -default "" target]

    if {$target eq ""} {
      # If there is no target frame specified, see if a default
      # target was specified in a <base> tag i.e. <base target="_top">.
      set n [lindex [[$myHv3 html] search base] 0]
      if {$n ne ""} { set target [$n attr -default "" target] }
    }
 
    set theTopFrame [lindex [$myBrowser get_frames] 0]

    if {$href ne ""} {
      set href [$myHv3 resolve_uri $href]

      # Find the target frame widget.
      switch -- $target {
        ""        { set widget $self }
        "_self"   { set widget $self }
        "_top"    { set widget $theTopFrame }

        "_parent" { 
          set w [winfo parent $self]
          while {$w ne "" && [lsearch [$myBrowser get_frames] $w] < 0} {
            set w [winfo parent $w]
          }
          if {$w ne ""} {
            set widget $w
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

          foreach f [$myBrowser get_frames] {
            set n [$f cget -name]
            if {$n eq $target} {
              set widget $f
              break
            }
          }
        }
      }

      # Load the specified resource.
      $widget goto $href
    }
  }

  proc SetFontTable {args} {
if 0 {
    foreach f [$myBrowser get_frames] {
      [$f hv3] configure -fonttable $::hv3::fontsize_table
    }
}
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
        set new [.notebook addbg]
        $new goto $uri
      }
      downloadlink {
        $myHv3 download $uri
      }
      copylink {
        selection own ${win}.hyperlinkmenu
        selection handle ${win}.hyperlinkmenu [list ::hv3::returnX $uri]
      }
      browselink {
        ::HtmlDebug::browse $myHv3 $myHyperlinkNode
      }
      default {
        error "Internal error"
      }
    }
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

  method goto {uri} {
    $myHv3 goto $uri
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
  delegate option -cancelrequestcmd   to myHv3
  delegate option -pendingvar         to myHv3

  delegate method stop to myHv3
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

  method statusvar {}   {return [myvar myStatusVar]}
  method locationvar {} {return [myvar myLocationVar]}
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

    # Create the history sub-system
    set myHistory [::hv3::history %AUTO% [$myMainFrame hv3]]
    $myHistory configure -gotocmd [mymethod goto]
    $myHistory configure -locationvar [myvar myLocationVar]
    
    # Configure application hotkeys and so forth. To make these
    # work in frameset documents, the [bindtags] command must be
    # used to add the tag "$self" to the html widget for every 
    # frame in the frameset.
    bind $self <Escape>          [mymethod Escape]
    bind $self <Control-f>       [mymethod Find find]
    bind $self <KeyPress-slash>  [mymethod Find find]
    bind $self <KeyPress-q>      exit
    bind $self <KeyPress-Q>      exit
    bind $self <1>               +[list focus %W]

    # Todo: The following [bindtags] trick for all html widgets in a 
    # frameset document.
    set HTML [[$myMainFrame hv3] html]
    bindtags $HTML [concat Hv3HotKeys $self [bindtags $HTML]]

    $self configurelist $args
  }

  destructor {
    if {$myProtocol ne ""} { $myProtocol destroy }
    if {$myHistory ne ""}  { $myHistory destroy }
  }

  # Interface used by code in class ::hv3::browser_frame for frame management.
  #
  variable myFrames [list]
  method add_frame {frame} {lappend myFrames $frame}
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
  method Find {t} {

    set fdname ${win}.findwidget

    if {[llength [info commands $fdname]] == 0} {
  
      switch $t {
        find { 
          ::hv3::findwidget $fdname [$myMainFrame hv3] 
        }
        google { 
          ::hv3::googlewidget $fdname -getcmd [mymethod goto]
        }
      }

      $self packwidget $fdname
      $fdname configure -borderwidth 1 -relief raised

      # When the findwidget is destroyed, return focus to the html widget. 
      bind $fdname <Escape>  [list destroy $fdname]
    }
    focus ${fdname}.entry
  }

  option -stopbutton -default "" -configuremethod Configurestopbutton

  delegate option -historymenu   to myHistory
  delegate option -backbutton    to myHistory
  delegate option -forwardbutton to myHistory

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

  variable myFontTable [list 8 9 10 11 13 15 17]
  variable myFontScale 100%
  variable myForceFontMetrics 1
  variable myEnableImages 1
  variable myDoubleBuffer 0

  variable myMenu ""
  
  constructor {menu_path} {
    set myMenu $menu_path
    ::hv3::menu $myMenu

    # Add the 'Font Size Table' menu
    create_fontsize_menu ${myMenu}.font [myvar myFontTable]
    $myMenu add cascade -label {Font Size Table} -menu ${myMenu}.font

    # Add the 'Font Scale' menu
    create_fontscale_menu ${myMenu}.font2 [myvar myFontScale]
    $myMenu add cascade -label {Font Scale} -menu ${myMenu}.font2

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

    trace add variable myFontTable        write [mymethod ConfigureCurrent]
    trace add variable myFontScale        write [mymethod ConfigureCurrent]
    trace add variable myForceFontMetrics write [mymethod ConfigureCurrent]
    trace add variable myEnableImages     write [mymethod ConfigureCurrent]
    trace add variable myDoubleBuffer     write [mymethod ConfigureCurrent]
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
      {Tcl Wiki}  t         \
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

    set findcmd [list gui_current Find find]
    $myMenu add command \
        -label {Find in page...} -command $findcmd -accelerator (Ctrl-F)

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

      foreach {label2 event} $myHotKeys {
        if {$label2 eq $label} {
        }
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
    if {[llength [info commands $fdname]] > 0} return

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
  ::hv3::entry .entry.entry

  ::hv3::toolbutton .entry.back    -text {Back} -tooltip    "Go Back"
  ::hv3::toolbutton .entry.stop    -text {Stop} -tooltip    "Stop"
  ::hv3::toolbutton .entry.forward -text {Forward} -tooltip "Go Forward"

  ::hv3::toolbutton .entry.new -text {New Tab} -command {.notebook add}
  ::hv3::toolbutton .entry.del -text {Close Tab} -command {.notebook close}

  .entry.new configure -tooltip "Open New Tab"
  .entry.del configure -tooltip "Close Current Tab"

  catch {
    set backimg [image create photo -data $::hv3::back_icon]
    .entry.back configure -image $backimg
    set forwardimg [image create photo -data $::hv3::forward_icon]
    .entry.forward configure -image $forwardimg
    set stopimg [image create photo -data $::hv3::stop_icon]
    .entry.stop configure -image $stopimg
    set newimg [image create photo -data $::hv3::new_icon]
    .entry.new configure -image $newimg
    set delimg [image create photo -data $::hv3::del_icon]
    .entry.del configure -image $delimg
  } 

  # Create the middle bit - the browser window
  # ::hv3::browser_toplevel .browser
  ::hv3::notebook .notebook              \
      -newcmd    gui_new                 \
      -switchcmd gui_switch              \
      -delbutton .entry.del       

  # And the bottom bit - the status bar
  ::hv3::label .status -anchor w -width 1

  # Set the widget-array variables
  set G(new_button)     .entry.new
  set G(close_button)   .entry.del
  set G(stop_button)    .entry.stop
  set G(back_button)    .entry.back
  set G(forward_button) .entry.forward
  set G(location_entry) .entry.entry
  set G(status_label)   .status
  set G(notebook)       .notebook

  # Pack the elements of the "top bit" into the .entry frame
  pack .entry.new -side left
  pack .entry.back -side left
  pack .entry.stop -side left
  pack .entry.forward -side left
  pack .entry.del -side right
  pack .entry.entry -fill x -expand true

  # Pack the top, bottom and middle, in that order. The middle must be 
  # packed last, as it is the bit we want to shrink if the size of the 
  # main window is reduced.
  pack .entry -fill x -side top 
  pack .status -fill x -side bottom
  pack .notebook -fill both -expand true
}

proc goto_gui_location {browser entry} {
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
  # trace add variable $varname write ::hv3::browser_frame::SetFontTable
  set $varname [list 8 9 10 11 13 15 17]
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
  # trace add variable $varname write ::hv3::browser_frame::SetFontTable
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

# gui_menu
#
proc gui_menu {widget_array} {
  upvar $widget_array G

  # Attach a menu widget - .m - to the toplevel application window.
  . config -menu [::hv3::menu .m]

  # Add the 'File menu'
  .m add cascade -label {File} -menu [::hv3::menu .m.file]
  set openfilecmd [list guiOpenFile $G(notebook)]
  .m.file add command -label "Open File..." -command $openfilecmd
  .m.file add separator

  # Add the Polipo, Tkcon, Browser and Cookies entries to the File menu.
  .m.file add command -label Polipo -command ::hv3::polipo::popup
  catch {
    # If the [gui_load_tkcon] proc cannot find the Tkcon package, it
    # throws an exception. No menu item will be added in this case.
    gui_load_tkcon
    .m.file add command -label Tkcon -command {tkcon show}
  }
  .m.file add command -label Events -command [list gui_log_window $G(notebook)]

  .m.file add command -label Browser -command [list gui_current browse]
  .m.file add command -label Cookies -command [list gui_current debug_cookies]
  .m.file add command -label Style   -command [list gui_current debug_style]

  # Add a separator and the inevitable Exit item to the File menu.
  .m.file add separator
  .m.file add command -label Exit -command exit

  # Add the 'Search' menu
  set G(search) [::hv3::search %AUTO% .m.search]
  .m add cascade -label {Search} -menu [$G(search) menu]

  # Add the 'Config' menu
  set G(config) [::hv3::config %AUTO% .m.config]
  .m add cascade -label {View} -menu [$G(config) menu]

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

  set old [.notebook current]
  # Detach the GUI elements from ::hv3::browser_toplevel $old 
  if {$old ne ""} {
    $new configure -historymenu   ""
    $new configure -backbutton    ""
    $new configure -stopbutton    ""
    $new configure -forwardbutton ""
  }

  # Attach the GUI elements to ::hv3::browser_toplevel $new
  if {$new ne ""} {
    $new configure -historymenu   $G(history_menu)
    $new configure -backbutton    $G(back_button)
    $new configure -stopbutton    $G(stop_button)
    $new configure -forwardbutton $G(forward_button)

    # Binding for hitting enter in the location entry field.
    set gotocmd [list goto_gui_location $new $G(location_entry)]
    bind $G(location_entry) <KeyPress-Return> $gotocmd

    $G(status_label) configure -textvar [$new statusvar]
    $G(location_entry) configure -textvar [$new locationvar]
  }

  # Configure the new widget
  $G(config) configurebrowser $new
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

  return $new
}

proc gui_settitle {browser var args} {
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
#     The main() program for the application.
#
proc main {{doc home:}} {
  # Build the GUI
  gui_build     ::hv3::G
  gui_menu      ::hv3::G

#  ::hv3::cookiemanager ::hv3::the_cookie_manager
#  ::hv3::the_cookie_manager Import cookies.txt

  # After the event loop has run to create the GUI, run [main2]
  # to load the startup document. It's better if the GUI is created first,
  # because otherwise if an error occurs Tcl deems it to be fatal.
  after idle [list main2 $doc]
}
proc main2 {doc} {
  set tab [$::hv3::G(notebook) add $doc]
  focus $tab
}

# Set variable $::hv3::maindir to the directory containing the 
# application files. Then run the [main] command with the command line
# arguments passed to the application.
set ::hv3::maindir [file dirname [info script]] 
eval [concat main $argv]
#--------------------------------------------------------------------------

