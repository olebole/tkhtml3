namespace eval hv3 { set {version($Id: hv3_dom_ns.tcl,v 1.13 2007/06/03 10:35:19 danielk1977 Exp $)} 1 }

#---------------------------------
# List of DOM objects in this file:
#
#     Navigator
#     Window
#     Location
#     History
#
namespace eval ::hv3::dom {
  proc getNSClassList {} {
    list Navigator Location Window History
  }
}

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
#     Navigator.javaEnabled()
#
::hv3::dom2::stateless Navigator {} {

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
  dom_get oscpu { eval [SELF] Get platform }

  # No. Absolutely not. Never going to happen.
  dom_call javaEnabled {THIS} { list boolean false }
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
::hv3::dom2::stateless Location {} {

  dom_parameter myHv3

  dom_default_value {
    list string [$myHv3 location]
  }

  #---------------------------------------------------------------------
  # Properties:
  #
  #     Todo: Writing to properties is not yet implemented.
  #
  dom_get hostname {
    set auth [$myHv3 uri cget -authority]
    set hostname ""
    regexp {^([^:]*)} -> hostname
    list string $hostname
  }
  dom_get port {
    set auth [$myHv3 uri cget -authority]
    set port ""
    regexp {:(.*)$} -> port
    list string $port
  }
  dom_get host     { list string [$myHv3 uri cget -authority] }
  dom_get href     { list string [$myHv3 uri get] }
  dom_get pathname { list string [$myHv3 uri cget -path] }
  dom_get protocol { list string [$myHv3 uri cget -scheme]: }
  dom_get search   { 
    set query [$myHv3 uri cget -query]
    set str ""
    if {$query ne ""} {set str "?$query"}
    list string $str
  }
  dom_get hash   { 
    set fragment [$myHv3 uri cget -fragment]
    set str ""
    if {$fragment ne ""} {set str "#$fragment"}
    list string $str
  }

  #---------------------------------------------------------------------
  # Methods:
  #
  dom_call -string assign  {THIS uri} { $myHv3 goto $uri }
  dom_call -string replace {THIS uri} { $myHv3 goto $uri -nosave }
  dom_call -string reload  {THIS force} { 
    if {![string is boolean $force]} { error "Bad boolean arg: $force" }
    set cc normal
    if {$force} { set cc no-cache }
    $myHv3 goto [$myHv3 location] -nosave 
  }
  dom_call toString {THIS} { eval [SELF] DefaultValue }
}

#-------------------------------------------------------------------------
# "Window" DOM object.
#
set BaseList {EventTarget}
::hv3::dom2::stateless Window $BaseList {

  dom_parameter myHv3

  dom_call_todo scrollBy

  #-----------------------------------------------------------------------
  # Property implementations:
  # 
  #     Window.document
  #
  dom_get document {
    list object [list ::hv3::DOM::HTMLDocument $myDom $myHv3]
  }

  #-----------------------------------------------------------------------
  # The "Image" property object. This is so that scripts can
  # do the following:
  #
  #     img = new Image();
  #
  dom_construct Image {THIS} {
    set node [$myHv3 fragment "<img>"]
    list object [$myDom node_to_dom $node]
  }

  #-----------------------------------------------------------------------
  # The "XMLHttpRequest" property object. This is so that scripts can
  # do the following:
  #
  #     request = new XMLHttpRequest();
  #
  # dom_todo XMLHttpRequest
  dom_construct XMLHttpRequest {THIS args} {
    list object [::hv3::DOM::XMLHttpRequest %AUTO% $myDom -hv3 $myHv3]
  }

  dom_get Node {
    set obj [list ::hv3::DOM::Node $myDom]
    list object $obj
  }

  #-----------------------------------------------------------------------
  # The Window.location property (Gecko compatibility)
  #
  #     This is an alias for the document.location property.
  #
  dom_get location {
    set document [lindex [eval [SELF] Get document] 1]
    eval $document Get location
  }
  dom_put location {value} {
    set document [lindex [eval [SELF] Get document] 1]
    eval $document Put location [list $value]
  }

  #-----------------------------------------------------------------------
  # The "navigator" object.
  #
  dom_get navigator { 
    list object [list ::hv3::DOM::Navigator $myDom]
  }

  #-----------------------------------------------------------------------
  # The "history" object.
  #
  dom_get history { 
    list object [list ::hv3::DOM::History $myDom $myHv3]
  }

  #-----------------------------------------------------------------------
  # The "parent" property. This should: 
  #
  #     "Returns a reference to the parent of the current window or subframe.
  #      If a window does not have a parent, its parent property is a reference
  #      to itself."
  #
  # For now, this always returns a "reference to itself".
  #
  dom_get parent { return [list object [SELF]] }
  dom_get top    { return [list object [SELF]] }
  dom_get self   { return [list object [SELF]] }
  dom_get window { return [list object [SELF]] }

  #-----------------------------------------------------------------------
  # The "alert()" method.
  #
  dom_call -string alert {THIS msg} {
    tk_dialog .alert "Super Dialog Alert!" $msg "" 0 OK
    return ""
  }

  #-----------------------------------------------------------------------
  # The event property.
  #
  dom_get event {
    set event [$myDom getWindowEvent]
    if {$event ne ""} {
      list object $event
    } else {
      list undefined
    }
  }

  dom_call -string jsputs {THIS args} {
    puts $args
  }
}

#-------------------------------------------------------------------------
# "History" DOM object.
#
#     http://developer.mozilla.org/en/docs/DOM:window.history
#     http://www.w3schools.com/htmldom/dom_obj_history.asp
#
# Right now this is a placeholder. It does not work.
# 
::hv3::dom2::stateless History {} {
  dom_parameter myHv3

  dom_get length   { list number 0 }
  dom_call back    {THIS}     { }
  dom_call forward {THIS}     { }
  dom_call go      {THIS arg} { }
}

