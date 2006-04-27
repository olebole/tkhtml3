#
# The code in this file is partitioned into the following classes:
#
#     ::hv3::scrolledhtml
#     ::hv3::selectionmanager
#     ::hv3::downloadmanager
#     ::hv3::dynamicmanager
#     ::hv3::hyperlinkmanager
#
#
package require Tkhtml 3.0
package require snit
source [file join [file dirname [info script]] hv3_form.tcl]
source [file join [file dirname [info script]] hv3_widgets.tcl]

#--------------------------------------------------------------------------
# Class ::hv3::downloadmanager
#
snit::type ::hv3::downloadmanager {
  variable myProtocol -array { file Hv3FileProtocol } 
  variable myDownloads {} 
  variable myBinary 0

  # Register a new protocol handler script
  method protocol {protocol script} {
    if {$script eq ""} {
      catch {unset myProtocol($protocol)}
    } else {
      set myProtocol($protocol) $script
    }
  }

  # Abandon any pending downloads
  method reset {} {
    foreach handle $myDownloads {
      catch {
        $handle configure -redirscript {} -finscript {} -incrscript {}
      }
    }
    set myDownloads {}
  }

  # Download a URI
  method download {uri redirscript incrscript finscript {postdata ""}} {
    set uri_obj [Hv3Uri %AUTO% $uri]
    set protocol [$uri_obj cget -scheme]
    $uri_obj destroy

    if {![info exists myProtocol($protocol)]} {
      error "Unknown URI scheme: $protocol"
    }
    set dl_obj [::hv3::download %AUTO% -binary 0 -uri $uri]
    $dl_obj configure -redirscript $redirscript
    $dl_obj configure -incrscript $incrscript
    $dl_obj configure -finscript $finscript
    $dl_obj configure -binary $myBinary
    $dl_obj configure -postdata $postdata

    eval [linsert $myProtocol($protocol) end $dl_obj]
    lappend myDownloads $dl_obj
    set myBinary 0
  }

  # Call this to make the next call to [download] look for binary data.
  #
  method binary {} {
    set myBinary 1
  }
}
#
# End of ::hv3::downloadmanager
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class ::hv3::download
#
#     Instances of this class are used to interface between 
#     protocol-implementations and the hv3 widget. Refer to the hv3
#     man page for a more complete description of the interface as used by
#     protocol implementations. Briefly, the protocol implementation uses only
#     the following object methods:
#
#         binary
#         uri
#         authority
#
#         append
#         finish
#         redirect
#
snit::type ::hv3::download {
  variable myData ""
  variable myChunksize 2048

  option -binary      -default 0
  option -uri         -default ""
  option -incrscript  -default ""
  option -finscript   -default ""
  option -redirscript -default ""
  option -postdata    -default ""

  # Constructor and destructor
  constructor {args} {eval $self configure $args}
  destructor  {}

  # Query interface used by protocol implementations
  method binary    {} {return $options(-binary)}
  method uri       {} {return $options(-uri)}
  method postdata  {} {return $options(-postdata)}
  method authority {} {
    set obj [Hv3Uri %AUTO% $options(-uri)]
    set authority [$obj cget -authority]
    $obj destroy
    return $authority
  }

  # Interface for returning data.
  method append {data} {
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

  # Interface for returning a redirect
  method redirect {new_uri} {
    if {$options(-redirscript) != ""} {
      eval [linsert $options(-redirscript) end $new_uri]
    } 
    set options(-uri) $new_uri
    set myData {}
  }
}
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Protocol implementation for file:// protocol.
#
proc Hv3FileProtocol {downloadHandle} {
  set uri [$downloadHandle uri]
  set url_obj [Hv3Uri %AUTO% $uri]
  set fname [$url_obj cget -path]
  $url_obj destroy

  # Account for wierd windows filenames
  set fname [regsub {^/(.)/} $fname {\1:/}]

  set rc [catch {
    set f [open $fname]
    if {[$downloadHandle binary]} {
      fconfigure $f -encoding binary -translation binary
    }
    set data [read $f]
    $downloadHandle append $data
    close $f
  } msg]

  $downloadHandle finish

  if {$rc} {
    after idle [list error $msg]
  }
}
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class Hv3Uri:
#
#     A very simple class for handling URI references. A partial 
#     implementation of the syntax specification found at: 
#
#         http://www.gbiv.com/protocols/uri/rfc/rfc3986.html
# 
# Usage:
#
#     set uri_obj [Hv3Uri %AUTO% $URI]
#
#     $uri_obj load $URI
#     $uri_obj get
#     $uri_obj cget ?option?
#
#     $uri_obj destroy
#
snit::type Hv3Uri {

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
    # Instead of using regular expressions, code a parser by hand. This is
    # actually easier. Save each of the components, if they exist, in the
    # variables $Scheme, $Authority, $Path, $Query and $Fragment.
    set str $uri
    foreach {re var} [list \
        {([A-Za-z][A-Za-z0-9+-\.]+):(.*)} Scheme            \
        {//([^/#?]*)(.*)}                 Authority         \
        {([^#?]*)(.*)}                    Path              \
        {\?([^#]*)(.*)}                   Query             \
        {#(.*)(.*)}                       Fragment          \
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

# End of class Hv3Uri
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Automated tests for Hv3Uri:
#
#     The following block runs some quick regression tests on the Hv3Uri 
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

  set obj [Hv3Uri %AUTO%]
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
# End of tests for Hv3Uri.
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class ::hv3::selectionmanager
#
snit::type ::hv3::selectionmanager {
  variable myHtml
  variable myState 0               ;# True when left-button is held down

  constructor {htmlwidget args} {
    set myHtml $htmlwidget
  }

  method press {nodelist x y} {
    set myState 1

    set from [$myHtml node -index $x $y]
    if {[llength $from]==2} {
      foreach {node index} $from {}
      $myHtml select from $node $index
      $myHtml select to $node $index
    }
  }

  method release {nodelist x y} {
    set myState 0
  }

  method motion {nodelist x y} {
    if {0 == $myState} return
    set to [$myHtml node -index $x $y]
    if {[llength $to]==2} {
      foreach {node index} $to {}
      $myHtml select to $node $index
    }
  }
}
#
# End of ::hv3::selectionmanager
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class ::hv3::dynamicmanager
#
snit::type ::hv3::dynamicmanager {
  variable myHtml
  variable myHoverNodes [list]

  constructor {htmlwidget} {
    set myHtml $htmlwidget
  }

  method reset {} {
    set myHoverNodes [list]
  }

  method press {nodelist x y} {
  }

  method motion {nodelist x y} {
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
  }

  method release {nodelist x y} {
  }
}
#
# End of ::hv3::dynamicmanager
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class ::hv3::hyperlinkmanager
#
snit::type ::hv3::hyperlinkmanager {
  variable myHtml
  variable myNodes [list]

  option -hyperlinkcmd -default ""

  constructor {htmlwidget} {
    set myHtml $htmlwidget
    $myHtml handler node a [mymethod a_node_handler]
  }

  method a_node_handler {node} {
    if {[$node attr -default "" href] ne ""} {
      $node dynamic set link
    }
  }

  method press {nodelist x y} {
    set myNodes [list]
    foreach node $nodelist {
      for {set n $node} {$n ne ""} {set n [$n parent]} {
        if {[$n tag] eq "a" && [$n attr -default "" href] ne ""} {
          lappend myNodes $n
        }
      }
    }
  
  }

  method motion {nodelist x y} {
    set text 0
    set framewidget [[winfo parent $myHtml] hull]
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

  method release {nodelist x y} {
    set saved_nodes $myNodes
    set myNodes [list]
    if {$options(-hyperlinkcmd) ne ""} {
      foreach node [$myHtml node $x $y] {
        for {set n $node} {$n ne ""} {set n [$n parent]} {
          if {[lsearch $saved_nodes $n] >= 0} {
            eval [linsert $options(-hyperlinkcmd) end [$n attr href]]
            return
          }
        }
      }
    }
  }
}
#
# End of ::hv3::dynamicmanager
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class hv3 - the public widget class
#
snit::widget hv3 {
  hulltype frame

  # Component objects
  component myScrolledHtml           ;# The ::hv3::scrolledhtml
  component myHyperlinkManager       ;# The ::hv3::hyperlinkmanager
  variable  myDynamicManager         ;# The ::hv3::dynamicmanager
  variable  mySelectionManager       ;# The ::hv3::selectionmanager
  variable  myDownloadManager        ;# The ::hv3::downloadmanager
  component myFormManager            ;# The ::hv3::formmanager
  variable  myUri                    ;# The current document URI

  variable  myStyleCount 0 
  variable  myPostData "" 

  constructor {args} {
    # set myScrolledHtml     [::hv3::scrolled html ${win}.scrolledhtml]
    set myScrolledHtml     [html ${win}.scrolledhtml]
    set myDownloadManager  [::hv3::downloadmanager %AUTO%]

    set mySelectionManager [::hv3::selectionmanager %AUTO% $myScrolledHtml]
    set myDynamicManager   [::hv3::dynamicmanager %AUTO% $myScrolledHtml]
    set myHyperlinkManager [::hv3::hyperlinkmanager %AUTO% $myScrolledHtml]
    set myFormManager      [::hv3::formmanager %AUTO% $myScrolledHtml]
    set myUri              [Hv3Uri %AUTO% file://[pwd]/index.html]

    bind $myScrolledHtml <ButtonPress-1>   [mymethod event press %x %y]
    bind $myScrolledHtml <ButtonRelease-1> [mymethod event release %x %y]
    bind $myScrolledHtml <Motion>          [mymethod event motion %x %y]

    $myScrolledHtml configure -imagecmd [mymethod imagecmd]

    $myScrolledHtml handler node link     [mymethod link_node_handler]
    $myScrolledHtml handler script style  [mymethod style_script_handler]
    $myScrolledHtml handler script script [mymethod script_script_handler]

    pack $myScrolledHtml -expand true -fill both

    set newtags [concat [bindtags [$self html]] $win]
    bindtags [$self html] $newtags
    $self set_location_var
  }

  destructor {
    if {[info exists mySelectionManager]} { $mySelectionManager destroy }
    if {[info exists myDownloadManager]}  { $myDownloadManager destroy }
    if {[info exists myDynamicManager]}   { $myDynamicManager destroy }
    if {[info exists myHyperlinkManager]} { $myHyperlinkManager destroy }
    if {[info exists myUri]}              { $myUri destroy }
  }

  # Based on the current contents of instance variable $myUri, set the
  # variable identified by the -locationvar option, if any.
  method set_location_var {} {
    if {$options(-locationvar) ne ""} {
      uplevel #0 [subst {set $options(-locationvar) [$myUri get]}]
    }
  }

  method resolve_uri {baseuri {uri {}}} {
    if {$uri eq ""} {
      set uri $baseuri
      set baseuri [$myUri get -nofragment]
    } 
    set obj [Hv3Uri %AUTO% $baseuri]
    $obj load $uri
    set ret [$obj get -nofragment]
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
  method imagecmd {uri} {
    set name [image create photo]
    set full_uri [$self resolve_uri $uri]
    $myDownloadManager binary
    $myDownloadManager download $full_uri {} {} [mymethod imagecallback $name]
    return [list $name [list image delete $name]]
  }

  # This proc is called when an image requested by the -imagecmd callback
  # ([imagecmd]) has finished downloading. The first argument is the name of
  # a Tk image. The second argument is the downloaded data (presumably a
  # binary image format like gif). This proc sets the named Tk image to
  # contain the downloaded data.
  #
  method imagecallback {name data} {
    if {[info commands $name] == ""} return 
    $name configure -data $data
  }

  # Node handler script for <link> tags.
  #
  method link_node_handler {node} {
    set rel  [$node attr -default "" rel]
    set href [$node attr -default "" href]
    set media [$node attr -default all media]
    if {$rel eq "stylesheet" && $href ne "" && [regexp all|screen $media]} {
      set full_uri [$self resolve_uri $href]
      set id        author.[format %.4d [incr myStyleCount]]
      set importcmd [mymethod import_handler $id]
      set urlcmd    [mymethod resolve_uri $full_uri]
      append id .9999
      set finscript [list \
        $myScrolledHtml style -id $id -importcmd $importcmd -urlcmd $urlcmd
      ]
      $myDownloadManager download $full_uri {} {} $finscript
    }
  }

  # Handler for CSS @import directives.
  #
  method import_handler {parent_id uri} {
    set id        ${parent_id}.[format %.4d [incr myStyleCount]]
    set importcmd [mymethod import_handler $id]
    set urlcmd    [mymethod resolve_uri $uri]
    append id .9999
    set finscript [list \
      $myScrolledHtml style -id $id -importcmd $importcmd -urlcmd $urlcmd
    ]
    $myDownloadManager download $uri {} {} $finscript
  }

  # Script handler for <style> tags.
  #
  method style_script_handler {script} {
    set id        author.[format %.4d [incr myStyleCount]]
    set importcmd [mymethod import_handler $id]
    set urlcmd    [mymethod resolve_uri]
    append id .9999
    $myScrolledHtml style -id $id -importcmd $importcmd -urlcmd $urlcmd $script
    return ""
  }

  # Script handler for <script> tags.
  #
  method script_script_handler {script} {
    return ""
  }

  # This proc is invoked when an event occurs in the html widget. Arguments $x
  # and $y are the relative x and y coordinates of the pointer when the event
  # occured. Argument $action indentifies the specific event that occured, as
  # per the following table:
  #
  #     <ButtonPress-1>         press
  #     <ButtonRelease-1>       release
  #     <Motion>                motion
  # 
  method event {action x y} {
    set nodelist [$myScrolledHtml node $x $y]
    foreach component [list \
        $mySelectionManager $myDynamicManager $myHyperlinkManager
    ] {
      $component $action $nodelist $x $y
    }

    switch -- $action {
      "press"  {focus $win}
      "motion" {
        if {$options(-motioncmd) ne ""} {
          eval [linsert $options(-motioncmd) end $nodelist $x $y]
        }
      }
    }
  }

  # This method is called by the download-manager when a document is
  # redirected. We need this callback to update the myUri variable, so
  # that any relative URIs found in the document will be resolved with respect
  # to the correct document URI. The argument $uri, may be a full URI, or may
  # be relative to the current URI.
  method redirect {uri} {
    $myUri load $uri
    $self set_location_var
  }

  method goto_fragment {} {
    set fragment [$myUri cget -fragment]
    if {$fragment ne ""} {
      set selector [format {[name="%s"]} $fragment]
      set goto_node [lindex [$myScrolledHtml search $selector] 0]
      if {$goto_node ne ""} {
        $myScrolledHtml yview $goto_node
      }
    }
  }

  method documentcallback {final text} {
    $myScrolledHtml parse $text
    if {$final} {
      $myScrolledHtml parse -final {}
      $self goto_fragment
    }
  }

  #--------------------------------------------------------------------------
  # PUBLIC INTERFACE TO HV3 WIDGET STARTS HERE:
  #
  #     Method              Delegate
  # --------------------------------------------
  #     goto                N/A
  #     protocol            $myDownloadManager
  #     xview               $myScrolledHtml
  #     yview               $myScrolledHtml
  #     html                N/A
  #     hull                N/A
  #   

  method postdata {encdata} {
    set myPostData $encdata
  }

  method goto {uri} {
    set current_uri [$myUri get -nofragment]
    $myUri load $uri
    $self set_location_var
    set full_uri [$myUri get -nofragment]
    if {$full_uri eq $current_uri && "" eq $myPostData} {
      $self yview moveto 0.0
      $self goto_fragment
      return [$myUri get]
    }
    set myForceReload 0

    set myStyleCount 0
    $myDownloadManager reset
    $myScrolledHtml    reset
    $myDynamicManager  reset
    $myFormManager     reset

    set redirscript [mymethod redirect]
    set finscript   [mymethod documentcallback 1]
    set incrscript  [mymethod documentcallback 0]
    set p           $myPostData
    $myDownloadManager download $full_uri $redirscript $incrscript $finscript $p
    set myPostData ""
    return [$myUri get]
  }

  method html {} { return $myScrolledHtml }
  method hull {} { return $hull }

  option -motioncmd   -default ""
  option -locationvar -default ""

  # Delegated public methods
  delegate method protocol      to myDownloadManager
  delegate method node          to myScrolledHtml
  delegate method search        to myScrolledHtml
  delegate method dumpforms     to myFormManager

  # Delegated public options
  delegate option -hyperlinkcmd to myHyperlinkManager
  delegate option -getcmd       to myFormManager
  delegate option -postcmd      to myFormManager
  delegate option -fonttable    to myScrolledHtml

  # Scrollbar and geometry related stuff is delegated to the html widget
  delegate method xview           to myScrolledHtml
  delegate method yview           to myScrolledHtml
  delegate option -xscrollcommand to myScrolledHtml
  delegate option -yscrollcommand to myScrolledHtml
  delegate option -width          to myScrolledHtml
  delegate option -height         to myScrolledHtml
}
bind Hv3 <KeyPress-Up>     { %W yview scroll -1 units }
bind Hv3 <KeyPress-Down>   { %W yview scroll  1 units }
bind Hv3 <KeyPress-Return> { %W yview scroll  1 units }
bind Hv3 <KeyPress-Right>  { %W xview scroll  1 units }
bind Hv3 <KeyPress-Left>   { %W xview scroll -1 units }
bind Hv3 <KeyPress-Next>   { %W yview scroll  1 pages }
bind Hv3 <KeyPress-space>  { %W yview scroll  1 pages }
bind Hv3 <KeyPress-Prior>  { %W yview scroll -1 pages }
