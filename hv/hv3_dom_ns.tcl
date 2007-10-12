namespace eval hv3 { set {version($Id: hv3_dom_ns.tcl,v 1.28 2007/10/12 06:12:59 danielk1977 Exp $)} 1 }

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
    list Navigator History Screen
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

  # Fairly obviously, this is an Hv3 specific property.
  dom_get hv3_version    { list string [::hv3::hv3_version] }

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
::hv3::dom2::stateless2 Location {
  -- One location object is allocated for each [Ref Window] object in the
  -- system. It contains the URI of the current document. Writing to
  -- this object causes the window to load the new URI. The location
  -- object is available via either of the following two global
  -- references:
  --
  -- <PRE>
  -- window.location
  -- window.document.location
  -- </PRE>
  --
  -- This is not based on any standard, but on the Gecko class of
  -- the same name:
  --
  -- <P class=refs>
  -- [Ref http://developer.mozilla.org/en/docs/DOM:window.location]
  -- [Ref http://developer.mozilla.org/en/docs/DOM:document.location]
  XX

  dom_parameter myHv3
  dom_default_value { list string [$myHv3 uri get] }

  #---------------------------------------------------------------------
  # Properties:
  #
  #     Todo: Writing to properties is not yet implemented.
  #

  -- The authority part of the URI, not including any 
  -- {<code>:&lt;port-number&gt;</code>} section.
  dom_get hostname {
    set auth [$myHv3 uri authority]
    set hostname ""
    regexp {^([^:]*)} -> hostname
    list string $hostname
  }

  -- The port-number specified as part of the URI authority, or an empty
  -- string if no port-number was specified.
  dom_get port {
    set auth [$myHv3 uri authority]
    set port ""
    regexp {:(.*)$} -> port
    list string $port
  }

  -- The URI authority.
  dom_get host     { list string [$myHv3 uri authority] }

  -- The URI path.
  dom_get pathname { list string [$myHv3 uri path] }

  -- The URI scheme, including the colon following the scheme name. For 
  -- example, <code>"http:"</code> or <code>"https:"</code>.
  dom_get protocol { list string [$myHv3 uri scheme]: }

  -- The URI query, including the '?' character.
  dom_get search   { 
    set query [$myHv3 uri query]
    set str ""
    if {$query ne ""} {set str "?$query"}
    list string $str
  }

  -- The URI fragment, including the '#' character.
  dom_get hash   { 
    set fragment [$myHv3 uri fragment]
    set str ""
    if {$fragment ne ""} {set str "#$fragment"}
    list string $str
  }

  -- The entire URI as a string.
  dom_get href { list string [$myHv3 uri get] }
  dom_put -string href value { 
    Location_assign $myHv3 $value 
  }

  #---------------------------------------------------------------------
  # Methods:
  #

  -- Set the location to the supplied URI. The window loads the document
  -- at the specified URI.
  dom_call -string assign  {THIS uri} { Location_assign $myHv3 $uri }

  -- Set the location to the supplied URI. The window loads the document
  -- at the specified URI. The difference between this method and the
  -- assign() method is that this method does not add a new entry to
  -- the browser Back/Forward list - it replaces the current one.
  dom_call -string replace {THIS uri} { $myHv3 goto $uri -nosave }

  -- Refresh the current URI. If the boolean <I>force</I> argument is true,
  -- reload the document from the server, bypassing any caches. If <I>force</I>
  -- is false, normal HTTP caching rules apply.
  dom_call -string reload  {THIS force} { 
    if {![string is boolean $force]} { error "Bad boolean arg: $force" }
    set cc normal
    if {$force} { set cc no-cache }
    $myHv3 goto [$myHv3 location] -nosave 
  }

  -- Returns the same value as reading the <I>href</I> property.
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
::hv3::dom2::stateless2 Window {

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
    list object [::hv3::dom::wrapWidgetNode $myDom $node]
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
    set obj [list ::hv3::DOM::NodePrototype $myDom]
    list object $obj
  }

  -- Return the [Ref Location] object associated with this window. This 
  -- property can be set to the string representation of a URI, causing 
  -- the browser to load the document at the location specified. i.e. 
  -- 
  -- <PRE>
  --   window.location = {"http://tkhtml.tcl.tk/hv3.html"} 
  -- </PRE>
  dom_get location {
    list object [list ::hv3::DOM::Location $myDom $myHv3]
    list object $obj
  }
  dom_put location {value} {
    Location_assign $myHv3 $value
  }

  -- A reference to the [Ref Navigator] object.
  --
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

  -- Returns a reference to the parent of the current window or subframe.
  -- If the window does not have a parent, then a reference to the window
  -- itself is returned.
  --
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

  -- Pop up a modal dialog box with a single button - \"OK\". The <i>msg</i>
  -- argument is displayed in the dialog. This function does not return
  -- until the user dismisses the dialog.
  --
  -- Returns null.
  --
  dom_call -string alert {THIS msg} {
    tk_dialog .alert "Super Dialog Alert!" $msg "" 0 OK
    return ""
  }

  -- Pop up a modal dialog box with two buttons: - \"OK\" and \"Cancel\". 
  -- The <i>msg</i> argument is displayed in the dialog. This function 
  -- does not return until the user dismisses the dialog by clicking one
  -- of the buttons.
  --
  -- If the user chooses the \"OK\" button, true is returned. If the
  -- user chooses \"Cancel\", false is returned.
  --
  dom_call -string confirm {THIS msg} {
    set i [tk_dialog .alert "Super Dialog Alert!" $msg "" 0 OK Cancel]
    list boolean [expr {$i ? 0 : 1}]
  }

  -- The current height of the browser window in pixels (the area 
  -- available for displaying HTML documents), including the horizontal 
  -- scrollbar if one is displayed.
  --
  -- <P class=refs>
  -- [Ref http://developer.mozilla.org/en/docs/DOM:window.innerWidth]
  -- [Ref http://tkhtml.tcl.tk/cvstrac/tktview?tn=175]
  -- [Ref http://www.howtocreate.co.uk/tutorials/javascript/browserwindow]
  dom_get innerHeight { list number [winfo height $myHv3] }

  -- The current width of the browser window in pixels (the area 
  -- available for displaying HTML documents), including the vertical 
  -- scrollbar if one is displayed.
  --
  --     http://developer.mozilla.org/en/docs/DOM:window.innerHeight
  --
  dom_get innerWidth  { list number [winfo width $myHv3] }

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

if 0 {
  dom_call -string jsputs {THIS args} {
    puts $args
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

