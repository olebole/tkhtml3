namespace eval hv3 { set {version($Id: hv3_main.tcl,v 1.113 2007/01/06 09:47:59 danielk1977 Exp $)} 1 }

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
source [sourcefile hv3_string.tcl]

#--------------------------------------------------------------------------
# Widget ::hv3::browser_frame
#
#     This mega widget is instantiated for each browser frame (a regular
#     html document has one frame, a <frameset> document may have more
#     than one). This widget is not considered reusable - it is designed
#     for the web browser only. The following application-specific
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
    # $myHv3 configure -downloadcmd [list $myBrowser savehandle]
    $myHv3 configure -downloadcmd [list ::hv3::the_download_manager savehandle]

    # Create bindings for motion, right-click and middle-click.
    bind $myHv3 <Motion> +[mymethod motion %x %y]
    bind $myHv3 <3>       [mymethod rightclick %x %y %X %Y]
    bind $myHv3 <2>       [mymethod goto_selection]

    # When the hyperlink menu "owns" the selection (happens after 
    # "Copy Link Location" is selected), invoke method 
    # [GetCopiedLinkLocation] with no arguments to retrieve it.

    # Register a handler command to handle <frameset>.
    set html [$myHv3 html]
    $html handler node frameset [list ::hv3::frameset_handler $self]
    $html handler node iframe [list ::hv3::iframe_handler $self]

    # Add this object to the browsers frames list. It will be removed by
    # the destructor proc. Also override the default -targetcmd
    # option of the ::hv3::hv3 widget with our own version.
    $myBrowser add_frame $self
    $myHv3 configure -targetcmd [mymethod Targetcmd]

    ::hv3::menu ${win}.hyperlinkmenu
    selection handle ${win}.hyperlinkmenu [mymethod GetCopiedLinkLocation]
  }

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
    catch {destroy ${win}.hyperlinkmenu}
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

    set m ${win}.hyperlinkmenu
    $m delete 0 end

    set nodelist [$myHv3 node $x $y]

    set a_href ""
    set img_src ""
    set select [$myHv3 selected]
    set leaf ""

    foreach leaf $nodelist {
      for {set N $leaf} {$N ne ""} {set N [$N parent]} {
        set tag [$N tag]

        if {$a_href eq "" && $tag eq "a"} {
          set a_href [$N attr -default "" href]
        }
        if {$img_src eq "" && $tag eq "img"} {
          set img_src [$N attr -default "" src]
        }

      }
    }

    if {$a_href ne ""}  {set a_href [$myHv3 resolve_uri $a_href]}
    if {$img_src ne ""} {set img_src [$myHv3 resolve_uri $img_src]}

    set MENU [list \
      a_href "Open Link"             [mymethod menu_select open $a_href]      \
      a_href "Open Link in Bg Tab"   [mymethod menu_select opentab $a_href]   \
      a_href "Download Link"         [mymethod menu_select download $a_href]  \
      a_href "Copy Link Location"    [mymethod menu_select copy $a_href]      \
      a_href --                      ""                                       \
      img_src "View Image"           [mymethod menu_select open $img_src]     \
      img_src "View Image in Bg Tab" [mymethod menu_select opentab $img_src]  \
      img_src "Download Image"       [mymethod menu_select download $img_src] \
      img_src "Copy Image Location"  [mymethod menu_select copy $img_src]     \
      img_src --                     ""                                       \
      select  "Copy Selected Text"   [mymethod menu_select copy $select]      \
      select  --                     ""                                       \
      leaf    "Open Tree browser..." [list ::HtmlDebug::browse $myHv3 $leaf]  \
    ]

    foreach {var label cmd} $MENU {
      if {$var eq "" || [set $var] ne ""} {
        if {$label eq "--"} {
          $m add separator
        } else {
          $m add command -label $label -command $cmd
        }
      }
    }

    $::hv3::G(config) populate_hidegui_entry $m
    $m add separator

    # Add the "File", "Search", "View" and "Debug" menus.
    foreach sub [list File Search View Debug History] {
      catch {
        set menu_widget $m.[string tolower $sub]
        gui_populate_menu $sub [::hv3::menu $menu_widget]
      }
      $m add cascade -label $sub -menu $menu_widget -underline 0
    }

    tk_popup $m $X $Y
  }

   # Called when an option has been selected on the hyper-link menu. The
   # argument identifies the specific option. May be one of:
   #
   #     open
   #     opentab
   #     download
   #     copy
   #
  method menu_select {option uri} {
    switch -- $option {
      open { 
        set top_frame [lindex [$myBrowser get_frames] 0]
        $top_frame goto $uri 
      }
      opentab { set new [.notebook addbg $uri] }
      download { $myBrowser saveuri $uri }
      copy {
        set myCopiedLinkLocation $uri
        selection own ${win}.hyperlinkmenu
        clipboard clear
        clipboard append $uri
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

  method hv3     {} { return $myHv3 }
  method browser {} { return $myBrowser }

  # The [isframeset] method returns true if this widget instance has
  # been used to parse a frameset document (widget instances may parse
  # either frameset or regular HTML documents).
  #
  method isframeset {} {
    # When a <FRAMESET> tag is parsed, a node-handler in hv3_frameset.tcl
    # creates a widget to manage the frames and then uses [place] to 
    # map it on top of the html widget created by this ::hv3::browser_frame
    # widget. Todo: It would be better if this code was in the same file
    # as the node-handler, otherwise this test is a bit obscure.
    #
    set html [[$self hv3] html]
    set slaves [place slaves $html]
    set isFrameset [expr {[llength $slaves] > 0}]
    return $isFrameset
  }

  option -statusvar        -default ""

  delegate option -doublebuffer     to myHv3
  delegate option -forcefontmetrics to myHv3
  delegate option -fonttable        to myHv3
  delegate option -fontscale        to myHv3
  delegate option -zoom             to myHv3
  delegate option -enableimages     to myHv3

  delegate method dumpforms         to myHv3

  delegate option -width         to myHv3
  delegate option -height        to myHv3

  delegate option -requestcmd         to myHv3
  delegate option -resetcmd           to myHv3
  delegate option -pendingvar         to myHv3

  delegate method stop to myHv3
  delegate method titlevar to myHv3
  delegate method javascriptlog to myHv3
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
  # TODO: Frames bug?
  variable myPendingVar 0

  constructor {args} {
    # Create the main browser frame (always present)
    set myMainFrame [::hv3::browser_frame $win.browser_frame $self]
    pack $myMainFrame -expand true -fill both -side top

    # Create the protocol
    set myProtocol [::hv3::protocol %AUTO%]
    $myMainFrame configure -requestcmd       [list $myProtocol requestcmd]
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

  # This method is called to activate the download-manager to download
  # the specified URI ($uri) to the local file-system.
  #
  method saveuri {uri} {
    set handle [::hv3::download %AUTO%              \
        -uri         $uri                           \
        -mimetype    application/gzip               \
    ]
    $handle configure \
        -incrscript [list ::hv3::the_download_manager savehandle $handle] \
        -finscript  [list ::hv3::the_download_manager savehandle $handle]

    $myProtocol requestcmd $handle
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

  # This method is called by a [trace variable ... write] hook attached
  # to the myProtocolStatus variable. Set myStatusVar.
  method Writestatus {args} {
    set myStatusVar "$myProtocolStatus    $myFrameStatus"
  }

  method Setstopbutton {args} {
    if {$options(-stopbutton) ne ""} {
      if {$myPendingVar} { 
        $options(-stopbutton) configure -state normal
        $hull configure -cursor watch
      } else {
        $options(-stopbutton) configure -state disabled
        $hull configure -cursor ""
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
  method escape {} {
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

    # Bind up, down, next and prior key-press events to scroll the
    # main hv3 widget. This means you can use the keyboard to scroll
    # window (vertically) without shifting focus from the 
    # find-as-you-type box.
    #
    set hv3 [$self hv3]
    bind ${fdname} <KeyPress-Up>    [list $hv3 yview scroll -1 units]
    bind ${fdname} <KeyPress-Down>  [list $hv3 yview scroll  1 units]
    bind ${fdname} <KeyPress-Next>  [list $hv3 yview scroll  1 pages]
    bind ${fdname} <KeyPress-Prior> [list $hv3 yview scroll -1 pages]

    # When the findwidget is destroyed, return focus to the html widget. 
    bind ${fdname} <KeyPress-Escape> gui_escape

    ${fdname}.entry insert 0 $initval
    focus ${fdname}.entry
  }

  method history {} {
    return $myHistory
  }

  method reload {} {
    $myHistory reload
  }

  delegate method populate_history_menu to myHistory as populate_menu

  option -stopbutton -default "" -configuremethod Configurestopbutton

  delegate option -backbutton    to myHistory
  delegate option -forwardbutton to myHistory
  delegate option -locationentry to myHistory

  delegate method locationvar to myHistory
  delegate method populatehistorymenu to myHistory

  delegate method debug_cookies  to myProtocol

  delegate option * to myMainFrame
  delegate method * to myMainFrame
}

# ::hv3::config
#
#     An instance of this class manages the application "View" menu, 
#     which contains all the runtime configuration options (font size, 
#     image loading etc.).
#
snit::type ::hv3::config {

  foreach {opt def type} [list \
    -doublebuffer     0                         Boolean \
    -enableimages     1                         Boolean \
    -forcefontmetrics 1                         Boolean \
    -hidegui          0                         Boolean \
    -zoom             1.0                       Double  \
    -fontscale        1.0                       Double  \
    -guifont          11                        Integer \
    -fonttable        [list 8 9 10 11 13 15 17] SevenIntegers \
  ] {
    option $opt -default $def -validatemethod $type -configuremethod SetOption
  }
  
  constructor {args} {
    if {$::tcl_platform(platform) eq "windows"} {
      set options(-doublebuffer) 1
    }
    $self configurelist $args
  }

  method populate_menu {path} {


    # Add the 'Gui Font (size)' menu
    ::hv3::menu ${path}.guifont
    $self PopulateRadioMenu ${path}.guifont -guifont [list \
        8      "8 pts" \
        9      "9 pts" \
        10    "10 pts" \
        11    "11 pts" \
        12    "12 pts" \
        14    "14 pts" \
        16    "16 pts" \
    ]
    $path add cascade -label {Gui Font} -menu ${path}.guifont

    $self populate_hidegui_entry $path
    $path add separator

    # Add the 'Zoom' menu
    ::hv3::menu ${path}.zoom
    $self PopulateRadioMenu ${path}.zoom -zoom [list \
        0.25    25% \
        0.5     50% \
        0.75    75% \
        0.87    87% \
        1.0    100% \
        1.131  113% \
        1.25   125% \
        1.5    150% \
        2.0    200% \
    ]
    $path add cascade -label {Browser Zoom} -menu ${path}.zoom

    # Add the 'Font Scale' menu
    ::hv3::menu ${path}.fontscale
    $self PopulateRadioMenu ${path}.fontscale -fontscale [list \
        0.8     80% \
        0.9     90% \
        1.0    100% \
        1.2    120% \
        1.4    140% \
        2.0    200% \
    ]
    $path add cascade -label {Browser Font Scale} -menu ${path}.fontscale
      
    # Add the 'Font Size Table' menu
    set fonttable [::hv3::menu ${path}.fonttable]
    $self PopulateRadioMenu $fonttable -fonttable [list \
        {7 8 9 10 12 14 16}    "Normal"            \
        {8 9 10 11 13 15 17}   "Medium"            \
        {9 10 11 12 14 16 18}  "Large"             \
        {11 12 13 14 16 18 20} "Very Large"        \
        {13 14 15 16 18 20 22} "Extra Large"       \
        {15 16 17 18 20 22 24} "Recklessly Large"  \
    ]
    $path add cascade -label {Browser Font Size Table} -menu $fonttable

    foreach {option label} [list \
        -forcefontmetrics "Force CSS Font Metrics" \
        -enableimages     "Enable Images" \
        -doublebuffer     "Double-buffer" \
    ] {
      set var [myvar options($option)]
      set cmd [mymethod Reconfigure $option]
      $path add checkbutton -label $label -variable $var -command $cmd
    }

    $path add separator
    $path add checkbutton \
        -label {Enable ECMAscript} \
        -variable ::hv3::dom::use_scripting_option
    if {![::hv3::dom::have_scripting]} {
      $path entryconfigure end -state disabled
    }
  }

  method populate_hidegui_entry {path} {
    $path add checkbutton -label "Hide Gui" -variable [myvar options(-hidegui)]
    $path entryconfigure end -command [mymethod Reconfigure -hidegui]
  }

  method PopulateRadioMenu {path option config} {
    foreach {val label} $config {
      $path add radiobutton                      \
        -variable [myvar options($option)]       \
        -value $val                              \
        -command [mymethod Reconfigure $option]  \
        -label $label } }

  method Reconfigure {option} {
    $self configure $option $options($option)
  }

  method Boolean {option value} {
    if {![string is boolean $value]} { error "Bad boolean value: $value" }
  }
  method Double {option value} {
    if {![string is double $value]} { error "Bad double value: $value" }
  }
  method Integer {option value} {
    if {![string is integer $value]} { error "Bad integer value: $value" }
  }
  method SevenIntegers {option value} {
    set len [llength $value]
    if {$len != 7} { error "Bad seven-integers value: $value" }
    foreach elem $value {
      if {![string is integer $elem]} { 
        error "Bad seven-integers value: $value"
      }
    }
  }

  method SetOption {option value} {
    set options($option) $value

    switch -- $option {
      -hidegui {
        if {$value} {
          . configure -menu ""
          pack forget .status
          pack forget .toolbar
        } else {
          . configure -menu .m
          pack .status -after .notebook -fill x -side bottom
          pack .toolbar -before .notebook -fill x -side top
        }
      }
      -guifont {
        ::hv3::SetFont [list -size $options(-guifont)]
      }
      default {
        $self configurebrowser [.notebook current]
      } 
    }
  }

  method configurebrowser {b} {
    foreach {option var} [list                       \
        -fonttable        options(-fonttable)        \
        -fontscale        options(-fontscale)        \
        -zoom             options(-zoom)             \
        -forcefontmetrics options(-forcefontmetrics) \
        -enableimages     options(-enableimages)     \
        -doublebuffer     options(-doublebuffer)     \
    ] {
      if {[$b cget $option] ne [set $var]} {
        $b configure $option [set $var]
      }
    }
  }
}

snit::type ::hv3::search {

  typevariable SearchHotKeys -array [list  \
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

  constructor {} {
    bind Hv3HotKeys <Control-f>  [list gui_current Find]
    bind Hv3HotKeys <Control-F>  [list gui_current Find]
    foreach {label} [array names SearchHotKeys] {
      set lc $SearchHotKeys($label)
      set uc [string toupper $SearchHotKeys($label)]
      bind Hv3HotKeys <Control-$lc> [mymethod search $label]
      bind Hv3HotKeys <Control-$uc> [mymethod search $label]
    }
  }

  method populate_menu {path} {
    set cmd [list gui_current Find] 
    set acc (Ctrl-F)
    $path add command -label {Find in page...} -command $cmd -accelerator $acc

    foreach {label uri} $mySearchEngines {
      if {[string match ---* $label]} {
        $path add separator
        continue
      }

      $path add command -label $label -command [mymethod search $label]

      if {[info exists SearchHotKeys($label)]} {
        set acc "(Ctrl-[string toupper $SearchHotKeys($label)])"
        $path entryconfigure end -accelerator $acc
      }
    }
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
    bind ${fdname}.entry <KeyPress-Escape> gui_escape

    ${fdname}.entry insert 0 $initval
    focus ${fdname}.entry
  }
}

snit::type ::hv3::file_menu {

  variable MENU

  constructor {} {
    set MENU [list \
      "Open File..."  [list gui_openfile $::hv3::G(notebook)]           o  \
      "Open Tab"      [list $::hv3::G(notebook) add]                    t  \
      "Open Location" [list gui_openlocation $::hv3::G(location_entry)] l  \
      "-----"         ""                                                "" \
      "Downloads"     [list ::hv3::the_download_manager show]           "" \
      "-----"         ""                                                "" \
      "Close Tab"     [list $::hv3::G(notebook) close]                  "" \
      "Exit"          exit                                              q  \
    ]
  }

  method populate_menu {path} {
    $path delete 0 end

    foreach {label command key} $MENU {
      if {[string match ---* $label]} {
        $path add separator
        continue
      }
      $path add command -label $label -command $command 
      if {$key ne ""} {
        set acc "(Ctrl-[string toupper $key])"
        $path entryconfigure end -accelerator $acc
      }
    }

    if {[llength [$::hv3::G(notebook) tabs]] < 2} {
      $path entryconfigure "Close Tab" -state disabled
    }
  }

  method setup_hotkeys {} {
    foreach {label command key} $MENU {
      if {$key ne ""} {
        set uc [string toupper $key]
        bind Hv3HotKeys <Control-$key> $command
        bind Hv3HotKeys <Control-$uc> $command
      }
    }
  }
}

snit::type ::hv3::debug_menu {

  variable MENU

  constructor {} {
    set MENU [list \
      "Cookies"       [list $::hv3::G(notebook) add cookies:]           "" \
      "About"         [list $::hv3::G(notebook) add about:]             "" \
      "Polipo..."     ::hv3::polipo::popup                              "" \
      "-----"         ""                                                "" \
      "Events..."     [list gui_log_window $::hv3::G(notebook)]         "" \
      "Browser..."    [list gui_current browse]                         "" \
      "Javascript..." [list gui_current javascriptlog]                  j \
      "-----"         ""                                                "" \
      "firefox -remote" gui_firefox_remote                              "" \
    ]
  }

  method populate_menu {path} {
    $path delete 0 end
    foreach {label command key} $MENU {
      if {[string match ---* $label]} {
        $path add separator
        continue
      }
      $path add command -label $label -command $command 
      if {$key ne ""} {
        set acc "(Ctrl-[string toupper $key])"
        $path entryconfigure end -accelerator $acc
      }
    }
  }

  method setup_hotkeys {} {
    foreach {label command key} $MENU {
      if {$key ne ""} {
        set uc [string toupper $key]
        bind Hv3HotKeys <Control-$key> $command
        bind Hv3HotKeys <Control-$uc> $command
      }
    }
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
  frame .toolbar
  frame .toolbar.b
  ::hv3::locationentry .toolbar.entry
  ::hv3::toolbutton .toolbar.b.back    -text {Back} -tooltip    "Go Back"
  ::hv3::toolbutton .toolbar.b.stop    -text {Stop} -tooltip    "Stop"
  ::hv3::toolbutton .toolbar.b.forward -text {Forward} -tooltip "Go Forward"

  ::hv3::toolbutton .toolbar.b.new -text {New Tab} -command [list .notebook add]
  ::hv3::toolbutton .toolbar.b.home -text Home -command [list \
      gui_current goto $::hv3::homeuri
  ]
  ::hv3::toolbutton .toolbar.b.reload -text Reload -command {gui_current reload}

  .toolbar.b.new configure -tooltip "Open New Tab"
  .toolbar.b.home configure -tooltip "Go Home"
  .toolbar.b.reload configure -tooltip "Reload Current Document"

  catch {
    set backimg [image create photo -data $::hv3::back_icon]
    .toolbar.b.back configure -image $backimg
    set forwardimg [image create photo -data $::hv3::forward_icon]
    .toolbar.b.forward configure -image $forwardimg
    set stopimg [image create photo -data $::hv3::stop_icon]
    .toolbar.b.stop configure -image $stopimg
    set newimg [image create photo -data $::hv3::new_icon]
    .toolbar.b.new configure -image $newimg
    set homeimg [image create photo -data $::hv3::home_icon]
    .toolbar.b.home configure -image $homeimg
    set reloadimg [image create photo -data $::hv3::reload_icon]
    .toolbar.b.reload configure -image $reloadimg
  }

  # Create the middle bit - the browser window
  #
  ::hv3::notebook .notebook              \
      -newcmd    gui_new                 \
      -switchcmd gui_switch

  # And the bottom bit - the status bar
  ::hv3::label .status -anchor w -width 1

  # Set the widget-array variables
  set G(new_button)     .toolbar.b.new
  set G(stop_button)    .toolbar.b.stop
  set G(back_button)    .toolbar.b.back
  set G(forward_button) .toolbar.b.forward
  set G(home_button)    .toolbar.b.home
  set G(reload_button)  .toolbar.b.reload
  set G(location_entry) .toolbar.entry
  set G(status_label)   .status
  set G(notebook)       .notebook

  # Pack the elements of the "top bit" into the .entry frame
  pack .toolbar.b.new -side left
  pack .toolbar.b.back -side left
  pack .toolbar.b.forward -side left
  pack .toolbar.b.reload -side left
  pack .toolbar.b.stop -side left
  pack .toolbar.b.home -side left
  pack [frame .toolbar.b.spacer -width 2 -height 1] -side left

  pack .toolbar.b -side left
  pack .toolbar.entry -fill x -expand true

  # Pack the top, bottom and middle, in that order. The middle must be 
  # packed last, as it is the bit we want to shrink if the size of the 
  # main window is reduced.
  pack .toolbar -fill x -side top 
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

proc gui_openlocation {location_entry} {
  $location_entry selection range 0 end
  $location_entry OpenDropdown *
  focus ${location_entry}.entry
}

proc gui_populate_menu {eMenu menu_widget} {
  switch -- [string tolower $eMenu] {
    file {
      set cmd [list $::hv3::G(file_menu) populate_menu $menu_widget]
      $menu_widget configure -postcommand $cmd
    }

    search {
      $::hv3::G(search) populate_menu $menu_widget
    }

    view {
      $::hv3::G(config) populate_menu $menu_widget
    }

    debug {
      $::hv3::G(debug_menu) populate_menu $menu_widget
    }

    history {
      set cmd [list gui_current populate_history_menu $menu_widget]
      $menu_widget configure -postcommand $cmd
    }

    default {
      error "gui_populate_menu: No such menu: $eMenu"
    }
  }
}

proc gui_menu {widget_array} {
  upvar $widget_array G

  # Attach a menu widget - .m - to the toplevel application window.
  . config -menu [::hv3::menu .m]

  set G(file_menu)  [::hv3::file_menu %AUTO%]
  set G(debug_menu) [::hv3::debug_menu %AUTO%]
  set G(search)     [::hv3::search %AUTO%]
  set G(config)     [::hv3::config %AUTO%]

  # Add the "File", "Search" and "View" menus.
  foreach m [list File Search View Debug History] {
    set menu_widget .m.[string tolower $m]
    gui_populate_menu $m [::hv3::menu $menu_widget]
    .m add cascade -label $m -menu $menu_widget -underline 0
  }

  $G(file_menu) setup_hotkeys
  $G(debug_menu) setup_hotkeys
}
#--------------------------------------------------------------------------

proc gui_current {args} {
  eval [linsert $args 0 [.notebook current]]
}

proc gui_firefox_remote {} {
  set url [.toolbar.entry get]
  exec firefox -remote "openurl($url,new-tab)"
}

proc gui_switch {new} {
  upvar #0 ::hv3::G G

  # Loop through *all* tabs and detach them from the history
  # related controls. This is so that when the state of a background
  # tab is updated, the history menu is not updated (only the data
  # structures in the corresponding ::hv3::history object).
  #
  foreach browser [.notebook tabs] {
    $browser configure -backbutton    ""
    $browser configure -stopbutton    ""
    $browser configure -forwardbutton ""
    $browser configure -locationentry ""
  }

  # Configure the new current tab to control the history controls.
  #
  set new [.notebook current]
  $new configure -backbutton    $G(back_button)
  $new configure -stopbutton    $G(stop_button)
  $new configure -forwardbutton $G(forward_button)
  $new configure -locationentry $G(location_entry)

  # Attach some other GUI elements to the new current tab.
  #
  set gotocmd [list goto_gui_location $new $G(location_entry)]
  $G(location_entry) configure -command $gotocmd
  $G(status_label) configure -textvar [$new statusvar]

  # Configure the new current tab with the contents of the drop-down
  # config menu (i.e. font-size, are images enabled etc.).
  #
  $G(config) configurebrowser $new

  # Set the top-level window title to the title of the new current tab.
  #
  wm title . [.notebook get_title $new]

  # Focus on the root HTML widget of the new tab.
  #
  focus [[$new hv3] html]
}

proc gui_new {path args} {
  set new [::hv3::browser_toplevel $path]

  set var [$new titlevar]
  trace add variable $var write [list gui_settitle $new $var]

  set var [$new locationvar]
  trace add variable $var write [list gui_settitle $new $var]

  if {[llength $args] == 0} {
    $new goto $::hv3::homeuri
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

  # [[$new hv3] html] configure -logcmd print

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
proc gui_openfile {notebook} {
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

proc gui_escape {} {
  upvar ::hv3::G G
  gui_current escape
  $G(location_entry) escape
  focus [[gui_current hv3] html]
}
bind Hv3HotKeys <KeyPress-Escape> gui_escape

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

proc JS {args} {
  set script [join $args " "]
  [[gui_current hv3] dom] javascript $script
}

#--------------------------------------------------------------------------
# main URI
#
#     The main() program for the application. This proc handles
#     parsing of command line arguments.
#
proc main {args} {

  # Default startup page is "home:///"
  set docs [list]

  for {set ii 0} {$ii < [llength $args]} {incr ii} {
    set val [lindex $args $ii]
    switch -glob -- $val {
      -s* {                  # -statefile <file-name>
        if {$ii == [llength $args] - 1} ::hv3::usage
        incr ii
        set ::hv3::statefile [lindex $args $ii]
      }
      default {
        lappend docs $val
      }
    }
  }

  if {[llength $docs] == 0} {set docs [list home:///]}
  set ::hv3::homeuri [lindex $docs 0]

  # Build the GUI
  gui_build     ::hv3::G
  gui_menu      ::hv3::G

  ::hv3::downloadmanager ::hv3::the_download_manager
  ::hv3::dbinit

  # After the event loop has run to create the GUI, run [main2]
  # to load the startup document. It's better if the GUI is created first,
  # because otherwise if an error occurs Tcl deems it to be fatal.
  after idle [list main2 $docs]
}
proc main2 {docs} {
  foreach doc $docs {
    set tab [$::hv3::G(notebook) add $doc]
  }
  focus $tab
}
proc ::hv3::usage {} {
  puts stderr "Usage:"
  puts stderr "    $::argv0 ?-statefile <file-name>? ?<uri>?"
  puts stderr ""
  tcl_exit
}

set ::hv3::statefile ":memory:"

# Remote scaling interface:
proc hv3_zoom      {newval} { $::hv3::G(config) set_zoom $newval }
proc hv3_fontscale {newval} { $::hv3::G(config) set_fontscale $newval }
proc hv3_forcewidth {forcewidth width} { 
  [[gui_current hv3] html] configure -forcewidth $forcewidth -width $width
}

proc hv3_guifont {newval} { $::hv3::G(config) set_guifont $newval }

proc hv3_html {args} { 
  set html [[gui_current hv3] html]
  eval [concat $html $args]
}

# Set variable $::hv3::maindir to the directory containing the 
# application files. Then run the [main] command with the command line
# arguments passed to the application.
set ::hv3::maindir [file dirname [info script]] 
eval [concat main $argv]

proc print {args} { puts [join $args] }

#--------------------------------------------------------------------------

