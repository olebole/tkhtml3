
package require snit

# This file contains the following types:
#
# ::hv3::JavascriptObject
# ::hv3::dom
#
# ::hv3::dom::HTMLDocument
# ::hv3::dom::HTMLCollection
# ::hv3::dom::HTMLElement
# ::hv3::dom::Text
#
# ::hv3::dom::Navigator
# ::hv3::dom::Window
#
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
snit::type ::hv3::dom::HTMLDocument {

  variable myHv3

  # If not set to an empty string, this option contains the name of
  # a Tcl variable to accumulate the strings passed to document.write()
  # and document.writeln() in.
  #
  # If it is an empty string, any javascript calls to write() or writeln()
  # are effectively no-ops.
  #
  option -writevar -default ""

  # document.images (HTMLCollection object).
  variable myImages ""

  # Object to handle generic operations.
  component myJavascriptObject

  # Call() is a no-op.
  delegate method Call to myJavascriptObject

  constructor {hv3 args} {
    set myHv3 $hv3

    # Initialise the superclass.
    set myJavascriptObject [::hv3::JavascriptObject %AUTO%]

    # Initialise document object methods.
    $myJavascriptObject CreateMethod write   [mymethod write]
    $myJavascriptObject CreateMethod writeln [mymethod writeln]

    $self configurelist $args
  }

  # The document.write() method
  #
  method write {this str} {
    if {$options(-writevar) ne ""} {
      set val [lindex $str 1]
      append $options(-writevar) $val
    }
    return ""
  }

  # The document.writeln() method. This just calls the write() method
  # with a newline appended to the argument string. I guess this is
  # used to populate <PRE> blocks or some such trickery.
  #
  method writeln {this str} {
    set val [lindex $str 1]
    $self write $this [list string "$val\n"]
    return ""
  }

  # This method is called for javascript of the form "new Image()". This
  # is equivalent to "document.createElement("img")".
  #
  method newImage {this args} {
    set node [$myHv3 fragment "<img>"]
    return [list object [[$myHv3 dom] node_to_dom $node]]
  }

  method Get {property} {
    switch -- $property {

      images {
        if {$myImages eq ""} {
          set myImages [::hv3::dom::HTMLCollection %AUTO% $myHv3 img]
        }
        return [list object $myImages]
      }

      default {
        set val [$myJavascriptObject Get $property]

        if {$val eq ""} {
          set css "\[name=\"$property\"\]"
          set node [lindex [$myHv3 search $css] 0]
          if {$node ne ""} {
            set val [list object [[$myHv3 dom] node_to_dom $node]]
          }
        }

        return $val
      }
    }
  }

  method Put {property value} {
    set readonly [list images]
    if {0 <= [lsearch $readonly $property]} {
      error "Read-only property: $property"
    }
    $myJavascriptObject Put $property $value
  }

  method CanPut {property} {
    puts "CanPut $property"
  }
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
  component myJavascriptObject
  delegate method * to myJavascriptObject

  method js_initialize $arglist $body
}
::snit::macro js_get {varname code} {
  lappend ::hv3::dom::js_get_methods $varname get_$varname
  method get_$varname {} $code
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
    return [$myJavascriptObject Get $property]
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
    return [$myJavascriptObject Put $property $value]
  }

  constructor {args} {
    set myJavascriptObject [::hv3::JavascriptObject %AUTO%]
    eval $self js_initialize $args
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
    ::hv3::JsObj %AUTO% -construct [list $myDocument newImage]
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
    $myDocument destroy
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

  # Object to handle generic operations.
  component myJavascriptObject
  delegate method * to myJavascriptObject

  constructor {hv3 selector} {
    set myHv3 $hv3
    set mySelector $selector
    set myJavascriptObject [::hv3::JavascriptObject %AUTO%]

    # Attributes
    $myJavascriptObject CreateReadOnlyAttr length [mymethod get_length]

    # Methods
    $myJavascriptObject CreateMethod item         [mymethod call_item]
    $myJavascriptObject CreateMethod namedItem    [mymethod call_namedItem]
  }

  method call_item {this args} {
    $self Refresh
    if {[llength $args] != 1} {
        error "Bad arguments to HTMLCollection.item()"
    }
    set idx [lindex $args 0 1]

    set domobj [[$myHv3 dom] node_to_dom [lindex $myNodes $idx]]
    return [list object $domobj]
  }

  method call_namedItem {this args} {
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

  method get_length {} {
    $self Refresh
    return [list number [llength $myNodes]]
  }

  method Get {property} {
    # Try to retrieve a conventionally stored property. If there
    # is no such property defined, an empty string is returned.
    set res [$myJavascriptObject Get $property]

    if {$res eq ""} {
      # No explicit property matches. If $property looks like a number,
      # treat it as an index into $myNodes. Otherwise look for a node
      # with the "name" or "id" attribute set to the attribute name.
      if {[string is double $property]} {
        set res [$self call_item THIS [list number $property]]
      } else {
        set res [$self call_namedItem THIS [list string $property]]
      }
    }

    return $res
  }

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
snit::type ::hv3::dom::HTMLElement {
  variable myNode ""
  variable myHv3 ""

  component myJavascriptObject

  constructor {hv3 node} {
    set myNode $node
    set myHv3 $hv3
    set myJavascriptObject [::hv3::JavascriptObject %AUTO%]
  }

  method Get {property} {

    set attr ""
    catch {set attr $ATTR_DATABASE(,$property)}
    catch {set attr $ATTR_DATABASE([$myNode tag],$property)}
    if {$attr ne ""} {
      foreach {attribute type default} $attr {}
      set val [$myNode attribute -default $default $attribute]

      if {$type eq "number"} {
        set f 0.0
        scan $val %f f
        set val $f
      } elseif {$type eq "boolean"} {
        set val [expr ($val == 0 ? 0 : 1)]
      }

      return [list $type $val]
    }

    switch -- $property {
      nodeName {}
      nodeValue {}
      nodeType {}
      parentNode {}
      childNodes {}
      firstChild {}
      lastChild {}
      previousSibling {}
      nextSibling {}
      attributes {}
      ownerDocument {}

      tagName {             # Element.tagName
        return [list string [string toupper [$myNode tag]]] 
      }
    }

    return [$myJavascriptObject Get $property]
  }

  method Put {property value} {

    # Special hack to ensure pre-loading of images works. For example:
    #
    #    i = new Image();
    #    i.src = "preload_this_image.gif"
    #
    if {$property eq "src" && [$myNode tag] eq "img"} {
      $myHv3 preload [lindex $value 1]
    }

    set attr ""
    catch {set attr $ATTR_DATABASE(,$property)}
    catch {set attr $ATTR_DATABASE([$myNode tag],$property)}
    if {$attr ne ""} {
      foreach {attribute type default} $attr {}
      $myNode attribute $attribute [lindex $value 1]
      return
    }

    return [$myJavascriptObject Put $property $value]
  }

  typevariable ATTR_DATABASE -array [list]

  typemethod setup_ATTR_DATABASE {} {

    # Attributes of class HTMLElement 
    set ATTR_DATABASE(,id)        [list id    string ""] 
    set ATTR_DATABASE(,title)     [list title string ""] 
    set ATTR_DATABASE(,lang)      [list lang  string ""] 
    set ATTR_DATABASE(,dir)       [list dir   string ""] 
    set ATTR_DATABASE(,className) [list class string ""] 

    # Attributes of class HTMLImageElement 
    set ATTR_DATABASE(img,name)     [list name     string ""]
    set ATTR_DATABASE(img,align)    [list align    string ""]
    set ATTR_DATABASE(img,alt)      [list alt      string ""]
    set ATTR_DATABASE(img,border)   [list border   string ""]
    set ATTR_DATABASE(img,height)   [list height   string ""]
    set ATTR_DATABASE(img,hspace)   [list hspace   string ""]
    set ATTR_DATABASE(img,longDesc) [list longdesc string ""]
    set ATTR_DATABASE(img,src)      [list src      string ""]
    set ATTR_DATABASE(img,useMap)   [list usemap   string ""]
    set ATTR_DATABASE(img,vspace)   [list vspace   string ""]
    set ATTR_DATABASE(img,width)    [list width    string ""]
    set ATTR_DATABASE(img,isMap)    [list ismap    boolean ""]
  }
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
      if {[catch {$mySee eval $script} msg]} {
        after idle [list error $msg]
      }
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

  # Set up all the sub-classes of ::HTMLElement.
  #
  ::hv3::dom::HTMLElement setup_ATTR_DATABASE
}
proc ::hv3::dom::have_scripting {} {
  return [expr {[info commands ::see::interp] ne ""}]
}


proc ::hv3::dom::jsputs {this args} {
  ::puts [lindex $args 0 1]
  return ""
}

::hv3::dom::init
# puts "Have scripting: [::hv3::dom::have_scripting]"


