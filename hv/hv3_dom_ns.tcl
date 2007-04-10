namespace eval hv3 { set {version($Id: hv3_dom_ns.tcl,v 1.2 2007/04/10 16:22:09 danielk1977 Exp $)} 1 }

#---------------------------------
# List of DOM objects in this file:
#
#     Navigator
#     Window
#     Location
#
namespace eval ::hv3::dom {
  proc getNSClassList {} {
    list Navigator Location Window
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
::hv3::dom::type Location {} {

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

#-------------------------------------------------------------------------
# "Window" DOM object.
#
#     Window.setTimeout()
#     Window.clearTimeout()
#
#     Window.document
#     Window.navigator
#     Window.Image
#
#     Window.parent
#     Window.top
#     Window.self
#     Window.window
#
# Mixes in the Node interface.
#
::hv3::dom::type Window EventTarget {
  variable myHv3
  variable mySee

  dom_snit { 
    option -hv3 -default ""
    option -see -default ""
  }

  #-----------------------------------------------------------------------
  # Property implementations:
  # 
  #     Window.document
  #
  # js_getobject document { ::hv3::dom::HTMLDocument %AUTO% $myDom $myHv3 }
  dom_get -cache document {
    list object [::hv3::DOM::HTMLDocument %AUTO% $myDom -hv3 $options(-hv3)]
  }

  #-----------------------------------------------------------------------
  # The "Image" property object. This is so that scripts can
  # do the following:
  #
  #     img = new Image();
  #
  dom_get -cache Image {
    set cons [mymethod newRequest]
    list object [::hv3::JavascriptObject %AUTO% $myDom -construct $cons]
  }
  dom_snit {
    method newImage {args} {
      set node [$myHv3 fragment "<img>"]
      list object [$myDom node_to_dom $node]
    }
  }

  #-----------------------------------------------------------------------
  # The "XMLHttpRequest" property object. This is so that scripts can
  # do the following:
  #
  #     request = new XMLHttpRequest();
  #
  dom_get -cache XMLHttpRequest {
    set cons [mymethod newRequest]
    list object [::hv3::JavascriptObject %AUTO% $myDom -construct $cons]
  }
  dom_snit {
    method newRequest {args} {
      list object [::hv3::dom::XMLHttpRequest %AUTO% $myDom $myHv3]
    }
  }

  dom_get -cache Node {
    set obj [::hv3::DOM::NodePrototype %AUTO% $myDom]
    list object $obj
  }

  #-----------------------------------------------------------------------
  # The Window.location property (Gecko compatibility)
  #
  #     This is an alias for the document.location property.
  #
  dom_get location {
    set document [lindex [$self Get document] 1]
    $document Get location
  }
  dom_put location {value} {
    set document [lindex [$self Get document] 1]
    $document Put location $value
  }

  #-----------------------------------------------------------------------
  # The "navigator" object.
  #
  dom_get -cache navigator { 
    list object [::hv3::DOM::Navigator %AUTO% $myDom]
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
  dom_get parent { return [list object $self] }
  dom_get top    { return [list object $self] }
  dom_get self   { return [list object $self] }
  dom_get window { return [list object $self] }

  #-----------------------------------------------------------------------
  # Method Implementations: 
  #
  #     Window.setTimeout(code, delay) 
  #     Window.setInterval(code, delay) 
  #
  #     Window.clearTimeout(timeoutid)
  #     Window.clearInterval(timeoutid)
  #
  dom_snit {
    variable myTimerIds -array [list]
    variable myNextTimerId 0
  
    method SetTimer {isRepeat js_code js_delay} {
      set ms [format %.0f [lindex $js_delay 1]] 
      set code [lindex $js_code 1]
      $self CallTimer "" $isRepeat $ms $code
    }
  
    method ClearTimer {js_timerid} {
      set timerid [lindex $js_timerid 1]
      after cancel $myTimerIds($timerid)
      unset myTimerIds($timerid)
      return ""
    }
  
    method CallTimer {timerid isRepeat ms code} {
      if {$timerid ne ""} {
        unset myTimerIds($timerid)
        set rc [catch {$mySee eval -file setTimeout() $code} msg]
        $myDom Log "setTimeout()" $code $rc $msg
      }
  
      if {$timerid eq "" || $isRepeat} {
        if {$timerid eq ""} {set timerid [incr myNextTimerId]}
        set tclid [after $ms [mymethod CallTimer $timerid $isRepeat $ms $code]]
        set myTimerIds($timerid) $tclid
      }
  
      list string $timerid
    }
  }

  dom_call setInterval {THIS js_code js_delay} {
    $self SetTimer 1 $js_code $js_delay
  }
  dom_call setTimeout {THIS js_code js_delay} {
    $self SetTimer 0 $js_code $js_delay
  }
  dom_call clearTimeout  {THIS js_timerid} { $self ClearTimer $js_timerid }
  dom_call clearInterval {THIS js_timerid} { $self ClearTimer $js_timerid }
  #-----------------------------------------------------------------------

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
      list object event
    } else {
      list undefined
    }
  }

  dom_call -string jsputs {THIS args} {
    puts $args
  }

  dom_finalize {
    # Cancel any outstanding timers created by Window.setTimeout().
    #
    foreach timeoutid [array names myTimerIds] {
      after cancel $myTimerIds($timeoutid)
    }
    array unset myTimeoutIds
  }
}

