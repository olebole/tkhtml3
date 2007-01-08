namespace eval hv3 { set {version($Id: hv3.tcl,v 1.143 2007/01/08 11:48:07 danielk1977 Exp $)} 1 }

#
# This file contains the mega-widget hv3::hv3 used by the hv3 demo web 
# browser. An instance of this widget displays a single HTML frame.
#
# Standard Functionality:
#
#     xview
#     yview
#     -xscrollcommand
#     -yscrollcommand
#     -width
#     -height
# 
# Widget Specific Options:
#
#     -requestcmd
#         If not an empty string, this option specifies a script to be
#         invoked for a GET or POST request. The script is invoked with a
#         download handle appended to it. See the description of class
#         ::hv3::download for a description.
#
#     -targetcmd
#         If not an empty string, this option specifies a script for
#         the widget to invoke when a hyperlink is clicked on or a form
#         submitted. The script is invoked with the node handle of the 
#         clicked hyper-link element appended. The script must return
#         the name of an hv3 widget to load the new document into. This
#         is intended to be used to implement frameset handling.
#
#     -isvisitedcmd
#         If not an empty string, this option specifies a script for
#         the widget to invoke to determine if a hyperlink node should
#         be styled with the :link or :visited pseudo-class. The
#         script is invoked with the node handle appended to it. If
#         true is returned, :visited is used, otherwise :link.
#
#     -fonttable
#         Delegated through to the html widget.
#
#     -locationvar
#         Set to the URI of the currently displayed document.
#
#     -pendingvar
#         Name of var to set to true while resource requests are
#         pending for the currently displayed document. This is
#         useful for a web browser GUI that needs to disable the 
#         "stop" button after all resource requests are completed.
#
#     -scrollbarpolicy
#         This option may be set to either a boolean value or "auto". It
#         determines the visibility of the widget scrollbars. TODO: This
#         is now set internally by the value of the "overflow" property
#         on the root element. Maybe the option should be removed?
#
#
# Widget Sub-commands:
#
#     goto URI ?OPTIONS?
#         Load the content at the specified URI into the widget. 
#
#     stop
#         Cancel all pending downloads.
#
#     node        
#         Caching wrapper around html widget [node] command.
#
#     reset        
#         Wrapper around the html widget command of the same name. Also
#         resets all document related state stored by the mega-widget.
#
#     html        
#         Return the path of the underlying html widget. This should only
#         be used to determine paths for child widgets. Bypassing hv3 and
#         accessing the html widget interface directly may confuse hv3.
#
#     title        
#         Return the "title" of the currently loaded document.
#
#     location        
#         Return the location URI of the widget.
#
#     selected        
#         Return the currently selected text, or an empty string if no
#         text is currently selected.
#
#
# Widget Custom Events:
#
#     <<Goto>>
#         This event is generated whenever the goto method is called.
#
#     <<Complete>>
#         This event is generated once all of the resources required
#         to display a document have been loaded.
#
#     <<Reset>>
#         This event is generated just before [$html reset] is called
#         and mega-widget state data discarded (because a new document
#         is about to be loaded). This gives the application a final 
#         chance to query the current state of the browser before it 
#         is discarded.
#
#     <<Location>>
#         This event is generated whenever the "location" is set.
#
#     <<SaveState>>
#         Generated whenever the widget state should be saved.

#
# The code in this file is partitioned into the following classes:
#
#     ::hv3::uri
#     ::hv3::hv3
#     ::hv3::download
#     ::hv3::selectionmanager
#     ::hv3::dynamicmanager
#     ::hv3::hyperlinkmanager
#
# ::hv3::hv3 is, of course, the main mega-widget class. Class ::hv3::uri
# is a class that encapsulates code to manipulate URI strings. Class
# ::hv3::download is part of the public interface to ::hv3::hv3. A
# single instance of ::hv3::download represents a resource request made
# by the mega-widget package - for document, stylesheet, image or 
# object data.
#
# The three "manager" classes all implement the following interface. Each
# ::hv3::hv3 widget has exactly one of each manager class as a component.
# Further manager objects may be added in the future. Interface:
#
#     set manager [::hv3::XXXmanager $hv3]
#
#     $manager motion  X Y
#     $manager release X Y
#     $manager press   X Y
#
# The -targetcmd option of ::hv3::hv3 is delegated to the
# ::hv3::hyperlinkmanager component.
#
package require Tkhtml 3.0
package require snit

source [file join [file dirname [info script]] hv3_form.tcl]
source [file join [file dirname [info script]] hv3_widgets.tcl]
source [file join [file dirname [info script]] hv3_object.tcl]
source [file join [file dirname [info script]] hv3_doctype.tcl]
source [file join [file dirname [info script]] hv3_request.tcl]
source [file join [file dirname [info script]] hv3_dom.tcl]

#--------------------------------------------------------------------------
# Class ::hv3::hv3::mousemanager
#
#     This type contains code for the ::hv3::hv3 widget to manage 
#     dispatching mouse events that occur in the HTML widget to the 
#     rest of the application. The following HTML4 events are handled:
#
#         onmouseover
#         onmousemove
#         onmouseout
#         onclick
#         onmousedown
#         onmouseup
#
#     TODO: Registering a handler for this is not an error, but the
#     event is never generated:
#
#         ondblclick
#
snit::type ::hv3::hv3::mousemanager {

  variable myHv3 ""

  variable myReset 0

  # Database of callback scripts for each event type.
  #
  variable myScripts -array [list]

  # List of nodes currently "hovered" over and "active". An entry in
  # the correspondoing array indicates the condition is true.
  #
  variable myHoverNodes -array [list]
  variable myActiveNodes -array [list]


  # List of handled HTML4 event types (a constant)
  variable EVENTS [list onmouseover onmousemove onmouseout onclick \
      ondblclick onmousedown onmouseup]

  constructor {hv3} {
    foreach e $EVENTS {
      set myScripts($e) [list]
    }

    set myHv3 $hv3
    bind $myHv3 <Motion>          "+[mymethod Motion %x %y]"
    bind $myHv3 <ButtonPress-1>   "+[mymethod Press %x %y]"
    bind $myHv3 <ButtonRelease-1> "+[mymethod Release %x %y]"
  }

  method subscribe {event script} {

    # Check that the $event argument is Ok:
    if {0 > [lsearch $EVENTS $event]} {
      error "No such mouse-event: $event"
    }

    # Append the script to the callback list.
    lappend myScripts($event) $script
  }

  method reset {} {
    array unset myActiveNodes
    array unset myHoverNodes
    set myReset 1
  }

  # Generate a $event event on node $node.
  #
  method Generate {event node} {
    foreach script $myScripts($event) {
      eval $script $node
    }
  }

  method GenerateEvents {eventlist} {
    foreach {event node} $eventlist {
      foreach script $myScripts($event) {
        set myReset 0
        eval $script $node
        if {$myReset} return
      }
    }
  }

  method Motion {x y} {

    # Figure out the node the cursor is currently hovering over. Todo:
    # When the cursor is over multiple nodes (because overlapping content
    # has been generated), maybe this should consider all overlapping nodes
    # as "hovered".
    set nodelist [lindex [$myHv3 node $x $y] end]

    # After the loop runs, hovernodes will contain the list of 
    # currently hovered nodes.
    array set hovernodes [list]

    # Events to generate:
    set events(onmousemove) [list]
    set events(onmouseout)  [list]
    set events(onmouseover) [list]

    foreach node $nodelist {
      if {[$node tag] eq ""} {set node [$node parent]}

      for {set n $node} {$n ne ""} {set n [$n parent]} {
        if {[info exists hovernodes($n)]} {
          break
        } else {
          if {[info exists myHoverNodes($n)]} {
            unset myHoverNodes($n)
          } else {
            lappend events(onmouseover) $n
          }
          set hovernodes($n) ""
        }
      }
    }
    set events(onmouseout)  [array names myHoverNodes]
    set events(onmousemove) [\
        concat [array names hovernodes] $events(onmouseout) 
    ]

    array unset myHoverNodes
    array set myHoverNodes [array get hovernodes]

    set eventlist [list]
    foreach key [list onmouseover onmousemove onmouseout] {
      foreach node $events($key) {
        lappend eventlist $key $node
      }
    }
    $self GenerateEvents $eventlist
  }

  method Press {x y} {

    set N [lindex [$myHv3 node $x $y] end]

    if {$N ne ""} {
      if {[$N tag] eq ""} {set N [$N parent]}
    }

    for {set n $N} {$n ne ""} {set n [$n parent]} {
      set myActiveNodes($n) 1
    }

    set eventlist [list]
    foreach node [array names myActiveNodes] {
      lappend eventlist onmousedown $node
    }
    $self GenerateEvents $eventlist
  }

  method Release {x y} {

    set onclick_nodes [list]
    set N [lindex [$myHv3 node $x $y] end]
    if {$N ne ""} {
      if {[$N tag] eq ""} {set N [$N parent]}
    }
    for {set n $N} {$n ne ""} {set n [$n parent]} {
      if {[info exists myActiveNodes($n)]} {
        lappend onclick_nodes $n
      }
    }

    set eventlist [list]
    foreach node [array names myActiveNodes] {
      lappend eventlist onmouseup $node
    }
    foreach node $onclick_nodes {
      lappend eventlist onclick $node
    }
    $self GenerateEvents $eventlist

    array unset myActiveNodes
  }
}

#--------------------------------------------------------------------------
# ::hv3::hv3::selectionmanager
#
#     This type encapsulates the code that manages selecting text
#     in the html widget with the mouse.
#
snit::type ::hv3::hv3::selectionmanager {

  # Variable myMode may take one of the following values:
  #
  #     "char"           -> Currently text selecting by character.
  #     "word"           -> Currently text selecting by word.
  #     "line"           -> Currently text selecting by line.
  #
  variable myState false             ;# True when left-button is held down
  variable myMode char

  # The ::hv3::hv3 widget.
  #
  variable myHv3

  variable myFromNode ""
  variable myFromIdx ""

  variable myToNode ""
  variable myToIdx ""

  constructor {hv3} {
    set myHv3 $hv3
    selection handle $myHv3 [list ::hv3::bg [mymethod get_selection]]

    bind $myHv3 <Motion>               "+[mymethod motion %x %y]"
    bind $myHv3 <ButtonPress-1>        "+[mymethod press %x %y]"
    bind $myHv3 <Double-ButtonPress-1> "+[mymethod doublepress %x %y]"
    bind $myHv3 <ButtonRelease-1>      "+[mymethod release %x %y]"
  }

  # Clear the selection.
  #
  method clear {} {
    $myHv3 tag delete selection
    $myHv3 tag configure selection -foreground white -background darkgrey
    set myFromNode ""
    set myToNode ""
  }

  method press {x y} {
    # Single click -> Select by character.
    $self clear
    set myState true
    set myMode char
    $self motion $x $y
  }

  # Given a node-handle/index pair identifying a character in the 
  # current document, return the index values for the start and end
  # of the word containing the character.
  #
  proc ToWord {node idx} {
    set t [$node text]
    set cidx [::tkhtml::charoffset $t $idx]
    set cidx1 [string wordstart $t $cidx]
    set cidx2 [string wordend $t $cidx]
    set idx1 [::tkhtml::byteoffset $t $cidx1]
    set idx2 [::tkhtml::byteoffset $t $cidx2]
    return [list $idx1 $idx2]
  }

  # Add the widget tag "selection" to the word containing the character
  # identified by the supplied node-handle/index pair.
  #
  method TagWord {node idx} {
    foreach {i1 i2} [ToWord $node $idx] {}
    $myHv3 tag add selection $node $i1 $node $i2
  }

  # Remove the widget tag "selection" to the word containing the character
  # identified by the supplied node-handle/index pair.
  #
  method UntagWord {node idx} {
    foreach {i1 i2} [ToWord $node $idx] {}
    $myHv3 tag remove selection $node $i1 $node $i2
  }

  method doublepress {x y} {
    # Single click -> Select by word.
    $self clear
    set myMode word
    set myState true
    $self motion $x $y
  }

  method release {x y} {
    set myState false
  }

  method reset {} {
    set myState false

    # Unset the myFromNode variable, since the node handle it (may) refer 
    # to is now invalid. If this is not done, a future call to the [selected]
    # method of this object will cause an error by trying to use the
    # (now invalid) node-handle value in $myFromNode.
    set myFromNode ""
    set myToNode ""
  }

  method motion {x y} {
    if {!$myState} return

    set to [$myHv3 node -index $x $y]
    if {[llength $to] == 0} return
    foreach {toNode toIdx} $to {}

    if {$myFromNode eq ""} {
      set myFromNode $toNode
      set myFromIdx $toIdx
    }

    if {$myToNode ne $toNode || $toIdx != $myToIdx} {
      switch -- $myMode {
        char {
          if {$myToNode ne ""} {
            $myHv3 tag remove selection $myToNode $myToIdx $toNode $toIdx
          }
          $myHv3 tag add selection $myFromNode $myFromIdx $toNode $toIdx
        }

        word {
          if {$myToNode ne ""} {
            $myHv3 tag remove selection $myToNode $myToIdx $toNode $toIdx
            $self UntagWord $myToNode $myToIdx
            $self UntagWord $toNode $toIdx
          }

          $myHv3 tag add selection $myFromNode $myFromIdx $toNode $toIdx
          $self TagWord $toNode $toIdx
          $self TagWord $myFromNode $myFromIdx
        }
      }

      set myToNode $toNode
      set myToIdx $toIdx
    }

    selection own $myHv3
  }

  # get_selection OFFSET MAXCHARS
  #
  #     This command is invoked whenever the current selection is selected
  #     while it is owned by the html widget. The text of the selected
  #     region is returned.
  #
  method get_selection {offset maxChars} {
    set t [$myHv3 text text]

    set n1 $myFromNode
    set i1 $myFromIdx
    set n2 $myToNode
    set i2 $myToIdx

    set stridx_a [$myHv3 text offset $myFromNode $myFromIdx]
    set stridx_b [$myHv3 text offset $myToNode $myToIdx]
    if {$stridx_a > $stridx_b} {
      foreach {stridx_a stridx_b} [list $stridx_b $stridx_a] {}
    }

    if {$myMode eq "word"} {
      set stridx_a [string wordstart $t $stridx_a]
      set stridx_b [string wordend $t $stridx_b]
    }
  
    set T [string range [$myHv3 text text] $stridx_a [expr $stridx_b - 1]]
    set T [string range $T $offset [expr $offset + $maxChars]]

#puts "document text {[$myHv3 text text]}"
#puts "from -> to {$n1 $i1 -> $n2 $i2}"
#puts "from -> to {$stridx_a -> $stridx_b}"

    return $T
  }

  method selected {} {
    if {$myFromNode eq ""} {return ""}
    return [$self get_selection 0 10000000]
  }

}
#
# End of ::hv3::hv3::selectionmanager
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class ::hv3::hv3::dynamicmanager
#
#     This class is responsible for setting the dynamic :hover flag on
#     document nodes in response to cursor movements. It may one day
#     be extended to handle :focus and :active, but it's not yet clear
#     exactly how these should be dealt with.
#
snit::type ::hv3::hv3::dynamicmanager {

  constructor {hv3} {
    $hv3 Subscribe onmouseover [mymethod handle_mouseover]
    $hv3 Subscribe onmouseout  [mymethod handle_mouseout]
    $hv3 Subscribe onmousedown [mymethod handle_mousedown]
    $hv3 Subscribe onmouseup   [mymethod handle_mouseup]
  }

  method handle_mouseover {node} { $node dynamic set hover }
  method handle_mouseout {node}  { $node dynamic clear hover }

  method handle_mousedown {node} { $node dynamic set active }
  method handle_mouseup {node}   { $node dynamic clear active }
}
#
# End of ::hv3::hv3::dynamicmanager
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class ::hv3::hv3::hyperlinkmanager
#
# Each instance of the hv3 widget contains a single hyperlinkmanager as
# a component. The hyperlinkmanager takes care of:
#
#     * -targetcmd option and associate callbacks
#     * -isvisitedcmd option and associate callbacks
#     * Modifying the cursor to the hand shape when over a hyperlink
#     * Setting the :link or :visited dynamic condition on hyperlink 
#       elements (depending on the return value of -isvisitedcmd).
#
# This class installs a node handler for <a> elements. It also subscribes
# to the <Motion>, <ButtonPress-1> and <ButtonRelease-1> events on the
# associated hv3 widget.
#
snit::type ::hv3::hv3::hyperlinkmanager {
  variable myHv3

  variable myLinkHoverCount 0

  option -isvisitedcmd -default ""
  option -targetcmd -default ""

  constructor {hv3} {
    set myHv3 $hv3

    # Set upt the default -targetcmd script to always return $myHv3.
    set options(-targetcmd) [list ::hv3::ReturnWithArgs $hv3]

    $myHv3 handler node a [mymethod a_node_handler]
    bind $myHv3 <Motion>         "+[mymethod motion %x %y]"
    $myHv3 Subscribe onclick     [mymethod handle_onclick]
  }

  method reset {} {
    set myLinkHoverCount 0
  }

  method a_node_handler {node} {
    if {[$node attr -default "" href] ne ""} {
      if {
        $options(-isvisitedcmd) ne "" && 
        [eval [linsert $options(-isvisitedcmd) end $node]]
      } {
        $node dynamic set visited
      } else {
        $node dynamic set link
      }
    }
  }

  method handle_onclick {node} {
    set href [$node attr -default "" href]
    if {$href ne "" && [$node tag] eq "a"} {
      set hv3 [eval [linsert $options(-targetcmd) end $node]]
      $hv3 goto $href -referer [$myHv3 location]
    }
  }

  method motion {x y} {
    set nodelist [$myHv3 node $x $y]
    set text 0
    set framewidget [$myHv3 hull]
    foreach node $nodelist {
      if {[$node tag] eq ""} {set text 1}
      for {set n $node} {$n ne ""} {set n [$n parent]} {
        if {[$n tag] eq "a" && [$n attr -default "" href] ne ""} {
          $framewidget configure -cursor hand2
          return 
        }
      }
    }
    if {$text == 0} {
      $framewidget configure -cursor ""
    } else {
      $framewidget configure -cursor xterm
    }
  }
}
#
# End of ::hv3::hv3::hyperlinkmanager
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class hv3 - the public widget class.
#
snit::widget ::hv3::hv3 {

  # Object components
  component myHtml                   ;# The [::hv3::scrolled html] widget
  component myHyperlinkManager       ;# The ::hv3::hv3::hyperlinkmanager
  component myDynamicManager         ;# The ::hv3::hv3::dynamicmanager
  component mySelectionManager       ;# The ::hv3::hv3::selectionmanager
  component myFormManager            ;# The ::hv3::formmanager

  variable myDom

  component myMouseManager           ;# The ::hv3::hv3::mousemanager
  delegate method Subscribe to myMouseManager as subscribe

  # The current location URI and the current base URI. If myBase is "",
  # use the URI stored in myUri as the base.
  #
  component myUri -public uri
  variable  myBase ""                ;# The current URI (type ::hv3::hv3uri)

  # Used to assign internal stylesheet ids.
  variable  myStyleCount 0 

  # This variable may be set to "unknown", "quirks" or "standards".
  variable myQuirksmode unknown

  # List of currently outstanding download-handles. See methods makerequest,
  # Finrequest and <TODO: related to stop?>.
  variable myCurrentDownloads [list]

  variable myFirstReset 1

  # Current value to set the -cachecontrol option of download handles to.
  variable myCacheControl normal

  # This variable stores the current type of resource being displayed.
  # When valid, it is set to one of the following:
  #
  #     * html
  #     * image
  #
  # Otherwise, it is set to an empty string, indicating that the resource
  # has been requested, but has not yet arrived.
  #
  variable myMimetype ""

  # This variable is set to true while parsing the first chunk of an
  # html document. i.e. code below effectively does:
  #
  #     set myChangeEncodingOk 1
  #     $myHtml parse $first_chunk
  #     set myChangeEncodingOk 0
  #
  # This is because we only do anything about <meta type="content-type">
  # tags if they are encountered in the first chunk of the document. The
  # default chunk-size is 2048 bytes, so this is reasonably safe.
  #
  variable myChangeEncodingOk 0

  # If this variable is set to anything other than an empty string, then
  # it is set to the encoding of the document.
  #
  variable myEncoding ""

  variable myEncodedDocument ""

  # This variable is only used when ($myMimetype eq "image"). It stores
  # the data for the image about to be displayed. Once the image
  # has finished downloading, the data in this variable is loaded into
  # a Tk image and this variable reset to "".
  #
  variable myImageData ""

  # If this variable is not set to the empty string, it is the id of an
  # [after] event that will refresh the current document (i.e from a 
  # Refresh header or <meta type=http-equiv> markup). This scheduled 
  # event should be cancelled when the [reset] method is called.
  #
  # There should only be one Refresh event scheduled at any one time.
  # The [Refresh] method, which calls [after] to schedule the events,
  # cancels any pending event before scheduling a new one.
  #
  variable myRefreshEventId ""

  constructor {} {
    # Create the scrolled html widget and bind it's events to the
    # mega-widget window.
    set myHtml [::hv3::scrolled html ${win}.html]
    bindtags [$self html] [concat [bindtags [$self html]] $self]
    pack $myHtml -expand true -fill both

    set myMouseManager [::hv3::hv3::mousemanager %AUTO% $self]

    # $myHtml configure -layoutcache 0

    set myDom [::hv3::dom %AUTO% $self]

    # Create the event-handling components.
    set myHyperlinkManager [::hv3::hv3::hyperlinkmanager %AUTO% $self]
    set mySelectionManager [::hv3::hv3::selectionmanager %AUTO% $self]
    set myDynamicManager   [::hv3::hv3::dynamicmanager   %AUTO% $self]

    # Location URI. The default URI is index.html in the applications
    # current working directory.
    set myUri              [::hv3::uri %AUTO% file://[pwd]/index.html]

    set myFormManager [::hv3::formmanager %AUTO% $self]
    $myFormManager configure -getcmd  [mymethod Formcmd get]
    $myFormManager configure -postcmd [mymethod Formcmd post]

    # Attach an image callback to the html widget
    $myHtml configure -imagecmd [mymethod Imagecmd]

    # Register node handlers to deal with the various elements
    # that may appear in the document <head>. In html, the <head> section
    # may contain the following elements:
    #
    #     <script>, <style>, <meta>, <link>, <object>, <base>, <title>
    #
    # All except <title> are handled by code in ::hv3::hv3. Note that the
    # handler for <object> is the same whether the element is located in
    # the head or body of the html document.
    #
    $myHtml handler node   link     [mymethod link_node_handler]
    $myHtml handler node   base     [mymethod base_node_handler]
    $myHtml handler node   meta     [mymethod meta_node_handler]
    $myHtml handler node   title    [mymethod title_node_handler]
    $myHtml handler script style    [mymethod style_script_handler]

    # $myHtml handler script script   [mymethod script_script_handler]
    $myHtml handler script script   [list $myDom script]

    # Register handler commands to handle <object> and kin.
    $myHtml handler node object   [list hv3_object_handler $self]
    $myHtml handler node embed    [list hv3_object_handler $self]

    # Register handler commands to handle <body>.
    $myHtml handler node body   [mymethod body_node_handler]
  }

  destructor {
    # Destroy the components. We don't need to destroy the scrolled
    # html component because it is a Tk widget - it is automatically
    # destroyed when it's parent widget is.
    catch { $mySelectionManager destroy }
    catch { $myDynamicManager   destroy }
    catch { $myHyperlinkManager destroy }
    catch { $myUri              destroy }
    catch { $myFormManager      destroy }
    catch { $myMouseManager     destroy }
    catch { $myBase             destroy }

    if {$myRefreshEventId ne ""} {
      after cancel $myRefreshEventId
      set myRefreshEventId ""
    }
  }

  # Return the location URI of the widget.
  #
  method location {} { return [$myUri get] }

  # The argument download-handle contains a configured request. This 
  # method initiates the request. It is used by hv3 and it's component
  # objects (i.e. code in hv3_object_handler).
  #
  method makerequest {downloadHandle} {

    # Put the handle in the myCurrentDownloads list. Add a wrapper to the
    # code in the -failscript and -finscript options to remove it when the
    # download is finished.
    lappend myCurrentDownloads $downloadHandle
    $self set_pending_var
    $downloadHandle destroy_hook [mymethod Finrequest $downloadHandle] 

    # Execute the -requestcmd script. Fail the download and raise
    # an exception if an error occurs during script evaluation.
    set cmd [concat $options(-requestcmd) [list $downloadHandle]]
    set rc [catch $cmd errmsg]
    if {$rc} {
      set einfo $::errorInfo
      catch {$downloadHandle finish}
      error $errmsg $einfo
    }
  }

  # This method is only called internally, via download-handle -failscript
  # and -finscript scripts. It removes the argument handle from the
  # myCurrentDownloads list and invokes [concat $script [list $data]].
  # 
  method Finrequest {downloadHandle} {
    set idx [lsearch $myCurrentDownloads $downloadHandle]
    if {$idx >= 0} {
      set myCurrentDownloads [lreplace $myCurrentDownloads $idx $idx]
      $self set_pending_var
    }
  }

  # Based on the current contents of instance variable $myUri, set the
  # variable identified by the -locationvar option, if any.
  #
  method set_location_var {} {
    if {$options(-locationvar) ne ""} {
      uplevel #0 [list set $options(-locationvar) [$myUri get]]
    }
    event generate $win <<Location>>
  }

  method set_pending_var {} {
    if {$options(-pendingvar) ne ""} {
      set val [expr [llength $myCurrentDownloads] > 0]
      uplevel #0 [list set $options(-pendingvar) $val]
    }
    after cancel [mymethod MightBeComplete]
    after idle [mymethod MightBeComplete]
  }

  method MightBeComplete {} {
    if {[llength $myCurrentDownloads] == 0} {
      $myHtml delay 0
      event generate $win <<Complete>>
    }
  }

  method resolve_uri {baseuri {uri {}}} {
    if {$uri eq ""} {
      set uri $baseuri
      if {$myBase ne ""} {
        set baseuri [$myBase get -nofragment]
      } else {
        set baseuri [$myUri get -nofragment]
      }
    } 
    set obj [::hv3::uri %AUTO% [string trim $baseuri]]
    $obj load [string trim $uri]
    set ret [$obj get]
    $obj destroy
    return $ret
  }

  # This proc is registered as the -imagecmd script for the Html widget.
  # The argument is the URI of the image required.
  #
  # This proc creates a Tk image immediately. It also kicks off a fetch 
  # request to obtain the image data. When the fetch request is complete,
  # the contents of the Tk image are set to the returned data in proc 
  # ::hv3::imageCallback.
  #
  method Imagecmd {uri} {

    # Massage the URI a bit. Trim whitespace from either end.
    set uri [string trim $uri]

    if {[string match replace:* $uri]} {
        set img [string range $uri 8 end]
        return $img
    }
    set name [image create photo]

    if {$uri ne ""} {
      set full_uri [$self resolve_uri $uri]
    
      # Create and execute a download request. For now, "expect" a mime-type
      # of image/gif. This should be enough to tell the protocol handler to
      # expect a binary file (of course, this is not correct, the real
      # default mime-type might be some other kind of image).
      set handle [::hv3::download %AUTO%              \
          -uri          $full_uri                      \
          -mimetype     image/gif                      \
          -cachecontrol $myCacheControl                \
      ]
      $handle configure -finscript [mymethod Imagecallback $handle $name]
      $self makerequest $handle
    }

    # Return a list of two elements - the image name and the image
    # destructor script. See tkhtml(n) for details.
    return [list $name [list image delete $name]]
  }

  # This method is called to handle the "Location" header for all requests
  # except requests for the main document (see the [Refresh] method for
  # these). If there is a Location method, then the handle object is
  # destroyed, a new one dispatched and 1 returned. Otherwise 0 is returned.
  #
  method HandleLocation {handle} {
    # Check for a "Location" header. TODO: Handling Location
    # should be done in one common location for everything except 
    # the main document. The main document is a bit different...
    # or is it?
    set location ""
    foreach {header value} [$handle cget -header] {
      if {$header eq "Location"} {
        set location $value
      }
    }

    if {$location ne ""} {
      set finscript [$handle cget -finscript]
      $handle destroy
      set full_location [$self resolve_uri $location]
      set handle2 [::hv3::download $handle               \
          -uri          $full_location                   \
          -mimetype     image/gif                        \
          -cachecontrol $myCacheControl                  \
      ]
      $handle2 configure -finscript $finscript
      $self makerequest $handle2
      return 1
    }
    return 0
  }

  # This proc is called when an image requested by the -imagecmd callback
  # ([imagecmd]) has finished downloading. The first argument is the name of
  # a Tk image. The second argument is the downloaded data (presumably a
  # binary image format like gif). This proc sets the named Tk image to
  # contain the downloaded data.
  #
  method Imagecallback {handle name data} {
    if {0 == [$self HandleLocation $handle]} {
      # If the image data is invalid, it is not an error. Possibly hv3
      # should log a warning - if it had a warning system....
      catch { $name configure -data $data }
      $handle destroy
    }
  }

  # Request the resource located at URI $full_uri and treat it as
  # a stylesheet. The parent stylesheet id is $parent_id. This
  # method is used for stylesheets obtained by either HTML <link> 
  # elements or CSS "@import {...}" directives.
  #
  method Requeststyle {parent_id full_uri} {
    set id        ${parent_id}.[format %.4d [incr myStyleCount]]
    set importcmd [mymethod Requeststyle $id]
    set urlcmd    [mymethod resolve_uri $full_uri]
    append id .9999

    set handle [::hv3::download %AUTO%              \
        -uri         $full_uri                      \
        -mimetype    text/css                       \
        -cachecontrol $myCacheControl               \
    ]
    $handle configure -finscript [
        mymethod Finishstyle $handle $id $importcmd $urlcmd
    ]
    $self makerequest $handle
  }

  # Callback invoked when a stylesheet request has finished. Made
  # from method Requeststyle above.
  #
  method Finishstyle {handle id importcmd urlcmd data} {
    if {0 == [$self HandleLocation $handle]} {
      $myHtml style -id $id -importcmd $importcmd -urlcmd $urlcmd $data
      $self goto_fragment
      $handle destroy
    }
  }

  # Node handler script for <meta> tags.
  #
  method meta_node_handler {node} {
    set httpequiv [string tolower [$node attr -default "" http-equiv]]
    set content   [$node attr -default "" content]

    switch -- $httpequiv {
      refresh {
        $self Refresh $content
      }

      content-type {
        if {$myChangeEncodingOk} {
          foreach {a b enc} [::hv3::string::parseContentType $content] {}
          set myEncoding $enc
          if {[string match -nocase *utf-8* $myEncoding]} {
            set myEncoding ""
          }
        }
      }
    }
  }

  # This method is called to handle "Refresh" and "Location" headers
  # delivered as part of the response to a request for a document to
  # display in the main window. Refresh headers specified as 
  # <meta type=http-equiv> markup are also handled. The $content argument
  # contains a the content portion of the Request header, for example:
  #
  #     "5 ; URL=http://www.news.com"
  #
  # (wait 5 seconds before loading the page www.news.com).
  #
  # In the case of Location headers, a synthetic Refresh content header is
  # constructed to pass to this method.
  #
  method Refresh {content} {
    # Use a regular expression to extract the URI and number of seconds
    # from the header content. Then dequote the URI string.
    set uri ""
    set re {([[:digit:]]+) *; *[Uu][Rr][Ll] *= *([^ ]+)}
    regexp $re $content -> seconds uri
    regexp {[^\"\']+} $uri uri                  ;# Primitive dequote

    if {$uri ne ""} {
      if {$myRefreshEventId ne ""} {
          after cancel $myRefreshEventId
      }
      set cmd [list $self RefreshEvent $uri]
      set myRefreshEventId [after [expr {$seconds*1000}] $cmd]

      # puts "Parse of content for http-equiv refresh successful! ($uri)"
    } else {
      # puts "Parse of content for http-equiv refresh failed..."
    }
  }

  method RefreshEvent {uri} {
    set myRefreshEventId ""
    $self goto $uri -nosave
  }

  # System for handling <title> elements. This object exports
  # a method [titlevar] that returns a globally valid variable name
  # to a variable used to store the string that should be displayed as the
  # "title" of this document. The idea is that the caller add a trace
  # to that variable.
  #
  method title_node_handler {node} {
    set val ""
    foreach child [$node children] {
      append val [$child text]
    }
    set myTitleVar $val
  }
  variable myTitleVar ""
  method titlevar {}    {return [myvar myTitleVar]}

  # Node handler script for <body> tags. The purpose of this handler
  # and the [body_style_handler] method immediately below it is
  # to handle the 'overflow' property on the document root element.
  # 
  # Also, fire the DOM "onload" event on the body node.
  #
  method body_node_handler {node} {
    $node replace dummy -stylecmd [mymethod body_style_handler $node]
    $myDom event onload $node
  }
  method body_style_handler {bodynode} {

    if {$options(-scrollbarpolicy) ne "auto"} {
      $myHtml configure -scrollbarpolicy $options(-scrollbarpolicy)
      return
    }

    set htmlnode [$bodynode parent]
    set overflow [$htmlnode property overflow]

    # Variable $overflow now holds the value of the 'overflow' property
    # on the root element (the <html> tag). If this value is not "visible",
    # then the value is used to govern the viewport scrollbars. If it is
    # visible, then use the value of 'overflow' on the <body> element.
    # See section 11.1.1 of CSS2.1 for details.
    #
    if {$overflow eq "visible"} {
      set overflow [$bodynode property overflow]
    }
    switch -- $overflow {
      visible { $myHtml configure -scrollbarpolicy auto }
      auto    { $myHtml configure -scrollbarpolicy auto }
      hidden  { $myHtml configure -scrollbarpolicy 0 }
      scroll  { $myHtml configure -scrollbarpolicy 1 }
      default {
        puts stderr "Hv3 is confused: <body> has \"overflow:$overflow\"."
        $myHtml configure -scrollbarpolicy auto
      }
    }
  }

  # Node handler script for <link> tags.
  #
  method link_node_handler {node} {
    set rel  [string tolower [$node attr -default "" rel]]
    set href [string trim [$node attr -default "" href]]
    set media [string tolower [$node attr -default all media]]
    if {
        [string match *stylesheet* $rel] &&
        ![string match *alternat* $rel] &&
        $href ne "" && 
        [regexp all|screen $media]
    } {
      set full_uri [$self resolve_uri $href]
      $self Requeststyle author $full_uri
    }
  }

  # Node handler script for <base> tags.
  #
  method base_node_handler {node} {
    set baseuri [$node attr -default "" href]
    if {$baseuri ne ""} {
      # Technically, a <base> tag is required to specify an absolute URI.
      # If a relative URI is specified, hv3 resolves it relative to the
      # current location URI. This is not standards compliant, but seems
      # like a reasonable idea.
      if {$myBase ne ""} {$myBase destroy}
      set myBase [::hv3::uri %AUTO% [$myUri get -nofragment]]
      $myBase load $baseuri
    }
  }

  # Script handler for <style> tags.
  #
  method style_script_handler {attr script} {
    array set attributes $attr
    if {[info exists attributes(media)]} {
      if {0 == [regexp all|screen $attributes(media)]} return ""
    }

    set id        author.[format %.4d [incr myStyleCount]]
    set importcmd [mymethod Requeststyle $id]
    set urlcmd    [mymethod resolve_uri]
    append id .9999
    $myHtml style -id $id -importcmd $importcmd -urlcmd $urlcmd $script
    return ""
  }

  method goto_fragment {} {
    set fragment [$myUri cget -fragment]
    if {$fragment ne ""} {
      set selector [format {[name="%s"]} $fragment]
      set goto_node [lindex [$myHtml search $selector] 0]

      # If there was no node with the name attribute set to the fragment,
      # search for a node with the id attribute set to the fragment.
      if {$goto_node eq ""} {
        set selector [format {[id="%s"]} $fragment]
        set goto_node [lindex [$myHtml search $selector] 0]
      }

      if {$goto_node ne ""} {
        $myHtml yview $goto_node
      }
    }
  }

  method documentcallback {handle savestate final data} {

    if {$myMimetype eq ""} {
  
      # TODO: Real mimetype parser...
      set mimetype  [string trim [$handle cget -mimetype]]
      foreach {major minor} [split $mimetype /] {}
  
      switch -- $major {
        text {
          set myQuirksmode [::hv3::configure_doctype_mode $myHtml $data]
          $self reset $savestate
          set myMimetype html
          set myEncoding ""
          set myChangeEncodingOk 1
        }
  
        image {
          set myImageData ""
          $self reset $savestate
          set myMimetype image
        }
  
        default {
          # Neither text nor an image. This is the upper layers problem.
          if {$options(-downloadcmd) ne ""} {
            # Remove the download handle from the list of handles to cancel
            # if [$hv3 stop] is invoked (when the user clicks the "stop" button
            # we don't want to cancel pending save-file operations).
            set idx [lsearch $myCurrentDownloads $handle]
            if {$idx >= 0} {
              set myCurrentDownloads [lreplace $myCurrentDownloads $idx $idx]
              $self set_pending_var
            }
            eval [linsert $options(-downloadcmd) end $handle $data]
          } else {
            $handle destroy
            set sheepish "Don't know how to handle \"$mimetype\""
            tk_dialog .apology "Sheepish apology" $sheepish 0 OK
          }
          return
        }
      }
  
      $myUri load [$handle cget -uri]
      $self set_location_var
      set myForceReload 0
      set myStyleCount 0

      # If there is a "Location" or "Refresh" header, handle it now.
      set refreshheader ""
      foreach {name value} [$handle cget -header] {
        switch -- $name {
          Location {
            set refreshheader "0 ; URL=$value"
          }
          Refresh {
            set refreshheader $value
          }
          Content-Type {
            set tokens [::hv3::string::tokenise $value]
            foreach {a b enc} [::hv3::string::parseContentType $value] {}
            if {$enc ne ""} {
              set myChangeEncodingOk 0
            }
          }
        }
      }
      if {$refreshheader ne ""} {
        $self Refresh $refreshheader
      }
    }

    switch -- $myMimetype {
      html  {$self HtmlCallback $handle $final $data}
      image {$self ImageCallback $handle $final $data}
    }
    set myChangeEncodingOk 0


    if {$final} {
      $handle destroy
    }
  }

  method EncodingConvertfrom {encoding input} {
    switch -regexp -- $encoding {
      ^iso- {
        set encoding iso[string range $encoding 4 end]
      }

      {^windows-874$} {
        set encoding tis-620
      }
    }
    
    set utf8 $input
    if {[catch {
      set utf8 [encoding convertfrom $encoding $utf8]
    } msg]} {
      tk_dialog .dialog "Unknown Encoding" $msg error 0 Ok
    }
    return $utf8
  }

  method HtmlCallback {handle isFinal data} {
    if {$myEncoding eq ""} {
      $myHtml parse $data
    }
    if {$myEncoding ne ""} {
      $myHtml reset
      append myEncodedDocument $data
    }
    if {$isFinal} {
      if {$myEncoding ne ""} {
        set utf8 [$self EncodingConvertfrom $myEncoding $myEncodedDocument]
        set myEncodedDocument ""
        $myHtml parse -final $utf8
      } else {
        $myHtml parse -final {}
      }
      $self goto_fragment
    }
  }

  method ImageCallback {handle isFinal data} {
    append myImageData $data
    if {$isFinal} {
      set img [image create photo -data $myImageData]
      set myImageData ""
      set imagecmd [$myHtml cget -imagecmd]
      $myHtml configure -imagecmd [list ::hv3::ReturnWithArgs $img]
      $myHtml parse -final { <img src="unused"> }
      $myHtml force
      $myHtml configure -imagecmd $imagecmd
    }
  }

  method Formcmd {method node uri querytype encdata} {
    set cmd [linsert [$self cget -targetcmd] end $node]
    [eval $cmd] Formcmd2 $method $uri $querytype $encdata
  }

  method Formcmd2 {method uri querytype encdata} {
    # puts "Formcmd $method $uri $querytype $encdata"
    set full_uri [$self resolve_uri $uri]

    event generate $win <<Goto>>

    set handle [::hv3::download %AUTO% -mimetype text/html]
    set myMimetype ""
    $handle configure                                       \
        -incrscript [mymethod documentcallback $handle 1 0] \
        -finscript  [mymethod documentcallback $handle 1 1] \
        -requestheader [list Referer [$self location]]

    if {$method eq "post"} {
      $handle configure -uri $full_uri -postdata $encdata
      $handle configure -enctype $querytype
      $handle configure -cachecontrol normal
    } else {
      $handle configure -uri "${full_uri}?${encdata}"
      $handle configure -cachecontrol $myCacheControl
    }  
    $self makerequest $handle

    # Grab the keyboard focus for this widget. This is so that after
    # the form is submitted the arrow keys and PgUp/PgDown can be used
    # to scroll the main display.
    #
    focus [$self html]
  }

  method seturi {uri} {
    $myUri load $uri
  }

  #--------------------------------------------------------------------------
  # PUBLIC INTERFACE TO HV3 WIDGET STARTS HERE:
  #
  #     Method              Delegate
  # --------------------------------------------
  #     goto                N/A
  #     xview               $myHtml
  #     yview               $myHtml
  #     html                N/A
  #     hull                N/A
  #   

  # The caching version of the html widget [node] subcommand. The rational
  # here is that several different application components need to be notified
  # of the list of nodes under the cursor every time the cursor moves.
  #
  variable myNodeArgs 
  variable myNodeRes
  method node {args} {
    if {![info exists myNodeArgs] || $myNodeArgs ne $args} {
      set myNodeArgs $args
      set myNodeRes [eval [linsert $args 0 $myHtml node]]
    }
    return $myNodeRes
  }
  method invalidate_nodecache {} {
    unset -nocomplain myNodeArgs
  }

  method dom {} { return $myDom }

  #--------------------------------------------------------------------
  # Load the URI specified as an argument into the main browser window.
  # This method has the following syntax:
  #
  #     $hv3 goto URI ?OPTIONS?
  #
  # Where supported options are:
  #
  #     -cachecontrol "normal"|"relax-transparency"|"no-cache"
  #     -nosave
  #     -referer URI
  #
  # The -cachecontrol option (default "normal") specifies the value 
  # that will be used for all ::hv3::request objects issued as a 
  # result of this load URI operation.
  #
  # Normally, a <<SaveState>> event is generated. If -nosave is specified, 
  # this is suppressed.
  # 
  method goto {uri args} {

    # Process the argument switches. Local variable $cachecontrol
    # is set to the effective value of the -cachecontrol option.
    # Local boolean var $savestate is true unless the -nogoto
    # option is specified.
    set savestate 1
    set cachecontrol normal
    set referer ""

    for {set iArg 0} {$iArg < [llength $args]} {incr iArg} {
      switch -- [lindex $args $iArg] {
        -cachecontrol {
          incr iArg
          set cachecontrol [lindex $args $iArg]
        }
        -referer {
          incr iArg
          set referer [lindex $args $iArg]
        }
        -nosave {
          set savestate 0
        }
        default {
          error "Bad option \"[lindex $args $iArg]\" to \[::hv3::hv3 goto\]"
        }
      }
    }

    # Special case. If this URI begins with "javascript:" (case independent),
    # pass it to the current running DOM implementation instead of loading
    # anything into the current browser.
    if {[string match -nocase javascript:* $uri]} {
      $myDom javascript [string range $uri 11 end]
      return
    }

    set myCacheControl $cachecontrol

    set current_uri [$myUri get -nofragment]
    set uri_obj [::hv3::uri %AUTO% $current_uri]
    $uri_obj load $uri
    # set full_uri [$uri_obj get -nofragment]
    set full_uri [$uri_obj get -nofragment]
    set fragment [$uri_obj cget -fragment]

    # Generate the <<Goto>> event.
    event generate $win <<Goto>>

    if {$full_uri eq $current_uri && $fragment ne ""} {
      # Save the current state in the history system. This ensures
      # that back/forward controls work when navigating between
      # different sections of the same document.
      if {$savestate} {
        event generate $win <<SaveState>>
      }

      $myUri load $uri
      $self goto_fragment
      $self set_location_var

      return [$myUri get]
    }

    # Abandon any pending requests
    $self stop

    # Base the expected type on the extension of the filename in the
    # URI, if any. If we can't figure out an expected type, assume
    # text/html. The protocol handler may override this anyway.
    set mimetype text/html
    set path [$uri_obj cget -path]
    if {[regexp {\.([A-Za-z0-9]+)$} $path dummy ext]} {
      switch -- [string tolower $ext] {
	jpg  { set mimetype image/jpeg }
        jpeg { set mimetype image/jpeg }
        gif  { set mimetype image/gif  }
        png  { set mimetype image/png  }
        gz   { set mimetype application/gzip  }
        gzip { set mimetype application/gzip  }
        zip  { set mimetype application/gzip  }
        kit  { set mimetype application/binary }
      }
    }

    # Create a download request for this resource. We expect an html
    # document, but at this juncture the URI may legitimately refer
    # to kind of resource.
    #
    set handle [::hv3::download %AUTO%             \
        -uri         [$uri_obj get]                \
        -mimetype    $mimetype                     \
        -cachecontrol $myCacheControl              \
    ]
    set myMimetype ""
    $handle configure                                     \
        -incrscript [mymethod documentcallback $handle $savestate 0] \
        -finscript  [mymethod documentcallback $handle $savestate 1]
    if {$referer ne ""} {
      $handle configure -requestheader [list Referer $referer]
    }

    $self makerequest $handle
    $uri_obj destroy
  }

  # Abandon all currently pending downloads. This method is part of the
  # public interface.
  method stop {} {
    foreach dl $myCurrentDownloads {
      $dl finish
    }
  }

  method reset {isSaveState} {

    # Cancel any pending "Refresh" event.
    if {$myRefreshEventId ne ""} {
      after cancel $myRefreshEventId
      set myRefreshEventId ""
    }

    # Generate the <<Reset>> and <<SaveState> events.
    if {!$myFirstReset && $isSaveState} {
      event generate $win <<SaveState>>
    }
    set myFirstReset 0
    event generate $win <<Reset>>

    $self invalidate_nodecache
    set myTitleVar ""
    set myEncoding ""
    set myEncodedDocument ""

    foreach m [list \
        $myMouseManager $myFormManager $myDom   \
        $mySelectionManager $myHyperlinkManager \
    ] {
      if {$m ne ""} {$m reset}
    }
    $myHtml reset

    if {$myBase ne ""} {
      $myBase destroy
      set myBase ""
    }

    set myQuirksmode unknown
  }

  method SetOption {option value} {
    set options($option) $value
    switch -- $option {
      -enableimages {
        # The -enableimages switch. If false, configure an empty string
        # as the html widget's -imagecmd option. If true, configure the
        # same option to call the [Imagecmd] method of this mega-widget.
        # In either case reload the frame contents.
        #
        if {$value} {
          $myHtml configure -imagecmd [mymethod Imagecmd]
        } else {
          $myHtml configure -imagecmd ""
        }
        set uri [$myUri get]
        $self reset 0
        $self goto $uri -nosave
      }
    }
  }

  method pending {}  { return [llength $myCurrentDownloads] }
  method location {} { return [$myUri get] }
  method html {}     { return [$myHtml widget] }
  method hull {}     { return $hull }

  option -enableimages -default 1 -configuremethod SetOption
  option -scrollbarpolicy -default auto

  option          -locationvar      -default ""
  option          -pendingvar       -default ""
  option          -downloadcmd      -default ""
  option          -requestcmd       -default ""
  delegate option -isvisitedcmd     to myHyperlinkManager
  delegate option -targetcmd        to myHyperlinkManager

  # Delegated public methods
  delegate method javascriptlog     to myDom
  delegate method dumpforms         to myFormManager
  delegate method selected          to mySelectionManager
  delegate method *                 to myHtml

  # Standard scrollbar and geometry stuff is delegated to the html widget
  delegate option -xscrollcommand to myHtml
  delegate option -yscrollcommand to myHtml
  delegate option -width          to myHtml
  delegate option -height         to myHtml

  # Display configuration options implemented entirely by the html widget
  delegate option -fonttable        to myHtml
  delegate option -fontscale        to myHtml
  delegate option -zoom             to myHtml
  delegate option -forcefontmetrics to myHtml
  delegate option -doublebuffer     to myHtml
}

bind Hv3 <KeyPress-Up>     { %W yview scroll -1 units }
bind Hv3 <KeyPress-Down>   { %W yview scroll  1 units }
bind Hv3 <KeyPress-Return> { %W yview scroll  1 units }
bind Hv3 <KeyPress-Right>  { %W xview scroll  1 units }
bind Hv3 <KeyPress-Left>   { %W xview scroll -1 units }
bind Hv3 <KeyPress-Next>   { %W yview scroll  1 pages }
bind Hv3 <KeyPress-space>  { %W yview scroll  1 pages }
bind Hv3 <KeyPress-Prior>  { %W yview scroll -1 pages }

proc ::hv3::bg {script args} {
  set eval [concat $script $args]
  set rc [catch [list uplevel $eval] result]
  if {$rc} {
    set cmd [list bgerror $result]
    set error [list $::errorInfo $::errorCode]
    after idle [list foreach {::errorInfo ::errorCode} $error $cmd]
    set ::errorInfo ""
    return ""
  }
  return $result
}

proc ::hv3::ReturnWithArgs {retval args} {
  return $retval
}

