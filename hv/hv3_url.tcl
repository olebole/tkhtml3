###########################################################################
# hv3_url.tcl --
#
#     This file contains code to manipulate and download data from URI's.
#

#--------------------------------------------------------------------------
# Global variables section
array unset url_g_scripts
set http_current_socket -1
array unset http_name_cache

#--------------------------------------------------------------------------

#
# "Url" public commands:
#
#      url_resolve
#      url_fetch
#      url_cancel
#
# The url procedures use the following global variables:
#
#      url_g_scripts
#      http_current_socket
#      http_name_cache
#
# And the following internal procedures:
#
#      url_callback
#      http_get_socket
#      http_get_url
#


#--------------------------------------------------------------------------
# cache_init, cache_store, cache_query, cache_fetch --
#
#         cache_init
#         cache_store URL DATA
#         cache_query URL
#         cache_fetch URL
#
#     A tiny API to implement a primitive web cache.
#
proc cache_init {file} {
  sqlite3 dbcache $file
  .html var cache dbcache
  catch {
    [.html var cache] eval {CREATE TABLE cache(url PRIMARY KEY, data BLOB);}
  }
}
proc cache_store {url data} {
  set sql {REPLACE INTO cache(url, data) VALUES($url, $data);}
  [.html var cache] eval $sql
}
proc cache_query {url} {
  set sql {SELECT count(*) FROM cache WHERE url = $url}
  return [[.html var cache] one $sql]
}
proc cache_fetch {url} {
  set sql {SELECT data FROM cache WHERE url = $url}
  return [[.html var cache] one $sql]
}
#
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Enhancement to the http package - asychronous socket connection
#
#     The tcllib http package does not support asynchronous connections
#     (although asynchronous IO is supported after the connection is
#     established). The following code is a hack to work around that.
#
#     Note: Asynchronous dns lookup is required too!
#
set UA "Mozilla/5.0 (compatible; Konqueror/3.3; Linux) (KHTML, like Gecko)"
::http::register http 80 http_get_socket
::http::config -useragent $UA

set http_current_socket -1
array set http_name_cache [list]

proc http_get_socket {args} {
  set ret $::http_current_socket
  set ::http_current_socket -1
  return $ret
}

proc http_get_url {url args} {
  array set urlargs [::uri::split $url]
  set server $urlargs(host)
  set port $urlargs(port)
  if {$port == ""} {
    set port 80
  }

  if {![info exists ::http_name_cache($server)]} {
    set ::http_name_cache($server) [::tk::htmlresolve $server]
  }
  set server_ip $::http_name_cache($server)

  set script [concat [list ::http::geturl $url] $args]
  set s [socket -async $server_ip $port]
  fileevent $s writable [subst -nocommands {
    set ::http_current_socket $s
    $script
    if {[http_get_socket] != -1} {error "assert()"}
  }]
}
#--------------------------------------------------------------------------

# url_resolve --
#
#         url_resolve URL ?options?
#
#         -setbase       (default false) 
#
#     Return a canonical, non-relative version of URL. If the -setbase
#     option is true, then the base of the returned URL is stored in the
#     widget dictionary and used by later invocations of url_resolve.
#
swproc url_resolve {url {setbase 0 1}} {
  set base [.html var baseurl]
  set ret $url

  set ret [uri::canonicalize [uri::resolve $base $url]]

  if {$setbase} {
    if {0 == [regexp {^(.*/)[^/]*$} $ret newbase]} {
      set newbase {}
    }
    .html var baseurl $newbase
  }

  return $ret
}

# url_fetch --
#
#         url_fetch URL ?-switches ...?
#
#         -script        (default {})
#         -id            (default {})
#         -cache         (default {})
#         -binary
# 
#     This procedure is used to retrieve remote files. Argument -url is the
#     url to retrieve. When it has been retrieved, the data is appended to
#     the script -script (if any) and the result invoked.
# 
swproc url_fetch {url {script {}} {id {}} {cache {}} {binary 0 1}} {

  # Check the cache before doing anything else.
  if {[cache_query $url]} {
    if {$script != ""} {
      set data [cache_fetch $url]
      lappend script $data
      eval $script
    }
    return
  }

  switch -regexp -- $url {

    {^file://} {
      # Handle file:// URLs by loading the contents of the specified file
      # system entry.
      set rc [catch {
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
      } msg]
    }

    {^http://} {
      if {0 == [info exists ::url_g_scripts($url)]} {
        set rc [catch {
          http_get_url $url -command [list url_callback] -timeout 120000
        } msg]
        set ::url_g_scripts($url) [list]
        gui_log "DOWNLOAD Start: \"$url\""
      }
      if {$script != ""} {
        lappend ::url_g_scripts($url) $script
      }
    }

    default {
      # Any other kind of URL is an error
      set rc 1
      set msg {}
    }
  }
}

# url_cancel --
#
#         url_cancel ID
#
#     Cancel all currently executing downloads associated with id $id.
#
proc url_cancel {id} {
}

# url_callback --
#
#         url_callback SCRIPT TOKEN
#
#     This callback is made by the http package when the response to an
#     http request has been received.
#
proc url_callback {token} {
  # The following line is a trick of the http package. $token is the name
  # of a Tcl array in the global context that contains the downloaded
  # information. The [upvar] command makes "state" a local alias for that
  # array variable.
  upvar #0 $token state

  gui_log "DOWNLOAD Finished: \"$state(url)\""
  cache_store $state(url) $state(body)

  # Check if this is a redirect. If so, do not invoke $script, just fetch
  # the URL we are redirected to. TODO: Need to the -id and -cache options
  # to [url_fetch] here.
  foreach {n v} $state(meta) {
    if {[regexp -nocase ^location$ $n]} {
      foreach script $::url_g_scripts($state(url)) {
        url_fetch [string trim $v] -script $script
      }
      url_fetch [string trim $v]
      return
    }
  }
  
  foreach script $::url_g_scripts($state(url)) {
    lappend script $state(body)
    eval $script
  }

  # Cleanup the record of the download.
  ::http::cleanup $token
}
