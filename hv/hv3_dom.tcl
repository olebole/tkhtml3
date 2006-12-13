
package require snit

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
#
# Defacto Standards:
#     ::hv3::dom::Navigator
#     ::hv3::dom::Window
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

  # Similar to -call, but for construction (i.e. "new Object()") calls.
  #
  option -construct -default ""

  # Javascript interpreter this object is associated with.
  variable mySee

  # Array of simple javascript properties.
  variable myProperties -array [list]

  constructor {args} {
    $self configurelist $args
  }

  method CreateMethod {name command} {
    set js [::hv3::JavascriptObject %AUTO% -call $command]
    $self Put $name [list object $js]
  }

  method CreateReadOnlyAttr {name command} {
    $self Put $name [list ro $command]
  }
  method CreateReadWriteAttr {name command} {
    $self Put $name [list rw $command]
  }

  method Get {property} {
    if {[info exists myProperties($property)]} {
      set type [lindex $myProperties($property) 0]
      if {$type eq "rw" || $type eq "ro"} {
        return [eval [lindex $myProperties($property) 1]]
      }
      return $myProperties($property)
    }

    return ""
  }

  method Put {property value} {
    if {[info exists myProperties($property)]} {
      set type [lindex $myProperties($property) 0]
      if {$type eq "rw"} {
        return [eval [linsert [lindex $myProperties($property) 1] end $value]]
      } elseif {$type eq "ro"} {
        error "Read-only property: $property"
      }
    }

    set myProperties($property) $value
  }

  method Call {args} {
    if {$options(-call) ne ""} {
      eval $options(-call) $args
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
}

# Shorter name for ::hv3::JavascriptObject
proc ::hv3::JsObj {args} {
  return [eval ::hv3::JavascriptObject $args]
}

#-------------------------------------------------------------------------
# js_XXX snit::macros used to make creating DOM objects easier.
#
#     js_init
#
#     js_get
#     js_getobject
#     js_put
#     js_call
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
  method get_$methodname {} "\$self GetMethod $methodname"
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

  method GetMethod {methodname} {
    if {![info exists myCallMethods($methodname)]} {
      set myCallMethods($methodname) [
          ::hv3::JavascriptObject %AUTO% -call [mymethod call_$methodname]
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

  constructor {args} {
    set myJavascriptParent ""
    eval $self js_initialize $args
    if {$myJavascriptParent eq ""} {
      set myJavascriptParent [::hv3::JavascriptObject %AUTO%]
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
}


# Snit class for the "document" object.
#
# DOM class: (Node -> Document -> HTMLDocument)
#
# Supports the following:
#
#     write(string)
#     writeln(string)
#
#     images
#
# Unsupported collections/elements:
#    
#     forms
#     anchors
#     links
#     applets
#     body
#
snit::type ::hv3::dom::HTMLDocument {

  # If not set to an empty string, this option contains the name of
  # a Tcl variable to accumulate the strings passed to document.write()
  # and document.writeln() in.
  #
  # If it is an empty string, any javascript calls to write() or writeln()
  # are effectively no-ops.
  #
  option -writevar -default ""

  variable myHv3

  js_init {hv3 args} {
    set myHv3 $hv3
    $self configurelist $args
  }

  #-------------------------------------------------------------------------
  # The document.write() method
  #
  js_call write {THIS str} {
    if {$options(-writevar) ne ""} {
      set val [lindex $str 1]
      append $options(-writevar) $val
    }
    return ""
  }

  #-------------------------------------------------------------------------
  # The document.writeln() method. This just calls the write() method
  # with a newline appended to the argument string. I guess this is
  # used to populate <PRE> blocks or some such trickery.
  #
  js_call writeln {THIS str} {
    set val [lindex $str 1]
    $self call_write $THIS [list string "$val\n"]
    return ""
  }

  #-------------------------------------------------------------------------
  # The document.images[] array (type HTMLCollection).
  #
  js_getobject images {
    hv3::dom::HTMLCollection %AUTO% $myHv3 img
  }

  #-------------------------------------------------------------------------
  # Handle unknown property requests.
  #
  # An unknown property may refer to certain types of document element
  # by either the "name" or "id" HTML attribute.
  #
  # 1: Have to find some reference for this behaviour...
  # 2: Maybe this is too inefficient. Maybe it should go to the 
  #    document.images and document.forms collections.
  #
  js_get * {

    # Allowable element types.
    set tags [list form img]

    # Selectors to use to find document nodes.
    set nameselector [subst -nocommands {[name="$property"]}]
    set idselector   [subst -nocommands {[id="$property"]}]
 
    foreach selector [list $nameselector $idselector] {
      set node [lindex [$myHv3 search $selector] 0]
      if {$node ne "" && [lsearch $tags [$node tag]] >= 0} {
        return [list object [[$myHv3 dom] node_to_dom $node]]
      }
    }

    return ""
  }

  js_finish {}
}

#-------------------------------------------------------------------------
# Snit type for "Navigator" DOM object.
#
snit::type ::hv3::dom::Navigator {

  js_init {} {}

  js_get appName    { return [list string "Netscape"] }
  js_get appVersion { return [list number 4] }

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

  js_init {see hv3} { 
    set mySee $see 
    set myHv3 $hv3 

    set myDocument [::hv3::dom::HTMLDocument %AUTO% $myHv3]
  }

  #-----------------------------------------------------------------------
  # Property implementations:
  # 
  #     Window.document
  #
  component myDocument 
  delegate option -writevar to myDocument
  js_get document { return [list object $myDocument] }

  #-----------------------------------------------------------------------
  # The "Image" property object. This is so that scripts can
  # do the following:
  #
  #     img = new Image();
  #
  js_getobject Image {
    ::hv3::JsObj %AUTO% -construct [mymethod newImage]
  }
  method newImage {} {
    set node [$myHv3 fragment "<img>"]
    return [list object [[$myHv3 dom] node_to_dom $node]]
  }

  #-----------------------------------------------------------------------
  # The "navigator" object.
  #
  js_getobject navigator {
    ::hv3::dom::Navigator %AUTO%
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
  js_get parent { return [list object $self] }
  js_get top    { return [list object $self] }
  js_get self   { return [list object $self] }
  js_get window { return [list object $self] }

  #-----------------------------------------------------------------------
  # Method Implementations: 
  #
  #     Window.setTimeout(code, delay) 
  #     Window.clearTimeout(timeoutid)
  #
  variable myTimeoutIds -array [list]
  variable myNextTimeoutId 0

  js_call setTimeout {THIS js_code js_delay} {
    set delay [lindex $js_delay 1]
    set code [lindex $js_code 1]

    set timeoutid [incr myNextTimeoutId]
    set myTimeoutIds($timeoutid) [
      after [format %.0f $delay] [mymethod Timeout $timeoutid $code]
    ]
    return [list string $timeoutid]
  }

  js_call clearTimeout {THIS js_timeoutid} {
    set timeoutid [lindex $js_timeoutid 1]
    after cancel $myTimeoutIds($timeoutid)
    unset myTimeoutIds($timeoutid)
    return ""
  }

  method Timeout {timeoutid code args} {
    unset myTimeoutIds($timeoutid)
    $mySee eval $code
  }
  #-----------------------------------------------------------------------

  js_finish {
    # Cancel any outstanding timers created by Window.setTimeout().
    #
    foreach timeoutid [array names myTimeoutIds] {
      after cancel $myTimeoutIds($timeoutid)
    }
    array unset myTimeoutIds

    # Destroy the document object.
    catch {$myDocument destroy}
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

  js_init {hv3 selector} {
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
    set idx [lindex $args 0 1]
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

  # This method is called externally when the underlying HTML document
  # changes structure. Any cache of the collection is purged.
  #
  method invalidate {} {
    set myIsValid 0
    set myNodes [list]
  }
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

  constructor {node} {
    set myNode $node
  }
}
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# Snit type for DOM type HTMLElement.
#
# DOM class: (Node -> Element -> HTMLElement)
#
# Supports the following interface:
#
#      Element.nodeName
#      Element.nodeValue
#      Element.nodeType
#      Element.parentNode
#      Element.childNodes
#      Element.firstChild
#      Element.lastChild
#      Element.previousSibling
#      Element.nextSibling
#      Element.attributes
#      Element.ownerDocument
#
snit::type ::hv3::dom::HTMLElement {
  variable myNode ""
  variable myHv3 ""

  js_init {hv3 node} {
    set myNode $node
    set myHv3 $hv3
  }

  js_get tagName { 
    return [list string [string toupper [$myNode tag]]]
  }

  # Get/Put functions for the attributes of $myNode:
  #
  method GetStringAttribute {prop} {
    return [list string [$myNode attribute -default "" $prop]]
  }
  method PutStringAttribute {prop value} {
    $myNode attribute $prop [lindex $value 1]
  }
  method GetBooleanAttribute {prop} {
    set bool [$myNode attribute -default 0 $prop]
    if {![catch {expr $bool}]} {
      return [list boolean [expr {$bool ? 1 : 0}]]
    } else {
      return [list boolean 1]
    }
  }
  method PutBooleanAttribute {prop value} {
    $myNode attribute $prop [lindex $value 1]
  }

  # The following string attributes are common to all elements:
  #
  #     HTMLElement.id
  #     HTMLElement.title
  #     HTMLElement.lang
  #     HTMLElement.dir
  #     HTMLElement.className
  #
  foreach {prop attr} {id id title title lang lang dir dir className class} {
    js_get $prop "\$self GetStringAttribute $prop"
    js_put $prop val "\$self PutStringAttribute $prop \$val"
  }

  js_finish {}
}

snit::type ::hv3::dom::HTMLImageElement {

  variable myNode ""
  variable myHv3 ""

  js_init {hv3 node} {
    set myHv3 $hv3
    set myNode $node
    set myJavascriptParent [::hv3::dom::HTMLElement %AUTO% $hv3 $node]
  }

  # Magic for "src" attribute. Whenever the "src" attribute is set on
  # any HTMLImageElement object, tell the corresponding HTML widget to
  # preload the image at the new value of "src".
  #
  js_get src { $self GetStringAttribute src }
  js_put src value { 
    $self PutStringAttribute src $value
    $myHv3 preload [lindex $value 1]
  }

  # The "isMap" attribute. Javascript type "boolean".
  #
  js_get isMap       { $self GetBooleanAttribute src }
  js_put isMap value { $self PutBooleanAttribute src $value }

  # Configure all the other string attributes.
  #
  foreach {attribute property} [list \
      name name         \
      align align       \
      alt alt           \
      border border     \
      height height     \
      hspace hspace     \
      longdesc longDesc \
      usemap useMap     \
      vspace vspace     \
      width width       \
  ] {
    js_get $property       "\$self GetStringAttribute $attribute"
    js_put $property value "\$self PutStringAttribute $attribute \$value"
  }

  js_finish {}
}

# List of scripting events (as per html 4.01, chapter 18):
#
# Document load/unload. These are activated when the [onload] and [onunload]
# methods of this object are invoked (by code within the ::hv3::hv3 type).
#     onload
#     onunload
#
# Click-related events. Handled by the [mouseevent] method of this object.
# The [mouseevent] method is invoked by subscribing to events dispatched by 
# the ::hv3::mousemanager object.
#     onclick
#     ondblclick
#     onmousedown
#     onmouseup
#
# Mouse movement. Also [mouseevent] (see above).
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
snit::type ::hv3::dom {
  variable mySee ""

  # Variable used to accumulate the arguments of document.write() and
  # document.writeln() invocations from within [script].
  #
  variable myWriteVar ""

  # Document window associated with this scripting environment.
  variable myHv3 ""

  # Document object.
  variable myWindow ""

  # Map from Tkhtml3 node-handle to corresponding DOM object.
  # Entries are added by the [::hv3::dom node_to_dom] method. The
  # array is cleared by the [::hv3::dom reset] method.
  #
  variable myNodeToDom -array [list]

  constructor {hv3 args} {
    set myHv3 $hv3
    if {[::hv3::dom::have_scripting]} {
      $self reset

      # Mouse events:
      foreach e [list onclick onmouseout onmouseover \
          onmouseup onmousedown onmousemove ondblclick
      ] {
        $myHv3 Subscribe $e [mymethod mouseevent $e]
      }
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
        destroy $myWindow
      }


      # Set up the new interpreter with the global "Window" object.
      set mySee [::see::interp]
      set myWindow [::hv3::dom::Window %AUTO% $mySee $myHv3]
      $mySee global $myWindow 
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
    if {[::hv3::dom::have_scripting]} {
      $myWindow configure -writevar [myvar myWriteVar]
      ::hv3::bg [list $mySee eval $script]
      set res $myWriteVar
      $myWindow configure -writevar ""
      set myWriteVar ""
      return $res
    }
    return ""
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
  #
  method mouseevent {event node} {
    set script [$node attr -default "" $event]
    if {$script ne ""} {
      # Instead of just calling [$mySee eval] on the script, wrap it
      # in an anonymous function. This is so that event scripts like the
      # following:
      #
      #     <a onmouseover="func(); return true">
      #
      # do not throw an exception on the "return" statement.
      #
      # Todo: Create some "event" object filled with event parameters.
      #
      $mySee eval "(function () {$script})()"
    }
  }


  # Execute the "onunload" script of the current document, if any. This is
  # required to so that Hv3 users don't miss out on all those cool
  # advertisements that sometimes pop up when you try to close an
  # unscrupulous website.
  #
  method onunload {} {
  }

  # Execute the "onload" script of the current document, if any. The onload
  # script is specified by the "onload" attribute of the document <BODY> or 
  # <FRAMESET> element(s).
  #
  # This method is called from within a node-handler for the <BODY> or
  # <FRAMESET> element passed as the first argument.
  #
  method onload {node} {
    if {[::hv3::dom::have_scripting]} {
      set onload [$node attr -default "" onload]
      if {$onload ne ""} {
        if {[catch {$mySee eval $onload} msg]} {
          after idle [list error $msg]
        }
      }
    }
    return ""
  }

  method node_to_dom {node} {
    if {![info exists myNodeToDom($node)]} {
      switch -- [$node tag] {
        "" {
          set domobj [::hv3::dom::Text %AUTO% $node]
        }
        img {
          set domobj [::hv3::dom::HTMLImageElement %AUTO% $myHv3 $node]
        }
        default {
          set domobj [::hv3::dom::HTMLElement %AUTO% $myHv3 $node]
        }
      }
      set myNodeToDom($node) $domobj
    }
    return $myNodeToDom($node)
  }
}

#
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
  catch { load [file join [file dirname [info script]] libtclsee.so] }
  catch { load /home/dan/javascript/tcl/libtclsee.so }
}
proc ::hv3::dom::have_scripting {} {
  return [expr {[info commands ::see::interp] ne ""}]
}

::hv3::dom::init
# puts "Have scripting: [::hv3::dom::have_scripting]"


