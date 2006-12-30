
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
    set myNative [[$dom see] native]
  }

  destructor {
    # Todo: Finalize $myNative.
  }

  method Get {property} {
    return [eval [$self see] $myNative Get $property]
  }

  method Put {property value} {
    return [eval [$self see] $myNative Put $property [list $value]]
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
  js_getobject document { ::hv3::dom::HTMLDocument %AUTO% [$myHv3 dom] $myHv3 }

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
  #     img = new Image();
  #
  js_getobject XMLHttpRequest {
    ::hv3::JavascriptObject %AUTO% [$myHv3 dom] -construct [mymethod newRequest]
  }
  method newRequest {args} {
    list object [::hv3::dom::XMLHttpRequest %AUTO% [$myHv3 dom] $myHv3]
  }

  #-----------------------------------------------------------------------
  # The "Node" object. This contains the constants for Node.nodeType
  #
  js_getobject Node {
    set obj [::hv3::JavascriptObject %AUTO% [$myHv3 dom]]
    $obj Put ELEMENT_NODE                [list number 1]
    $obj Put ATTRIBUTE_NODE              [list number 2]
    $obj Put TEXT_NODE                   [list number 3]
    $obj Put CDATA_SECTION_NODE          [list number 4]
    $obj Put ENTITY_REFERENCE_NODE       [list number 5]
    $obj Put ENTITY_NODE                 [list number 6]
    $obj Put PROCESSING_INSTRUCTION_NODE [list number 7]
    $obj Put COMMENT_NODE                [list number 8]
    $obj Put DOCUMENT_NODE               [list number 9]
    $obj Put DOCUMENT_TYPE_NODE          [list number 10]
    $obj Put DOCUMENT_FRAGMENT_NODE      [list number 11]
    $obj Put NOTATION_NODE               [list number 12]
    set obj
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
      if {$timerid eq ""} {set timerid [incr myNextTimeoutId]}
      set tclid [after $ms [mymethod CallTimer $timerid $isRepeat $ms $code]]
      set myTimerIds($timerid) [list $isRepeat $tclid]
    }

    list string $timerid
  }

  js_call setInterval {THIS js_code js_delay} {
    $self SetTimer 1 $js_code $js_delay
  }
  js_call setTimeout {THIS js_code js_delay} {
    $self SetTimer 0 $js_code $js_delay
  }
  js_call clearTimeout  {THIS js_timerid} { $self ClearTimer 0 $js_timerid }
  js_call clearInterval {THIS js_timerid} { $self ClearTimer 1 $js_timerid }
  #-----------------------------------------------------------------------

  #-----------------------------------------------------------------------
  # The "alert()" method.
  #
  js_scall alert {THIS msg} {
    tk_dialog .alert "Super Dialog Alert!" $msg "" 0 OK
    return ""
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
    foreach timeoutid [array names myTimeoutIds] {
      after cancel $myTimeoutIds($timeoutid)
    }
    array unset myTimeoutIds
  }
}

#-------------------------------------------------------------------------
# Snit class for DOM class "HTMLCollection".
#
# DOM class: (HTMLCollection)
#
# Supports the following javascript interface:
#
#     length
#     item(index)
#     namedItem(name)
#
# Also, a request for any property with a numeric name is mapped to a call
# to the item() method. A request for any property with a non-numeric name
# maps to a call to namedItem(). Hence, javascript references like:
#
#     collection[1]
#     collection["name"]
#     collection.name
#
# work as expected.
#
snit::type ::hv3::dom::HTMLCollection {

  variable myHv3 ""
  variable mySelector ""

  variable myNodes [list]
  variable myIsValid 0

  option -finalizable -default 0

  js_init {dom hv3 selector} {
    set myHv3 $hv3
    set mySelector $selector
  }

  #-------------------------------------------------------------------------
  # The HTMLCollection.length property
  #
  js_get length {
    $self Refresh
    return [list number [llength $myNodes]]
  }

  #-------------------------------------------------------------------------
  # The HTMLCollection.item() method
  #
  js_call item {THIS args} {
    if {[llength $args] != 1} {
        error "Bad arguments to HTMLCollection.item()"
    }

    $self Refresh
    set idx [format %.0f [lindex $args 0 1]]
    if {$idx < 0 || $idx >= [llength $myNodes]} {
      return ""
    }
    set domobj [[$myHv3 dom] node_to_dom [lindex $myNodes $idx]]
    return [list object $domobj]
  }

  #-------------------------------------------------------------------------
  # The HTMLCollection.namedItem() method
  #
  js_call namedItem {this args} {
    if {[llength $args] != 1} {
        error "Wrong number of arg to HTMLCollection.namedItem()"
    }
    $self Refresh

    set name [lindex $args 0 1]
    foreach node $myNodes {
      if {[$node attr -default "" id] eq $name} {
        set domobj [[$myHv3 dom] node_to_dom $node]
        return [list object $domobj]
      }
    }
    foreach node $myNodes {
      if {[$node attr -default "" name] eq $name} {
        set domobj [[$myHv3 dom] node_to_dom $node]
        return [list object $domobj]
      }
    }

    return ""
  }

  #-------------------------------------------------------------------------
  # Handle an attempt to retrieve an unknown property.
  #
  js_get * {

    # If $property looks like a number, treat it as an index into $myNodes.
    # Otherwise look for a node with the "name" or "id" attribute set to 
    # the attribute name.
    if {[string is double $property]} {
      set res [$self call_item THIS [list number $property]]
    } else {
      set res [$self call_namedItem THIS [list string $property]]
    }

    return $res
  }

  js_finish {}

  # Called to make sure the $myNodes list is current.
  #
  method Refresh {} {
    if {$myIsValid == 0} {
      set myNodes [$myHv3 search $mySelector]
      set myIsValid 1
    }
  }

  method Finalize {} {
    if {$options(-finalizable)} {$self destroy}
  }

  # This method is called externally when the underlying HTML document
  # changes structure. Any cache of the collection is purged.
  #
  method invalidate {} {
    set myIsValid 0
    set myNodes [list]
  }
}

snit::type ::hv3::dom::HTMLCollection_FE {
  variable myNode
  variable myDom

  js_init {dom hv3 node} {
    set myDom [$hv3 dom]
    set myNode $node
  }

  js_get length {
    set controlnodes [[$myNode replace] controls]
    return [list number [llength $controlnodes]]
  }

  js_call item {THIS js_index} {
    set controlnodes [[$myNode replace] controls]

    set len [llength $controlnodes]
    set idx [lindex $js_index 1]
    if {$idx < 0 || $idx >= $len} {return ""}
    
    return [list object [$myDom node_to_dom [lindex $controlnodes $idx]]]
  }

  js_call namedItem {THIS js_name} {
    set controlnodes [[$myNode replace] controls]
    set name [lindex $js_name 1]

    foreach c $controlnodes {
      if {[$c attribute -default "" name] eq $name} {
        return [list object [$myDom node_to_dom $c]]
      }
    }

    foreach c $controlnodes {
      if {[$c attribute -default "" id] eq $name} {
        return [list object [$myDom node_to_dom $c]]
      }
    }
    return ""
  }

  js_get * {

    # If $property looks like a number, treat it as an index into $myNodes.
    # Otherwise look for a node with the "name" or "id" attribute set to 
    # the attribute name.
    if {[string is double $property]} {
      set res [$self call_item THIS [list number $property]]
    } else {
      set res [$self call_namedItem THIS [list string $property]]
    }

    return $res
  }

  js_finish {}
}

# End of ::hv3::dom::HTMLCollection
#-------------------------------------------------------------------------


#-------------------------------------------------------------------------
# Snit type for DOM type Text.
#
# DOM class: (Node -> CharacterData -> Text) 
#
# Supports the following interface:
#
snit::type ::hv3::dom::Text {
  variable myNode ""

  js_init {dom hv3 node} {
    set myNode $node
  }

  js_get nodeType { list number 3 ;#3 -> TEXT_NODE }

  js_finish {}
}
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# Snit type for DOM type HTMLImageElement.
#
# DOM class: (Node -> Element -> HTMLElement -> HTMLImageElement)
#
snit::type ::hv3::dom::HTMLImageElement {

  variable myNode ""
  variable myHv3 ""

  js_init {dom hv3 node} {
    set myHv3 $hv3
    set myNode $node
    set myJavascriptParent [::hv3::dom::HTMLElement %AUTO% $dom $hv3 $node]
  }

  # Magic for "src" attribute. Whenever the "src" attribute is set on
  # any HTMLImageElement object, tell the corresponding HTML widget to
  # preload the image at the new value of "src".
  #
  js_get src { list string [$myNode attr -default "" src] }
  js_put src value { 
    set v [lindex $value 1]
    $myNode attr src $v 
    $myHv3 preload $v
  }

  # The "isMap" attribute. Javascript type "boolean".
  #
  js_get isMap       { $self GetBooleanAttribute src }
  js_put isMap value { $self PutBooleanAttribute src $value }

  # Configure all the other string attributes.
  #
  js_getput_attribute name     name
  js_getput_attribute align    align
  js_getput_attribute alt      alt
  js_getput_attribute border   border
  js_getput_attribute height   height
  js_getput_attribute hspace   hspace
  js_getput_attribute longDesc longdesc
  js_getput_attribute useMap   usemap
  js_getput_attribute vspace   vspace
  js_getput_attribute width    width

  js_finish {}
}

#-------------------------------------------------------------------------
# Snit type for DOM type HTMLFormElement.
#
# DOM class: (Node -> Element -> HTMLElement -> HTMLFormElement)
#
# Form-control objects:
#      HTMLSelectElement, HTMLInputElement, HTMLTextAreaElement,
#      HTMLButtonElement.
#
#      HTMLOptGroupElement, HTMLOptionElement,
#      HTMLLabelElement, HTMLFieldSetElement, HTMLLegendElement,
#
snit::type ::hv3::dom::HTMLFormElement {

  variable myNode ""
  variable myHv3 ""

  js_init {dom hv3 node} {
    set myHv3 $hv3
    set myNode $node
    set myJavascriptParent [::hv3::dom::HTMLElement %AUTO% $dom $hv3 $node]
  }

  #----------------------------------------------------------------------
  # Form control methods: submit() and reset().
  #
  js_call submit {THIS} {
    set form [$myNode replace]
    $form submit ""
  }
  js_call reset {THIS} {
    set form [$myNode replace]
    $form reset
  }

  #----------------------------------------------------------------------
  # Various Get/Put string property/attributes.
  #
  js_getput_attribute name          name
  js_getput_attribute target        target
  js_getput_attribute method        method
  js_getput_attribute action        action
  js_getput_attribute acceptCharset acceptcharset
  js_getput_attribute enctype       enctype

  #----------------------------------------------------------------------
  # The HTMLFormElement.elements array.
  #
  js_getobject elements {
    ::hv3::dom::HTMLCollection_FE %AUTO% $myHv3 $myNode
  }

  #----------------------------------------------------------------------
  # Unknown property handler. Try any unknown property requests on the
  # HTMLFormElement.elements object.
  #
  js_get * {
    set obj [lindex [$self Get elements] 1]
    return [$obj Get $property]
  }

  js_finish {}
}

#-------------------------------------------------------------------------
# Snit type for DOM type HTMLInputElement.
#
# DOM class: (Node -> Element -> HTMLElement -> HTMLInputElement)
#
snit::type ::hv3::dom::HTMLInputElement {

  variable myHv3 ""

  variable myNode ""

  variable myTextFilePassword 0
  variable myRadioCheckbox 0
  variable myRadioCheckboxButtonResetSubmit 0

  js_init {dom hv3 node} {
    set myJavascriptParent [::hv3::dom::HTMLElement %AUTO% $dom $hv3 $node]

    set myHv3 $hv3
    set myNode $node
    switch -- [string tolower [$myNode attribute -default text type]] {
      text     { set myTextFilePassword 1 }
      file     { set myTextFilePassword 1 }
      password { set myTextFilePassword 1 }
      radio    { 
        set myRadioCheckbox 1 
        set myRadioCheckboxButtonResetSubmit 1 
      }
      checkbox { 
        set myRadioCheckbox 1 
        set myRadioCheckboxButtonResetSubmit 1 
      }
      button { set myRadioCheckboxButtonResetSubmit 1 }
      reset  { set myRadioCheckboxButtonResetSubmit 1 }
      submit { set myRadioCheckboxButtonResetSubmit 1 }
    }
  }

  #----------------------------------------------------------------------
  # Read-only attribute "form".
  #
  js_get form { 
    set formnode [[$myNode replace] cget -formnode]
    if {$formnode ne ""} {
      return [list object [[$myHv3 dom] node_to_dom $formnode]]
    }
    return ""
  }

  #----------------------------------------------------------------------
  # Read-only attribute "type".
  #
  js_get type { list string [$myNode attribute -default text type] }

  #----------------------------------------------------------------------
  # Get and set the current value (value) and default value (defaultValue)
  # properties of the control. 
  #
  # For an <input> with type "text", "file" or "password", the 'value'
  # property is the current value of the control. For other <input> types,
  # it is the underlying "value" attribute of the node.
  #
  # For an <input> with type "text", "file" or "password", the 'defaultValue'
  # property is the value of the "value" attribute. For all others,
  # undefined.
  #
  js_get value { 
    if {$myTextFilePassword} {
      list string [[$myNode replace] value]
    } else {
      list string [$myNode attribute -default "" value]
    }
  }
  js_put value v {
    if {$myTextFilePassword} { 
      [$myNode replace] set_value [lindex $v 1] 
    } else {
      $myNode attribute value [lindex $v 1] 
    }
  }
  js_get defaultValue { 
    if {$myTextFilePassword} {
      list string [$myNode attribute -default "" value]
    }
  }
  js_put defaultValue v {
    if {$myTextFilePassword} { 
      [$myNode replace] set_value [lindex $v 1] 
      $myNode attribute value [lindex $v 1] 
    }
  }

  #----------------------------------------------------------------------
  # Get and set the current checked (checked) and default checked
  # (defaultChecked) properties of the control. 
  #
  js_get checked { 
    if {$myRadioCheckbox} {
      list boolean [[$myNode replace] checked]
    }
  }
  js_put checked v {
    if {$myRadioCheckbox} {
      set val [lindex $v 1]
      if {![string is boolean $val]} {
        error "Bad value for HTMLInputElement.checked"
      }
      [$myNode replace] set_checked [lindex $v 1]
    }
  }
  js_get defaultChecked { 
    if {$myRadioCheckbox} {
      list boolean [$myNode attribute -default 0 checked]
    }
  }
  js_put defaultChecked v { 
    if {$myRadioCheckbox} {
      [$myNode replace] set_checked [lindex $v 1]
      $myNode attribute checked [lindex $v 1]
    }
  }


  #----------------------------------------------------------------------
  # HTMLInputElement methods: focus(), blur(), select() and click().
  #
  #     focus()
  #     blur()
  #     select()   ("text", "file" and "password" only)
  #     click()    ("radio", "checkbox", "button", "reset" and "submit" only)
  #
  js_call focus {THIS} { [$myNode replace] dom_focus }
  js_call blur  {THIS} { [$myNode replace] dom_blur }
  js_call select {THIS} { 
    if {$myTextFilePassword} { [$myNode replace] dom_select }
  }
  js_call click  {THIS} { 
    if {$myRadioCheckboxButtonResetSubmit} { 
      [$myNode replace] dom_click 
    }
  }

  #----------------------------------------------------------------------
  # Vanilla string attribute/properties
  #
  # accept, accessKey, align, alt, maxlength, name, size, src, useMap
  #
  js_get accept    { list string [$myNode attribute -default "" accept] }
  js_get accessKey { list string [$myNode attribute -default "" accesskey] }
  js_get align     { list string [$myNode attribute -default "" align] }
  js_get alt       { list string [$myNode attribute -default "" alt] }
  js_get maxlength { list string [$myNode attribute -default "" maxlength] }
  js_get name      { list string [$myNode attribute -default "" name] }
  js_get size      { list string [$myNode attribute -default "" size] }
  js_get src       { list string [$myNode attribute -default "" src] }
  js_get useMap    { list string [$myNode attribute -default "" usemap] }

  js_put accept    v { $myNode attribute accept    $v }
  js_put accessKey v { $myNode attribute accesskey $v }
  js_put align     v { $myNode attribute align     $v }
  js_put alt       v { $myNode attribute alt       $v }
  js_put maxlength v { $myNode attribute maxlength $v }
  js_put name      v { $myNode attribute name      $v }
  js_put size      v { $myNode attribute size      $v }
  js_put src       v { $myNode attribute src       $v }
  js_put useMap    v { $myNode attribute usemap    $v }

  #----------------------------------------------------------------------
  # Other attribute/properties. These should eventually have relationship
  # with data structures in hv3_form.tcl - Todo. Note that property 
  # "tabIndex" is read-only.
  #
  js_get tabIndex  { list number [$myNode attribute -default 0 tabindex] }
  js_get accessKey { list string [$myNode attribute -default "" accesskey] }
  js_get align     { list string [$myNode attribute -default "" align] }

  js_put accessKey v { $myNode attribute accesskey [lindex $v 1] }
  js_put align     v { $myNode attribute align [lindex $v 1] }

  js_finish {}
}

snit::type ::hv3::dom::HTMLTextAreaElement {

  variable myNode ""
  variable myHv3 ""

  js_init {dom hv3 node} {
    set myHv3 $hv3
    set myNode $node
    set myJavascriptParent [::hv3::dom::HTMLElement %AUTO% $dom $hv3 $node]
  }

  #----------------------------------------------------------------------
  # Get and set the current value of the control.
  #
  js_get value       { list string [[$myNode replace] value] }
  js_put value value { [$myNode replace] set_value [lindex $value 1] }

  #----------------------------------------------------------------------
  # Read-only property "type". This is always the string "textarea", as
  # per DOM level 1.
  #
  js_get type {list string "textarea"}

  js_call focus  {THIS} { [$myNode replace] dom_focus }
  js_call blur   {THIS} { [$myNode replace] dom_blur }
  js_call select {THIS} { [$myNode replace] dom_select }

  js_finish {}
}

snit::type ::hv3::dom::HTMLAnchorElement {

  js_init {dom hv3 node} {
    set myJavascriptParent [::hv3::dom::HTMLElement %AUTO% $dom $hv3 $node]
  }

  js_getput_attribute accessKey accesskey
  js_getput_attribute charset   charset
  js_getput_attribute coords    coords
  js_getput_attribute href      href
  js_getput_attribute hreflang  hreflang
  js_getput_attribute name      name
  js_getput_attribute rel       rel
  js_getput_attribute rev       rev
  js_getput_attribute shape     shape
  js_getput_attribute tabIndex  tabindex
  js_getput_attribute target    target
  js_getput_attribute type      type

  js_finish {}
}

# List of scripting events (as per html 4.01, chapter 18):
#
# Document load/unload. These are activated when the [event] 
# method of this object is invoked (by code within the ::hv3::hv3 type).
#     onload
#     onunload
#
# Click-related events. Handled by the [event] method of this object.
# The [event] method is invoked by subscribing to events dispatched by 
# the ::hv3::mousemanager object.
#     onclick
#     ondblclick
#     onmousedown
#     onmouseup
#
# Mouse movement. Also [event] (see above).
#     onmouseover
#     onmousemove
#     onmouseout
#
# Keyboard:
#     onkeypress
#     onkeydown
#     onkeyup
#
# Form related stuff:
#     onfocus
#     onblur
#     onsubmit
#     onreset
#     onselect
#     onchange
#



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
      $myHv3 Subscribe $e [mymethod event $e]
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

  typevariable tag_to_obj -array [list         \
      ""       ::hv3::dom::Text                \
      img      ::hv3::dom::HTMLImageElement    \
      form     ::hv3::dom::HTMLFormElement     \
      input    ::hv3::dom::HTMLInputElement    \
      textarea ::hv3::dom::HTMLTextAreaElement \
      a        ::hv3::dom::HTMLAnchorElement        \
  ]

  method node_to_dom {node} {
    if {![info exists myNodeToDom($node)]} {

      set objtype ::hv3::dom::HTMLElement

      set tag [$node tag]
      if {[info exists tag_to_obj($tag)]} {
        set objtype $tag_to_obj($tag)
      } 

      set myNodeToDom($node) [$objtype %AUTO% $self $myHv3 $node]
    }
    return $myNodeToDom($node)
  }

  method see {} { return $mySee }

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
source [file join [file dirname [info script]] hv3_dom3.tcl]
source [file join [file dirname [info script]] hv3_dom2.tcl]

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

set ::hv3::dom::use_scripting_option 0
# set ::hv3::dom::reformat_scripts_option 0
set ::hv3::dom::reformat_scripts_option 1

