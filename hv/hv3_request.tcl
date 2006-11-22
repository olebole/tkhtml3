namespace eval hv3 { set {version($Id: hv3_request.tcl,v 1.1 2006/11/22 07:34:24 danielk1977 Exp $)} 1 }
#--------------------------------------------------------------------------
# Class ::hv3::request
#
#     Instances of this class are used to interface between the protocol
#     implementation and the hv3 widget.
#
# METHODS:
#
#     append DATA
#     finish
#     fail
#     authority
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

  # Expected size of the resource being requested. This is used
  # for displaying a progress bar when saving remote resources
  # to the local filesystem (aka downloadin').
  #
  option -expectedsize -default ""

  # Callbacks configured by the requestor.
  #
  option -incrscript   -default ""
  option -finscript    -default ""
  option -failscript   -default ""


  #----------------------------
  # IMPLEMENTATION STARTS HERE

  variable myData ""
  variable myChunksize 2048

  # Constructor and destructor
  constructor {args} {
    $self configurelist $args
  }
  destructor  {}

  # This method is called each time the -header option is set. This
  # is where the locally handled HTTP headers (see comments above the
  # -header option) are handled.
  #
  method ConfigureHeader {name option_value} {
    set options($name) $option_value
    foreach {name value} $option_value {
      switch -- $name {
        Set-Cookie {
          ::hv3::the_cookie_manager SetCookie $options(-uri) $value
        }
        Content-Type {
          if {[set idx [string first ";" $value]] >= 0} {
            set options(-mimetype) [string range $value 0 [expr $idx-1]]
          }
        }
        Content-Length {
          set options(-expectedsize) $value
        }
      }
    }
  }

  # Query interface used by protocol implementations
  method authority {} {
    set obj [::hv3::uri %AUTO% $options(-uri)]
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
    $self destroy
  }
}

