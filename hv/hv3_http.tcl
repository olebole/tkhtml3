
package require snit
package require Tk

snit::type Hv3HttpProtcol {

  option -proxyport -default 8123      -configuremethod _ConfigureProxy
  option -proxyhost -default localhost -configuremethod _ConfigureProxy

  # variable myCookies -array [list]

  variable myCookieManager ""

  constructor {args} {
    package require http
    $self configurelist $args
    $self _ConfigureProxy proxyport $options(-proxyport)
    set myCookieManager [::hv3_browser::cookiemanager %AUTO%]
  }

  destructor {
    if {$myCookieManager ne ""} {$myCookieManager destroy}
  }

  method download {downloadHandle} {
    set uri [$downloadHandle uri]
    set finish [mymethod _DownloadCallback $downloadHandle]
    set append [mymethod _AppendCallback $downloadHandle]

    set headers ""
    set authority [$downloadHandle authority]
#    if {[info exists myCookies($authority)]} {
#      set headers "Cookie "
#      foreach cookie $myCookies($authority) {
#        lappend headers $cookie
#      }
#    }

    set cookies [$myCookieManager get_cookies $authority]
    if {$cookies ne ""} {
      set headers [list Cookie $cookies]
    }

    set postdata [$downloadHandle postdata]

    if {$postdata ne ""} {
      ::http::geturl $uri     \
          -command $finish    \
          -handler $append    \
          -headers $headers   \
          -query   $postdata
    } else {
      ::http::geturl $uri -command $finish -handler $append -headers $headers
    }
  }

  method saveFile {uri} {
    set dler [::hv3::filedownloader .download%AUTO% $uri]
    set finish [list $dler finish]
    set append [list $dler append]
    ::http::geturl $uri     \
        -command $finish    \
        -handler $append
    set dest [file normal [tk_getSaveFile]]
    if {$dest eq ""} {
      destroy $dler
    } else {
      $dler set_dest $dest
    }
  }

  # Configure the http package to use a proxy as specified by
  # the -proxyhost and -proxyport options on this object.
  #
  method _ConfigureProxy {option value} {
    set options($option) $value
    ::http::config -proxyhost $options(-proxyhost)
    ::http::config -proxyport $options(-proxyport)
    ::http::config -useragent {Mozilla/5.0 Gecko/20050513}
    set ::http::defaultCharset utf-8
  }

  # Invoked when data is available from an http request. Pass the data
  # along to hv3 via the downloadHandle.
  #
  method _AppendCallback {downloadHandle socket token} {
    upvar \#0 $token state 

    set data [read $socket 2048]
    $downloadHandle append $data
    set nbytes [string length $data]
    return $nbytes
  }

  # Invoked when an http request has concluded.
  #
  method _DownloadCallback {downloadHandle token} {
    upvar \#0 $token state 

    if {[info exists state(meta)]} {
      foreach {name value} $state(meta) {
        if {$name eq "Set-Cookie"} {
          regexp {^([^= ]*)=([^ ;]*)} $value dummy name value
          $myCookieManager add_cookie [$downloadHandle authority] $name $value
        }
      }
      foreach {name value} $state(meta) {
        if {$name eq "Location"} {
          puts "REDIRECT: $value"
          $downloadHandle redirect $value
          $self download $downloadHandle
          return
        }
      }
    } 

    $downloadHandle append $state(body)
    $downloadHandle finish
  }

  method debug_cookies {} {
    $myCookieManager debug
  }
}

# A cookie manager is a database of http cookies. It supports the 
# following operations:
#
#     * Add cookie to database,
#     * Retrieve applicable cookies for an http request, and
#     * Delete the contents of the cookie database.
#
# Also, by invoking [pathName debug] a GUI to inspect and manipulate the
# database is created in a new top-level window.
#
snit::type ::hv3_browser::cookiemanager {

  variable myDebugWindow

  # The cookie data is stored in the following array variable. The
  # array keys are authority names. The array values are the list of cookies
  # associated with the authority. Each list element (a single cookie) is 
  # stored as a list of two elements, the cookie name and value. For
  # example, a two cookies from tkhtml.tcl.tk might be added to the database
  # using code such as:
  #
  #     set myCookies(tkhtml.tcl.tk) [list {login qwertyuio} {prefs 1234567}]
  # 
  variable myCookies -array [list]

  constructor {} {
    set myDebugWindow [string map {: _} ".${self}_toplevel"]
  }

  method add_cookie {authority name value} {
    if {0 == [info exists myCookies($authority)]} {
      set myCookies($authority) [list]
    }

    array set cookies $myCookies($authority)
    set cookies($name) $value
    set myCookies($authority) [array get cookies]

    if {[winfo exists $myDebugWindow]} {$self debug}
  }

  # Retrieve the cookies that should be sent to the specified authority.
  # The cookies are returned as a string of the following form:
  #
  #     "NAME1=OPAQUE_STRING1; NAME2=OPAQUE_STRING2 ..."
  #
  method get_cookies {authority} {
    set ret ""
    if {[info exists myCookies($authority)]} {
      foreach {name value} $myCookies($authority) {
        append ret [format "%s=%s; " $name $value]
      }
    }
    return $ret
  }

  method get_report {} {
    set Template {
      <html><head><style>$Style</style></head>
      <body>
        <h1>Hv3 Cookies</h1>
        <p>
	  <b>Note:</b> This window is automatically updated when Hv3's 
	  internal cookies database is modified in any way. There is no need to
          close and reopen the window to refresh it's contents.
        </p>
        <div id="clear"></div>
        <br clear=all>
        $Content
      </body>
      <html>
    }

    set Style {
      .authority { margin-top: 2em; font-weight: bold; }
      .name      { padding-right: 5ex; }
      #clear { 
        float: left; 
        margin: 1em; 
        margin-top: 0px; 
      }
    }

    set Content ""
    if {[llength [array names myCookies]] > 0} {
      append Content "<table>"
      foreach authority [array names myCookies] { 
        append Content "<tr><td><div class=authority>$authority</div>"
        foreach {name value} $myCookies($authority) {
          append Content [subst {
            <tr>
              <td><span class=name>$name</span>
              <td><span class=value>$value</span>
          }]
        }
      }
      append Content "</table>"
    } else {
      set Content {
        <p>The cookies database is currently empty.
      }
    }

    return [subst $Template]
  }

  method download_report {downloadHandle} {
    $downloadHandle append [$self get_report]
    $downloadHandle finish
  }

  method debug {} {
    set path $myDebugWindow
    if {![winfo exists $path]} {
      toplevel $path
      ::hv3::scrolled hv3 ${path}.hv3
      ${path}.hv3 configure -width 400 -height 400
      pack ${path}.hv3 -expand true -fill both
      set HTML [${path}.hv3 html]

      # The "clear database button"
      button ${HTML}.clear   -text "Clear Database" -command [subst {
        array unset [myvar myCookies]
        [mymethod debug]
      }]
    }
    raise $path
    ${path}.hv3 protocol report [mymethod download_report]
    ${path}.hv3 postdata POSTME!
    ${path}.hv3 goto report://

    set HTML [${path}.hv3 html]
    [lindex [${path}.hv3 search #clear] 0] replace ${HTML}.clear
  }
}

snit::widget ::hv3::filedownloader {
  hulltype toplevel

  # The channel to write to.
  variable myChannel ""

  # Buffer for data while waiting for a file-name.
  variable myBuffer ""
  variable myIsFinished 0

  # Variables used to update the dynamic label widgets.
  variable mySource ""
  variable myDestination ""
  variable myStatus ""
  variable myElapsed ""

  # Total expected bytes and bytes downloaded so far.
  variable mySize ""
  variable myDownloaded 0

  # Time the download started, according to [clock seconds]
  variable myStartTime 

  constructor {source} {
    set mySource $source

    label ${win}.source1 -text "Source:"
    label ${win}.source2 -textvariable [myvar mySource]

    foreach e [list \
        [list 0 "Source:" mySource] \
        [list 1 "Destination:" myDestination] \
        [list 2 "Status:" myStatus] \
        [list 3 "Elapsed:" myElapsed] \
    ] {
      foreach {n text var} $e {}
      set strlabel [label ${win}.row${n}_0 -text $text]
      set varlabel [label ${win}.row${n}_1 -textvariable [myvar $var]]
      grid configure $strlabel -column 0 -row $n -sticky w
      grid configure $varlabel -column 1 -row $n -sticky w
    }
    grid columnconfigure ${win} 1 -minsize 400

    label ${win}.progress_label -text "Progress:"
    canvas ${win}.progress -height 12 -borderwidth 2 -relief sunken
    ${win}.progress create rectangle 0 0 0 12 -fill blue -tags rect

    grid configure ${win}.progress_label -column 0 -row 4 -sticky w
    grid configure ${win}.progress -column 1 -row 4 -sticky ew

    button ${win}.button -text Cancel -command [list destroy $self]
    grid configure ${win}.button -column 1 -row 5 -sticky e

    set myStartTime [clock seconds]
    $self timed_callback
  }

  method set_dest {dest} {
    set myDestination $dest
    set myChannel [open $myDestination w]
    fconfigure $myChannel -encoding binary
    fconfigure $myChannel -translation binary
    puts -nonewline $myChannel $myBuffer
    set myBuffer ""
    $self update_labels
    if {$myIsFinished} {
      $self finish
    }
  }

  method timed_callback {} {
    $self update_labels
    after 500 [mymethod timed_callback]
  }

  method update_labels {} {
    set tm [expr [clock seconds] - $myStartTime]

    if {$myIsFinished} {
      set myStatus "$myDownloaded / $myDownloaded (finished)"
      set tm $myStartTime
      set percentage 100.0
    } elseif {$mySize eq ""} {
      set myStatus "$myDownloaded / ??"
      set percentage 50.0
    } else {
      set percentage [expr ${myDownloaded}.0 * 100.0 / ${mySize}.0]
      set percentage [format "%.1f" $percentage]
      set myStatus "$myDownloaded / $mySize ($percentage%)"
    }

    set w [expr [winfo width ${win}.progress].0 * $percentage / 100.0]
    ${win}.progress coords rect 0 0 $w 12

    set myElapsed "$tm seconds"
  }

  method append {socket token args} {
    upvar \#0 $token state 
    if {[info exists state(totalsize)] && $state(totalsize) != 0} {
      set mySize $state(totalsize)
    }
    set data [read $socket 4096]
    if {$myChannel ne ""} {
      puts -nonewline $myChannel $data
      set myDownloaded [file size $myDestination]
    } else {
      append myBuffer $data
      set myDownloaded [string length $myBuffer]
    }
    $self update_labels
    return [string length $data]
  }

  method finish {args} {
    if {$myChannel ne ""} {
      close $myChannel
      set myChannel ""
      after cancel [mymethod timed_callback]
      ${win}.button configure -text Ok
    }
    if {!$myIsFinished} {
      set myIsFinished 1
      set myStartTime [expr [clock seconds] - $myStartTime]
    }
    $self update_labels
  }

  destructor {
    catch { close $myChannel }
    after cancel [mymethod timed_callback]
  }
}

