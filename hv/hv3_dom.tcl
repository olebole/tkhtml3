namespace eval hv3 { set {version($Id: hv3_dom.tcl,v 1.27 2007/02/04 16:19:51 danielk1977 Exp $)} 1 }

#--------------------------------------------------------------------------
# Global interfaces in this file:
#
#     [::hv3::dom::have_scripting]
#         This method returns true if scripting is available, otherwise 
#         false. Scripting is available if the command [::see::interp]
#         is available (see file hv3see.c).
#
#     [::hv3::dom::use_scripting]
#         This method returns the logical OR of [::hv3::dom::use_scripting] 
#         and $::hv3::dom::use_scripting_option.
#
#     $::hv3::dom_use_scripting
#         Variable used by [::hv3::dom::use_scripting].
#
# Also, type ::hv3::dom. Summary:
#
#     set dom [::hv3::dom %AUTO% HV3]
#
#     $dom script ATTR JAVASCRIPT-CODE
#     $dom javascript JAVASCRIPT-CODE
#     $dom event EVENT-TYPE NODE-HANDLE
#
#     $dom reset
#
#     $dom destroy
#

# Events module:
#
#     class EventTarget
#     class EventListener (in ECMAScript is just a function reference)
#     class Event
#


#--------------------------------------------------------------------------


#--------------------------------------------------------------------------
# Internals:
#
# This file contains the following types:
#
# ::hv3::JavascriptObject
# ::hv3::dom
#
# DOM1 Standard classes:
#     ::hv3::dom::HTMLDocument
#     ::hv3::dom::HTMLCollection
#     ::hv3::dom::HTMLElement
#     ::hv3::dom::Text
#
#     ::hv3::dom::HTMLImageElement
#     ::hv3::dom::HTMLFormElement
#
# Defacto Standards:
#     ::hv3::dom::Navigator
#     ::hv3::dom::Window
#     ::hv3::dom::Location
#

package require snit


#--------------------------------------------------------------------------
# ::hv3::JavascriptObject
# 
# This type contains various convenience code to help with development 
# of the DOM bindings for Hv3.
#
snit::type ::hv3::JavascriptObject {

  # If this object is callable, then this option is set to a script to
  # invoke when it is called. Appended to the script before it is passed
  # to [eval] are the $this object and the script arguments.
  #
  # If this object is an empty string, then this object is not callable.
  #
  option -call -default ""

  # If this boolean option is true, then transform all script arguments 
  # to the -call script (except the "this" argument) by calling 
  # [$mySee tostring] on them before evaluating -call.
  #
  option -callwithstrings -default 0

  # Similar to -call, but for construction (i.e. "new Object()") calls.
  #
  option -construct -default ""

  # ::hv3::dom object that owns this object.
  #
  variable myDom ""
  method hv3 {} {$myDom hv3}
  method see {} {$myDom see}
  method dom {} {set myDom}

  # Reference to javascript object to store properties.
  variable myNative

  constructor {dom args} {
    set myDom $dom
    $self configurelist $args
  }

  destructor {
    # Todo: Finalize $myNative.
  }

  method Get {property} {
    return ""
  }

  method Put {property value} {
    return "native"
  }

  method ToString {js_value} {
    switch -- [lindex $js_value 0] {
      undefined {return "undefined"}
      null      {return "null"}
      boolean   {return [lindex $js_value 1]}
      number    {return [lindex $js_value 1]}
      string    {return [lindex $js_value 1]}
      object    {
        set val [eval [$self see] [lindex $js_value 1] DefaultValue]
        if {[lindex $val 1] eq "object"} {error "DefaultValue is object"}
        return [$self ToString $val]
      }
    }
  }

  method Call {THIS args} {
    if {$options(-call) ne ""} {
      set A $args
      if {$options(-callwithstrings)} {
        set see [$myDom see]
        set A [list]
        # foreach a $args { lappend A [$see tostring $a] }
        foreach a $args { lappend A [$self ToString $a] }
      }
      eval $options(-call) [list $THIS] $A
    } else {
      error "Cannot call this object"
    }
  }

  method Construct {args} {
    if {$options(-construct) ne ""} {
      eval $options(-construct) $args
    } else {
      error "Cannot call this as a constructor"
    }
  }

  method Finalize {} {
    # puts "Unimplemented Finalized method"
  }
}

#-------------------------------------------------------------------------
# js_XXX snit::macros used to make creating DOM objects easier.
#
#     js_init
#
#     js_get
#     js_getobject
#
#     js_put
#
#     js_call
#     js_scall
#
#     js_finish
#
::snit::macro js_init {arglist body} {
  namespace eval ::hv3::dom {
    set js_get_methods [list]
    set js_put_methods [list]
  }
  component myJavascriptParent
  delegate method * to myJavascriptParent

  method js_initialize $arglist $body
}

::snit::macro js_get {varname code} {
  lappend ::hv3::dom::js_get_methods $varname get_$varname
  if {$varname eq "*"} {
    method get_$varname {property} $code
  } else {
    method get_$varname {} $code
  }
}
::snit::macro js_getobject {varname code} {
  lappend ::hv3::dom::js_get_methods $varname get_$varname
  method get_$varname {} "\$self GetObject $varname {$code}"
}
::snit::macro js_put {varname argname code} {
  lappend ::hv3::dom::js_put_methods $varname put_$varname
  method put_$varname $argname $code
}
::snit::macro js_call {methodname arglist code} {
  method call_$methodname $arglist $code
  lappend ::hv3::dom::js_get_methods $methodname get_$methodname
  method get_$methodname {} "\$self GetMethod 0 $methodname"
}
::snit::macro js_scall {methodname arglist code} {
  method call_$methodname $arglist $code
  lappend ::hv3::dom::js_get_methods $methodname get_$methodname
  method get_$methodname {} "\$self GetMethod 1 $methodname"
}

::snit::macro js_finish {body} {
  typevariable get_methods  -array $::hv3::dom::js_get_methods
  typevariable put_methods  -array $::hv3::dom::js_put_methods

  variable myCallMethods -array [list]
  variable myObjectProperties -array [list]

  method Get {property} {
    if {[info exists get_methods($property)]} {
      return [$self $get_methods($property)]
    }
    if {[info exists get_methods(*)]} {
      set res [$self $get_methods(*) $property]
      if {$res ne ""} {return $res}
    }
    return [$myJavascriptParent Get $property]
  }

  method GetMethod {stringify methodname} {
    if {![info exists myCallMethods($methodname)]} {
      set dom [$self dom]
      set myCallMethods($methodname) [
          ::hv3::JavascriptObject %AUTO% $dom \
              -call [mymethod call_$methodname] -callwithstrings $stringify
      ]
    }
    return [list object $myCallMethods($methodname)]
  }

  method GetObject {property code} {
    if {![info exists myObjectProperties($property)]} {
      set myObjectProperties($property) [eval $code]
    }
    return [list object $myObjectProperties($property)]
  }

  method Put {property value} {
    if {[info exists put_methods($property)]} {
      return [$self $put_methods($property) $value]
    }
    if {[info exists get_methods($property)]} {
      error "Cannot set read-only property: $property"
    }
    return [$myJavascriptParent Put $property $value]
  }

  constructor {dom args} {
    set myJavascriptParent ""
    eval $self js_initialize $dom $args
    if {$myJavascriptParent eq ""} {
      set myJavascriptParent [::hv3::JavascriptObject %AUTO% $dom]
    }
  }

  method js_finalize {} $body
  destructor {
    $self js_finalize
    foreach key [array names myCallMethods] {
      $myCallMethods($key) destroy
    }
    foreach key [array names myObjectProperties] {
      $myObjectProperties($key) destroy
    }
  }

  method HasProperty {property} {
    if {[info exists get_methods($property)]} {
      return 1
    }
    return 0
  }

  method Enumerator {} {
    return [array names get_methods]
  }
}

::snit::macro js_getput_attribute {property {attribute ""}} {
   set G "list string \[\[\$self node\] attribute -default {} $attribute\]"
   set P "\[\$self node\] attribute $property \[lindex \$v 1\]"
   js_get $property $G
   js_put $property v $P
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
::snit::type ::hv3::dom::Location {

  variable myHv3

  js_init {dom hv3} {
    set myHv3 $hv3
  }

  # Default value.
  method DefaultValue {} { list string [$myHv3 location] }

  #---------------------------------------------------------------------
  # Properties:
  #
  #     Todo: Writing to properties is not yet implemented.
  #
  js_get hostname {
    set auth [$myHv3 uri cget -authority]
    set hostname ""
    regexp {^([^:]*)} -> hostname
    list string $hostname
  }
  js_get port {
    set auth [$myHv3 uri cget -authority]
    set port ""
    regexp {:(.*)$} -> port
    list string $port
  }
  js_get host     { list string [$myHv3 uri cget -authority] }
  js_get href     { list string [$myHv3 uri get] }
  js_get pathname { list string [$myHv3 uri cget -path] }
  js_get protocol { list string [$myHv3 uri cget -scheme]: }
  js_get search   { 
    set query [$myHv3 uri cget -query]
    set str ""
    if {$query ne ""} {set str "?$query"}
    list string $str
  }
  js_get hash   { 
    set fragment [$myHv3 uri cget -fragment]
    set str ""
    if {$query ne ""} {set str "#$fragment"}
    list string $str
  }

  #---------------------------------------------------------------------
  # Methods:
  #
  js_scall assign  {THIS uri} { $myHv3 goto $uri }
  js_scall replace {THIS uri} { $myHv3 goto $uri -nosave }
  js_scall reload  {THIS force} { 
    if {![string is boolean $force]} { error "Bad boolean arg: $force" }
    set cc normal
    if {$force} { set cc no-cache }
    $myHv3 goto [$myHv3 location] -nosave 
  }
  js_call toString {THIS} { $self DefaultValue }

  js_finish {}
}



#-------------------------------------------------------------------------
# Snit type for "Navigator" DOM object.
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
snit::type ::hv3::dom::Navigator {

  js_init {dom} {}

  js_get appCodeName    { list string "Mozilla" }
  js_get appName        { list string "Netscape" }
  js_get appVersion     { list number 4.0 }

  js_get product        { list string "Hv3" }
  js_get productSub     { list string "alpha" }
  js_get vendor         { list string "tkhtml.tcl.tk" }
  js_get vendorSub      { list string "alpha" }

  js_get cookieEnabled  { list boolean 1    }
  js_get language       { list string en-US }
  js_get onLine         { list boolean 1    }
  js_get securityPolicy { list string "" }

  js_get userAgent { 
    # Use the user-agent that the http package is currently configured
    # with so that HTTP requests match the value returned by this property.
    list string [::http::config -useragent]
  }

  js_get platform {
    # This will return something like "Linux i686".
    list string "$::tcl_platform(os) $::tcl_platform(machine)"
  }
  js_get oscpu { $self get_platform }

  js_finish {}
}

#-------------------------------------------------------------------------
# Snit type for "Window" DOM object.
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
snit::type ::hv3::dom::Window {
  variable myHv3
  variable mySee

  js_init {dom see hv3} { 
    set mySee $see 
    set myHv3 $hv3 
  }

  #-----------------------------------------------------------------------
  # Property implementations:
  # 
  #     Window.document
  #
  # js_getobject document { ::hv3::dom::HTMLDocument %AUTO% [$myHv3 dom] $myHv3 }
  js_getobject document { 
    ::hv3::DOM::HTMLDocument %AUTO% [$myHv3 dom] -hv3 $myHv3 

    # ::hv3::dom::HTMLDocument %AUTO% [$myHv3 dom] $myHv3
  }

  #-----------------------------------------------------------------------
  # The "Image" property object. This is so that scripts can
  # do the following:
  #
  #     img = new Image();
  #
  js_getobject Image {
    ::hv3::JavascriptObject %AUTO% [$myHv3 dom] -construct [mymethod newImage]
  }
  method newImage {args} {
    set node [$myHv3 fragment "<img>"]
    list object [[$myHv3 dom] node_to_dom $node]
  }

  #-----------------------------------------------------------------------
  # The "XMLHttpRequest" property object. This is so that scripts can
  # do the following:
  #
  #     request = new XMLHttpRequest();
  #
  js_getobject XMLHttpRequest {
    ::hv3::JavascriptObject %AUTO% [$myHv3 dom] -construct [mymethod newRequest]
  }
  method newRequest {args} {
    list object [::hv3::dom::XMLHttpRequest %AUTO% [$myHv3 dom] $myHv3]
  }

  js_getobject Node {
    set obj [::hv3::DOM::NodePrototype %AUTO% [$myHv3 dom]]
  }

  #-----------------------------------------------------------------------
  # The Window.location property (Gecko compatibility)
  #
  #     This is an alias for the document.location property.
  #
  js_get location {
    set document [lindex [$self Get document] 1]
    $document Get location
  }
  js_put location {value} {
    set document [lindex [$self Get document] 1]
    $document Put location $value
  }

  #-----------------------------------------------------------------------
  # The "navigator" object.
  #
  js_getobject navigator { ::hv3::dom::Navigator %AUTO% [$self dom] }

  #-----------------------------------------------------------------------
  # The "parent" property. This should: 
  #
  #     "Returns a reference to the parent of the current window or subframe.
  #      If a window does not have a parent, its parent property is a reference
  #      to itself."
  #
  # For now, this always returns a "reference to itself".
  #
  js_get parent { return [list object $self] }
  js_get top    { return [list object $self] }
  js_get self   { return [list object $self] }
  js_get window { return [list object $self] }

  #-----------------------------------------------------------------------
  # Method Implementations: 
  #
  #     Window.setTimeout(code, delay) 
  #     Window.setInterval(code, delay) 
  #
  #     Window.clearTimeout(timeoutid)
  #     Window.clearInterval(timeoutid)
  #
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
      set rc [catch {$mySee eval $code} msg]
      [$myHv3 dom] Log "setTimeout()" $code $rc $msg
    }

    if {$timerid eq "" || $isRepeat} {
      if {$timerid eq ""} {set timerid [incr myNextTimerId]}
      set tclid [after $ms [mymethod CallTimer $timerid $isRepeat $ms $code]]
      set myTimerIds($timerid) $tclid
    }

    list string $timerid
  }

  js_call setInterval {THIS js_code js_delay} {
    $self SetTimer 1 $js_code $js_delay
  }
  js_call setTimeout {THIS js_code js_delay} {
    $self SetTimer 0 $js_code $js_delay
  }
  js_call clearTimeout  {THIS js_timerid} { $self ClearTimer $js_timerid }
  js_call clearInterval {THIS js_timerid} { $self ClearTimer $js_timerid }
  #-----------------------------------------------------------------------

  #-----------------------------------------------------------------------
  # The "alert()" method.
  #
  js_scall alert {THIS msg} {
    tk_dialog .alert "Super Dialog Alert!" $msg "" 0 OK
    return ""
  }

  #-----------------------------------------------------------------------
  # The event property.
  #
  js_get event {
    set event [[$myHv3 dom] getWindowEvent]
    if {$event ne ""} {
      list object event
    } else {
      list undefined
    }
  }


  #-----------------------------------------------------------------------
  # DOM level 0 events:
  #
  #     onload
  #     onunload
  #
  # Note: For a frameset document, the Window.onload and Window.onunload
  # properties may be set by the onload and onunload attributes of 
  # the <FRAMESET> element, not the <BODY> (as is currently assumed).
  #
  variable myCompiledEvents 0
  method CompileEvents {} {
    if {$myCompiledEvents} return
    set body [lindex [$myHv3 search body] 0]
    set onload_script [$body attribute -default "" onload]
    if {$onload_script ne ""} {
      set ref [$mySee function $onload_script]
      $myJavascriptParent Put onload [list object $ref]
    }
    set myCompiledEvents 1
  }
  js_get onload {
    $self CompileEvents
    $myJavascriptParent Get onload
  }
  js_put onload value {
    $self CompileEvents
    $myJavascriptParent Put onload $value
  }
  #-----------------------------------------------------------------------

  js_call jsputs {THIS args} {
    puts $args
  }

  js_finish {
    # Cancel any outstanding timers created by Window.setTimeout().
    #
    foreach timeoutid [array names myTimerIds] {
      after cancel $myTimerIds($timeoutid)
    }
    array unset myTimeoutIds
  }
}



#-------------------------------------------------------------------------
# Class ::hv3::dom
#
#     set dom [::hv3::dom %AUTO% $hv3]
#
#     $dom script ATTR SCRIPT
#     $dom javascript SCRIPT
#     $dom event EVENT NODE
#     $dom reset
#     $dom javascriptlog
#
#     destroy $dom
#
snit::type ::hv3::dom {
  variable mySee ""

  # Variable used to accumulate the arguments of document.write() and
  # document.writeln() invocations from within [script].
  #
  variable myWriteVar ""

  # Document window associated with this scripting environment.
  variable myHv3 ""

  # Global object.
  variable myWindow ""

  # Map from Tkhtml3 node-handle to corresponding DOM object.
  # Entries are added by the [::hv3::dom node_to_dom] method. The
  # array is cleared by the [::hv3::dom reset] method.
  #
  variable myNodeToDom -array [list]

  constructor {hv3 args} {
    set myHv3 $hv3

    # Mouse events:
    foreach e [list onclick onmouseout onmouseover \
        onmouseup onmousedown onmousemove ondblclick
    ] {
      # $myHv3 Subscribe $e [mymethod event $e]
    }

    $self reset
  }

  destructor { 
    catch {
      destroy [$self LogWindow]
    }
  }

  method reset {} {
    if {[::hv3::dom::have_scripting]} {

      # Delete the old interpreter and the various objects, if they exist.
      # They may not exist, if this is being called from within the
      # object constructor. 
      if {$mySee ne ""} {
        $mySee destroy
        set mySee ""

        # Delete all the DOM objects in the $myNodeToDom array.
        foreach key [array names myNodeToDom] {
          $myNodeToDom($key) destroy
        }
        array unset myNodeToDom

        # Destroy the toplevel object.
        $myWindow destroy
      }


      if {[::hv3::dom::use_scripting]} {
        # Set up the new interpreter with the global "Window" object.
        set mySee [::see::interp]
        set myWindow [::hv3::dom::Window %AUTO% $self $mySee $myHv3]
        $mySee global $myWindow 
      }

      $self LogReset
    }
  }

  # This method is called as a Tkhtml3 "script handler" for elements
  # of type <SCRIPT>. I.e. this should be registered with the html widget
  # as follows:
  #
  #     $htmlwidget handler script script [list $dom script]
  #
  # This is done externally, not by code within this type definition.
  # If scripting is not enabled in this browser, this method is a no-op.
  #
  method script {attr script} {
    if {[::hv3::dom::use_scripting] && $mySee ne ""} {
      $myHv3 write wait
      array set a $attr
      if {[info exists a(src)]} {
        set fulluri [$myHv3 resolve_uri $a(src)]
        set handle [::hv3::download %AUTO%             \
            -uri         $fulluri                      \
            -mimetype    text/javascript               \
            -cachecontrol normal                       \
        ]
        $handle configure -finscript [mymethod scriptCallback $attr $handle]
        $myHv3 makerequest $handle
        $self Log "Dispatched script request - $handle" "" "" ""
      } else {
        return [$self scriptCallback $attr "" $script]
      }
    }
    return ""
  }
  
  # If a <SCRIPT> element has a "src" attribute, then the [script]
  # method will have issued a GET request for it. This is the 
  # successful callback.
  method scriptCallback {attr downloadHandle script} {
    if {$downloadHandle ne ""} { 
      $downloadHandle destroy 
    }

    if {$::hv3::dom::reformat_scripts_option} {
     set script [::see::format $script]
    }

    set rc [catch {$mySee eval $script} msg]

    set attributes ""
    foreach {a v} $attr {
      append attributes " [$self Escape $a]=\"[$self Escape $v]\""
    }
    $self Log "<SCRIPT$attributes> $downloadHandle" $script $rc $msg

    $myHv3 write continue
  }

  method javascript {script} {
    set msg ""
    if {[::hv3::dom::use_scripting] && $mySee ne ""} {
      set rc [catch {$mySee eval $script} msg]
      $self Log "javascript:" $script $rc $msg
    }
    return $msg
  }

  # This method is called when one an event of type $event occurs on the
  # document node $node. Argument $event is one of the following:
  #
  #     onclick 
  #     onmouseout 
  #     onmouseover
  #     onmouseup 
  #     onmousedown 
  #     onmousemove 
  #     ondblclick
  #     onload
  #
  method event {event node} {
    if {![::hv3::dom::use_scripting] || $mySee eq ""} {return ""}

    if {$event eq "onload"} {
      # The Hv3 layer passes the <BODY> node along with the onload
      # event, but DOM Level 0 browsers raise this event against
      # the Window object (the onload attribute of a <BODY> element
      # may be used to set the onload property of the corresponding 
      # Window object).
      #
      set js_obj $myWindow
    } else {

      set script [$node attr -default "" $event]
      if {$script ne ""} {
        $self node_to_dom $node
      }
      if {![info exists myNodeToDom($node)]} {return ""}
      set js_obj $myNodeToDom($node)
    }

    set eventobj [eval $js_obj Get [list $event]]
    if {[lindex $eventobj 0] eq "object"} {
      set ref [lindex $eventobj 1]
      set this [list object $js_obj]
      set rc [catch {eval $mySee $ref Call [list $this]} msg]
      $self Log "$node $event" [$mySee tostring $eventobj] $rc $msg
    }
  }

  # This method is called by the ::hv3::mousemanager object to 
  # dispatch a mouse-event into DOM country.
  #
  method mouseevent {event node x y args} {
    if {![::hv3::dom::use_scripting] || $mySee eq ""} {return 1}

    if {$node eq ""} {
      set Node [lindex [$myWindow Get document] 1]
    } else {
      set Node [$self node_to_dom $node]
    }
    eval ::hv3::dom::dispatchMouseEvent $self $event $Node $x $y $args
  }

  variable myWindowEvent ""
  method setWindowEvent {event} {
    set ret $myWindowEvent
    set myWindowEvent $event
    return $ret
  }
  method getWindowEvent {} {
    return $myWindowEvent
  }

#  typevariable tag_to_obj -array [list         \
#      ""       ::hv3::dom::Text                \
#      img      ::hv3::dom::HTMLImageElement    \
#      form     ::hv3::dom::HTMLFormElement     \
#      input    ::hv3::dom::HTMLInputElement    \
#      textarea ::hv3::dom::HTMLTextAreaElement \
#      a        ::hv3::dom::HTMLAnchorElement        \
#  ]
  typevariable tag_to_obj -array [list         \
      ""       ::hv3::DOM::Text                \
  ]

  #------------------------------------------------------------------
  # method node_to_dom
  #
  #     This is a factory method for HTMLElement/Text DOM wrappers
  #     around html widget node-handles.
  #
  method node_to_dom {node args} {
    if {![info exists myNodeToDom($node)]} {
      set myNodeToDom($node) [::hv3::dom::createWidgetNode $self $node]
      $myNodeToDom($node) configurelist $args
    }
    return $myNodeToDom($node)
  }

  #----------------------------------------------------------------
  # Given an html-widget node-handle, return the corresponding 
  # ::hv3::DOM::HTMLDocument object. i.e. the thing needed for
  # the Node.ownerDocument javascript property of an HTMLElement or
  # Text Node.
  #
  method node_to_document {node} {
    # TODO: Fix this for when there are other Document objects than
    # the main one floating around. i.e. This will fail if the node-handle
    # comes from a different <FRAME> than the script is being executed
    # in.
    #
    lindex [$myWindow Get document] 1
  }

  #----------------------------------------------------------------
  # Given an html-widget node-handle, return the corresponding 
  # ::hv3::hv3 object. i.e. the owner of the node-handle.
  #
  method node_to_hv3 {node} {
    # TODO: Same fix as for [node_to_document] is required.
    return $myHv3
  }

  method see {} { return $mySee }
  method hv3 {} { return $myHv3 }

  #------------------------------------------------------------------
  # Logging system follows.
  #

  # This variable contains the current javascript debugging log in HTML 
  # form. It is appended to by calls to [Log] and truncated to an
  # empty string by [LogReset]. If the debugging window is displayed,
  # the contents are identical to that of this variable.
  #
  variable myLogDocument ""

  method Log {heading script rc result} {

    set fscript "<table>"
    set num 1
    foreach line [split $script "\n"] {
      set eline [string trimright [$self Escape $line]]
      append fscript "<tr><td>$num<td><pre style=\"margin:0\">$eline</pre>"
      incr num
    }
    append fscript "</table>"

    set html [subst {
      <hr>
      <h3>[$self Escape $heading]</h3>
      $fscript

      <p>RC=$rc</p>
      <pre>[$self Escape $result]</pre>
    }]

    append myLogDocument $html
    set logwin [$self LogWindow]
    if {[winfo exists $logwin]} {
      $logwin append $html
    }
    # puts $myLogDocument
  }


  method LogReset {} {
    set myLogDocument ""
    set logwin [$self LogWindow]
    if {[winfo exists $logwin]} {
      [$logwin.hv3 html] reset
    }
  }

  method javascriptlog {} {
    set logwin [$self LogWindow]
    if {![winfo exists $logwin]} {
      ::hv3::dom::logwin $logwin
      $logwin append $myLogDocument
    }
  }

  method Escape {text} { string map {< &lt; > &gt;} $text }

  method LogWindow {} {
    set logwin ".[string map {: _} $self]_logwindow"
    return $logwin
  }
}

#-----------------------------------------------------------------------
# ::hv3::dom::logwin
#
#     Toplevel window widget used by ::hv3::dom code to display it's 
#     log file. 
#
snit::widget ::hv3::dom::logwin {
  hulltype toplevel

  constructor {} {
    set hv3 ${win}.hv3
    ::hv3::hv3 $hv3
    $hv3 configure -requestcmd [mymethod Requestcmd] -width 600 -height 400

    # Create an ::hv3::findwidget so that the report is searchable.
    #
    ::hv3::findwidget ${win}.find $hv3
    destroy ${win}.find.close

    bind $win <KeyPress-Up>     [list $hv3 yview scroll -1 units]
    bind $win <KeyPress-Down>   [list $hv3 yview scroll  1 units]
    bind $win <KeyPress-Next>   [list $hv3 yview scroll  1 pages]
    bind $win <KeyPress-Prior>  [list $hv3 yview scroll -1 pages]
    bind $win <Escape>          [list destroy $win]

    focus $win.find.entry

    pack ${win}.find -side bottom -fill x
    pack $hv3 -fill both -expand true
  }
  
  method append {html} {
    set hv3 ${win}.hv3
    [$hv3 html] parse $html
  }
}

#-----------------------------------------------------------------------
# Pull in the object definitions.
#
source [file join [file dirname [info script]] hv3_dom_compiler.tcl]
# source [file join [file dirname [info script]] hv3_style.tcl]
source [file join [file dirname [info script]] hv3_dom_xmlhttp.tcl]

#-----------------------------------------------------------------------
# Initialise the scripting environment. This should basically load (or
# fail to load) the javascript interpreter library. If it fails, then
# we have a scriptless browser. The test for whether or not the browser
# is script-enabled is:
#
#     if {[::hv3::dom::have_scripting]} {
#         puts "We have scripting :)"
#     } else {
#         puts "No scripting here. Probably better that way."
#     }
#
proc ::hv3::dom::init {} {
  # Load the javascript library.
  #
  catch { load [file join tclsee0.1 libTclsee.so] }
  catch { package require Tclsee }
}
::hv3::dom::init
# puts "Have scripting: [::hv3::dom::have_scripting]"
proc ::hv3::dom::have_scripting {} {
  return [expr {[info commands ::see::interp] ne ""}]
}
proc ::hv3::dom::use_scripting {} {
  set r [expr [::hv3::dom::have_scripting]&&$::hv3::dom::use_scripting_option]
  return $r
}

# set ::hv3::dom::reformat_scripts_option 0

set ::hv3::dom::use_scripting_option 1
set ::hv3::dom::reformat_scripts_option 1

