
catch {memory init on}

package require Tk
package require Tkhtml 3.0

# If possible, load package "Img". Without it the script can still run,
# but won't be able to load many image formats.
if {[catch { package require Img } errmsg]} {
  puts stderr "WARNING: $errmsg"
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
sourcefile hv3_home.tcl
sourcefile hv3_frameset.tcl

snit::type ::hv3_browser::history {

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

  constructor {args} {
    $self configurelist $args
 
    set myHv3      [::hv3::hv3 $win.hv3]
    pack $myHv3 -expand true -fill both
    catch {$myHv3 configure -fonttable $::hv3::fontsize_table}

    # Set up a binding to press "Q" to exit the application.
    bind $myHv3 <KeyPress-q> exit
    bind $myHv3 <KeyPress-Q> exit
    bind $myHv3 <Control-f>  [mymethod find]

    # Click to focus (so that this frame accepts keyboard input).
    bind $myHv3 <1>          +[list focus %W]

    # Create bindings for motion, right-click and middle-click.
    bind $myHv3 <Motion> +[mymethod motion %x %y]
    bind $myHv3 <3>       [mymethod rightclick %x %y %X %Y]
    bind $myHv3 <2>       [mymethod goto_selection]

    # Create the hyper-link menu (right-click on hyper-link to access)
    set m [menu ${win}.hyperlinkmenu]
    foreach {l c} [list                     \
      "Open Link"          openlink         \
      "Download Link"      downloadlink     \
      "Copy Link Location" copylink         \
      "Open Tree Browser"  browselink       \
    ] {
      $m add command -label $l -command [mymethod hyperlinkmenu_select $c]
    }

    # Register a handler command to handle <frameset>.
    set html [$myHv3 html]
    $html handler node frameset [list ::hv3::frameset_handler $self]

    # Add this object to the $theFrames list. It will be removed by
    # the destructor proc. Also override the default -hyperlinkcmd
    # option of the ::hv3::hv3 widget with our own version.
    lappend theFrames $self
    if {$theTopFrame eq ""} {
      set theTopFrame $self
    }
    $myHv3 configure -hyperlinkcmd [mymethod Hyperlinkcmd]
  }

  # The following type-variable contains a list of currently instantiated
  # ::hv3::browser_frame objects. This is used to find the correct
  # frame to load linked documents into. See the Hyperlinkcmd method
  # of this widget class for details.
  typevariable theFrames   [list]
  typevariable theTopFrame ""

  # The name of this frame (as specified by the "name" attribute of 
  # the <frame> element).
  option -name -default ""

  method Hyperlinkcmd {node} {
    set href   [$node attr -default "" href]
    set target [$node attr -default "" target]

    if {$target eq ""} {
      # If there is no target frame specified, see if a default
      # target was specified in a <base> tag i.e. <base target="_top">.
      set n [lindex [[$myHv3 html] search base] 0]
      if {$n ne ""} { set target [$n attr -default "" target] }
    }
 
    if {$href ne ""} {
      set href [$myHv3 resolve_uri $href]

      # Find the target frame widget.
      switch -- $target {
        ""        { set widget $self }
        "_self"   { set widget $self }
        "_top"    { set widget $theTopFrame }

        "_parent" { 
          set w [winfo parent $self]
          while {$w ne "" && [lsearch $theFrames $w] < 0} {
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

          foreach f $theFrames {
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
    foreach f $theFrames {
      [$f hv3] configure -fonttable $::hv3::fontsize_table
    }
  }

  destructor {
    # Remove this object from the $theFrames list.
    set idx [lsearch $theFrames $self]
    set theFrames [lreplace $theFrames $idx $idx]
    if {$self eq $theTopFrame} {
      set theTopFrame ""
    }
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
    puts [::hv3::resolve_uri [$myHv3 location] [$node attr href]]
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
    set uri [$myHv3 resolve_uri [$myHyperlinkNode attr href]]
    switch -- $option {
      openlink {
        $theTopFrame goto $uri
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

  # Launch the find dialog.
  method find {} {
    if {[llength [info commands ${win}_finddialog]] == 0} {
      ::hv3::finddialog ${win}_finddialog $myHv3
    }
    raise ${win}_finddialog
  }

  method hv3 {} {
    return $myHv3
  }

  option -statusvar  -default ""

  delegate option -fonttable     to myHv3
  delegate method dumpforms      to myHv3

  delegate option -width         to myHv3
  delegate option -height        to myHv3

  delegate option -requestcmd         to myHv3
  delegate option -cancelrequestcmd   to myHv3
  delegate option -pendingvar         to myHv3

  delegate method stop to myHv3
}

snit::widget ::hv3::browser {

  component myHistory
  component myProtocol
  component myMainFrame 

  # Variable passed to [$myProtocol configure -statusvar]. Used to
  # create the value for the -statusvar variable of this object
  # (see method update_statusvar).
  variable myProtocolStatus ""
  variable myFrameStatus ""

  variable myPendingVar 0

  constructor {args} {
    # Create the main browser frame (always present)
    set myMainFrame [::hv3::browser_frame $win.browser_frame]
    pack $myMainFrame -expand true -fill both

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

    # Link in the "home:" scheme handler (from hv3_home.tcl)
    ::hv3::home_scheme_init [$myMainFrame hv3] $myProtocol

    # Create the history sub-system
    set myHistory [::hv3_browser::history %AUTO% [$myMainFrame hv3]]
    $myHistory configure -gotocmd [mymethod goto]

    $self configurelist $args
  }

  destructor {
    if {$myProtocol ne ""} { $myProtocol destroy }
    if {$myHistory ne ""}  { $myHistory destroy }
  }

  # This method is called by a [trace variable ... write] hook attached
  # to the myProtocolStatus variable. We need to regenerate the
  # -statusvar value for this object whenever this is called.
  method Writestatus {args} {
    if {$options(-statusvar) ne ""} {
      set value "$myProtocolStatus    $myFrameStatus"
      uplevel #0 [list set $options(-statusvar) $value]
    }
  }

  method Setstopbutton {args} {
    if {$options(-stopbutton) ne ""} {
      if {$myPendingVar} { 
        $options(-stopbutton) configure -state normal
      } else {
        $options(-stopbutton) configure -state disabled
      }
    }
  }
  method Configurestopbutton {option value} {
    set options(-stopbutton) $value
    $options(-stopbutton) configure -command [list $myMainFrame stop]
    $self Setstopbutton
  }

  option -stopbutton -default "" -configuremethod Configurestopbutton
  option -statusvar  -default ""

  delegate option -locationvar   to myHistory
  delegate option -historymenu   to myHistory
  delegate option -backbutton    to myHistory
  delegate option -forwardbutton to myHistory

  delegate method debug_cookies  to myProtocol

  delegate option * to myMainFrame
  delegate method * to myMainFrame
}

#--------------------------------------------------------------------------
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
#         browser              The ::hv3::browser instance
#         status_label         The label used for a status bar
#         history_menu         The pulldown menu used for history
#
proc gui_build {widget_array} {
  upvar $widget_array G
  global HTML

  # Create the top bit of the GUI - the URI entry and buttons.
  frame .entry
  entry .entry.entry
  button .entry.back    -text {Back} 
  button .entry.stop    -text {Stop} 
  button .entry.forward -text {Forward}

  # Create the middle bit - the browser window
  ::hv3::browser .browser

  # And the bottom bit - the status bar
  label .status -anchor w -width 1

  # Set the widget-array variables
  set G(stop_button)    .entry.stop
  set G(back_button)    .entry.back
  set G(forward_button) .entry.forward
  set G(location_entry) .entry.entry
  set G(browser)        .browser
  set G(status_label)   .status

  # Pack the elements of the "top bit" into the .entry frame
  pack .entry.back -side left
  pack .entry.stop -side left
  pack .entry.forward -side left
  pack .entry.entry -fill both -expand true

  # Pack the top, bottom and middle, in that order. The middle must be 
  # packed last, as it is the bit we want to shrink if the size of the 
  # main window is reduced.
  pack .entry -fill x -side top 
  pack .status -fill x -side bottom
  pack .browser -fill both -expand true
  focus .browser
}

# A helper function for gui_menu.
#
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

# A helper function for gui_menu.
#
# Create a menu widget named $menupath and populate it with entries
# to set the font-size table of the hv3 widget at $hv3path to various
# values (i.e. normal, large etc.).
#
# Return the name of the new menu widget.
#
proc create_fontsize_menu {menupath varname} {
  menu $menupath
  foreach {label table} [list \
    Normal {7 8 9 10 12 14 16} \
    Large  {9 10 11 12 14 16 18} \
    {Very Large}  {11 12 13 14 16 18 20} \
    {Extra Large}  {13 14 15 16 18 20 22} \
    {Recklessly Large}  {15 16 17 18 20 22 24}
  ] {
    $menupath add radiobutton       \
      -variable $varname            \
      -value $table                 \
      -label $label
  }
  trace add variable $varname write ::hv3::browser_frame::SetFontTable
  set $varname [list 7 8 9 10 12 14 16]
  return $menupath
}


proc gui_menu {widget_array} {
  upvar $widget_array G

  # Attach a menu widget - .m - to the toplevel application window.
  . config -menu [menu .m]

  # Add the 'File menu'
  .m add cascade -label {File} -menu [menu .m.file]
  set openfilecmd [list guiOpenFile $G(browser)]
  .m.file add command -label "Open File..." -command $openfilecmd
  .m.file add separator

  # Add the Tkcon, Browser and Cookies entries to the File menu.
  catch {
    # If the [load_tkcon] proc cannot find the Tkcon package, it
    # throws an exception. No menu item will be added in this case.
    load_tkcon
    .m.file add command -label Tkcon -command {tkcon show}
  }
  .m.file add command -label Browser -command [list $G(browser) browse]
  .m.file add command -label Cookies -command [list $G(browser) debug_cookies]

  # Add a separator and the inevitable Exit item to the File menu.
  .m.file add separator
  .m.file add command -label Exit -command exit

  # Add the "Edit" menu and "Find..." function
  .m add cascade -label {Edit} -menu [menu .m.edit]
  .m.edit add command -label {Find in page...} -command [list $G(browser) find]

  # Add the 'Font Size Table' menu
  set fontsize_menu [create_fontsize_menu .m.edit.font ::hv3::fontsize_table]
  .m.edit add cascade -label {Font Size Table} -menu $fontsize_menu

  # Add the 'History' menu
  .m add cascade -label {History} -menu [menu .m.history]
  set G(history_menu) .m.history
}

proc gui_configure {widget_array} {
  upvar $widget_array G

  # Binding for hitting enter in the location entry field.
  set gotocmd "$G(browser) goto \[$G(location_entry) get\]"
  bind $G(location_entry) <KeyPress-Return> $gotocmd

  # Connect the -statusvar and -locationvar values provided by the browser
  # widget to the status-bar and URI entry widgets respectively.
  $G(browser) configure -statusvar hv3_status_var
  $G(browser) configure -locationvar hv3_location_var
  $G(location_entry) configure -textvar hv3_location_var
  $G(status_label) configure -textvar hv3_status_var

  # Configure the browser controls
  $G(browser) configure -historymenu   $G(history_menu)
  $G(browser) configure -backbutton    $G(back_button)
  $G(browser) configure -stopbutton    $G(stop_button)
  $G(browser) configure -forwardbutton $G(forward_button)
}

# This procedure is invoked when the user selects the File->Open menu
# option. It launches the standard Tcl file-selector GUI. If the user
# selects a file, then the corresponding URI is passed to [.hv3 goto]
#
proc guiOpenFile {browser} {
  set f [tk_getOpenFile -filetypes [list \
      {{Html Files} {.html}} \
      {{Html Files} {.htm}}  \
      {{All Files} *}
  ]]
  if {$f != ""} {
    $browser goto file://$f 
  }
}

# Override the [exit] command to check if the widget code leaked memory
# or not before exiting.
#
rename exit tcl_exit
proc exit {args} {
  destroy $::hv3::G(browser)
  catch {destroy .prop.hv3}
  catch {::tkhtml::htmlalloc}
  eval [concat tcl_exit $args]
}

# main URI
#
proc main {{doc home:}} {
  
  # Build the GUI
  gui_build     ::hv3::G
  gui_menu      ::hv3::G
  gui_configure ::hv3::G
 
  # Goto the first document
  after idle [list $::hv3::G(browser) goto $doc]
}

# Kick off main()
eval [concat main $argv]

