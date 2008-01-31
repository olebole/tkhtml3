

package require sqlite3
package require Tkhtml 3.0

proc sourcefile {file} [string map              \
  [list %HV3_DIR% [file dirname [info script]]] \
{ 
  return [file join {%HV3_DIR%} $file] 
}]

source [sourcefile snit.tcl]
source [sourcefile hv3_widgets.tcl]
source [sourcefile hv3_notebook.tcl]
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
source [sourcefile hv3_dom.tcl]
source [sourcefile hv3_object.tcl]

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
namespace eval ::hv3::browser_frame {

  proc new {me browser args} {
    upvar #0 $me O

    # The name of this frame (as specified by the "name" attribute of 
    # the <frame> element).
    set O(-name) ""

    # If this [::hv3::browser_frame] is used as a replacement object
    # for an <iframe> element, then this option is set to the Tkhtml3
    # node-handle for that <iframe> element.
    #
    set O(-iframe) ""

    set O(-statusvar) ""

    set O(myBrowser) $browser
    eval $me configure $args

    set O(myNodeList) ""                  ;# Current nodes under the pointer
    set O(myX) 0                          ;# Current location of pointer
    set O(myY) 0                          ;# Current location of pointer

    set O(myBrowser) $browser             ;# ::hv3::browser widget
    set O(myPositionId) ""                ;# See sub-command [positionid]

    # If "Copy Link Location" has been selected, store the selected text
    # (a URI) in set $O(myCopiedLinkLocation).
    set O(myCopiedLinkLocation) ""
 
    #set O(myHv3)      [::hv3::hv3 $O(win).hv3]
    #pack $O(myHv3) -expand true -fill both
    set O(myHv3) $O(hull)

    ::hv3::the_visited_db init $O(myHv3)

    catch {$O(myHv3) configure -fonttable $::hv3::fontsize_table}
    $O(myHv3) configure -downloadcmd {::hv3::the_download_manager savehandle}

    # Create bindings for motion, right-click and middle-click.
    $O(myHv3) Subscribe motion [list $me motion]
    bind $O(win) <3>       [list $me rightclick %x %y %X %Y]
    bind $O(win) <2>       [list $me goto_selection]

    # When the hyperlink menu "owns" the selection (happens after 
    # "Copy Link Location" is selected), invoke method 
    # [GetCopiedLinkLocation] with no arguments to retrieve it.

    # Register a handler command to handle <frameset>.
    set html [$O(myHv3) html]
    $html handler node frameset [list ::hv3::frameset_handler $me]

    # Register handler commands to handle <object> and kin.
    $html handler node object   [list hv3_object_handler $O(myHv3)]
    $html handler node embed    [list hv3_object_handler $O(myHv3)]

    $html handler node      iframe [list ::hv3::iframe_handler $me]
    $html handler attribute iframe [list ::hv3::iframe_attr_handler $me]

    # Add this object to the browsers frames list. It will be removed by
    # the destructor proc. Also override the default -targetcmd
    # option of the ::hv3::hv3 widget with our own version.
    $O(myBrowser) add_frame $O(win)
    $O(myHv3) configure -targetcmd [list $me Targetcmd]

    ::hv3::menu $O(win).hyperlinkmenu
    selection handle $O(win).hyperlinkmenu [list $me GetCopiedLinkLocation]
  }
  proc destroy {me} {
    upvar #0 $me O
    catch {$self ConfigureName -name ""}
    # Remove this object from the $theFrames list.
    catch {$O(myBrowser) del_frame $O(win)}
    catch {destroy $O(win).hyperlinkmenu}
  }

  proc configure-name {me} {
    upvar #0 $me O
puts "TODODODODODOD"
return

    # This method is called when the "name" of attribute of this
    # frame is modified. If javascript is enabled we have to update
    # the properties on the parent window object (if any).
    set dom [$me cget -dom]
    if {$dom ne "" && [$dom cget -enable]} {
      set parent [$me parent_frame]
      if {$parent ne ""} {
        set parent_window [list ::hv3::DOM::Window $dom [$parent hv3]]
        set this_win [list ::hv3::DOM::Window $dom $O(myHv3)]
        if {$O(-name) ne ""} {
          $dom set_object_property $parent_window $O(-name) undefined
        }
        if {$value ne ""} {
          $dom set_object_property $parent_window $value [list object $this_win]
        }
      }
    }

    set O(-name) $value
  }

  proc Targetcmd {me node} {
    upvar #0 $me O
    set target [$node attr -default "" target]
    if {$target eq ""} {
      # If there is no target frame specified, see if a default
      # target was specified in a <base> tag i.e. <base target="_top">.
      set n [lindex [[$O(myHv3) html] search base] 0]
      if {$n ne ""} { set target [$n attr -default "" target] }
    }

    set theTopFrame [[lindex [$O(myBrowser) get_frames] 0] hv3]

    # Find the target frame widget.
    set widget $O(myHv3)
    switch -- $target {
      ""        { set widget $O(myHv3) }
      "_self"   { set widget $O(myHv3) }
      "_top"    { set widget $theTopFrame }

      "_parent" { 
        set w [winfo parent $O(myHv3)]
        while {$w ne "" && [lsearch [$O(myBrowser) get_frames] $w] < 0} {
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
        foreach f [$O(myBrowser) get_frames] {
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

  proc parent_frame {me } {
    upvar #0 $me O
    set frames [$O(myBrowser) get_frames]
    set w [winfo parent $O(win)]
    while {$w ne "" && [lsearch $frames $w] < 0} {
      set w [winfo parent $w]
    }
    return $w
  }
  proc top_frame {me } {
    upvar #0 $me O
    lindex [$O(myBrowser) get_frames] 0
  }
  proc child_frames {me } {
    upvar #0 $me O
    set ret [list]
    foreach c [$O(myBrowser) frames_tree $O(win)] {
      lappend ret [lindex $c 0]
    }
    set ret
  }

  # This method returns the "position-id" of a frame, an id that is
  # used by the history sub-system when loading a historical state of
  # a frameset document.
  #
  proc positionid {me } {
    upvar #0 $me O
    if {$O(myPositionId) eq ""} {
      set w $O(win)
      while {[set p [winfo parent $w]] ne ""} {
        set class [winfo class $p]
        if {$class eq "Panedwindow"} {
          set a [lsearch [$p panes] $w]
          set w $p
          set p [winfo parent $p]
          set b [lsearch [$p panes] $w]

          set O(myPositionId) [linsert $O(myPositionId) 0 "${b}.${a}"]
        }
        if {$class eq "Hv3" && $O(myPositionId) eq ""} {
          set node $O(-iframe)
          set idx [lsearch [$p search iframe] $node]
          set O(myPositionId) [linsert $O(myPositionId) 0 iframe.${idx}]
        }
        set w $p
      }
      set O(myPositionId) [linsert $O(myPositionId) 0 0]
    }
    return $O(myPositionId)
  }

  # This callback is invoked when the user right-clicks on this 
  # widget. If the mouse cursor is currently hovering over a hyperlink, 
  # popup the hyperlink menu. Otherwise launch the tree browser.
  #
  # Arguments $x and $y are the the current cursor position relative to
  # this widget's window. $X and $Y are the same position relative to
  # the root window.
  #
  proc rightclick {me x y X Y} {
    upvar #0 $me O
    if {![info exists ::hv3::G]} return

    set m $O(win).hyperlinkmenu
    $m delete 0 end

    set nodelist [$O(myHv3) node $x $y]

    set a_href ""
    set img_src ""
    set select [$O(myHv3) selected]
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

    if {$a_href ne ""}  {set a_href [$O(myHv3) resolve_uri $a_href]}
    if {$img_src ne ""} {set img_src [$O(myHv3) resolve_uri $img_src]}

    set MENU [list \
      a_href "Open Link"             [list $me menu_select open $a_href]     \
      a_href "Open Link in Bg Tab"   [list $me menu_select opentab $a_href]  \
      a_href "Download Link"         [list $me menu_select download $a_href] \
      a_href "Copy Link Location"    [list $me menu_select copy $a_href]     \
      a_href --                      ""                                        \
      img_src "View Image"           [list $me menu_select open $img_src]    \
      img_src "View Image in Bg Tab" [list $me menu_select opentab $img_src] \
      img_src "Download Image"       [list $me menu_select download $img_src]\
      img_src "Copy Image Location"  [list $me menu_select copy $img_src]    \
      img_src --                     ""                                        \
      select  "Copy Selected Text"   [list $me menu_select copy $select]     \
      select  --                     ""                                        \
      leaf    "Open Tree browser..." [list ::HtmlDebug::browse $O(myHv3) $leaf]   \
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
  proc menu_select {me option uri} {
    upvar #0 $me O
    switch -- $option {
      open { 
        set top_frame [lindex [$O(myBrowser) get_frames] 0]
        $top_frame goto $uri 
      }
      opentab { set new [.notebook addbg $uri] }
      download { $O(myBrowser) saveuri $uri }
      copy {
        set O(myCopiedLinkLocation) $uri
        selection own $O(win).hyperlinkmenu
        clipboard clear
        clipboard append $uri
      }

      default {
        error "Internal error"
      }
    }
  }

  proc GetCopiedLinkLocation {me args} {
    upvar #0 $me O
    return $O(myCopiedLinkLocation)
  }

  # Called when the user middle-clicks on the widget
  proc goto_selection {me} {
    upvar #0 $me O
    set theTopFrame [lindex [$O(myBrowser) get_frames] 0]
    $theTopFrame goto [selection get]
  }

  proc motion {me N x y} {
    upvar #0 $me O
    set O(myX) $x
    set O(myY) $y
    set O(myNodeList) $N
    $me update_statusvar
  }

  proc node_to_string {me node {hyperlink 1}} {
    upvar #0 $me O
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

  proc update_statusvar {me} {
    upvar #0 $me O
    if {$O(-statusvar) ne ""} {
      global $O(-statusvar)
      set str ""

      set status_mode browser
      catch { set status_mode $::hv3::G(status_mode) }

      switch -- $status_mode {
        browser-tree {
          set value [$me node_to_string [lindex $O(myNodeList) end]]
          set str "($O(myX) $O(myY)) $value"
        }
        browser {
          for {set n [lindex $O(myNodeList) end]} {$n ne ""} {set n [$n parent]} {
            if {[$n tag] eq "a" && [$n attr -default "" href] ne ""} {
              set str "hyper-link: [string trim [$n attr href]]"
              break
            }
          }
        }
      }

      if {$O(-statusvar) ne $str} {
        set $O(-statusvar) $str
      }
    }
  }
 
  #--------------------------------------------------------------------------
  # PUBLIC INTERFACE
  #--------------------------------------------------------------------------

  proc goto {me args} {
    upvar #0 $me O
    eval [concat $O(myHv3) goto $args]
    set O(myNodeList) ""
    $me update_statusvar
  }

  # Launch the tree browser
  proc browse {me} {
    upvar #0 $me O
    ::HtmlDebug::browse $O(myHv3) [$O(myHv3) node]
  }

  proc hv3 {me} { 
    upvar #0 $me O
    return $O(myHv3) 
  }
  proc browser {me} {
    upvar #0 $me O
    return $O(myBrowser) 
  }

  # The [isframeset] method returns true if this widget instance has
  # been used to parse a frameset document (widget instances may parse
  # either frameset or regular HTML documents).
  #
  proc isframeset {me} {
    upvar #0 $me O
    # When a <FRAMESET> tag is parsed, a node-handler in hv3_frameset.tcl
    # creates a widget to manage the frames and then uses [place] to 
    # map it on top of the html widget created by this ::hv3::browser_frame
    # widget. Todo: It would be better if this code was in the same file
    # as the node-handler, otherwise this test is a bit obscure.
    #
    set html [[$me hv3] html]
    set slaves [place slaves $html]
    set isFrameset 0
    if {[llength $slaves]>0} {
      set isFrameset [expr {[winfo class [lindex $slaves 0]] eq "Frameset"}]
    }
    return $isFrameset
  }

  set DelegateOption(-forcefontmetrics) myHv3
  set DelegateOption(-fonttable) myHv3
  set DelegateOption(-fontscale) myHv3
  set DelegateOption(-zoom) myHv3
  set DelegateOption(-enableimages) myHv3
  set DelegateOption(-dom) myHv3
  set DelegateOption(-width) myHv3
  set DelegateOption(-height) myHv3
  set DelegateOption(-requestcmd) myHv3
  set DelegateOption(-resetcmd) myHv3

  proc stop {me args} {
    upvar #0 $me O
    eval $O(myHv3) stop $args
  }
  proc titlevar {me args} {
    upvar #0 $me O
    eval $O(myHv3) titlevar $args
  }
  proc dumpforms {me args} {
    upvar #0 $me O
    eval $O(myHv3) dumpforms $args
  }
  proc javascriptlog {me args} {
    upvar #0 $me O
    eval $O(myHv3) javascriptlog $args
  }
}
::hv3::make_constructor ::hv3::browser_frame ::hv3::hv3

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
    set myMainFrame [::hv3::browser_frame $win.frame $self]
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
    set handle [::hv3::request %AUTO%              \
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

  delegate method debug_cookies to myProtocol

  delegate option * to myMainFrame
  delegate method * to myMainFrame
}

set ::hv3::maindir [file dirname [info script]] 

