#
# Public API to hv3 functionality.
#
# hv3Init PATH ?option value...?
#
#     Initialise an instance of the Hv3 embedded app. The parameter PATH is a
#     Tk window path-name for a frame widget that will be created. The 
#     following widgets are created when this command is invoked:
#
#         $PATH                 Frame widget
#         $PATH.html            Html widget
#         $PATH.vsb             Vertical scrollbar widget
#         $PATH.hsb             Horizontal scrollbar widget
#         $PATH.status          Label widget (browser "status")
#
#     Options are:
#
#         -gotocallback         Goto callback.
#
#
# hv3Destroy PATH
#
#     Destroy the widgets created by hv3Init and deallocate all resources
#     for application instance PATH.
#
#
# hv3Goto PATH URI
#
#     Retrieve and display the document at URI $URI.
#
#
# hv3RegisterProtocol PATH protocol cmd
#
#     Register a protocol handler command. The protocol is the first part of
#     the fully-qualified URIs that the command can handle, i.e "file" or 
#     "http". A protocol command must support the following invocations:
#
#         $cmd URI -callback SCRIPT ?-binary BOOLEAN?
#         $cmd -reset
#     

swproc hv3Init {PATH {gotocallback ""}} {
  ::hv3::initVars $PATH
  ::hv3::importVars $PATH

  set myUrl file://[pwd]/
  set myGotoCallback $gotocallback

  # Create the widgets and pack them all into the frame. The caller 
  # must pack the frame itself.
  frame $PATH
  html $PATH.html
  scrollbar $PATH.vsb -orient vertical
  scrollbar $PATH.hsb -orient horizontal
  label $PATH.status -height 1 -anchor w
  pack $PATH.vsb -fill y -side right
  pack $PATH.status -fill x -side bottom 
  pack $PATH.hsb -fill x -side bottom
  pack $PATH.html -fill both -expand true

  # Set up scrollbar callbacks.
  $PATH.html configure -yscrollcommand "::hv3::scrollCallback $PATH.vsb"
  $PATH.hsb configure -command "$PATH.html xview"
  $PATH.html configure -xscrollcommand "::hv3::scrollCallback $PATH.hsb"
  $PATH.vsb configure -command "$PATH.html yview"

  # Image callback.
  $PATH.html configure -imagecmd [list ::hv3::imageCmd $PATH]

  # Set up Handler callbacks for <link>, <style>, <script> and <img> tags.
  $PATH.html handler script script "::hv3::handleScriptScript"
  # $PATH.html handler node img      "::hv3::handleImgNode $PATH"
  $PATH.html handler node link     "::hv3::handleLinkNode     $PATH"
  $PATH.html handler script style  "::hv3::handleStyleScript  $PATH"

  # Widget bindings
  bind $PATH.html <Motion> "::hv3::guiMotion $PATH %x %y"
  bind $PATH.html <1> "::hv3::guiLeftClick $PATH \[$PATH.html node %x %y\]"
  bind $PATH.html <2> "::hv3::guiMiddleClick $PATH"
  bind $PATH.html <3> "::hv3::guiRightClick $PATH \[$PATH.html node %x %y\]"
  bind $PATH.html <KeyPress-q> exit
  bind $PATH.html <KeyPress-Q> exit

  bind $PATH.html <ButtonPress-1>   "::hv3::guiLeftPress $PATH %x %y"
  bind $PATH.html <ButtonRelease-1> "::hv3::guiLeftRelease $PATH %x %y"

  ::hv3::guiMiddleClick $PATH

  # Register the built-in URI protocol "file"
  hv3RegisterProtocol $PATH file ::hv3::fileProtocol

  return $PATH
}

swproc hv3Goto {PATH url {nocallback 0 1}} {
  ::hv3::importVars $PATH

  set url [url_resolve $myUrl $url]

  if {$myGotoCallback != "" && !$nocallback} {
    eval $myGotoCallback $url
  }

  url_get $url -fragment f -prefragment prefragment
  url_get $myUrl -prefragment current
  set myUrl $url

  if {$current == $prefragment && $f != ""} {
    ::hv3::goto_fragment $PATH $f
  } else {
    set script [list ::hv3::parse $PATH $f] 
    ::hv3::download $PATH $prefragment -script $script -type Document -reset
  }
}

proc hv3Destroy {PATH} {
}

proc hv3RegisterProtocol {PATH protocol cmd} {
  ::hv3::importVars $PATH
  array set protocols $myProtocols
  set protocols($protocol) $cmd
  set myProtocols [array get protocols]
}

namespace eval hv3 {

  proc VAR {n {d {}}} {uplevel [list lappend vars $n $d]}

  VAR myProxyHost 
  VAR myProxyPort
  VAR myGotoCallback
  VAR myStateVar 

  VAR myStyleCount 0       ;# Used by [styleId] to assign style-ids
  VAR myUrl                ;# Current URL being displayed (or loaded)
  VAR myStatus1            ;# Cursor position status text
  VAR myStatus2            ;# Download progress status text
  VAR myStatusVar 2        ;# Either 1 or 2 - the current status text
  VAR myStatusInfo         ;# Serialized array of download status info
  VAR myProtocols          ;# Serialized array of protocol commands
  VAR myDrag 0             ;# True when dragging the cursor

  proc initVars {PATH} {
    foreach {var default} $::hv3::vars {
      set ::hv3::objectvars(${PATH},$var) $default
    }
  }
  proc importVars {PATH} {
    foreach {var default} $::hv3::vars {
      uplevel [subst {
        upvar #0 ::hv3::objectvars(${PATH},$var) $var
      }]
    }
  }

  set ::hv3::image_name 0

  # scrollCallback SB args
  #
  #     This proc is registered as the -xscrollcommand and -yscrollcommand 
  #     options of the html widget. The first argument is the name of a 
  #     scrollbar - either "$PATH.hsb" or "$PATH.vsb". The remaining args
  #     are those appended by the widget.
  #
  #     The position of the scrollbar is adjusted. If it is not required,
  #     the scrollbar is made to disappear by setting it's width and border
  #     width to zero.
  proc scrollCallback {sb args} {
    eval [concat $sb set $args]
    if {$args == "0.0 1.0"} {
      $sb configure -width 0 -borderwidth 0
    } else {
      $sb configure -width 15 -borderwidth 2 
    }
  }

  # handleStyleScript PATH STYLE
  #
  #     This is called as a [node handler script] to handle a <style> tag.
  #     The second argument, $script, contains the text of the stylesheet.
  proc handleStyleScript {PATH style} {
    importVars $PATH
    set id [styleId $PATH author]
    ::hv3::styleCallback $PATH $myUrl $id $style
  }

  # handleScriptScript SCRIPT
  #
  #     This is invoked as a [node handler script] callback to handle 
  #     the contents of a <script> tag. It is a no-op.
  proc handleScriptScript {script} {
    return ""
  }

  # handleImgNode PATH NODE
  #
  #     This is called as a [node handler node] callback to handle
  #     an <img> node. This callback works by invoking the script 
  #     configured as the -imagecmd option of the html widget. All the 
  #     image handling is done there (see imageCmd and imageCallback).
  proc handleImgNode {PATH node} {
    set src [$node attr src]
    if {$src != ""} {
      set imagecmdout [eval [$PATH.html cget -imagecmd] $src]
      foreach {img del} $imagecmdout {
        $node replace $img -deletecmd $del
      }
    }
  }

  # handleLinkNode PATH NODE
  #
  #     Handle a <link> node. The only <link> nodes we care about are those
  #     that load stylesheets for media-types "all" or "visual". i.e.:
  #
  #         <link rel="stylesheet" media="all" href="style.css">
  #
  proc handleLinkNode {PATH node} {
    importVars $PATH
    set rel   [$node attr -default "" rel]
    set media [$node attr -default all media]
    if {$rel=="stylesheet" && [regexp all|screen $media]} {
      set id [styleId $PATH author]
      set url [url_resolve $myUrl [$node attr href]]
      set cmd [list ::hv3::styleCallback $PATH $url $id] 
      download $PATH $url -script $cmd -type Stylesheet
    }
  }
  
  # imageCallback IMAGE-NAME DATA
  #
  #     This proc is called when an image requested by the -imagecmd callback
  #     ([imageCmd]) has finished downloading. The first argument is the name of
  #     a Tk image. The second argument is the downloaded data (presumably a
  #     binary image format like gif). This proc sets the named Tk image to
  #     contain the downloaded data.
  #
  proc imageCallback {name data} {
    if {[info commands $name] == ""} return 
    $name configure -data $data
  }
  
  # imageCmd PATH URL
  # 
  #     This proc is registered as the -imagecmd script for the Html widget.
  #     The first argument is the name of the Html widget. The second argument
  #     is the URI of the image required.
  #
  #     This proc creates a Tk image immediately. It also kicks off a fetch 
  #     request to obtain the image data. When the fetch request is complete,
  #     the contents of the Tk image are set to the returned data in proc 
  #     ::hv3::imageCallback.
  #
  proc imageCmd {PATH url} {
    importVars $PATH
    set name hv3_image[incr ::hv3::image_name]
    image create photo $name
    set fullurl [url_resolve $myUrl $url]
    set cmd [list ::hv3::imageCallback $name] 
    download $PATH $fullurl -script $cmd -binary -type Image
    return [list $name [list image delete $name]]
  }

  # styleCallback PATH url id style
  #
  #     This proc is invoked when a stylesheet has finished downloading 
  #     (stylesheet downloads may be started by procs ::hv3::handleLinkNode 
  #     or ::hv3::styleImportCmd).
  proc styleCallback {PATH url id style} {
    set importcmd [list ::hv3::styleImport $PATH $id] 
    set urlcmd [list url_resolve $url]
    $PATH.html style -id $id -importcmd $importcmd -urlcmd $urlcmd $style
  }

  # styleImport styleImport PATH PARENTID URL
  #
  #     This proc is passed as the -importcmd option to a [html style] 
  #     command (see proc styleCallback).
  proc styleImport {PATH parentid url} {
    importVars $PATH
    set id [styleId $PATH $parentid]
    set script [list ::hv3::styleCallback $PATH $url $id] 
    download $PATH $url -script $script -type Stylesheet
  }

  # styleId PATH parentid
  #
  #     Return the id string for a stylesheet with parent stylesheet 
  #     $parentid.
  proc styleId {PATH parentid} {
    importVars $PATH
    format %s.%.4d $parentid [incr myStyleCount]
  }

  # guiRightClick PATH x y
  #
  #     Called when the user right-clicks on the html window. 
  proc guiRightClick {PATH node} {
    if {$node!=""} {
      prop_browse $PATH.html -node $node
    }
  }

  # guiMiddleClick
  #
  #     Called when the user right-clicks on the html window. 
  proc guiMiddleClick {PATH} {
    importVars $PATH
    set myStatusVar [expr ($myStatusVar % 2) + 1]
    set v ::hv3::objectvars($PATH,myStatus$myStatusVar)
    $PATH.status configure -textvariable $v
  }

  proc guiDrag {PATH x y} {
    set to [$PATH.html node -index $x $y]
    if {[llength $to]==2} {
      foreach {node index} $to {}
      $PATH.html select to $node $index
    }
  }
  proc guiLeftPress {PATH x y} {
    importVars $PATH
    set from [$PATH.html node -index $x $y]
    if {[llength $from]==2} {
      foreach {node index} $from {}
      $PATH.html select from $node $index
      $PATH.html select to $node $index
    }
    for {set n [$PATH.html node $x $y]} {$n!=""} {set n [$n parent]} {
      if {[$n tag]=="a" && [$n attr -default "" href]!=""} {
        hv3Goto $PATH [$n attr href]
        break
      }
    }
    set myDrag 1
  }
  proc guiLeftRelease {PATH x y} {
    importVars $PATH
    set myDrag 0
  }

  # guiMotion PATH node
  #
  #     Called when the mouse moves over the html window. Parameter $node is
  #     the node the mouse is currently floating over, or "" if the pointer is
  #     not over any node.
  proc guiMotion {PATH x y} {
    importVars $PATH
    set node [$PATH.html node $x $y]
    set txt ""
    $PATH configure -cursor ""
    for {set n $node} {$n!=""} {set n [$n parent]} {
      set tag [$n tag]
      if {$tag=="a" && [$n attr -default "" href]!=""} {
        $PATH configure -cursor hand2
        set txt "hyper-link: [$n attr href]"
        break
      } elseif {$tag==""} {
        $PATH configure -cursor xterm
        set txt [string range [$n text] 0 20]
      } else {
        set txt "<[$n tag]>$txt"
      }
    }
    set myStatus1 [string range $txt 0 80]

    if {$myDrag} {
      guiDrag $PATH $x $y
    }
  }

  proc goto_fragment {PATH fragment} {
    set H $PATH.html
    set selector [format {[name="%s"]} $fragment]
    set goto_node [lindex [$H search $selector] 0]
    if {$goto_node!=""} {
      set coords2 [$H bbox [$H node]]
      set coords  [$H bbox $goto_node]
      while {[llength $coords] == 0 && $goto_node!=[$H node]} {
        set next_node [$goto_node right_sibling]
        if {$next_node==""} {
          set next_node [$goto_node parent]
        }
        set goto_node $next_node
        set coords  [$H bbox $goto_node]
      }
      if {[llength $coords] > 0} {
        set ypix [lindex $coords 1]
        set ycanvas [lindex $coords2 3]
        $H yview moveto [expr double($ypix) / double($ycanvas)]
      }
    }
  }

  # parse PATH fragment text 
  #
  #     Append the text TEXT to the current document.  If argument FRAGMENT is
  #     not "", then it is the name of an anchor within the document to jump
  #     to.
  #
  proc parse {PATH fragment text} {
    $PATH.html reset

    importVars $PATH
    set myStyleCount 0
    $PATH.html parse $text

    if {$fragment != ""} {
      goto_fragment $PATH $fragment
    }
  }

  # download PATH url ?-script script? ?-type type? ?-binary? ?-reset?
  #
  #     Retrieve the contents of the url $url, by invoking one of the 
  #     protocol callbacks registered via proc hv3RegisterProtocol.
  swproc download {PATH url \
    {script ""} \
    {type Document} \
    {binary 0 1} \
    {reset 0 1}} \
  {
    importVars $PATH
    url_get $url -scheme scheme
    array set protocols $myProtocols
    if {[catch {set cmd $protocols($scheme)}]} {
      error "Unknown protocol: \"$scheme\""
    }

    if {$reset} {
      $cmd -reset
      set myStatusInfo [list]
    }

    array set status $myStatusInfo
    if {[info exists status($type)]} {
      lset status($type) 1 [expr [lindex $status($type) 1] + 1]
    } else {
      set status($type) [list 0 1]
    }
    set myStatusInfo [array get status]
    
    set callback [list ::hv3::downloadCallback $PATH $script $type]
    if {[catch {
      $cmd $url -script $callback -binary $binary
    }]} {
      puts "Error: Cannot fetch $url"
      return
    }
    downloadSetStatus $PATH
  }

  proc downloadCallback {PATH script type data} {
    importVars $PATH
    array set status $myStatusInfo
    lset status($type) 0 [expr [lindex $status($type) 0] + 1]
    set myStatusInfo [array get status]
    lappend script $data
    eval $script
    downloadSetStatus $PATH
  }

  proc downloadSetStatus {PATH} {
    importVars $PATH
    set myStatus2 ""
    foreach {k v} $myStatusInfo {
      append myStatus2 "$k [lindex $v 0]/[lindex $v 1]  "
    }
  }

  # fileProtocol
  #
  #     This command is registered as the handler for the builtin file://
  #     protocol.
  # 
  swproc fileProtocol {url {script ""} {binary 0}} {
    if {$url=="-reset"} {
      return
    }
    set fname [string range $url 7 end]
    set f [open $fname]
    if {$binary} {
      fconfigure $f -encoding binary -translation binary
    }
    set data [read $f]
    close $f
    if {$script != ""} {
      lappend script $data
      eval $script
    }
  }
}



#--------------------------------------------------------------------------
# urlNormalize --
#
#         urlNormalize PATH
#
#     The argument is expected to be the path component of a URL (i.e. similar
#     to a unix file system path). ".." and "." components are removed and the
#     result returned.
#
proc urlNormalize {path} {
    set ret [list]
    foreach c [split $path /] {
        if {$c == ".."} {
            set ret [lrange $ret 0 end-1]
        } elseif {$c == "."} {
            # Do nothing...
        } else {
            lappend ret $c
        }
    }
    return [join $ret /]
}

#--------------------------------------------------------------------------
# urlSplit --
#
#         urlSplit URL
#
#     Form of URL parsed:
#         <scheme>://<host>:<port>/<path>?<query>#<fragment>
#
proc urlSplit {url} {
    set re_scheme   {((?:[a-z]+:)?)}
    set re_host     {((?://[A-Za-z0-9.]*)?)}
    set re_port     {((?::[0-9]*)?)}
    set re_path     {((?:[^#?]*)?)}
    set re_query    {((?:\?[^?]*)?)}
    set re_fragment {((?:#.*)?)}

    set re "${re_scheme}${re_host}${re_port}${re_path}${re_query}${re_fragment}"

    if {![regexp $re $url X \
            u(scheme) \
            u(host) \
            u(port) \
            u(path) \
            u(query) \
            u(fragment)
    ]} {
        error "Bad URL: $url"
    }

    return [array get u]
}

#--------------------------------------------------------------------------
# url_resolve --
#
#     This command is used to transform a (possibly) relative URL into an
#     absolute URL. Example:
#
#         $ url_resolve http://host.com/dir1/dir2/doc.html ../dir3/doc2.html
#         http://host.com/dir1/dir3/doc2.html
#
#     This is purely a string manipulation procedure.
#
proc url_resolve {baseurl url} {

    array set u [urlSplit $url]
    array set b [urlSplit $baseurl]

    set ret {}
    foreach part [list scheme host port] {
        if {$u($part) != ""} {
            append ret $u($part)
        } else {
            append ret $b($part)
        }
    }

    if {$b(path) == ""} {set b(path) "/"}

    if {[regexp {^/} $u(path)] || $u(host) != ""} {
        set path $u(path)
    } else {
        if {$u(path) == ""} {
            set path $b(path)
        } else {
            regexp {.*/} $b(path) path
            append path $u(path)
        }
    }

    append ret [urlNormalize $path]
    append ret $u(query)
    append ret $u(fragment)

    # puts "$baseurl + $url -> $ret"
    return $ret
}

#--------------------------------------------------------------------------
# url_get --
#
#     This is a high-level string manipulation procedure to extract components
#     from a URL.
#
#         -fragment
#         -prefragment
#         -port
#         -host
#
#     For the url "http://www.google.com:1234/index.html#part5"
#
swproc url_get {url \
    {fragment ""} {prefragment ""} {port ""} {host ""} {scheme ""}} \
{
    array set u [urlSplit $url]

    if {$fragment != ""} {
        uplevel [subst {
            set $fragment "[string range $u(fragment) 1 end]"
        }]
    }

    if {$prefragment != ""} {
        uplevel [subst {
            set $prefragment "$u(scheme)$u(host)$u(port)$u(path)$u(query)"
        }]
    }

    if {$host != ""} {
        uplevel [subst {
            set $host "[string range $u(host) 2 end]"
        }]
    }

    if {$port != ""} {
        uplevel [subst {
            set $port "[string range $u(port) 1 end]"
        }]
    }

    if {$scheme != ""} {
        uplevel [subst {
            set $scheme "[string range $u(scheme) 0 end-1]"
        }]
    }
}
