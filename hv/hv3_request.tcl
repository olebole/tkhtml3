namespace eval hv3 { set {version($Id: hv3_request.tcl,v 1.20 2007/12/09 06:43:49 danielk1977 Exp $)} 1 }

#--------------------------------------------------------------------------
# This file contains the implementation of two types used by hv3:
#
#     ::hv3::download
#

#--------------------------------------------------------------------------
# Class ::hv3::request
#
#     Instances of this class are used to interface between the protocol
#     implementation and the hv3 widget.
#
# OVERVIEW:
#
# HOW CHARSETS ARE HANDLED:
#
#     The protocol implementation (the thing that calls [$download append] 
#     and [$download finish]) passes binary data to this object. This
#     object converts the binary data to utf-8 text, based on the encoding
#     assigned to the request. An encoding may be assigned either by an
#     http header or a <meta> tag.
#
#     Assuming the source of the data is http (or https), then the
#     encoding may be specified by way of a Content-Type HTTP header.
#     In this case, when the protocol configures the -header option
#     (which it does before calling [append] for the first time) the 
#     -encoding option will be automatically set.
#
#
# OPTIONS:
#
#     The following options are set only by the requestor (the Hv3 widget)
#     for the protocol to use as request parameters:
#
#       -cachecontrol
#       -uri
#       -postdata
#       -requestheader
#       -enctype
#       -encoding
#
#     This is set by the requestor also to show the origin of the request:
#
#       -hv3
#
#     These are set by the requestor before the request is made to 
#     configure callbacks invoked by this object when requested data 
#     is available:
#    
#       -incrscript
#       -finscript
#
#     This is initially set by the requestor. It may be modified by the
#     protocol implementation before the first invocation of -incrscript
#     or -finscript is made.
#
#       -mimetype
#
#     The protocol implementation also sets:
#
#       -header
#       -expectedsize
#
# METHODS:
#
#     Methods used by the protocol implementation:
#
#         append DATA
#         finish
#         fail
#         authority         (return the authority part of the -uri option)
#
#     finish_hook SCRIPT
#         Configure the object with a script to be invoked just before
#         the object is about to be destroyed. If more than one of
#         these is configured, then the scripts are called in the
#         same order as they are configured in (i.e. most recently
#         configured is invoked last).
#
#     reference
#     release
#
#     data
#     encoding
#
snit::type ::hv3::download {

  # The requestor (i.e. the creator of the ::hv3::download object) sets the
  # following configuration options. The protocol implementation may set the
  # -mimetype option before returning.
  #
  # The -cachecontrol option may be set to the following values:
  #
  #     * normal             (try to be clever about caching)
  #     * no-cache           (never return cached resources)
  #     * relax-transparency (return cached resources even if stale)
  #
  option -cachecontrol -default normal
  option -uri          -default ""
  option -postdata     -default ""
  option -mimetype     -default ""
  option -enctype      -default ""

  # The hv3 widget that issued this request. This is used
  # (a) to notify destruction of root request,
  # (b) by the handler for home:// uris and
  # (c) to call [$myHtml reset] in restartCallback.
  #
  option -hv3      -default ""

  # If a connection is canceled too early (ex. for META triggered reloading),
  # proxy(polipo) can fall into corrupted state. To avoid this,
  # canceled connection should be kept open (but fileevent is disabled)
  # until overall download tree is finished.
  #
  option -token    -default ""

  # The protocol implementation sets this option to contain the 
  # HTTP header (or it's equivalent). The format is a serialised array.
  # Example:
  # 
  #     {Set-Cookie safe-search=on Location http://www.google.com}
  #
  # The following http-header types are handled locally by the ConfigureHeader
  # method, as soon as the -header option is set:
  #
  #     Set-Cookie         (Call ::hv3::the_cookie_manager method)
  #     Content-Type       (Set the -mimetype option)
  #     Content-Length     (Set the -expectedsize option)
  #
  option -header -default "" -configuremethod ConfigureHeader

  option -requestheader -default ""

  # Expected size of the resource being requested. This is used
  # for displaying a progress bar when saving remote resources
  # to the local filesystem (aka downloadin').
  #
  option -expectedsize -default ""

  # Callbacks configured by the requestor.
  #
  option -incrscript   -default ""
  option -finscript    -default ""

  # This -encoding option is used to specify explicit conversion of
  # incoming http/file data.
  # When this option is set, [http::geturl -binary] is used.
  # Then [$self append] will call [encoding convertfrom].
  #
  # See also [encoding] and [suggestedEncoding] methods.
  #
  option -encoding -default ""

  # True if the -encoding option has been set by the transport layer. 
  # If this is true, then any encoding specified via a <meta> element
  # in the main document is ignored.
  #
  option -hastransportencoding -default 0

  # END OF OPTIONS
  #----------------------------

  variable myData ""
  variable myChunksize 2048

  # The binary data returned by the protocol implementation is 
  # accumulated in this variable.
  variable myRaw  {}

  # If this variable is non-zero, then the first $myRawPos bytes of
  # $myRaw have already been passed to Hv3 via the -incrscript 
  # callback.
  variable myRawPos 0

  # These objects are referenced counted. Initially the reference count
  # is 1. It is increased by calls to the [reference] method and decreased
  # by the [release] method. The object is deleted when the ref-count 
  # reaches zero.
  variable myRefCount 1

  variable myIsText  1; # Whether mimetype is text/* or not.

  # Make sure finish is processed only once.
  variable myIsFinished 0

  # Destroy-hook scripts configured using the [finish_hook] method.
  variable myFinishHookList [list]

  # Constructor and destructor
  constructor {args} {
    $self configurelist $args
    # puts stderr new\t$self\t[$self cget -uri]\t[clock clicks]
  }

  destructor {
    foreach hook $myFinishHookList { eval $hook }
  }

  method data {} {
    set raw [string range $myRaw 0 [expr {$myRawPos-1}]]
    if {$myIsText} {
      return [encoding convertfrom [$self encoding] $raw]
    }
    return $raw
  }

  # Increment the object refcount.
  #
  method reference {} {
    incr myRefCount
  }

  # Decrement the object refcount.
  #
  method release {} {
    incr myRefCount -1
    if {$myRefCount == 0} {
      $self destroy
    }
  }

  # Add a script to be called just before the object is destroyed. See
  # description above.
  #
  method finish_hook {script} {
    lappend myFinishHookList $script
  }

  # This method is called each time the -header option is set. This
  # is where the locally handled HTTP headers (see comments above the
  # -header option) are handled.
  #
  method ConfigureHeader {name option_value} {
    set options($name) $option_value
    foreach {name value} $option_value {
      switch -- [string tolower $name] {
        set-cookie {
          ::hv3::the_cookie_manager SetCookie $options(-uri) $value
        }
        content-type {
          set parsed [hv3::string::parseContentType $value]
          foreach {major minor charset} $parsed break
          set options(-mimetype) $major/$minor
          if {$charset ne ""} {
            set options(-hastransportencoding) 1
            set options(-encoding) [::hv3::encoding_resolve $charset]
          }
        }
        content-length {
          set options(-expectedsize) $value
        }
      }
    }
  }

  onconfigure -mimetype mimetype {
      set options(-mimetype) $mimetype
      set myIsText [string match text* $mimetype]
  }

  onconfigure -encoding enc {
      set options(-encoding) [::hv3::encoding_resolve $enc]
  }

  # Return the "authority" part of the URI configured as the -uri option.
  #
  method authority {} {
    set obj [::tkhtml::uri $options(-uri)]
    set authority [$obj authority]
    $obj destroy
    return $authority
  }

  # Interface for returning data.
  method append {raw} {
    append myRaw $raw

    if {$options(-incrscript) != ""} {
      # There is an -incrscript callback configured. If enough data is 
      # available, invoke it.
      set nLast 0
      foreach zWhite [list " " "\n" "\t"] {
        set n [string last $zWhite $myRaw]
        if {$n>$nLast} {set nLast $n ; break}
      }
      set nAvailable [expr {$nLast-$myRawPos}]
      if {$nAvailable > $myChunksize} {

        set zDecoded [string range $myRaw $myRawPos $nLast]
        if {$myIsText} {
          set zDecoded [encoding convertfrom [$self encoding] $zDecoded]
        }
        set myRawPos [expr {$nLast+1}]
        if {$myChunksize < 30000} {
          set myChunksize [expr $myChunksize * 2]
        }

        eval [linsert $options(-incrscript) end $zDecoded] 
      }
    }
  }

  # Called after all data has been passed to [append].
  #
  method finish {{raw ""}} {
    if {$myIsFinished} {error "finish called twice on $self"}
    set myIsFinished 1
    append myRaw $raw

    set zDecoded [string range $myRaw $myRawPos end]
    if {$myIsText} {
      set zDecoded [encoding convertfrom [$self encoding] $zDecoded]
    }

    foreach hook $myFinishHookList {
      eval $hook
    }
    set myFinishHookList [list]
    set myRawPos [string length $myRaw]
    eval [linsert $options(-finscript) end $zDecoded] 
  }

  method isFinished {} {set myIsFinished}

  method fail {} {
    # TODO: Need to do something here...
    puts FAIL
  }

  method encoding {} {
    string-or $options(-encoding) [encoding system]
  }

  proc string-or args {
      foreach i $args {
	  if {$i ne ""} {return $i}
      }
  }
}

