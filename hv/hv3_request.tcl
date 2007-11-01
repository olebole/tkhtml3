namespace eval hv3 { set {version($Id: hv3_request.tcl,v 1.18 2007/11/01 07:06:07 danielk1977 Exp $)} 1 }

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
# METHODS:
#
#     Methods used by the protocol implementation:
#
#         append DATA
#         finish
#         fail
#         authority         (return the authority part of the -uri option)
#
#     destroy_hook SCRIPT
#         Configure the object with a script to be invoked just before
#         the object is about to be destroyed. If more than one of
#         these is configured, then the scripts are called in the
#         same order as they are configured in (i.e. most recently
#         configured is invoked last).
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

  # Lifetime of download(request) object is managed like a (flat) tree.
  # The root of download is remembered by -hv3. All other child downloads
  # are remembered by the root.
  #
  # For child download, when download is finished, it is simply destroyed
  # and removed from root.
  #
  option -root     -default no

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

  # END OF OPTIONS
  #----------------------------

  variable myData ""
  variable myChunksize 2048
  variable myRaw  ""; # for encoding conversion.
  variable readPos 0; # myRaw read pointer.
  variable isText  1; # Whether mimetype is text/* or not.

  # Transport layer may contain encoding specification.
  # This should be distinguished from explicitly specified one.
  variable suggestedEncoding {}

  # To temporarily backup fileevent when reloading.
  variable freezedReadEvent {}

  # To bypass destruction logic when reloading.
  variable nowReloading   0

  # To avoid queueing of checkPending task.
  variable nowDestructing 0

  # List of child downloads.
  variable myChildren [list]

  # Make sure finish is processed only once.
  variable isFinished 0

  # Destroy-hook scripts configured using the [destroy_hook] method.
  variable myDestroyHookList [list]

  # Constructor and destructor
  constructor {args} {
    $self configurelist $args
    # puts stderr new\t$self\t[$self cget -uri]\t[clock clicks]
  }

  destructor  {
    set nowDestructing 1
      # puts stderr destroy\t$self\t[$self cget -uri]\t[$self cget -encoding]\t[$self cget -finscript]\t[lrange [info level -3] 0 1]
    if {$options(-root)} {
	$self checkPending cancel
    }
    foreach child $myChildren {
	if {![$child isFinished]} {
	    $child finish
	}
	# finish can destroy $child.
	if {[llength [info commands $child]]} {
	    $child destroy
	}
    }
    foreach hook $myDestroyHookList {
      eval $hook
    }
    if {$options(-root)} {
	$options(-hv3) Forgetrequest
    }
  }

  # Milder destruction request.
  method release {} {
      # puts stderr release\t$self\t[clock seconds]
      if {! $options(-root)} {
	  $self destroy
      } else {
	  # Enqueue checkPending task. This may destroy $self later.
	  # At this time, parsing is not yet finished and
	  # not all child downloads are created. So we must delay
	  # destruction.
	  # puts "release for root $self is requested"
	  $self checkPending
      }
  }

  # checkPending task management.
  #
  method checkPending {{method "enqueue"}} {
      set task [list $self checkPending now]
      switch $method {
	  now {
	      # Check pendings immediately and then destroy itself.
	      if {[$self pending]} return
	      if {$isFinished} {
		  $self destroy
	      } else {
		  $self finish
		  # finish can create another download. So reschedule.
		  $self checkPending
	      }
	  }

	  cancel {
	      after cancel $task
	  }

	  enqueue {
	      # Reschedule the task, unless under destruction.
	      after cancel $task
              if {!$nowDestructing} {
		  after idle $task
              }
	  }
	  default {
	      error "Invalid option for checkPending: $method"
	  }
      }
  }

  # Add a script to be called just before the object is destroyed. See
  # description above.
  #
  method destroy_hook {script} {
    lappend myDestroyHookList $script
  }

  method pending {} {
      expr {[llength $myChildren] || [$self hasLivingSocket] || $nowReloading}
  }

  method addChild {handle} {
      lappend myChildren $handle
      $handle destroy_hook [list $self forget $handle]
  }

  method forget {handle} {
      set idx [lsearch $myChildren $handle]
      set has_removed [expr {$idx >= 0}]
      if {$has_removed} {
	  set myChildren [lreplace $myChildren $idx $idx]
      }
      if {![llength $myChildren]} {
	  # Now all child downloads are forgotten.
	  set nowReloading 0
	  $self checkPending
      }
      set has_removed
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
           foreach {major minor charset} \
               [hv3::string::parseContentType $value] break
            set options(-mimetype) $major/$minor
           if {$charset ne ""} {
               $self suggestEncoding $charset
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
      set isText [string match text* $mimetype]
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
    set decoded [if {$isText} {
	$self EncodingGetConvertible myRaw
    } else {
	set raw
    }]
    # if {$isText && $options(-encoding) ne ""} { puts "(($decoded))" }
    ::append myData $decoded
    set nData [string length $myData]
    if {$options(-incrscript) != "" && $nData >= $myChunksize} {
	# puts stderr incr\t$self\t$nData-[string bytelength $myRaw]-[string bytelength $raw]-$myChunksize\t$options(-incrscript)\t[clock seconds]
      eval [linsert $options(-incrscript) end $myData]
      set myData {}
      if {$myChunksize < 30000} {
        set myChunksize [expr $myChunksize * 2]
      }
    }
  }

  method isFinished {} {set isFinished}
  # Called after all data has been passed to [append].
  method finish {{raw ""}} {
    if {$isFinished} {error "finish called twice on $self"}
    set isFinished 1
    append myRaw $raw
    if {$isText && $myRaw ne ""} {
	append myData [$self EncodingGetConvertible myRaw 1]
	# puts stderr fin\t$self\t[$self cget -uri]\t[clock seconds]\n[string length $myData]\n[lrange [info level -1] 0 1]\n$myData
    } else {
	append myData $myRaw
    }
    eval [linsert $options(-finscript) end $myData] 
  }

  method fail {} {puts FAIL}

  method reload_with_encoding enc {

      # At present, this method is called on the "root" download by
      # the Hv3 widget when it encounters a tag of the form:
      #
      #     <META http-equiv="content-type" content="text/html;charset=ENC">
      #
      # where ENC is not the same as the encoding specified by the 
      # transport layer (or the system encoding, if the transport layer did
      # not specify an encoding).
      #
      # Usually, but not always, this occurs before the widget requests any
      # other resources (the HTML5 specification at www.whatwg.org suggests
      # that UA's should ignore such <META> elements in other circumstances,
      # but this doesn't work on the web in 2007). In this case, all that
      # is required is to translate the binary data to the new encoding
      # and reinvoke the -incrscript/-finscript callbacks.
      #
      if {[llength $myChildren] == 0} {
          set options(-encoding) $enc
          set readPos 0
          set myData ""
          if {$isFinished} {
              set isFinished 0 
              $self finish
          } else {
              $self append ""
          }
          return
      }

      set nowReloading 1
      # puts "freezing root request."
      $self freeze
      foreach child $myChildren {
	  $child freeze
      }

      # This is workaround for co-occurence of META-reload and
      # META-refresh 0. Could be removed future.
      update idletask

      $self configure -encoding $enc
      # puts "queueing restart of $self"
      after idle [list $self restartCallback]
  }

  method hasLivingSocket {} {
      upvar 0 $options(-token) state
      set vn state(sock)
      info exists $vn
  }

  method freeze {} {
      if {$options(-token) ne ""} {
	  upvar 0 $options(-token) state
	  set vn state(sock)
	  if {[info exists $vn]} {
	      set freezedReadEvent [fileevent [set $vn] readable]
	      # puts "saving readevent $freezedReadEvent"
	      fileevent [set $vn] readable {}
	  } else {
	      # puts "token $options(-token) is already closed"
	  }
      }
      set readPos 0; # myRaw is kept as is.
      set myData {}
  }

  method restartCallback {} {
      # puts "restart with token $options(-token)"
      if {$options(-root) && $options(-hv3) ne ""} {
	  # This is required currently. But why? Who calls [$myHtml parse]
	  # after meta_node_handler?
	  [$options(-hv3) html] reset
      }
      if {$isFinished} {
	  # To reset readPos and reinvoke -finscript.
	  set isFinished 0
	  $self finish
      } else {
	  if {$options(-token) ne ""} {
	      upvar 0 $options(-token) state
	      set vn state(sock)
	      if {[info exists $vn]} {
		  fileevent [set $vn] readable $freezedReadEvent
	      }
	  }
	  $self append {}
      }
  }

  method encoding {} {
      if {[set enc [string-or $options(-encoding) \
			$suggestedEncoding]] ne ""} {
	  set enc
      } else {
	  encoding system
      }
  }

  method suggestEncoding enc {
      puts "suggested encoding $enc"
      set suggestedEncoding [::hv3::encoding_resolve $enc]
      if {$options(-root) && $options(-hv3) ne ""} {
	  puts " -> send it to hv3"
	  $options(-hv3) setEncoding $enc
      }
  }

  method suggestedEncoding {} {
      set suggestedEncoding
  }

  method EncodingGetConvertible {varName {all 0}} {
      upvar 1 $varName raw
      if {$all} {
	  set found [string bytelength $raw]
      } else {
	  set found -1
	  foreach ch {\n " " \t} {
	      if {[set found [string last $ch $raw end]] > 0} break
	  }
	  if {$found <= 0} return
      }
      set decoded [encoding convertfrom [$self encoding] \
		       [string range $raw $readPos $found]]
      if {$options(-root)} {
	  set readPos [expr {$found+1}]
      } else {
	  set raw [string replace $raw 0 $found]
      }
      set decoded
  }

  proc string-or args {
      foreach i $args {
	  if {$i ne ""} {return $i}
      }
  }
}

