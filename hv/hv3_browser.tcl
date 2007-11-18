
package require sqlite3
package require Tkhtml 3.0

proc sourcefile {file} [string map              \
  [list %HV3_DIR% [file dirname [info script]]] \
{ 
  return [file join {%HV3_DIR%} $file] 
}]

source [sourcefile hv3_profile.tcl]
source [sourcefile snit.tcl]
source [sourcefile hv3_widgets.tcl]
source [sourcefile hv3_encodings.tcl]
source [sourcefile hv3_db.tcl]
source [sourcefile hv3_home.tcl]
source [sourcefile hv3.tcl]
source [sourcefile hv3_prop.tcl]
source [sourcefile hv3_log.tcl]
source [sourcefile hv3_http.tcl]
source [sourcefile hv3_frameset.tcl]
source [sourcefile hv3_polipo.tcl]
source [sourcefile hv3_icons.tcl]
source [sourcefile hv3_history.tcl]
source [sourcefile hv3_string.tcl]
source [sourcefile hv3_bookmarks.tcl]
source [sourcefile hv3_bugreport.tcl]
source [sourcefile hv3_debug.tcl]

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

  variable myBrowser ""                   ;# ::hv3::browser widget
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
    $myHv3 Subscribe motion [list $self motion]
    bind $myHv3 <3>       [list $self rightclick %x %y %X %Y]
    bind $myHv3 <2>       [list $self goto_selection]

    # When the hyperlink menu "owns" the selection (happens after 
    # "Copy Link Location" is selected), invoke method 
    # [GetCopiedLinkLocation] with no arguments to retrieve it.

    # Register a handler command to handle <frameset>.
    set html [$myHv3 html]
    $html handler node frameset [list ::hv3::frameset_handler $self]

    $html handler node      iframe [list ::hv3::iframe_handler $self]
    $html handler attribute iframe [list ::hv3::iframe_attr_handler $self]

    # Add this object to the browsers frames list. It will be removed by
    # the destructor proc. Also override the default -targetcmd
    # option of the ::hv3::hv3 widget with our own version.
    $myBrowser add_frame $self
    $myHv3 configure -targetcmd [list $self Targetcmd]

    ::hv3::menu ${win}.hyperlinkmenu
    selection handle ${win}.hyperlinkmenu [list $self GetCopiedLinkLocation]
  }

  # The name of this frame (as specified by the "name" attribute of 
  # the <frame> element).
  option -name -default "" -configuremethod ConfigureName

  # If this [::hv3::browser_frame] is used as a replacement object
  # for an <iframe> element, then this option is set to the Tkhtml3
  # node-handle for that <iframe> element.
  #
  option -iframe -default ""

  method ConfigureName {-name value} {
    # This method is called when the "name" of attribute of this
    # frame is modified. If javascript is enabled we have to update
    # the properties on the parent window object (if any).
    set dom [$self cget -dom]
    if {$dom ne "" && [$dom cget -enable]} {
      set parent [$self parent_frame]
      if {$parent ne ""} {
        set parent_window [list ::hv3::DOM::Window $dom [$parent hv3]]
        set this_win [list ::hv3::DOM::Window $dom $myHv3]
        if {$options(-name) ne ""} {
          $dom set_object_property $parent_window $options(-name) undefined
        }
        if {$value ne ""} {
          $dom set_object_property $parent_window $value [list object $this_win]
        }
      }
    }

    set options(-name) $value
  }

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

  method parent_frame {} {
    set frames [$myBrowser get_frames]
    set w [winfo parent $self]
    while {$w ne "" && [lsearch $frames $w] < 0} {
      set w [winfo parent $w]
    }
    return $w
  }
  method top_frame {} {
    lindex [$myBrowser get_frames] 0
  }
  method child_frames {} {
    set ret [list]
    foreach c [$myBrowser frames_tree $self] {
      lappend ret [lindex $c 0]
    }
    set ret
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
          set a [lsearch [$p panes] $w]
          set w $p
          set p [winfo parent $p]
          set b [lsearch [$p panes] $w]

          set myPositionId [linsert $myPositionId 0 "${b}.${a}"]
        }
        if {$class eq "Hv3" && $myPositionId eq ""} {
          set node $options(-iframe)
          set idx [lsearch [$p search iframe] $node]
          set myPositionId [linsert $myPositionId 0 iframe.${idx}]
        }
        set w $p
      }
      set myPositionId [linsert $myPositionId 0 0]
    }
    return $myPositionId
  }

  destructor {
    catch {$self ConfigureName -name ""}
    # Remove this object from the $theFrames list.
    catch {$myBrowser del_frame $self}
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
    if {![info exists ::hv3::G]} return

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
      a_href "Open Link"             [list $self menu_select open $a_href]     \
      a_href "Open Link in Bg Tab"   [list $self menu_select opentab $a_href]  \
      a_href "Download Link"         [list $self menu_select download $a_href] \
      a_href "Copy Link Location"    [list $self menu_select copy $a_href]     \
      a_href --                      ""                                        \
      img_src "View Image"           [list $self menu_select open $img_src]    \
      img_src "View Image in Bg Tab" [list $self menu_select opentab $img_src] \
      img_src "Download Image"       [list $self menu_select download $img_src]\
      img_src "Copy Image Location"  [list $self menu_select copy $img_src]    \
      img_src --                     ""                                        \
      select  "Copy Selected Text"   [list $self menu_select copy $select]     \
      select  --                     ""                                        \
      leaf    "Open Tree browser..." [list ::HtmlDebug::browse $myHv3 $leaf]   \
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
    foreach sub [list File Search Options Debug History] {
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

  method motion {N x y} {
    set myX $x
    set myY $y
    set myNodeList $N
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
      global $options(-statusvar)
      set str ""

      set status_mode browser
      catch { set status_mode $::hv3::G(status_mode) }

      switch -- $status_mode {
        browser-tree {
          set value [$self node_to_string [lindex $myNodeList end]]
          set str "($myX $myY) $value"
        }
        browser {
          for {set n [lindex $myNodeList end]} {$n ne ""} {set n [$n parent]} {
            if {[$n tag] eq "a" && [$n attr -default "" href] ne ""} {
              set str "hyper-link: [string trim [$n attr href]]"
              break
            }
          }
        }
      }

      if {$options(-statusvar) ne $str} {
        set $options(-statusvar) $str
      }
    }
  }
 
  #--------------------------------------------------------------------------
  # PUBLIC INTERFACE
  #--------------------------------------------------------------------------

  method goto {args} {
    eval [concat $myHv3 goto $args]
    set myNodeList ""
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
    set isFrameset 0
    if {[llength $slaves]>0} {
      set isFrameset [expr {[winfo class [lindex $slaves 0]] eq "Frameset"}]
    }
    return $isFrameset
  }

  option -statusvar        -default ""

  delegate option -forcefontmetrics to myHv3
  delegate option -fonttable        to myHv3
  delegate option -fontscale        to myHv3
  delegate option -zoom             to myHv3
  delegate option -enableimages     to myHv3
  delegate option -dom              to myHv3

  delegate method dumpforms         to myHv3

  delegate option -width         to myHv3
  delegate option -height        to myHv3

  delegate option -requestcmd         to myHv3
  delegate option -resetcmd           to myHv3

  delegate method stop to myHv3
  delegate method titlevar to myHv3
  delegate method javascriptlog to myHv3
}

# An instance of this widget represents a top-level browser frame (not
# a toplevel window - an html frame not contained in any frameset 
# document). These are the things managed by the notebook widget.
#
snit::widget ::hv3::browser {

  component myHistory                ;# The back/forward system
  component myProtocol               ;# The ::hv3::protocol
  component myMainFrame              ;# The browser_frame widget
  component myDom                    ;# The ::hv3::dom object

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

  method statusvar {} {return [myvar myStatusVar]}
  delegate method titlevar to myMainFrame

  constructor {args} {

    # Initialize the global database connection if it has not already
    # been initialized. TODO: Remove the global variable.
    ::hv3::dbinit

    set myDom [::hv3::dom %AUTO% $self]

    # Create the main browser frame (always present)
    set myMainFrame [::hv3::browser_frame $win.browser_frame $self]
    pack $myMainFrame -expand true -fill both -side top

    # Create the protocol
    set myProtocol [::hv3::protocol %AUTO%]
    $myMainFrame configure -requestcmd       [list $myProtocol requestcmd]

    set psc [list $self ProtocolStatusChanged]
    trace add variable [myvar myProtocolStatus] write $psc
    trace add variable [myvar myFrameStatus]    write [list $self Writestatus]
    $myMainFrame configure -statusvar [myvar myFrameStatus]
    $myProtocol  configure -statusvar [myvar myProtocolStatus]

    # Link in the "home:" and "about:" scheme handlers (from hv3_home.tcl)
    ::hv3::home_scheme_init [$myMainFrame hv3] $myProtocol
    ::hv3::cookies_scheme_init $myProtocol
    ::hv3::download_scheme_init [$myMainFrame hv3] $myProtocol

    # Create the history sub-system
    set myHistory [::hv3::history %AUTO% [$myMainFrame hv3] $myProtocol $self]
    $myHistory configure -gotocmd [list $self goto]

    $self configurelist $args
  }

  destructor {
    if {$myProtocol ne ""} { $myProtocol destroy }
    if {$myHistory ne ""}  { $myHistory destroy }
    if {$myDom ne ""}      { $myDom destroy }
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
    bind $HTML <KeyPress-slash>  [list $self Find]
    bindtags $HTML [concat Hv3HotKeys $self [bindtags $HTML]]
    if {[$myDom cget -enable]} {
      $frame configure -dom $myDom
    }
    catch {$::hv3::G(config) configureframe $frame}
  }
  method del_frame {frame} {
    set idx [lsearch $myFrames $frame]
    if {$idx >= 0} {
      set myFrames [lreplace $myFrames $idx $idx]
    }
  }
  method get_frames {} {return $myFrames}

  # Return a list describing the current structure of the frameset 
  # displayed by this browser.
  #
  method frames_tree {{head {}}} {
    set ret ""

    array set A {}
    foreach f [lsort $myFrames] {
      set p [$f parent_frame]
      lappend A($p) $f
      if {![info exists A($f)]} {set A($f) [list]}
    }

    foreach f [concat [lsort -decreasing $myFrames] [list {}]] {
      set new [list]
      foreach child $A($f) {
        lappend new [list $child $A($child)]
      }
      set A($f) $new
    }
    
    set A($head)
  }

  # This method is called by a [trace variable ... write] hook attached
  # to the myProtocolStatus variable. Set myStatusVar.
  method Writestatus {args} {
    set protocolstatus Done
    if {[llength $myProtocolStatus] > 0} {
      foreach {nWaiting nProgress nPercent} $myProtocolStatus break
      set protocolstatus "$nWaiting waiting, $nProgress progress  ($nPercent%)"
    }
    set myStatusVar "$protocolstatus    $myFrameStatus"
  }

  method ProtocolStatusChanged {args} {
    $self pendingcmd [llength $myProtocolStatus]
    $self Writestatus
  }

  method set_frame_status {text} {
    set myFrameStatus $text
  }

  # The widget may be in one of two states - "pending" or "not pending".
  # "pending" state is when the browser is still waiting for one or more
  # downloads to finish before the document is correctly displayed. In
  # this mode the default cursor is an hourglass and the stop-button
  # widget is in normal state (stop button is clickable).
  #
  # Otherwise the default cursor is "" (system default) and the stop-button
  # widget is disabled.
  #
  variable myIsPending 0

  method pendingcmd {isPending} {
    if {$options(-stopbutton) ne "" && $myIsPending != $isPending} {
      if {$isPending} { 
        $hull configure -cursor watch
        $options(-stopbutton) configure        \
            -command [list $myMainFrame stop]  \
            -image hv3_stopimg                 \
            -tooltip "Stop Current Download"
      } else {
        $hull configure -cursor ""
        $options(-stopbutton) configure        \
            -command [list gui_current reload] \
            -image hv3_reloadimg               \
            -tooltip "Reload Current Document"
      }
    }
    set myIsPending $isPending
  }

  method Configurestopbutton {option value} {
    set options(-stopbutton) $value
    set val $myIsPending
    set myIsPending -1
    $self pendingcmd $val
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
    bind $w <Destroy> [list catch [list focus [[$myMainFrame hv3] html]]]
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

  # ProtocolGui --
  #
  #     This method is called when the "toggle-protocol-gui" control
  #     (implemented externally) is manipulated. The argument must
  #     be one of the following strings:
  #
  #       "show"            (display gui)
  #       "hide"            (hide gui)
  #       "toggle"          (display if hidden, hide if displayed)
  #
  method ProtocolGui {cmd} {
    set name ${win}.protocolgui
    set exists [winfo exists $name]

    switch -- $cmd {
      show   {if {$exists} return}
      hide   {if {!$exists} return}
      toggle {
        set cmd "show"
        if {$exists} {set cmd "hide"}
      }

      default { error "Bad arg" }
    }

    if {$cmd eq "hide"} {
      destroy $name
    } else {
      $myProtocol gui $name
      $self packwidget $name
    }
  }

  method history {} {
    return $myHistory
  }

  method reload {} {
    $myHistory reload
  }

  option -enablejavascript                         \
      -default 0                                   \
      -configuremethod ConfigureEnableJavascript   \
      -cgetmethod      CgetEnableJavascript

  method ConfigureEnableJavascript {option value} {
    $myDom configure -enable $value
    set dom ""
    if {$value} { set dom $myDom }
    foreach f $myFrames {
      $f configure -dom $dom
    }
  }
  method CgetEnableJavascript {option} {
    $myDom cget -enable
  }

  delegate method populate_history_menu to myHistory as populate_menu

  option -stopbutton -default "" -configuremethod Configurestopbutton

  option -unsafe -default 0

  delegate option -backbutton    to myHistory
  delegate option -forwardbutton to myHistory
  delegate option -locationentry to myHistory

  delegate method locationvar to myHistory
  delegate method populatehistorymenu to myHistory

  delegate method debug_cookies  to myProtocol

  delegate option * to myMainFrame
  delegate method * to myMainFrame
}

set ::hv3::maindir [file dirname [info script]] 

