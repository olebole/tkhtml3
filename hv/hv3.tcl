namespace eval hv3 { set {version($Id: hv3.tcl,v 1.109 2006/09/17 07:36:43 danielk1977 Exp $)} 1 }

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
#     -cancelrequestcmd
#         If not an empty string, this option specifies a script that
#         is invoked by the widget to cancel an earlier invocation of
#         the -requestcmd script. The download handle to be cancelled
#         is appended to the script before it is invoked.
#
#     -hyperlinkcmd
#         If not an empty string, this option specifies a script for
#         the widget to invoke when a hyperlink is clicked on. The script
#         is invoked with the node handle of the clicked hyper-link element
#         appended.
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
#     goto URI
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
#         Return the path of the underlying html widget. This should only
#         be used to determine paths for child widgets. Bypassing hv3 and
#         accessing the html widget interface directly may confuse hv3.
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
#         This event is generated whenever the "location" is set. The
#         field %location contains the new URI.
#
#


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
# The -hyperlinkcmd option of ::hv3::hv3 is delegated to an
# ::hv3::hyperlinkmanager component.
#
package require Tkhtml 3.0
package require snit

source [file join [file dirname [info script]] hv3_form.tcl]
source [file join [file dirname [info script]] hv3_widgets.tcl]
source [file join [file dirname [info script]] hv3_object.tcl]
source [file join [file dirname [info script]] hv3_doctype.tcl]

proc assert {expr} {
  if { 0 == [uplevel [list expr $expr]] } {
    error "assert() failed - $expr"
  }
}

#--------------------------------------------------------------------------
# Class ::hv3::uri:
#
#     A very simple class for handling URI references. A partial 
#     implementation of the syntax specification found at: 
#
#         http://www.gbiv.com/protocols/uri/rfc/rfc3986.html
# 
# Usage:
#
#     set uri_obj [::hv3::uri %AUTO% $URI]
#
#     $uri_obj load $URI
#     $uri_obj get
#     $uri_obj cget ?option?
#
#     $uri_obj destroy
#
snit::type ::hv3::uri {

  # Public get/set variables for URI components
  option -scheme    file
  option -authority ""
  option -path      "/"
  option -query     ""
  option -fragment  ""

  # Constructor and destructor
  constructor {{url {}}} {$self load $url}
  destructor  {}

  # Remove any dot segments "." or ".." from the argument path and 
  # return the result.
  proc remove_dot_segments {path} {
    set output [list]
    foreach component [split $path /] {
      switch -- $component {
        .       { #Do nothing }
        ..      { set output [lreplace $output end end] }
        default { 
          if {[string match ?: $component]} {
            set component [string range $component 0 0]
          }
          if {$output ne "" || $component ne ""} {
            lappend output $component 
          }
        }
      }
    }
    return "/[join $output /]"
  }

  proc merge {path1 path2} {
    return [regsub {[^/]*$} $path1 $path2]
  }

  # Set the contents of the object to the specified URI.
  method load {uri} {

    # First, parse the argument URI into it's 5 main components. All five
    # components are optional, as shown in the following syntax (each bracketed
    # section is optional).
    #
    #     (SCHEME ":") ("//" AUTHORITY) (PATH) ("?" QUERY) ("#" FRAGMENT)
    #
    # Save each of the components, if they exist, in the variables 
    # $Scheme, $Authority, $Path, $Query and $Fragment.
    set str $uri
    foreach {re var} [list \
        {^([A-Za-z][A-Za-z0-9+-\.]+):(.*)} Scheme            \
        {^//([^/#?]*)(.*)}                 Authority         \
        {^([^#?]*)(.*)}                    Path              \
        {^\?([^#]*)(.*)}                   Query             \
        {^#(.*)(.*)}                       Fragment          \
    ] {
      if {[regexp $re $str dummy A B]} {
        set $var $A
        set str $B
      }
    }
    if {$str ne ""} {
      error "Bad URL: $url"
    }

    # Using the current contents of the option variables as a base URI,
    # transform the relative argument URI to an absolute URI. The algorithm
    # used is defined in section 5.2.2 of RFC 3986.
    #
    set hasScheme 1
    set hasAuthority 1
    set hasQuery 1
    if {![info exists Path]}      {set Path ""}
    if {![info exists Fragment]}  {set Fragment ""}
    if {![info exists Scheme]}    {set Scheme ""    ; set hasScheme 0}
    if {![info exists Authority]} {set Authority "" ; set hasAuthority 0}
    if {![info exists Query]}     {set Query ""     ; set hasQuery 0}

    if {$hasScheme} {
      set options(-scheme)    $Scheme
      set options(-authority) $Authority
      set options(-path)      [remove_dot_segments $Path]
      set options(-query)     $Query
    } else {
      if {$hasAuthority} {
        set options(-authority) $Authority
        set options(-path)      [remove_dot_segments $Path]
        set options(-query)     $Query
      } else {
        if {$Path eq ""} {
          if {$hasQuery} {
            set options(-query) $Query
          }
        } else {
          if {[string match {/*} $Path]} {
            set options(-path) [remove_dot_segments $Path]
          } else {
            set merged_path [merge $options(-path) $Path]
            set options(-path) [remove_dot_segments $merged_path]
          }
          set options(-query) $Query
        }
      }
    }
    set options(-fragment) $Fragment
  }

  # Return the contents of the object formatted as a URI.
  method get {{nofragment ""}} {
    set result "$options(-scheme)://$options(-authority)"
    ::append result "$options(-path)"
    if {$options(-query) ne ""}    {
      ::append result "?$options(-query)"
    }
    if {$nofragment eq "" && $options(-fragment) ne ""} {
      ::append result "#$options(-fragment)"
    }
    return $result
  }
}

# End of class ::hv3::uri
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Automated tests for ::hv3::uri:
#
#     The following block runs some quick regression tests on the ::hv3::uri 
#     implementation. These take next to no time to run, so there's little
#     harm in leaving them in.
#
if 1 {
  set test_data [list                                                 \
    {http://tkhtml.tcl.tk/index.html}                                 \
        -scheme http       -authority tkhtml.tcl.tk                   \
        -path /index.html  -query "" -fragment ""                     \
    {http:///index.html}                                              \
        -scheme http       -authority ""                              \
        -path /index.html  -query "" -fragment ""                     \
    {http://tkhtml.tcl.tk}                                            \
        -scheme http       -authority tkhtml.tcl.tk                   \
        -path "/"          -query "" -fragment ""                     \
    {/index.html}                                                     \
        -scheme http       -authority tkhtml.tcl.tk                   \
        -path /index.html  -query "" -fragment ""                     \
    {other.html}                                                      \
        -scheme http       -authority tkhtml.tcl.tk                   \
        -path /other.html  -query "" -fragment ""                     \
    {http://tkhtml.tcl.tk:80/a/b/c/index.html}                        \
        -scheme http       -authority tkhtml.tcl.tk:80                \
        -path "/a/b/c/index.html" -query "" -fragment ""              \
    {http://wiki.tcl.tk/}                                             \
        -scheme http       -authority wiki.tcl.tk                     \
        -path "/"          -query "" -fragment ""                     \
    {file:///home/dan/fbi.html}                                       \
        -scheme file       -authority ""                              \
        -path "/home/dan/fbi.html"  -query "" -fragment ""            \
    {http://www.tclscripting.com}                                     \
        -scheme http       -authority "www.tclscripting.com"          \
        -path "/"  -query "" -fragment ""                             \
    {file:///c:/dir1/dir2/file.html}                                  \
        -scheme file       -authority ""                              \
        -path "/c/dir1/dir2/file.html"  -query "" -fragment ""       \
    {relative.html}                                                   \
        -scheme file       -authority ""                              \
        -path "/c/dir1/dir2/relative.html"  -query "" -fragment ""   \
    ]

  set obj [::hv3::uri %AUTO%]
  for {set ii 0} {$ii < [llength $test_data]} {incr ii} {
    set uri [lindex $test_data $ii]
    $obj load $uri 
    while {[string match -* [lindex $test_data [expr $ii+1]]]} {
      set switch [lindex $test_data [expr $ii+1]]
      set value [lindex $test_data [expr $ii+2]]
      if {[$obj cget $switch] ne $value} {
        puts "URI: $uri"
        puts "SWITCH: $switch"
        puts "EXPECTED: $value"
        puts "GOT: [$obj cget $switch]"
        puts ""
      }
      incr ii 2
    }
  }
  $obj destroy
}
# End of tests for ::hv3::uri.
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class ::hv3::hv3::selectionmanager
#
snit::type ::hv3::hv3::selectionmanager {
  variable myHv3
  variable myState 0               ;# True when left-button is held down

  variable myFromNode
  variable myFromIdx

  variable myToNode
  variable myToIdx

  constructor {hv3} {
    set myHv3 $hv3
    selection handle $myHv3 [list ::hv3::bg [mymethod get_selection]]

    bind $myHv3 <Motion>          "+[mymethod motion %x %y]"
    bind $myHv3 <ButtonPress-1>   "+[mymethod press %x %y]"
    bind $myHv3 <ButtonRelease-1> "+[mymethod release %x %y]"
  }

  method press {x y} {
    set myState 1
    set from [$myHv3 node -index $x $y]
    if {[llength $from]==2} {
      foreach {node index} $from {}
      $myHv3 tag delete selection
      $myHv3 tag configure selection -foreground white -background darkgrey
      set myFromNode $node
      set myFromIdx $index
      set myToNode $node
      set myToIdx $index
    } else {
      set myToNode ""
    }
  }

  method release {x y} {
    set myState 0
  }

  method reset {} {
    $myHv3 tag delete selection
    set myState 0
  }

  method motion {x y} {
    if {0 == $myState} return
    if {$myToNode eq ""} {
      $self press $x $y
      return
    }
    set to [$myHv3 node -index $x $y]
    if {[llength $to]==2} {
      foreach {node index} $to {}
      if {$myToNode ne $node || $index != $myToIdx} {
        $myHv3 tag remove selection $myToNode $myToIdx $node $index
        set myToNode $node
        set myToIdx $index
        $myHv3 tag add selection $myFromNode $myFromIdx $myToNode $myToIdx
      }
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
    set n1 $myFromNode
    set i1 $myFromIdx
    set n2 $myToNode
    set i2 $myToIdx

    set stridx_a [$myHv3 text offset $myFromNode $myFromIdx]
    set stridx_b [$myHv3 text offset $myToNode $myToIdx]
    if {$stridx_a > $stridx_b} {
      foreach {stridx_a stridx_b} [list $stridx_b $stridx_a] {}
    }
  
    set T [string range [$myHv3 text text] $stridx_a [expr $stridx_b - 1]]
    set T [string range $T $offset [expr $offset + $maxChars]]

    return $T
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
  variable myHv3
  variable myHoverNodes [list]
  variable myActiveNodes [list]

  constructor {hv3} {
    set myHv3 $hv3
    bind $myHv3 <Motion> "+[mymethod motion %x %y]"
    bind $myHv3 <ButtonPress-1>   "+[mymethod press %x %y]"
    bind $myHv3 <ButtonRelease-1> "+[mymethod release %x %y]"
  }

  method reset {} {
    set myHoverNodes [list]
    set myActiveNodes [list]
  }

  method press {x y} {
    set N [lindex [$myHv3 node $x $y] end]
    while {$N ne ""} {
      lappend myActiveNodes $N
      $N dynamic set active
      set N [$N parent]
    }
  }

  method release {x y} {
    foreach N $myActiveNodes {
      $N dynamic clear active
    }
    set myActiveNodes [list]
  }

  method motion {x y} {
    set nodelist [lindex [$myHv3 node $x $y] end]
    set hovernodes $myHoverNodes
    set myHoverNodes [list]
    foreach node $nodelist {
      for {set n $node} {$n ne ""} {set n [$n parent]} {
        set idx [lsearch $hovernodes $n]
        lappend myHoverNodes $n
        if {$idx < 0} {
          $n dynamic set hover
        } else {
          set hovernodes [lreplace $hovernodes $idx $idx]
        }
      }
    }
    foreach node $hovernodes {
      $node dynamic clear hover
    }
    set myHoverNodes [lsort -unique $myHoverNodes]
  }
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
#     * -hyperlinkcmd option and associate callbacks
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
  variable myNodes [list]

  option -hyperlinkcmd -default ""
  option -isvisitedcmd -default ""

  constructor {hv3} {
    set myHv3 $hv3
    set options(-hyperlinkcmd) [mymethod default_hyperlinkcmd]

    $myHv3 handler node a [mymethod a_node_handler]
    bind $myHv3 <Motion>          "+[mymethod motion %x %y]"
    bind $myHv3 <ButtonPress-1>   "+[mymethod press %x %y]"
    bind $myHv3 <ButtonRelease-1> "+[mymethod release %x %y]"
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

  method press {x y} {
    set nodelist [$myHv3 node $x $y]
    set myNodes [list]
    foreach node $nodelist {
      for {set n $node} {$n ne ""} {set n [$n parent]} {
        if {[$n tag] eq "a" && [$n attr -default "" href] ne ""} {
          lappend myNodes $n
        }
      }
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

  method release {x y} {
    set nodelist [$myHv3 node $x $y]
    set saved_nodes $myNodes
    set myNodes [list]
    if {$options(-hyperlinkcmd) ne ""} {
      foreach node [$myHv3 node $x $y] {
        for {set n $node} {$n ne ""} {set n [$n parent]} {
          if {[lsearch $saved_nodes $n] >= 0} {
            # Node $n is a hyper-link that has been clicked on.
            # Invoke the -hyperlinkcmd.
            eval [linsert $options(-hyperlinkcmd) end $n]
            return
          }
        }
      }
    }
  }

  method default_hyperlinkcmd {node} {
    set href [string trim [$node attr -default "" href]]
    if {$href ne ""} {
      $myHv3 goto $href
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

  #------------------------------------------------------------------------
  # The following two type-scoped arrays are used as a layer of
  # indirection for the -incrscript and -finscript scripts of download
  # handles. Download handles created by this class may change their
  # behaviour based on their -mimetype value, which is not known
  # until after the download handle is locked (see the -lockscript option).
  #
  # The related type-scoped procs are [RedirectIncr], [RedirectFinish]
  # ,[MakeRedirectable] and [FinishWithData].
  #
  typevariable theHandleIncr   -array ""
  typevariable theHandleFinish -array ""

  proc RedirectIncr {handle data} {
    eval [linsert $theHandleIncr($handle) end $data]
  }
  proc RedirectFinish {handle data} {
    eval [linsert $theHandleFinish($handle) end $data]
    unset theHandleIncr($handle)
    unset theHandleFinish($handle)
  }

  proc MakeRedirectable {handle} {
    set incrscript [$handle cget -incrscript]
    if {$incrscript ne ""} {
      set theHandleIncr($handle) $incrscript
      $handle configure -incrscript [namespace code [list RedirectIncr $handle]]
    }
    set theHandleFinish($handle) [$handle cget -finscript]
    $handle configure -finscript [namespace code [list RedirectFinish $handle]]
  }

  proc FinishWithData {handle data} {
    $handle append $data
    $handle finish
  }
  # End of system for redirecting handles
  #--------------------------------------

  # Object components
  component myHtml                   ;# The [::hv3::scrolled html] widget
  component myHyperlinkManager       ;# The ::hv3::hv3::hyperlinkmanager
  component myDynamicManager         ;# The ::hv3::hv3::dynamicmanager
  component mySelectionManager       ;# The ::hv3::hv3::selectionmanager
  component myFormManager            ;# The ::hv3::formmanager

  # The current location URI and the current base URI. If myBase is "",
  # use the URI stored in myUri as the base.
  #
  variable  myUri                    ;# The current URI (type ::hv3::hv3uri)
  variable  myBase ""                ;# The current URI (type ::hv3::hv3uri)

  # Used to assign internal stylesheet ids.
  variable  myStyleCount 0 

  # This variable may be set to "unknown", "quirks" or "standards".
  variable myQuirksmode unknown

  # List of currently outstanding download-handles. See methods makerequest,
  # Finrequest and <TODO: related to stop?>.
  variable myCurrentDownloads [list]

  variable myInternalObject

  variable myDeps [list]

  constructor {} {
    # Create the scrolled html widget and bind it's events to the
    # mega-widget window.
    set myHtml [::hv3::scrolled html ${win}.html]
    bindtags [$self html] [concat [bindtags [$self html]] $self]
    pack $myHtml -expand true -fill both

    # $myHtml configure -layoutcache 0

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
    $myHtml handler script script   [mymethod script_script_handler]

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
    if {[info exists mySelectionManager]} { $mySelectionManager destroy }
    if {[info exists myDynamicManager]}   { $myDynamicManager   destroy }
    if {[info exists myHyperlinkManager]} { $myHyperlinkManager destroy }
    if {[info exists myUri]}              { $myUri              destroy }
    if {$myBase ne ""}                    { $myBase             destroy }
  }

  # The argument download-handle contains a configured request. This 
  # method initiates the request. It is used by hv3 and it's component
  # objects (i.e. the form-manager).
  #
  method makerequest {downloadHandle} {

    # Put the handle in the myCurrentDownloads list. Add a wrapper to the
    # code in the -failscript and -finscript options to remove it when the
    # download is finished.
    lappend myCurrentDownloads $downloadHandle
    $self set_pending_var
    ::hv3::download_destructor $downloadHandle [
      mymethod Finrequest $downloadHandle 
    ]

    # Check if the full-uri begins with the string "internal:". If so,
    # link this handle to the handle currently stored in object variable
    # $myInternalObject. Otherwise, invoke the -requestcmd script.
    if {[string range [$downloadHandle uri] 0 8] eq "internal:"} {

      # Redirect the -incrscript and -finscript commands of myInternalObject
      # to this new downloadHandle. See the [lockcallback] method for
      # an explanation of what's going on here.
      assert {[info exists myInternalObject] && $myInternalObject ne ""}
      assert {[info exists theHandleFinish($myInternalObject)]}
      assert {[info exists theHandleIncr($myInternalObject)]}
      set theHandleIncr($myInternalObject)   [list $downloadHandle append]
      set theHandleFinish($myInternalObject) [
           namespace code [list FinishWithData $downloadHandle]
      ]
      unset myInternalObject

    } else {

      # Execute the -requestcmd script. Fail the download and raise
      # an exception if an error occurs during script evaluation.
      set cmd [concat $options(-requestcmd) [list $downloadHandle]]
      set rc [catch $cmd errmsg]
      if {$rc} {
        $downloadHandle fail
        error $errmsg $::errorInfo
      }
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
    set obj [::hv3::uri %AUTO% $baseuri]
    $obj load $uri
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
          -uri         $full_uri                      \
          -mimetype    image/gif                      \
      ]
      $handle configure -finscript [mymethod Imagecallback $handle $name]
      $self makerequest $handle
    }

    # Return a list of two elements - the image name and the image
    # destructor script. See tkhtml(n) for details.
    return [list $name [list image delete $name]]
  }

  # This proc is called when an image requested by the -imagecmd callback
  # ([imagecmd]) has finished downloading. The first argument is the name of
  # a Tk image. The second argument is the downloaded data (presumably a
  # binary image format like gif). This proc sets the named Tk image to
  # contain the downloaded data.
  #
  method Imagecallback {handle name data} {
    if {[info commands $name] == ""} return 
    lappend myDeps [$handle uri]

    # If the image data is invalid, it is not an error. Possibly hv3
    # should log a warning - if it had a warning system....
    catch { $name configure -data $data }
  }

  # Request the resource located at URI $full_uri and treat it as
  # a stylesheet. The parent stylesheet id is $parent_id. This
  # method is used for stylesheets obtained by either HTML <link> 
  # elements or CSS "@import {...}" directives.
  #
  method Requeststyle {parent_id full_uri} {
    set id        ${parent_id}.[format %.4d [incr myStyleCount]]
    set importcmd [mymethod import_handler $id]
    set urlcmd    [mymethod resolve_uri $full_uri]
    append id .9999

    set handle [::hv3::download %AUTO%              \
        -uri         $full_uri                      \
        -mimetype    text/css                       \
    ]
    $handle configure -finscript [
        mymethod Finishstyle $handle $id $importcmd $urlcmd
    ]
# puts "Stylesheet at: $full_uri"
    $self makerequest $handle
  }

  # Callback invoked when a stylesheet request has finished. Made
  # from method Requeststyle above.
  #
  method Finishstyle {handle id importcmd urlcmd data} {
# puts "Stylesheet finish: [$handle uri]"
    $myHtml style -id $id -importcmd $importcmd -urlcmd $urlcmd $data
    lappend myDeps [$handle uri]
    $self goto_fragment
  }

  # Handler for CSS @import directives.
  #
  method import_handler {parent_id uri} {
    $self Requeststyle $parent_id $uri
  }

  # Node handler script for <meta> tags.
  #
  method meta_node_handler {node} {
    set httpequiv [string tolower [$node attr -default "" http-equiv]]
    set content   [$node attr -default "" content]

    switch -- $httpequiv {
      refresh {
        # Regular expression to parse content attribute:
        set re {([[:digit:]]+) *; *[Uu][Rr][Ll] *= *([^ ]+)}
        set match [regexp $re $content dummy seconds uri]
        if {$match} {
          regexp {[^\"\']+} $uri uri
          if {$uri ne ""} {
            after [expr $seconds * 1000] [list $self goto $uri]
            # puts "Parse of content for http-equiv refresh successful!"
            # puts $uri
          }
        } else {
          # puts "Parse of content for http-equiv refresh failed..."
        }
      }
    }
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

  method dependencies {} {return $myDeps}


  # Node handler script for <body> tags. The purpose of this handler
  # and the [body_style_handler] method immediately below it is
  # to handle the 'overflow' property on the document root element.
  #
  method body_node_handler {node} {
    $node replace dummy -stylecmd [mymethod body_style_handler $node]
  }
  method body_style_handler {bodynode} {
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
      visible { $self configure -scrollbarpolicy auto }
      auto    { $self configure -scrollbarpolicy auto }
      hidden  { $self configure -scrollbarpolicy 0 }
      scroll  { $self configure -scrollbarpolicy 1 }
      default {
        puts stderr "Hv3 is confused: <body> has \"overflow:$overflow\"."
        $self configure -scrollbarpolicy auto
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
    set importcmd [mymethod import_handler $id]
    set urlcmd    [mymethod resolve_uri]
    append id .9999
    $myHtml style -id $id -importcmd $importcmd -urlcmd $urlcmd $script
    return ""
  }

  # Script handler for <script> tags.
  #
  method script_script_handler {attr script} {
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

  # This method is called as the -lockscript for a download-handle 
  # retrieving content for display in the browser window (possibly, 
  # actually depends on the mimetype).
  #
  method lockcallback {handle} {
    set mimetype  [string trim [$handle mimetype]]

    # TODO: Real mimetype parser...
    foreach {major minor} [split $mimetype /] {}

    switch -- $major {
      text {
        $self reset
      }

      image {
        $self reset
        set myInternalObject $handle
        $self parse -final {
          <html><head></head><body>
            <img src="internal://">
          </body></html>
        }
        $self force
      }

      default {
        # Neither text nor an image. Give the user the option to
        # save the file to disk. What else can you expect from a "demo"?
        $self Savefile $handle
        return
      }
    }

    $myUri load [$handle cget -uri]
    $self set_location_var
    set myForceReload 0
    set myStyleCount 0
  }

  method Savefile {handle} {

    # Create a GUI to handle this download
    set dler [::hv3::filedownload %AUTO%     \
        -source    [$handle uri]           \
        -size      [$handle expected_size] \
        -cancelcmd [list catch [list $handle fail]]     \
    ]
    ::hv3::the_download_manager show

    # Redirect the -incrscript and -finscript commands to the download GUI.
    assert {[info exists theHandleFinish($handle)]}
    set theHandleFinish($handle) [list $dler finish]
    if {[info exists theHandleIncr($handle)]} {
      set theHandleIncr($handle) [list $dler append]
    }

    # Remove the download handle from the list of handles to cancel
    # if [$hv3 stop] is invoked (when the user clicks the "stop" button
    # we don't want to cancel pending save-file operations).
    set idx [lsearch $myCurrentDownloads $handle]
    if {$idx >= 0} {
      set myCurrentDownloads [lreplace $myCurrentDownloads $idx $idx]
      $self set_pending_var
    }

    # Pop up a GUI to select a "Save as..." filename. Schedule this as 
    # a background job to avoid any recursive entry to our event handles.
    set suggested ""
    regexp {/([^/]*)$} [$handle uri] dummy suggested
    set cmd [subst -nocommands {
      $dler set_destination [file normal [
          tk_getSaveFile -initialfile {$suggested}
      ]]
    }]
    after idle $cmd
  }

  method download {uri} {
    set handle [::hv3::download %AUTO%              \
        -uri         $uri                           \
        -mimetype    application/gzip               \
        -incrscript  blah -finscript blah           \
    ]
    MakeRedirectable $handle
    $self makerequest $handle
    $self Savefile $handle
  }

  method documentcallback {handle final data} {

    if {$myQuirksmode eq "unknown"} {
      set myQuirksmode [::hv3::configure_doctype_mode $myHtml $data]
      $myHtml reset
      $myHtml delay 500
    }

    $myHtml parse $data
    if {$final} {
      $myHtml parse -final {}
      $self goto_fragment
    }
  }

  method Formcmd {method uri querytype encdata} {
    # puts "Formcmd $method $uri $querytype $encdata"
    set full_uri [$self resolve_uri $uri]

    event generate $win <<Goto>>

    set handle [::hv3::download %AUTO% -mimetype text/html]
    $handle configure                                     \
        -lockscript [mymethod lockcallback $handle]       \
        -incrscript [mymethod documentcallback $handle 0] \
        -finscript  [mymethod documentcallback $handle 1]
    if {$method eq "post"} {
      $handle configure -uri $full_uri -postdata $encdata
      $handle configure -enctype $querytype
    } else {
      $handle configure -uri "${full_uri}?${encdata}"
    }  
    MakeRedirectable $handle
    $self makerequest $handle
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

  method goto {uri} {

    # Generate the <<Goto>> event.
    event generate $win <<Goto>>

    set current_uri [$myUri get -nofragment]
    set uri_obj [::hv3::uri %AUTO% $current_uri]
    $uri_obj load $uri
    # set full_uri [$uri_obj get -nofragment]
    set full_uri [$uri_obj get -nofragment]
    set fragment [$uri_obj cget -fragment]

    if {$full_uri eq $current_uri && $fragment ne ""} {
      $myUri load $uri
      $self yview moveto 0.0
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
    ]
    $handle configure                                     \
        -lockscript [mymethod lockcallback $handle]       \
        -incrscript [mymethod documentcallback $handle 0] \
        -finscript  [mymethod documentcallback $handle 1]

    MakeRedirectable $handle

    $self makerequest $handle
    $uri_obj destroy
  }

  # Abandon all currently pending downloads. This method is part of the
  # public interface.
  method stop {} {
    foreach dl $myCurrentDownloads {
      $dl fail "Operation cancelled by user"
    }
  }

  method reset {} {

    # Generate the <<Reset>> event.
    event generate $win <<Reset>>

    $self invalidate_nodecache
    set myDeps [list]
    set myTitleVar ""

    foreach m [list $myDynamicManager $myFormManager $mySelectionManager] {
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
        # In either case reload the frame.
        #
        if {$value} {
          $myHtml configure -imagecmd [mymethod Imagecmd]
        } else {
          $myHtml configure -imagecmd ""
        }
        set uri [$myUri get]
        $self reset
        $self goto $uri
      }
    }
  }

  method pending {}  { return [llength $myCurrentDownloads] }
  method location {} { return [$myUri get] }
  method html {}     { return [$myHtml widget] }
  method hull {}     { return $hull }

  option -enableimages -default 1 -configuremethod SetOption

  option          -locationvar      -default ""
  option          -pendingvar       -default ""
  option          -requestcmd       -default ""
  option          -cancelrequestcmd -default ""
  delegate option -hyperlinkcmd     to myHyperlinkManager
  delegate option -isvisitedcmd     to myHyperlinkManager
  delegate option -scrollbarpolicy  to myHtml
  delegate option -fonttable        to myHtml
  delegate option -fontscale        to myHtml
  delegate option -forcefontmetrics to myHtml
  delegate option -doublebuffer     to myHtml

  # Delegated public methods
  delegate method dumpforms         to myFormManager
  delegate method *                 to myHtml

  # Standard scrollbar and geometry stuff is delegated to the html widget
  delegate option -xscrollcommand to myHtml
  delegate option -yscrollcommand to myHtml
  delegate option -width          to myHtml
  delegate option -height         to myHtml
}
bind Hv3 <KeyPress-Up>     { %W yview scroll -1 units }
bind Hv3 <KeyPress-Down>   { %W yview scroll  1 units }
bind Hv3 <KeyPress-Return> { %W yview scroll  1 units }
bind Hv3 <KeyPress-Right>  { %W xview scroll  1 units }
bind Hv3 <KeyPress-Left>   { %W xview scroll -1 units }
bind Hv3 <KeyPress-Next>   { %W yview scroll  1 pages }
bind Hv3 <KeyPress-space>  { %W yview scroll  1 pages }
bind Hv3 <KeyPress-Prior>  { %W yview scroll -1 pages }


#--------------------------------------------------------------------------
# Class ::hv3::download
#
#     Instances of this class are used to interface between the protocol
#     implementation and the hv3 widget. Refer to the hv3 man page for a more
#     complete description of the interface as used by protocol
#     implementations. Briefly, the protocol implementation uses only the
#     following object methods:
#
#     Queries:
#         uri
#         postdata
#         mimetype
#
#     Actions:
#         redirect URI
#         mimetype MIMETYPE
#         append DATA
#         finish
#         fail
#
snit::type ::hv3::download {
  variable myData ""
  variable myChunksize 2048

  # A download object is "locked" once the first call to [$handle append]
  # is made. After an object is locked it may not be redirected and nor
  # may the mimetype be changed. It is an error if the application attempts
  # to do either of these things.
  #
  variable myLocked 0

  variable myExpectedSize ""

  option -linkedhandle -default ""

  option -incrscript  -default ""
  option -finscript   -default ""
  option -failscript  -default ""
  option -redirscript -default ""
  option -lockscript  -default ""

  option -uri         -default ""
  option -postdata    -default ""
  option -mimetype    -default ""
  option -enctype     -default ""

  # Constructor and destructor
  constructor {args} {eval $self configure $args}
  destructor  {}

  # Query interface used by protocol implementations
  method uri       {} {return $options(-uri)}
  method postdata  {} {return $options(-postdata)}
  method enctype   {} {return $options(-enctype)}
  method authority {} {
    set obj [::hv3::uri %AUTO% $options(-uri)]
    set authority [$obj cget -authority]
    $obj destroy
    return $authority
  }
  method locked {} {return $myLocked}
  method mimetype {{newval ""}} {
    if {$newval ne ""} {
      if {$myLocked} {error "Download handle is locked"}
      set options(-mimetype) $newval
    }
    return $options(-mimetype)
  }
  method expected_size {{newval ""}} {
    if {$newval ne ""} {
      if {$myLocked} {error "Download handle is locked"}
      set myExpectedSize $newval
    }
    return $myExpectedSize
  }

  # Interface for returning data.
  method append {data} {

    # If this is the first call to [$handle append], the object becomes
    # "locked". If there is a -lockscript option, evaluate it. "locked"
    # means it is an error to change the mimetype or redirect the
    # URI from this point on.
    if {$myLocked == 0} {
      if {$options(-lockscript) ne ""} { eval $options(-lockscript) }
      set myLocked 1
    }

    ::append myData $data
    set nData [string length $myData]
    if {$options(-incrscript) != "" && $nData >= $myChunksize} {
      eval [linsert $options(-incrscript) end $myData]
      set myData {}
      if {$myChunksize < 30000} {
        set myChunksize [expr $myChunksize * 2]
      }
    }
  }

  # Called after all data has been passed to [append].
  method finish {} {
    if {$options(-finscript) != ""} { 
      eval [linsert $options(-finscript) end $myData] 
    } 
    $self destroy
  }

  # Called if the download has failed.
  method fail {{errmsg {Unspecified Error}}} {
    if {$options(-failscript) != ""} { 
      eval [concat $options(-failscript) [list $errmsg]]
    } 
    destroy $self
  }

  # Interface for returning a redirect. True is returned if the
  # redirect results in a different URI.
  #
  method redirect {new_uri} {
    if {$myLocked} {error "Download handle is locked"}

    set obj [::hv3::uri %AUTO $options(-uri)]
    $obj load $new_uri
    set new_uri [$obj get]
    $obj destroy

    if {$options(-redirscript) != ""} {
      eval [linsert $options(-redirscript) end $new_uri]
    } 
    set options(-uri) $new_uri
    set myData {}
    set options(-postdata) ""
  }
}
#--------------------------------------------------------------------------

# This proc is used to add destructors to a download-handle object.
#
proc ::hv3::download_destructor {downloadHandle script} {
  $downloadHandle configure -failscript [
    list ::hv3::eval2 $script [$downloadHandle cget -failscript]
  ]
  $downloadHandle configure -finscript [
    list ::hv3::eval2 $script [$downloadHandle cget -finscript]
  ]
}
proc ::hv3::eval2 {script finscript data} {
  if {$finscript ne ""} {
    eval [concat $finscript [list $data]]
  }
  eval $script
}

proc ::hv3::bg {script args} {
  set eval [concat $script $args]
  set rc [catch [list uplevel $eval] result]
  if {$rc} {
    after idle [list                     \
      set ::errorInfo $::errorInfo ;     \
      set ::errorCode $::errorCode ;     \
      bgerror $result ;
    ]
    set ::errorInfo ""
    return ""
  }
  return $result
}



