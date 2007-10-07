namespace eval hv3 { set {version($Id: hv3_request.tcl,v 1.11 2007/10/07 16:30:08 danielk1977 Exp $)} 1 }

#--------------------------------------------------------------------------
# This file contains the implementation of two types used by hv3:
#
#     ::hv3::download
#     ::hv3::uri
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

  # The hv3 widget that issued this request. This is only used by the
  # handler for home:// uris.
  #
  option -hv3      -default ""

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


  # END OF OPTIONS
  #----------------------------

  variable myData ""
  variable myChunksize 2048

  # Destroy-hook scripts configured using the [destroy_hook] method.
  variable myDestroyHookList [list]

  # Constructor and destructor
  constructor {args} {
    $self configurelist $args
  }

  destructor  {
    foreach hook $myDestroyHookList {
      eval $hook
    }
  }

  # Add a script to be called just before the object is destroyed. See
  # description above.
  #
  method destroy_hook {script} {
    lappend myDestroyHookList $script
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
          if {[set idx [string first ";" $value]] >= 0} {
            set options(-mimetype) [string range $value 0 [expr $idx-1]]
          }
        }
        content-length {
          set options(-expectedsize) $value
        }
      }
    }
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
  method append {data} {
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
    eval [linsert $options(-finscript) end $myData] 
  }

  method fail {} {puts FAIL}
}

