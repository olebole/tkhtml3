namespace eval hv3 { set {version($Id: hv3_dom_ns.tcl,v 1.21 2007/07/22 06:45:49 danielk1977 Exp $)} 1 }

#---------------------------------
# List of DOM objects in this file:
#
#     Navigator
#     Window
#     Location
#     History
#     Screen
#
namespace eval ::hv3::dom {
  proc getNSClassList {} {
    list Navigator Location Window History Screen
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
  dom_get appVersion     { list string "4.0 (Hv3)" }

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

  # I don't even know what this does. All I know is that if this method
  # is not implemented wikipedia thinks we are KHTML and serves up a
  # special stylesheet. The following page from wikipedia has some
  # good information on why that is a bad thing to do:
  #
  #    http://en.wikipedia.org/wiki/Browser_sniffing
  #
  # On the other hand, if I don't know what "taint" is, it's probably
  # not enabled. Ok. All is forgiven wikipedia, we can be friends again.
  #
  dom_call taintEnabled {THIS} { list boolean false }
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

  dom_get href { 
    list string [$myHv3 uri get] 
  }
  dom_put -string href value { 
    Location_assign $myHv3 $value 
  }

  #---------------------------------------------------------------------
  # Methods:
  #
  dom_call -string assign  {THIS uri} { Location_assign $myHv3 $uri }
  dom_call -string replace {THIS uri} { $myHv3 goto $uri -nosave }
  dom_call -string reload  {THIS force} { 
    if {![string is boolean $force]} { error "Bad boolean arg: $force" }
    set cc normal
    if {$force} { set cc no-cache }
    $myHv3 goto [$myHv3 location] -nosave 
  }
  dom_call toString {THIS} { eval [SELF] DefaultValue }
}
namespace eval ::hv3::DOM {
  proc Location_assign {hv3 loc} {
    set f [winfo parent $hv3]

    # TODO: When assigning a URI from javascript, resolve it relative 
    # to the top-level document... This has got to be wrong... Maybe
    # it is supposed to be the window object in scope...
    #
    # Test case in jsunit source: "jsunit/tests/jsUnitTestLoadData.html"
    #
    set top [$f top_frame]
    set loc [[$top hv3] resolve_uri $loc]
    $hv3 goto $loc
  }
}

#-------------------------------------------------------------------------
# "Window" DOM object.
#
::hv3::dom2::stateless Window {} {

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
  dom_construct Image {THIS args} {
    set w ""
    set h ""
    if {[llength $args] > 0} {
      set w " width=[lindex $args 0 1]"
    }
    if {[llength $args] > 1} {
      set h " height=[lindex $args 0 1]"
    }
    set node [$myHv3 fragment "<img${w}${h}>"]
    list object [$myDom node_to_dom $node]
  }

  #-----------------------------------------------------------------------
  # The "XMLHttpRequest" property object. This is so that scripts can
  # do the following:
  #
  #     request = new XMLHttpRequest();
  #
  dom_construct XMLHttpRequest {THIS args} {
    ::hv3::dom::newXMLHttpRequest $myDom $myHv3
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
    list cache transient [list ::hv3::DOM::Navigator $myDom]
  }

  #-----------------------------------------------------------------------
  # The "history" object.
  #
  dom_get history { 
    list object [list ::hv3::DOM::History $myDom $myHv3]
  }

  #-----------------------------------------------------------------------
  # The "screen" object.
  #
  dom_get screen { 
    list object [list ::hv3::DOM::Screen $myDom $myHv3]
  }

  #-----------------------------------------------------------------------
  # The "parent" property. This should: 
  #
  #     "Returns a reference to the parent of the current window or subframe.
  #      If a window does not have a parent, its parent property is a reference
  #      to itself."
  #
  dom_get parent {
    set frame [winfo parent $myHv3]
    set parent [$frame parent_frame]
    if {$parent eq ""} {set parent $frame}
    list object [$myDom hv3_to_window [$parent hv3]]
  }
  dom_get top { 
    set frame [winfo parent $myHv3]
    set top [$frame top_frame]
    list object [$myDom hv3_to_window [$top hv3]]
  }
  dom_get self   { return [list object [SELF]] }
  dom_get window { return [list object [SELF]] }

  dom_get frames {
    list object [list ::hv3::DOM::FramesList $myDom [winfo parent $myHv3]]
  }

  #-----------------------------------------------------------------------
  # The alert() and confirm() methods.
  #
  dom_call -string alert {THIS msg} {
    tk_dialog .alert "Super Dialog Alert!" $msg "" 0 OK
    return ""
  }
  dom_call -string confirm {THIS msg} {
    set i [tk_dialog .alert "Super Dialog Alert!" $msg "" 0 OK Cancel]
    list boolean [expr {$i ? 0 : 1}]
  }

  dom_events { list }

  # Pass any request for an unknown property to the FramesList object.
  #
  # TODO: I don't know about this. It seems to be required for JsUnit,
  # which has code like:
  #
  #     top.testContainer.document.getElementById(...);
  #
  # where testContainer is the name of a child frame.
  #
if 0 {
  dom_get * { 
    ::hv3::DOM::FramesList $myDom [winfo parent $myHv3] Get $property
  }
}

  dom_call -string jsputs {THIS args} {
    puts $args
  }

  dom_get hv3_bookmarks {
    set frame [winfo parent $myHv3]
    set top [$frame top_frame]
    if {[string first home: [[$frame hv3] uri get]] == 0} {
      list object [list ::hv3::DOM::Bookmarks $myDom ::hv3::the_bookmark_manager]
    } else {
      list
    }
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

#-------------------------------------------------------------------------
# "Screen" DOM object.
#
#     http://developer.mozilla.org/en/docs/DOM:window.screen
#
# 
::hv3::dom2::stateless Screen {} {
  dom_parameter myHv3

  dom_get colorDepth  { list number [winfo screendepth $myHv3] }
  dom_get pixelDepth  { list number [winfo screendepth $myHv3] }

  dom_get width       { list number [winfo screenwidth $myHv3] }
  dom_get height      { list number [winfo screenheight $myHv3] }
  dom_get availWidth  { list number [winfo screenwidth $myHv3] }
  dom_get availHeight { list number [winfo screenheight $myHv3] }

  dom_get availTop    { list number 0}
  dom_get availLeft   { list number 0}
  dom_get top         { list number 0}
  dom_get left        { list number 0}
}

