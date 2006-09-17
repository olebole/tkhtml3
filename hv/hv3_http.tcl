namespace eval hv3 { set {version($Id: hv3_http.tcl,v 1.25 2006/09/17 15:33:35 danielk1977 Exp $)} 1 }

#
# This file contains implementations of the -requestcmd and -cancelrequestcmd
# scripts used with the hv3 widget for the demo browser. Supported functions
# are:
#
#     * http:// (including cookies)
#     * file:// (code for this is now in hv3_file.tcl)
#     * data://
#     * https:// (if the "tls" package is available)
#

package require snit
package require Tk
package require http
catch { package require tls }

source [sourcefile hv3_file.tcl]

#
# ::hv3::protocol
#
#     Connect hv3 to the outside world via download-handle objects.
#
# Synopsis:
#
#     set protocol [::hv3::protocol %AUTO%]
#
#     $protocol requestcmd DOWNLOAD-HANDLE
#     $protocol cancelrequestcmd DOWNLOAD-HANDLE
#
#     $protocol schemehandler scheme handler
#
#     $protocol destroy
#
snit::type ::hv3::protocol {

  # Lists of waiting and in-progress http URI download-handles.
  variable myWaitingHandles    [list]
  variable myInProgressHandles [list]
 
  # If not set to an empty string, contains the name of a global
  # variable to set to a short string describing the state of
  # the object. The string is always of the form:
  #
  #     "X1 waiting, X2 in progress"
  #
  # where X1 and X2 are integers. An http request is said to be "waiting"
  # until the header identifying the mimetype is received, and "in progress"
  # from that point on until the resource has been completely retrieved.
  #
  option -statusvar -default "" -configuremethod ConfigureStatusvar
  option -relaxtransparency -default 0

  # Instance of ::hv3::cookiemanager
  variable myCookieManager ""

  # Both built-in ("http" and "file") and any configured scheme handlers 
  # (i.e. "home:") are stored in this array.
  variable mySchemeHandlers -array [list]

  constructor {args} {
    $self configurelist $args

    # It used to be that each ::hv3::protocol object would create it's
    # own cookie-manager database. This has now changed so that the
    # whole application (all browser tabs) use a single cookies 
    # database. The net effect is that you can log in to a web site
    # in one tab and then continue in another.
    #
    # The global cookie-manager object is named "::hv3::the_cookie_manager".
    #
    set myCookieManager ::hv3::the_cookie_manager
    if {[info commands $myCookieManager] eq ""} {
      ::hv3::cookiemanager $myCookieManager
    }

    # Register the 4 types of URIs the ::hv3::protocol code knows about.
    # Note that https:// URIs require the "tls" package.
    $self schemehandler file  ::hv3::request_file
    $self schemehandler http  [mymethod request_http]
    $self schemehandler data  [mymethod request_data]
    if {[info commands ::tls::socket] ne ""} {
      $self schemehandler https [mymethod request_https]
      ::http::register https 443 [list ::hv3::protocol SSocket]
    }

    # Configure the Tcl http package to pretend to be Gecko.
    ::http::config -useragent {Mozilla/5.0 Gecko/20050513}
    set ::http::defaultCharset utf-8
  }

  destructor { 
    # Nothing to do. We used to destroy the $myCookieManager object here,
    # but that object is now global and exists for the lifetime of the
    # application.
  }

  # Register a custom scheme handler command (like "home:").
  method schemehandler {scheme handler} {
    set mySchemeHandlers($scheme) $handler
  }

  # This method is invoked as the -cancelrequestcmd script of an hv3 widget
  method cancelrequestcmd {downloadHandle} {
    # TODO
  }

  # This method is invoked as the -requestcmd script of an hv3 widget
  method requestcmd {downloadHandle} {

    # Extract the URI scheme to figure out what kind of URI we are
    # dealing with. Currently supported are "file" and "http" (courtesy 
    # Tcl built-in http package).
    set uri_obj [::hv3::uri %AUTO% [$downloadHandle uri]]
    set uri_scheme [$uri_obj cget -scheme]
    $uri_obj destroy

    # Fold the scheme to lower-case. Should ::hv3::uri have already done this?
    set uri_scheme [string tolower $uri_scheme]

    # Execute the scheme-handler, or raise an error if no scheme-handler
    # can be found.
    if {[info exists mySchemeHandlers($uri_scheme)]} {
      eval [concat $mySchemeHandlers($uri_scheme) $downloadHandle]
    } else {
      error "Unknown URI scheme: \"$uri_scheme\""
    }
  }

  # Handle an http:// URI.
  #
  method request_http {downloadHandle} {
    set uri       [$downloadHandle uri]
    set authority [$downloadHandle authority]
    set postdata  [$downloadHandle postdata]
    set enctype   [$downloadHandle enctype]

    # Knock any #fragment off the end of the URI.
    set obj [::hv3::uri %AUTO% $uri]
    set uri [$obj get -nofragment]
    $obj destroy

    # Store the HTTP header containing the cookies in variable $headers
    set headers [$myCookieManager Cookie $uri]
    if {$headers ne ""} {
      set headers [list Cookie $headers]
    }

    # If the -relaxtransparency option is true, then set the custom
    # Cache-Control header to tell hv3_polipo not to bother validating 
    # this request.
    if {$options(-relaxtransparency)} {
      lappend headers Cache-Control relax-transparency=1
    }

    # Fire off a request via the http package.
    set geturl [list ::http::geturl $uri                     \
      -command [mymethod _DownloadCallback $downloadHandle]  \
      -handler [mymethod _AppendCallback $downloadHandle]    \
      -headers $headers                                      \
    ]
    if {$postdata ne ""} {
      lappend geturl -query $postdata
      if {$enctype ne ""} {
        lappend geturl -type $enctype
      }
    }

    set mimetype [$downloadHandle mimetype]
    if {$mimetype ne "" && ![string match text* $mimetype]} {
      lappend geturl -binary 1
    }
    
    set token [eval $geturl]
#puts "REQUEST $geturl -> $token"

    # Add this handle the the waiting-handles list. Also add a callback
    # to the -failscript and -finscript of the object so that it 
    # automatically removes itself from our lists (myWaitingHandles and
    # myInProgressHandles) after the retrieval is complete.
    lappend myWaitingHandles $downloadHandle
    ::hv3::download_destructor $downloadHandle [
      mymethod Finish_request $downloadHandle $token
    ]
    $self Updatestatusvar
  }

  # The following methods:
  #
  #     [request_https], 
  #     [SSocketReady], 
  #     [SSocketProxyReady], and
  #     [SSocket], 
  #
  # along with the type variable $theWaitingSocket, are part of the
  # https:// support implementation.
  # 
  method request_https {downloadHandle} {
    set obj [::hv3::uri %AUTO% [$downloadHandle uri]]

    set host [$obj cget -authority]
    set port 443
    regexp {^(.*):([0123456789]+)$} $host -> host port

    set proxyhost [::http::config -proxyhost]
    set proxyport [::http::config -proxyport]

    if {$proxyhost eq ""} {
      set fd [socket -async $host $port]
      fileevent $fd writable [mymethod SSocketReady $fd $downloadHandle]
    } else {
      set fd [socket $proxyhost $proxyport]
      fconfigure $fd -blocking 0 -buffering full
      puts $fd "CONNECT $host:$port HTTP/1.1"
      puts $fd ""
      flush $fd
      fileevent $fd readable [mymethod SSocketProxyReady $fd $downloadHandle]
    }
  }
  method SSocketReady {fd downloadHandle} {
    # There is now a tcp/ip socket connection to the https server ready 
    # to use. Invoke ::tls::import to add an SSL layer to the channel
    # stack. Then call [$self request_http] to format the HTTP request
    # as for a normal http server.
    fileevent $fd writable ""
    fileevent $fd readable ""
    set theWaitingSocket [::tls::import $fd]
    $self request_http $downloadHandle
  }
  method SSocketProxyReady {fd downloadHandle} {
    set str [gets $fd line]
    if {$line ne ""} {
      if {! [regexp {^HTTP/.* 200} $line]} { 
        puts "ERRRORR!: $line"
        close $fd
        return
      } 
      while {[gets $fd r] > 0} {}
      $self SSocketReady $fd $downloadHandle
    }
  }

  typevariable theWaitingSocket ""
  typemethod SSocket {host port} {
    set ss $theWaitingSocket
    set theWaitingSocket ""
    return $ss
  }
  # End of code for https://

  # Handle a data: URI.
  #
  # RFC2397: # http://tools.ietf.org/html/2397
  #
  #    dataurl    := "data:" [ mediatype ] [ ";base64" ] "," data
  #    mediatype  := [ type "/" subtype ] *( ";" parameter )
  #    data       := *urlchar
  #    parameter  := attribute "=" value
  #
  method request_data {downloadHandle} {
    set uri [$downloadHandle uri]
    set iData [expr [string first , $uri] + 1]

    set data [string range $uri $iData end]
    set header [string range $uri 0 [expr $iData - 1]]

    if {[string match {*;base64} $header]} {
      set bin [::tkhtml::decode -base64 $data]
    } else {
      set bin [::tkhtml::decode $data]
    }

    if {[regexp {^data:///([^,;]*)} $uri dummy mimetype]} {
        $downloadHandle mimetype $mimetype
    }

    $downloadHandle append $bin
    $downloadHandle finish
  }


  method Finish_request {downloadHandle token} {
    if {[set idx [lsearch $myInProgressHandles $downloadHandle]] >= 0} {
      set myInProgressHandles [lreplace $myInProgressHandles $idx $idx]
    }
    if {[set idx [lsearch $myWaitingHandles $downloadHandle]] >= 0} {
      set myWaitingHandles [lreplace $myWaitingHandles $idx $idx]
    }
    ::http::reset $token
    $self Updatestatusvar
  }

  # Update the value of the -statusvar variable, if the -statusvar
  # option is not set to an empty string.
  method Updatestatusvar {} {
    if {$options(-statusvar) ne ""} {
      set    value "[llength $myWaitingHandles] waiting, "
      append value "[llength $myInProgressHandles] in progress"
      uplevel #0 [list set $options(-statusvar) $value]
    }
  }
  
  # Invoked to set the value of the -statusvar option
  method ConfigureStatusvar {option value} {
    set options($option) $value
    $self Updatestatusvar
  }

  # Invoked when data is available from an http request. Pass the data
  # along to hv3 via the downloadHandle.
  #
  method _AppendCallback {downloadHandle socket token} {
    upvar \#0 $token state 

#puts "APPEND [$downloadHandle uri]"
    # If this download-handle is still in the myWaitingHandles list,
    # process the http header and move it to the in-progress list.
    if {0 <= [set idx [lsearch $myWaitingHandles $downloadHandle]]} {

      # Remove the entry from myWaitingHandles.
      set myWaitingHandles [lreplace $myWaitingHandles $idx $idx]

      foreach {name value} $state(meta) {
#puts "HEADER: $name -> $value"
        switch -- $name {
          Location {
            set redirect $value
            # puts "REDIRECT $value"
          }
          Set-Cookie {
            regexp {^([^= ]*)=([^ ;]*)(.*)$} $value dummy name val cookie_av
            $myCookieManager SetCookie [$downloadHandle uri] $value
          }
          Content-Type {
            if {[set idx [string first ";" $value]] >= 0} {
              set value [string range $value 0 [expr $idx-1]]
            }
            $downloadHandle mimetype $value
          }
          Content-Length {
            $downloadHandle expected_size $value
          }
        }
      }


      set current [$downloadHandle uri]
      if {[info exists redirect]} {
        ::http::reset $token
        $downloadHandle redirect $redirect
        $self requestcmd $downloadHandle
        return
      }

      lappend myInProgressHandles $downloadHandle 
      $self Updatestatusvar
    }

    set data [read $socket 2048]
    set rc [catch [list $downloadHandle append $data] msg]
    if {$rc} { puts "Error: $msg $::errorInfo" }
    set nbytes [string length $data]
    return $nbytes
  }

  # Invoked when an http request has concluded.
  #
  method _DownloadCallback {downloadHandle token} {
#puts "FINISH [$downloadHandle uri]"

    if {
      [lsearch $myInProgressHandles $downloadHandle] >= 0 ||
      [lsearch $myWaitingHandles $downloadHandle] >= 0
    } {
      $downloadHandle finish
    }
  }

  method debug_cookies {} {
    $myCookieManager debug
  }
}

#--------------------------------------------------------------------------
# ::hv3::cookiemanager
#
#     A cookie manager is a database of http cookies. It supports the 
#     following operations:
#    
#         * Add cookie to database,
#         * Retrieve applicable cookies for an http request, and
#         * Delete the contents of the cookie database.
#    
#     Also, a GUI to inspect and manipulate the database in a new top-level 
#     window is provided.
#    
#     Interface:
#    
#         $pathName SetCookie URI DATA
#         $pathName Cookie URI
#         $pathName debug
#
#     Reference:
#
#         http://wp.netscape.com/newsref/std/cookie_spec.html
#
snit::type ::hv3::cookiemanager {

  variable myDebugWindow
  variable myData -array [list]

  #--------------------------------------------------------------------
  # Cookie expiration policy. All text taken from the reference above.
  #
  # * The "expires" attribute specifies a date string that defines the 
  #   valid life time of that cookie. Once the expiration date has 
  #   been reached, the cookie will no longer be stored or given out.
  #
  # * "expires" is an optional attribute. If not specified, the cookie 
  #   will expire when the user's session ends.
  #
  # * This is a specification of the minimum number of cookies that a client
  #   should be prepared to receive and store:
  #      * 300 total cookies
  #      * 20 cookies per server or domain
  #
  # * When the 300 cookie limit or the 20 cookie per server limit is exceeded,
  #   clients should delete the least recently used cookie.
  #--------------------------------------------------------------------

  # This method is called whenever a new cookie is added to the database.
  # It should discard cookies from the database as required to satisfy
  # the 300 cookie or 20 cookie per server limit.
  #
  # TODO: The 20 cookie per server limit. Right now, only the 300 cookie limit
  # is considered.
  #
  method ExpireCookies {} {
    set MAX_COOKIES 300
    if {[array names myData] < [expr $MAX_COOKIES + 20]} return

    set cookies [list]
    foreach key [array names myData] {
      lappend cookies $myData($key)
    }
    unset myData

    set cookies [lsort -decreasing -integer -index 7 $cookies]
    foreach cookie [lrange $cookies 0 [expr $MAX_COOKIES - 1]] {
      foreach {domain flag path secure expires name value lasttime} $cookie {}
      set key "$domain $path $name"
      set myData($key) $cookie
    }
  }

  # Each cookie is stored as a list of 8 elements, as follows:
  #
  #     + domain
  #     + flag (TRUE/FALSE)
  #     + path
  #     + secure (TRUE/FALSE)
  #     + expires (time_t)
  #     + name
  #     + value
  #     + <time last updated or sent>
  #
  method SetCookie {uri data} {

    # Default values for "domain" and "path"
    set obj [::hv3::uri %AUTO% $uri]
    regexp {[^:]*} [$obj cget -authority] v(domain)
    set  v(path) [$obj cget -path]
    $obj destroy

    set  v(flag) TRUE
    
    set d [string trim $data]
    while {$d ne ""} {
      regexp {^([^;]*)(.*)} $d dummy pair d
      set d [string range $d 1 end]

      set idx [string first = $pair]
      set K [string trim [string range $pair 0 [expr $idx-1]]]
      set V [string trim [string range $pair [expr $idx+1] end]]

      if {![info exists name]} {
        set name $K
        set value $V
      } else {
        set v([string tolower $K]) $V
      }
    }

    if {[info exists v(secure)]} {
      set v(secure) TRUE
    } else {
      set v(secure) FALSE
    }

    # Try to convert the "expires" header to a time_t time. This
    # may fail, if the header specifies a date too far in the future 
    # or if it get's the format wrong. In any case it's not particularly
    # important, just set the cookie to never expire and move on.
    set rc [catch {
      set v(expires) [clock scan $v(expires)]
    }]
    if {$rc} {
      set v(expires) 0
    }

    if {[info exists name]} {
      set cookie [list \
          $v(domain) $v(flag) $v(path) $v(secure) $v(expires) $name $value \
          [clock seconds]
      ]
      set key "$v(domain) $v(path) $name"
      set myData($key) $cookie
    } else {
      puts "::hv3::cookiemanager SetCookie - parse failed"
    }

    $self ExpireCookies
    $self UpdateGui
  }

  # Retrieve the cookies that should be sent with the request to the specified
  # URI.  The cookies are returned as a string of the following form:
  #
  #     "NAME1=OPAQUE_STRING1; NAME2=OPAQUE_STRING2 ..."
  #
  method Cookie {uri} {
    set obj [::hv3::uri %AUTO% $uri]
    set uri_domain [$obj cget -authority]
    set uri_path [$obj cget -path]
    $obj destroy

    set ret ""

    foreach k [array names myData] {
      set cookie $myData($k)
      foreach {domain flag path secure expires name value DUMMY} $cookie {}

      # Check if the cookie has already expired. If so, remove it from
      # the database (and do not send it).
      if {$expires != 0 && [clock seconds] > $expires} {
        unset myData($k)
      } elseif {
          [string match *$domain $uri_domain] && 
          [string match ${path}* $uri_path]
      } {
        append ret [format "%s=%s; " $name $value]

        # Update the "last-used" field.
        lset myData($k) 7 [clock seconds]
      }
    }

    return $ret
  }

  method Report {} {
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
    if {[llength [array names myData]] > 0} {
      append Content "<table border=1 cellpadding=5>"
      append Content "<tr>"
      foreach h {Domain Flag Path Secure Expires Name Value} {
        append Content "<th>$h"
      }
      append Content "</tr>"
      foreach key [array names myData] { 
        set cookie $myData($key)
        append Content "<tr>"
        foreach e $cookie {append Content "<td>$e"}
        append Content "</tr>"
      }
      append Content "</table>"
    } else {
      set Content {
        <p>The cookies database is currently empty.
      }
    }

    return [subst $Template]
  } 

  # Clear
  # Import
  # Export
  #
  #     The following three methods are called when the "clear", "import"
  #     or "export" buttons of the Cookies gui are pressed. The import
  #     and export functions use the same cookies file format as 
  #     the gecko browsers (as of firefox 1.5).
  #
  method Clear {} {
    array unset [myvar myData]
    $self UpdateGui
  }
  method Import {{fname ""}} {
    if {$fname eq ""} {
      set fname [tk_getOpenFile]
      if {$fname eq ""} return
    }
    set fd [open $fname]
    while {![eof $fd]} {
      set line [gets $fd]
      set cookie [split $line "\t"]
      if {[llength $cookie] == 7} {
        foreach {domain flag path secure expires name value} $cookie {}
        set key "$domain $path $name"
        set myData($key) $cookie
      }
    }
    $self UpdateGui
  }
  method Export {} {
    set fname [tk_getSaveFile]
    if {$fname eq ""} return
    set fd [open $fname w]
    puts $fd "# cookies.txt file generated by Hv3."
    puts $fd ""
    foreach key [array names myData] {
      set cookie $myData($key)
      puts $fd [join $cookie "\t"]
    }
  }

  method getdata {} {
    return [array get myData]
  }
  method loaddata {data} {
    array set myData $data
  }

  constructor {} {
    set myDebugWindow .cookies_debug[string map {: _} $self]
  }


  variable myHv3List [list]
  method cookies_request {hv3 downloadHandle} {
    $downloadHandle append [$self Report]
    $downloadHandle finish

    if {[lsearch $myHv3List $hv3] < 0} {
      lappend myHv3List $hv3
    }
  }
  method UpdateGui {} {
    set newlist [list]
    foreach hv3 $myHv3List {
      if {[info commands $hv3] eq ""} continue

      if {[string match cookies* [$hv3 location]] && [$hv3 pending] == 0} {
        $hv3 goto cookies:
        lappend newlist $hv3
      } 
    }
    set myHv3List $newlist
  }
}

proc ::hv3::cookies_scheme_init {hv3 protocol} {
  $protocol schemehandler cookies [
       list ::hv3::the_cookie_manager cookies_request $hv3
  ]
}

# ::hv3::filedownload
#
# Each currently downloading file is managed by an instance of the
# following object type. All instances in the application are managed
# by the [::hv3::the_download_manager] object, an instance of
# class ::hv3::downloadmanager (see below).
#
# SYNOPSIS:
#
#     set obj [::hv3::filedownload %AUTO% ?OPTIONS?]
#
#     $obj set_destination $PATH
#     $obj append $DATA
#     $obj finish
#
# Options are:
#
#     Option        Default   Summary
#     -------------------------------------
#     -source       ""        Source of download (for display only)
#     -size         ""        Expected size in bytes
#     -cancelcmd    ""        Script to invoke to cancel the download
#
snit::type ::hv3::filedownload {

  # The destination path (in the local filesystem) and the corresponding
  # tcl channel (if it is open). These two variables also define the 
  # three states that this object can be in:
  #
  # INITIAL:
  #     No destination path has been provided yet. Both myDestination and
  #     myChannel are set to an empty string.
  #
  # STREAMING:
  #     A destination path has been provided and the destination file is
  #     open. But the download is still in progress. Neither myDestination
  #     nor myChannel are set to an empty string.
  #
  # FINISHED:
  #     A destination path is provided and the entire download has been
  #     saved into the file. We're just waiting for the user to dismiss
  #     the GUI. In this state, myChannel is set to an empty string but
  #     myDestination is not.
  #
  variable myDestination ""
  variable myChannel ""

  # Buffer for data while waiting for a file-name. This is used only in the
  # state named INITIAL in the above description. The $myIsFinished flag
  # is set to true if the download is finished (i.e. [finish] has been 
  # called).
  variable myBuffer ""
  variable myIsFinished 0

  option -source    -default ""
  option -size      -default ""
  option -cancelcmd -default ""
  option -updateguicmd -default ""

  # Total bytes downloaded so far.
  variable myDownloaded 0

  constructor {args} {
    $self configurelist $args
  }

  method set_destination {dest} {

    # It is an error if this method has been called before.
    if {$myDestination ne ""} {
      error "This ::hv3::filedownloader already has a destination!"
    }

    if {$dest eq ""} {
      # Passing an empty string to this method cancels the download.
      # This is for conveniance, because [tk_getSaveFile] returns an 
      # empty string when the user selects "Cancel".
      $self Cancel
      destroy $self
    } else {
      # Set the myDestination variable and open the channel to the
      # file to write. Todo: An error could occur opening the file.
      set myDestination $dest
      set myChannel [open $myDestination w]
      fconfigure $myChannel -encoding binary -translation binary

      # If a buffer has accumulated, write it to the new channel.
      puts -nonewline $myChannel $myBuffer
      set myBuffer ""

      # If the myIsFinished flag is set, then the entire download
      # was already in the buffer. We're finished.
      if {$myIsFinished} {
        $self finish {}
      }

      ::hv3::the_download_manager manage $self
    }
  }

  # This internal method is called to cancel the download. When this
  # returns the object will have been destroyed.
  #
  method Cancel {} {
    # Evaluate the -cancelcmd script and destroy the object.
    eval $options(-cancelcmd)
    if {$myDestination ne ""} {
      catch {close $myChannel}
      catch {file delete $myDestination}
    }
  }

  # Update the GUI to match the internal state of this object.
  #
  method Updategui {} {
    if {$options(-updateguicmd) ne ""} {
      eval $options(-updateguicmd)
    }
  }

  method append {data} {
    if {$myChannel ne ""} {
      puts -nonewline $myChannel $data
      set myDownloaded [file size $myDestination]
    } else {
      append myBuffer $data
      set myDownloaded [string length $myBuffer]
    }
    $self Updategui
  }

  # Called by the driver download-handle when the download is 
  # complete. All the data will have been already passed to [append].
  #
  method finish {data} {
    $self append $data

    # If the channel is open, close it. Also set the button to say "Ok".
    if {$myChannel ne ""} {
      close $myChannel
      set myChannel ""
    }

    # If myIsFinished flag is not set, set it and then set myElapsed to
    # indicate the time taken by the download.
    if {!$myIsFinished} {
      set myIsFinished 1
    }

    # Update the GUI.
    $self Updategui
  }

  destructor {
    catch { close $myChannel }
  }

  # Query interface used by ::hv3::downloadmanager GUI. It cares about
  # four things: 
  #
  #     * the percentage of the download has been completed, and
  #     * the state of the download (either "Downloading" or "Finished").
  #     * the source URI
  #     * the destination file
  #
  method state {} {
    if {$myIsFinished} {return "Finished"}
    return "Downloading"
  }
  method percentage {} {
    if {$myIsFinished} {return 100}
    if {$options(-size) eq ""} {return 50}
    return [expr double($myDownloaded) / double($options(-size)) * 100]
  }
  method source {} {
    return $options(-source)
  }
  method destination {} {
    return $myDestination
  }
}

snit::type ::hv3::downloadmanager {
  variable myDownloads [list]
  variable myHv3List [list]

  method manage {filedownload} {
    $filedownload configure -updateguicmd [mymethod UpdateGui $filedownload]
    lappend myDownloads $filedownload
    $self CheckGuiList
    foreach hv3 $myHv3List {
      $hv3 goto download:
    }
  }

  method CheckGuiList {} {
    # Make sure the list of GUI's is up to date.
    set newlist [list]
    foreach hv3 $myHv3List {
      if {[info commands $hv3] eq ""} continue
      if {[string match download* [$hv3 location]] && [$hv3 pending] == 0} {
        lappend newlist $hv3
      } 
    }
    set myHv3List $newlist
  }

  method UpdateGui {{fdownload ""}} {
# puts "UPDATE $fdownload"

    $self CheckGuiList

    set dl_list $fdownload
    if {[llength $dl_list] == 0} {
      set dl_list $myDownloads
    } 
    foreach filedownload $dl_list {
      set id [string map {: _} $filedownload]
      foreach hv3 $myHv3List {

        set search "#$id .progressbar"
        foreach N [$hv3 search $search] {
          $N override [list width [$filedownload percentage]%]
        }

        set search "#$id .downloading"
        set val visible
        if {[$filedownload state] eq "Finished"} {
          set val hidden
        }
        foreach N [$hv3 search $search] { 
          $N override [list visibility $val] 
        }

        set val hidden
        if {[$filedownload state] eq "Finished"} {
          set val visible
        }
        set search "#$id .finished"
        foreach N [$hv3 search $search] { 
          $N override [list visibility $val] 
        }

        set search "#$id .button"
        foreach N [$hv3 search $search] { 
          set a(Finished)    Dismiss
          set a(Downloading) Cancel
          $N attr value $a([$filedownload state])
        }

        set search "#$id .status span"
        foreach N [$hv3 search $search] { 
          set percent [format %.2f%s [$filedownload percentage] %]
          $N attr spancontent "[$filedownload state] ($percent)"
        }
      }
    }
  }

  method request {hv3 handle} {

    set uri [$handle uri]
    if {[regexp {.*delete=([^=&]*)} $uri -> delete]} {
      set dl [string map {_ :} $delete]
      set newlist [list]
      foreach download $myDownloads {
        if {$download ne $dl} {lappend newlist $download}
      }
      set myDownloads $newlist
      catch {
        if {[$dl state] ne "Finished"} {
          $dl Cancel
        }
        $dl destroy
      }
      $handle append ""
      $handle finish
      $self CheckGuiList
      foreach hv3 $myHv3List {
        after idle [list $hv3 goto download:]
      }
      return
    }

    set document {
      <html><head>
        <style>
          .download { border:solid black 1px; width:90%; margin: 1em auto; }
          .download td { padding: 0px 5px; } 
          .source { width:99%; }
          .progress .progressbarwrapper { border:solid black 1px; width:100%; }
          .progress .progressbar { background-color: navy; height: 1em; }
          .buttons { display:block;width:12ex }
          input { float:right; }
        </style>
        <title>Downloads</title>
        </head>
        <body>
          <h1 align=center>Downloads</h1>
    }
 
    append document "<p>There are [llength $myDownloads] downloads.</p>"

    foreach download $myDownloads {
      set id [string map {: _} $download]
      append document [subst {
        <table class="download" id="$id">
          <tr><td>Source:      
              <td class="source">[$download source]
              <td rowspan=4 valign=bottom>
                 <div class="buttons">
                      <form method=get action=download:///>
                      <input class="button"      type=submit value=Cancel>
                      <input name="delete"       type=hidden value=$id>
                      </form>
          <tr><td>Destination: 
              <td class="destination">[$download destination]
          <tr><td>Status:      
              <td class="status">
                <span spancontent="Waiting (0%)">
              </td>
          <tr><td>Progress:    
              <td class="progress">
                 <div class="progressbarwrapper">
                 <div class="progressbar">
        </table>
      }]
    }

    if {[lsearch $myHv3List $hv3] < 0} {
      lappend myHv3List $hv3
    }

    $handle append $document
    $handle finish

    after idle [list $self UpdateGui]
  }

  method show {} {
    $self CheckGuiList
    if {[llength $myHv3List] > 0} {
      set hv3 [lindex $myHv3List 0]
      set win [winfo parent [winfo parent $hv3]]
      .notebook.notebook select $win
    } else {
      .notebook add download:
    }
  }
}

proc ::hv3::download_scheme_init {hv3 protocol} {
  $protocol schemehandler download [
    list ::hv3::the_download_manager request $hv3
  ]
}

#-----------------------------------------------------------------------
# Work around a bug in http::Finish
#

# First, make sure the http package is actually loaded. Do this by 
# invoking ::http::geturl. The call will fail, since the arguments (none)
# passed to ::http::geturl are invalid.
catch {::http::geturl}

# Declare a wrapper around ::http::Finish
proc ::hv3::HttpFinish {token args} {
  upvar 0 $token state
  catch {
    close $state(sock)
    unset state(sock)
  }
  eval [linsert $args 0 ::http::FinishReal $token]
}

# Install the wrapper.
rename ::http::Finish ::http::FinishReal
rename ::hv3::HttpFinish ::http::Finish
#-----------------------------------------------------------------------

