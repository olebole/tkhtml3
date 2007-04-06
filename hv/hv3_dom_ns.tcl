namespace eval hv3 { set {version($Id: hv3_dom_ns.tcl,v 1.1 2007/04/06 16:22:26 danielk1977 Exp $)} 1 }

#
#     Navigator
#     Window
#     Location
#

#-------------------------------------------------------------------------
# "Navigator" DOM object.
#
# Similar to the Gecko object documented here (some properties are missing):
#
#     http://developer.mozilla.org/en/docs/DOM:window.navigator
#
#     Navigator.appCodeName
#     Navigator.appName
#     Navigator.appVersion
#     Navigator.cookieEnabled
#     Navigator.language
#     Navigator.onLine
#     Navigator.oscpu
#     Navigator.platform
#     Navigator.product
#     Navigator.productSub
#     Navigator.securityPolicy
#     Navigator.userAgent
#     Navigator.vendor
#     Navigator.vendorSub
#
::hv3::dom::type Navigator {} {

  dom_get appCodeName    { list string "Mozilla" }
  dom_get appName        { list string "Netscape" }
  dom_get appVersion     { list number 4.0 }

  dom_get product        { list string "Hv3" }
  dom_get productSub     { list string "alpha" }
  dom_get vendor         { list string "tkhtml.tcl.tk" }
  dom_get vendorSub      { list string "alpha" }

  dom_get cookieEnabled  { list boolean 1    }
  dom_get language       { list string en-US }
  dom_get onLine         { list boolean 1    }
  dom_get securityPolicy { list string "" }

  dom_get userAgent { 
    # Use the user-agent that the http package is currently configured
    # with so that HTTP requests match the value returned by this property.
    list string [::http::config -useragent]
  }

  dom_get platform {
    # This will return something like "Linux i686".
    list string "$::tcl_platform(os) $::tcl_platform(machine)"
  }
  dom_get oscpu { $self Get platform }
}

#-------------------------------------------------------------------------
# DOM class Location
#
#     This is not based on any standard, but on the Gecko class of
#     the same name. Primary use is as the HTMLDocument.location 
#     and Window.location properties.
#
#          hash
#          host
#          hostname
#          href
#          pathname
#          port
#          protocol
#          search
#          assign(string)
#          reload(boolean)
#          replace(string)
#          toString()
#
#     http://developer.mozilla.org/en/docs/DOM:window.location
#
#
::hv3::::dom::type Location {} {

  dom_snit {
    option -hv3 -default ""

    # Default value.
    method DefaultValue {} { list string [$options(-hv3) location] }
  }


  #---------------------------------------------------------------------
  # Properties:
  #
  #     Todo: Writing to properties is not yet implemented.
  #
  dom_get hostname {
    set auth [$options(-hv3) uri cget -authority]
    set hostname ""
    regexp {^([^:]*)} -> hostname
    list string $hostname
  }
  dom_get port {
    set auth [$options(-hv3) uri cget -authority]
    set port ""
    regexp {:(.*)$} -> port
    list string $port
  }
  dom_get host     { list string [$options(-hv3) uri cget -authority] }
  dom_get href     { list string [$options(-hv3) uri get] }
  dom_get pathname { list string [$options(-hv3) uri cget -path] }
  dom_get protocol { list string [$options(-hv3) uri cget -scheme]: }
  dom_get search   { 
    set query [$options(-hv3) uri cget -query]
    set str ""
    if {$query ne ""} {set str "?$query"}
    list string $str
  }
  dom_get hash   { 
    set fragment [$options(-hv3) uri cget -fragment]
    set str ""
    if {$query ne ""} {set str "#$fragment"}
    list string $str
  }

  #---------------------------------------------------------------------
  # Methods:
  #
  dom_snit {
    method Location_assign {uri} { $options(-hv3) goto $uri }
  }
  dom_call -string assign  {THIS uri} { $self Location_assign $uri }

  dom_call -string replace {THIS uri} { $options(-hv3) goto $uri -nosave }
  dom_call -string reload  {THIS force} { 
    if {![string is boolean $force]} { error "Bad boolean arg: $force" }
    set cc normal
    if {$force} { set cc no-cache }
    $options(-hv3) goto [$options(-hv3) location] -nosave 
  }
  dom_call toString {THIS} { $self DefaultValue }
}

