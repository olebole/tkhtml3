
package require snit

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

  method Get {property} {
    switch -- $property {

      images {
        if {$myImages eq ""} {
          set myImages [::hv3::dom::HTMLCollection %AUTO% $myHv3 img]
        }
        return [list object $myImages]
      }

      default {
        return [$myJavascriptObject Get $property]
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

# Snit type for DOM type HTMLElement.
#
# DOM class: (Node -> Element -> HTMLElement)
#
# Supports the following interface:
#
snit::type ::hv3::dom::HTMLElement {
  variable myNode ""

  component myJavascriptObject

  constructor {node} {
    set myNode $node
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
  catch { load [file join [file dirname [info script]] libtclsee.so] }
  catch { load /home/dan/javascript/tcl/libtclsee.so }

  ::hv3::dom::HTMLElement setup_ATTR_DATABASE
 
}
proc ::hv3::dom::have_scripting {} {
  return [expr {[info commands ::see::interp] ne ""}]
}


# List of scripting events (as per html 4.01, chapter 18):
#
# Document load/unload. These are activated when the [onload] and [onunload]
# methods of this object are invoked (by code within the ::hv3::hv3 type).
#     onload
#     onunload
#
# Click-related events.
#     onclick
#     ondblclick
#     onmousedown
#     onmouseup
#
# Mouse movement.
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
  variable myDocument ""

  # Debugging functions.
  variable myDebugObjects [list]

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
        $myDocument destroy

        # Delete all the DOM objects in the $myNodeToDom array.
        foreach key [array names myNodeToDom] {
          $myNodeToDom($key) destroy
        }
        array unset myNodeToDom

        foreach obj $myDebugObjects {
          $obj destroy
        }
        set myDebugObjects [list]
      }

      set mySee [::see::interp]

      # Set up the global "document" object
      set myDocument [::hv3::dom::HTMLDocument %AUTO% $myHv3]
      $mySee object document $myDocument 

      lappend myDebugObjects [
          ::hv3::JavascriptObject %AUTO% -call ::hv3::dom::puts
      ]
      $mySee object puts [lindex $myDebugObjects end]

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
      $myDocument configure -writevar [myvar myWriteVar]
      if {[catch {$mySee eval $script} msg]} {
        after idle [list error $msg]
      }
      set res $myWriteVar
      $myDocument configure -writevar ""
      set myWriteVar ""
      return $res
    }
    return ""
  }

  method mouseevent {event node} {
    set script [$node attr -default "" $event]
    if {$script ne ""} {
      #puts "mouseevent: $script"
      $mySee eval $script
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
  method onload {} {
  }

  method node_to_dom {node} {
    if {![info exists myNodeToDom($node)]} {
      switch -- [$node tag] {
        "" {
          set domobj [::hv3::dom::Text %AUTO% $node]
        }
        default {
          set domobj [::hv3::dom::HTMLElement %AUTO% $node]
        }
      }
      set myNodeToDom($node) $domobj
    }
    return $myNodeToDom($node)
  }
}

proc ::hv3::dom::puts {this args} {
  ::puts [lindex $args 0 1]
  return ""
}

::hv3::dom::init
# puts "Have scripting: [::hv3::dom::have_scripting]"


