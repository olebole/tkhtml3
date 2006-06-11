namespace eval hv3 { set {version($Id: hv3.tcl,v 1.77 2006/06/11 11:06:25 danielk1977 Exp $)} 1 }

#
# The code in this file is partitioned into the following classes:
#
#     ::hv3::hv3::hv3::selectionmanager
#     ::hv3::hv3::dynamicmanager
#     ::hv3::hyperlinkmanager
#
#
package require Tkhtml 3.0
package require snit

source [file join [file dirname [info script]] hv3_form.tcl]
source [file join [file dirname [info script]] hv3_widgets.tcl]
source [file join [file dirname [info script]] hv3_object.tcl]

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

  constructor {hv3} {
    set myHv3 $hv3
    selection handle $myHv3 [mymethod get_selection]

    bind $myHv3 <Motion>          "+[mymethod motion %x %y]"
    bind $myHv3 <ButtonPress-1>   "+[mymethod press %x %y]"
    bind $myHv3 <ButtonRelease-1> "+[mymethod release %x %y]"
  }

  method press {x y} {
    set myState 1
    set from [$myHv3 node -index $x $y]
    if {[llength $from]==2} {
      foreach {node index} $from {}
      $myHv3 select from $node $index
      $myHv3 select to $node $index
    }
  }

  method release {x y} {
    set myState 0
  }

  method motion {x y} {
    if {0 == $myState} return
    set to [$myHv3 node -index $x $y]
    if {[llength $to]==2} {
      foreach {node index} $to {}
      $myHv3 select to $node $index
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
    set span [$myHv3 select span]
    if {[llength $span] != 4} { return "" }
    foreach {n1 i1 n2 i2} $span {}

    set td [$myHv3 textdocument]
    set stridx_a [$td nodeToString $n1 $i1]
    set stridx_b [expr [$td nodeToString $n2 $i2] -1]
    set T [string range [$td text] $stridx_a $stridx_b]
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

  constructor {hv3} {
    set myHv3 $hv3
    bind $myHv3 <Motion> "+[mymethod motion %x %y]"
  }

  method reset {} {
    set myHoverNodes [list]
  }

  method motion {x y} {
    set nodelist [$myHv3 node $x $y]
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
#     * Modifying the cursor to the hand shape when over a hyperlink
#     * Setting the :link dynamic condition on hyperlink elements
#
# This class installs a node handler for <a> elements. It also subscribes
# to the <Motion>, <ButtonPress-1> and <ButtonRelease-1> events on the
# associated hv3 widget.
#
snit::type ::hv3::hv3::hyperlinkmanager {
  variable myHv3
  variable myNodes [list]

  option -hyperlinkcmd -default ""

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
      $node dynamic set link
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
    set href [$node attr -default "" href]
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

  # The current location URI
  variable  myUri                    ;# The current URI (type ::hv3::hv3uri)

  # Used to assign internal stylesheet ids.
  variable  myStyleCount 0 

  # Cached ::hv3::textdocument representation of the current document
  variable  myTextDocument "" 

  # TODO: Get rid of this...
  variable  myPostData "" 

  # This variable may be set to "unknown", "quirks" or "standards".
  variable myQuirksmode unknown

  # List of currently outstanding download-handles. See methods makerequest,
  # Finrequest and <TODO: related to stop?>.
  variable myCurrentDownloads [list]

  variable myInternalObject

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

    # Register handler commands to integrate CSS with the HTML document
    $myHtml handler node link     [mymethod link_node_handler]
    $myHtml handler script style  [mymethod style_script_handler]
    $myHtml handler script script [mymethod script_script_handler]

    # Register handler commands to handle <object> and kin.
    $myHtml handler node object   [list hv3_object_handler $self]
    $myHtml handler node embed    [list hv3_object_handler $self]
  }

  destructor {
    # Destroy the components. We don't need to destroy the scrolled
    # html component because it is a Tk widget - it is automatically
    # destroyed when it's parent widget is.
    if {[info exists mySelectionManager]} { $mySelectionManager destroy }
    if {[info exists myDynamicManager]}   { $myDynamicManager   destroy }
    if {[info exists myHyperlinkManager]} { $myHyperlinkManager destroy }
    if {[info exists myUri]}              { $myUri              destroy }
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
  method set_location_var {} {
    if {$options(-locationvar) ne ""} {
      uplevel #0 [subst {set $options(-locationvar) [$myUri get]}]
    }
  }

  method set_pending_var {} {
    if {$options(-pendingvar) ne ""} {
      set val [expr [llength $myCurrentDownloads] > 0]
      uplevel #0 [list set $options(-pendingvar) $val]
    }
  }

  method resolve_uri {baseuri {uri {}}} {
    if {$uri eq ""} {
      set uri $baseuri
      set baseuri [$myUri get -nofragment]
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
    set name [image create photo]
    set full_uri [$self resolve_uri $uri]

    # Create and execute a download request. For now, "expect" a mime-type
    # of image/gif. This should be enough to tell the protocol handler to
    # expect a binary file (of course, this is not correct, the real default
    # mime-type might be some other kind of image).
    set handle [::hv3::download %AUTO%              \
        -uri         $full_uri                      \
        -mimetype    image/gif                      \
        -finscript   [mymethod Imagecallback $name] \
    ]
    $self makerequest $handle

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
  method Imagecallback {name data} {
    if {[info commands $name] == ""} return 
    $name configure -data $data
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

    set finscript [mymethod \
      Finishstyle $id $importcmd $urlcmd
    ]
    set handle [::hv3::download %AUTO%              \
        -uri         $full_uri                      \
        -mimetype    text/css                       \
        -finscript   $finscript                     \
    ]
    $self makerequest $handle
  }

  # Callback invoked when a stylesheet request has finished. Made
  # from method Requeststyle above.
  #
  method Finishstyle {id importcmd urlcmd data} {
    $myHtml style -id $id -importcmd $importcmd -urlcmd $urlcmd $data
    $self goto_fragment
  }

  # Handler for CSS @import directives.
  #
  method import_handler {parent_id uri} {
    $self Requeststyle $parent_id $uri
  }

  # Node handler script for <link> tags.
  #
  method link_node_handler {node} {
    set rel  [string tolower [$node attr -default "" rel]]
    set href [$node attr -default "" href]
    set media [string tolower [$node attr -default all media]]
    if {$rel eq "stylesheet" && $href ne "" && [regexp all|screen $media]} {
      set full_uri [$self resolve_uri $href]
      $self Requeststyle author $full_uri
    }
  }

  # Script handler for <style> tags.
  #
  method style_script_handler {attr script} {
    array set attributes $attr
    if {[info exists attributes(media)]} {
      if {[lsearch [list all screen] $attributes(media)] < 0} return ""
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
    set w .download%AUTO%
    set dler [::hv3::filedownloader $w     \
        -source    [$handle uri]           \
        -size      [$handle expected_size] \
        -cancelcmd [list catch [list $handle fail]]     \
    ]

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
          tk_getSaveFile -initialfile $suggested
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
      set folded [string tolower [string range $data 0 200]]
      set A [string first doctype $folded]
      set B [string first html $folded]
      if {$A >= 0 && ($B <= 0 || $B > $A)} {
        $myHtml configure -defaultstyle [::tkhtml::htmlstyle]
        set myQuirksmode standards
      } else {
        $myHtml configure -defaultstyle [::tkhtml::htmlstyle]
        set myQuirksmode quirks
      }
      $myHtml reset
    }

    $myHtml parse $data
    if {$final} {
      $myHtml parse -final {}
      $self goto_fragment
    }
    $self invalidate_textdocument
  }

  method textdocument {} {
    if {$myTextDocument eq ""} {
      set myTextDocument [::hv3::textdocument %AUTO% $myHtml]
    }
    return $myTextDocument
  }

  method invalidate_textdocument {} {
    if {$myTextDocument ne ""} {
      $myTextDocument destroy
      set myTextDocument ""
    }
  }

  method Formcmd {method uri encdata} {
    puts "Formcmd $method $uri $encdata"
    set full_uri [$self resolve_uri $uri]

    set handle [::hv3::download %AUTO% -mimetype text/html]
    $handle configure                                     \
        -lockscript [mymethod lockcallback $handle]       \
        -incrscript [mymethod documentcallback $handle 0] \
        -finscript  [mymethod documentcallback $handle 1]
    if {$method eq "post"} {
      $handle configure -uri $full_uri -postdata $encdata
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

  method postdata {encdata} {
    set myPostData $encdata
  }

  method goto {uri} {

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
        -postdata    $myPostData                   \
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
    $self invalidate_nodecache
    $self invalidate_textdocument

    $myDynamicManager  reset
    $myFormManager     reset
    $myHtml            reset

    set myQuirksmode unknown
  }

  method location {} { return [$myUri get] }

  method html {} { return [$myHtml widget] }
  method hull {} { return $hull }

  option          -locationvar      -default ""
  option          -pendingvar       -default ""
  option          -requestcmd       -default ""
  option          -cancelrequestcmd -default ""
  delegate option -hyperlinkcmd     to myHyperlinkManager
  delegate option -scrollbarpolicy  to myHtml
  delegate option -fonttable        to myHtml

  # Delegated public methods
  delegate method dumpforms     to myFormManager

  delegate method *                to myHtml

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
#     -fonttable
#         Delegated through to the html widget.
#
#     -locationvar
#         Set to the URI of the currently displayed document.
#
#     -pendingvar
#         Name of var to set to true while resource requests are
#         pending for the currently displayed document.
#
#     -scrollbarpolicy
#         This option may be set to either a boolean value or "auto". It
#         determines the visibility of the widget scrollbars.
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

  # Constructor and destructor
  constructor {args} {eval $self configure $args}
  destructor  {}

  # Query interface used by protocol implementations
  method uri       {} {return $options(-uri)}
  method postdata  {} {return $options(-postdata)}
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
      # set myChunksize [expr $myChunksize * 2]
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

  # Interface for returning a redirect
  method redirect {new_uri} {
    if {$myLocked} {error "Download handle is locked"}
    if {$options(-redirscript) != ""} {
      eval [linsert $options(-redirscript) end $new_uri]
    } 
    set options(-uri) $new_uri
    set myData {}
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
  eval $script
  if {$finscript ne ""} {
    eval [concat $finscript [list $data]]
  }
}

