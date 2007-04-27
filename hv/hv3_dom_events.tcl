namespace eval hv3 { set {version($Id: hv3_dom_events.tcl,v 1.12 2007/04/27 10:47:19 danielk1977 Exp $)} 1 }

#-------------------------------------------------------------------------
# DOM Level 2 Events.
#
# This file contains the Hv3 implementation of javascript events. Hv3
# attempts to be compatible with the both the W3C and Netscape models.
# Where these are incompatible, copy Safari. This file contains 
# implementations of the following DOM interfaces:
#
#     DocumentEvent    (mixed into the DOM Document object)
#     EventTarget      (mixed into the DOM Node objects)
#
# And event object interfaces:
#
#                   Event
#                     |
#              +--------------+
#              |              |
#           UIEvent      MutationEvent
#              |
#              |
#          MouseEvent
#
# References:
# 
#   DOM:
#     http://www.w3.org/TR/DOM-Level-3-Events/
#
#   Gecko:
#     http://developer.mozilla.org/en/docs/DOM:event
#     http://developer.mozilla.org/en/docs/DOM:document.createEvent
#
#-------------------------------------------------------------------------

# List of HTML events handled by this module. This is used both at 
# runtime and when building DOM object definitions during application 
# startup.
#
set ::hv3::dom::HTML_Events_List [list                          \
  click dblclick mousedown mouseup mouseover mousemove mouseout \
  keypress keydown keyup focus blur submit reset select change  \
  load
]

proc ArgToBoolean {see a} {
  switch -- [lindex $a 0] {
    null      { expr 0 }
    undefined { expr 0 }
    default   { $see tostring $a }
  }
}

#-------------------------------------------------------------------------
# eventtarget (dom level 2 events)
#
#     this interface is mixed into all objects implementing the node 
#     interface. some of the node interface is invoked via the 
#     javascript protocol. i.e. stuff like the following is expected to 
#     work:
#
#         set value       [$self get parentnode]
#         set eventtarget [lindex $value 1]
#
#  javascript interface:
#
#     eventtarget.addeventlistener()
#     eventtarget.removeEventListener()
#     EventTarget.dispatchEvent()
#
#     Also special handling on traditional/inline event model attribute
#     ("onclick", "onsubmit" etc.). See $::hv3::dom::HTML_Events_List
#     above for a full list of HTML events.
#
::hv3::dom::type EventTarget {} {

  #-----------------------------------------------------------------------
  # EventTarget.addEventListener()
  #
  dom_call addEventListener {THIS event_type listener useCapture} {
    $self initEventTarget

    set see [$myDom see]
    set T [$see tostring $event_type]
    set L [lindex $listener 1]
    set C [ArgToBoolean $see $useCapture]
    $myEventTarget addEventListener $T $L $C
  }

  #-----------------------------------------------------------------------
  # EventTarget.removeEventListener()
  #
  dom_call removeEventListener {THIS event_type listener useCapture} {
    if {$myEventTarget eq ""} return

    set T [$see tostring $event_type]
    set L [lindex $listener 1]
    set C [$see tostring $useCapture]
    $myEventTarget addEventListener $T $L $C
  }

  #-----------------------------------------------------------------------
  # EventTarget.dispatchEvent()
  #
  dom_call dispatchEvent {THIS evt} {
    set b [$self doDispatchEvent [lindex $evt 1]]
    list boolean $b
  }

  #-----------------------------------------------------------------------
  # Code to get and set properties from the traditional events model.
  #
  # Basically does the following mapping for each event $E:
  #
  #     [$self Get on$E]       -> [$self EventTarget_Get $E]
  #     [$self Put on$E value] -> [$self EventTarget_Put $E $value]
  #
  # See below for the definitions of [EventTarget_Get] and [EventTarget_Put].
  #
  foreach event $::hv3::dom::HTML_Events_List {
    set get_script [subst -novar {$self EventTarget_Get [set event]}]
    set put_script [subst -novar {$self EventTarget_Put [set event] $value}]
    dom_get on$event $get_script
    dom_put on$event value $put_script
  }

  dom_snit {

    variable myEventTarget ""

    # Initialise the event-target object stored in $myEventTarget. Also
    # compile any event functions specified using the inline model (i.e.
    # the "onclick" attribute) here. The event-target object needs to
    # be initialised under the following circumstances:
    #
    #   * When EventTarget.addEventListener() is called,
    #   * When an event property from the traditional events model
    #     is set (i.e. EventTarget.onclick),
    #   * When an event is dispatched and there is an HTML attribute
    #     defined for the corresponding event.
    #
    # This method is a no-op if the event-target object has already been
    # initialised.
    #
    method initEventTarget {} {
      if {$myEventTarget ne ""} return
      set myEventTarget [[$myDom see] eventtarget]

      set node [$self getElementNode]
      if {$node ne ""} {
        # For each type of event ("click", "submit", "mouseout" ...)
        # search for an HTML attribute of the form "on$event"
        # Pass each to the event-target object to be compiled into an
        # event-listener function.
        # 
        foreach event $::hv3::dom::HTML_Events_List {
          set code [$node attribute -default "" "on${event}"]
          if {$code ne ""} {
            $myEventTarget setLegacyScript $event $code
          }
        }
      }
    }

    method getElementNode {} {
      # DOM Core level 1 defines the Node.ELEMENT_NODE constant as the
      # value 1. So if the javascript "nodeType" property of this object
      # is equal to "1", we are an element node. In this case, worry
      # about inline event definitions.
      #
      if {[lindex [$self Get nodeType] 1] == 1} {
        return $options(-nodehandle)
      }
      return ""
    }

    # This method calls the [initEventTarget] method if:
    #
    #     * the event-target object has not already been defined, and
    #     * this EventTarget interface is mixed into an Element node, and
    #     * the element node has an attribute defining an event of
    #       type $event using the inline model (i.e. if $event is "click", 
    #       check for the attribute "onclick").
    #
    method EventTarget_InitIfAttribute {event} {
      if {$myEventTarget eq ""} {
        # If this event-listener is not an element-node, or if there
        # is no "on$event" attribute defined for this event, return.
        # Otherwise, if the is an "on$event" attribute, construct the
        # event-target object. This will compile the inline event definition
        # into a function that will be executed by the [runEvent] method.
        #
        set node [$self getElementNode]
        if {$node ne "" && [$node attr -default "" "on$event"] ne ""} {
          $self initEventTarget
        }
      }
    }

    method EventTarget_Get {event} {
      $self EventTarget_InitIfAttribute $event
      if {$myEventTarget eq ""} { return undefined }
      list event $myEventTarget $event
    } 

    method EventTarget_Put {event value} {
      if {[lindex $value 0] ne "object"} {
        if {$myEventTarget eq ""} return
        $myEventTarget removeLegacyListener $event
      } else {
        $self initEventTarget
        $myEventTarget setLegacyListener $event [lindex $value 1]
      }
    } 

    # Returns one of the following, as per the C event-target
    # object (see hv3see.c):
    #
    #     "prevent"
    #     "ok"
    #     ""
    #
    method runEvent {event_type isCapture event} {
      $self EventTarget_InitIfAttribute $event_type
      if {$myEventTarget ne ""} {
        # Set the value of the Event.currentTarget property to this object
        # before running the event listeners.
        #
        $event configure -currenttarget $self
        $myEventTarget runEvent $event_type $isCapture $self $event
      }
    }

    method doDispatchEvent {event} {
      set event_type [$event cget -eventtype]
      set isRun 0          ;# Set to true if one or more scripts are run.
   
      set isBubbling [$event bubbles]

      # Set the value of the Event.target property to this object.
      #
      $event configure -target $self

      if {$isBubbling} {
        # Use the DOM Node.parentNode interface to determine the ancestry.
        #
        # Due to the strange nature of the browsers that came before us
        # object $self may either implement class "Node" (core module) or 
        # "Window" (ns module). In the "Window" case the parentNode property
        # will always be NULL (empty string form - "").
        #
        set N [$self Get parentNode]
        set nodes [list]
        while {[lindex $N 0] eq "object"} {
          set cmd [lindex $N 1]
          lappend nodes $cmd
          set N [$cmd Get parentNode]
        }
  
        # Capturing phase:
        $event configure -eventphase 1
        for {set ii [expr [llength $nodes] - 1]} {$ii >= 0} {incr ii -1} {
          if {[$event stoppropagationcalled]} break
          set node [lindex $nodes $ii]
          
          set rc [$node runEvent $event_type 1 $event]
          if {$rc ne ""} {set isRun 1}
        }
      }

      # Target phase:
      $event configure -eventphase 2
      if {![$event stoppropagationcalled]} {
        set rc [$self runEvent $event_type 0 $event]
        if {"prevent" eq $rc} {
          $event configure -preventdefault true
        } 
        if {$rc ne ""} {set isRun 1}
      }

      if {$isBubbling} {
        # Bubbling phase:
        $event configure -eventphase 3
        foreach node $nodes {
          if {[$event stoppropagationcalled]} break
          set rc [$node runEvent $event_type 0 $event]
          if {"prevent" eq $rc} {
            $event configure -preventdefault true
          }
          if {$rc ne ""} {set isRun 1}
        }
      }

      # If anyone called Event.preventDefault(), return "prevent". Otherwise,
      # if one or more scripts were executed, return "ok. If no scripts
      # were executed, return "".
      #
      if {[$event cget -preventdefault]} {return "prevent"}
      if {$isRun} {return "ok"}
      return ""
    }
  }

  dom_snit {
    method eventdump {} {
      $self initEventTarget
      $myEventTarget dump
    }
  }
}

::hv3::dom::type Event {} {

  # Constants for Event.eventPhase (Definition group PhaseType)
  #
  dom_get CAPTURING_PHASE { list number 1 }
  dom_get AT_TARGET       { list number 2 }
  dom_get BUBBLING_PHASE  { list number 3 }

  # Read-only attributes to access the values set by initEvent().
  #
  dom_get type       { list string  $myEventType }
  dom_get bubbles    { list boolean $myCanBubble }
  dom_get cancelable { list boolean $myCancelable }

  dom_get target        { list object $options(-target) }
  dom_get currentTarget { list object $options(-currenttarget) }
  dom_get eventPhase    { list number $options(-eventphase) }

  # TODO: Timestamp is supposed to return a timestamp in milliseconds
  # from the epoch. But the DOM spec notes that this information is not
  # available on all systems, in which case the property should return 0. 
  #
  dom_get timestamp  { list number 0 }

  dom_call stopPropagation {THIS} { set myStopPropagationCalled 1 }
  dom_call preventDefault {THIS}  { 
    set options(-preventdefault) true 
  }

  dom_call -string initEvent {eventType canBubble cancelable} {
    $self Event_initEvent $eventType $canBubble $cancelable
    set myEventType  $eventType
    set myCanBubble  $canBubble
    set myCancelable $cancelable
  }

  dom_snit {

    # The event-type. i.e. "click" or "load". Returned by the 
    # Event.type interface.
    option -eventtype -default ""

    option -preventdefault -default false

    # Objects for the Event.target and Event.currentTarget properties.
    #
    option -target         -default ""
    option -currenttarget  -default ""

    variable myCanBubble  1
    variable myCancelable 0

    method bubbles {} {return $myCanBubble}

    method Event_initEvent {eventType canBubble cancelable} {
      set options(-eventtype)  $eventType
      set myCanBubble  $canBubble
      set myCancelable $cancelable
      return ""
    }

    variable myStopPropagationCalled 0
    method stoppropagationcalled {} {return $myStopPropagationCalled}

    # The event phase, as returned by the Event.eventPhase interface
    # must be set to either 1, 2 or 3. Setting this option is done
    # by code in the [doDispatchEvent] method of the EventListener
    # interface.
    option -eventphase -default 1
  }
}

#-------------------------------------------------------------------------
# DocumentEvent (DOM Level 2 Events)
#
#     This interface is mixed into the Document object. It provides
#     a single method, used to create a new event object:
#
#         createEvent()
#
::hv3::dom::type DocumentEvent {} {

  # The DocumentEvent.createEvent() method. The argument (specified as
  # type DOMString in the spec) should be one of the following:
  #
  #     "HTMLEvents"
  #     "UIEvents"
  #     "MouseEvents"
  #     "MutationEvents"
  #
  dom_call -string createEvent {THIS eventType} {

    switch -- $eventType {
      HTMLEvents {
        list object [::hv3::DOM::Event %AUTO% $myDom]
      }

      MouseEvents {
        list object [::hv3::DOM::MouseEvent %AUTO% $myDom]
      }
    }

  }
}

::hv3::dom::type MouseEvent {UIEvent Event} {

  dom_call initMouseEvent {THIS 
      eventtype canBubble cancelable view detail
      screenX screenY clientX clientY 
      ctrlKey altKey shiftKey metaKey 
      button relatedTarget
  } {
      $self Event_initEvent \
          [lindex $eventtype 1] [lindex $canBubble 1] [lindex $cancelable 1]
  }

  dom_snit {
    option -button -default 0
    option -x
    option -y
  }

  dom_get button { list number $options(-button) }
  dom_get which  { list number [expr {$options(-button) + 1}]}

  dom_get clientX { list number $options(-x) }
  dom_get clientY { list number $options(-y) }
}

::hv3::dom::type MutationEvent {Event} {

  dom_call initMutationEvent {THIS
      eventtype canBubble cancelable 
      relatedNode prevValue newValue attrName attrChange
  } {
  }

}

::hv3::dom::type UIEvent {Event} {
  dom_call initUIEvent {THIS eventtype canBubble cancelable view detail} {
  }
}

# Recognised mouse event types.
#
#     Mapping is from the event-type to the value of the "cancelable"
#     property of the DOM MouseEvent object.
#
set ::hv3::dom::MouseEventType(click)     1
set ::hv3::dom::MouseEventType(mousedown) 1
set ::hv3::dom::MouseEventType(mouseup)   1
set ::hv3::dom::MouseEventType(mouseover) 1
set ::hv3::dom::MouseEventType(mousemove) 0
set ::hv3::dom::MouseEventType(mouseout)  1

# dispatchMouseEvent --
#
#     $dom         -> the ::hv3::dom object
#     $eventtype   -> One of the above event types, e.g. "click".
#     $EventTarget -> The DOM object implementing the EventTarget interface
#     $x, $y       -> Widget coordinates for the event
#
proc ::hv3::dom::dispatchMouseEvent {dom eventtype EventTarget x y extra} {

  set isCancelable $::hv3::dom::MouseEventType($eventtype)

  set event [::hv3::DOM::MouseEvent %AUTO% $dom -x $x -y $y]
  $event Event_initEvent $eventtype 1 $isCancelable
  $event configurelist $extra

  set evt [$dom setWindowEvent $event]
  $EventTarget doDispatchEvent $event
  $dom setWindowEvent $evt
}

# Recognised HTML event types.
#
#     Mapping is from the event-type to the value of the "cancelable"
#     property of the DOM Event object.
#
set ::hv3::dom::HtmlEventType(load)     0
set ::hv3::dom::HtmlEventType(submit)   1

# dispatchHtmlEvent --
#
#     $dom         -> the ::hv3::dom object
#     $EventTarget -> The DOM object implementing the EventTarget interface
#
#     Dispatch one of the following events:
#
#       load
#       submit
#
proc ::hv3::dom::dispatchHtmlEvent {dom type EventTarget} {
  set cancelable $::hv3::dom::HtmlEventType($type)
  set event [::hv3::DOM::Event %AUTO% $dom]
  $event Event_initEvent $type 0 $cancelable

  set evt [$dom setWindowEvent $event]
  set rc [$EventTarget doDispatchEvent $event]
  $dom setWindowEvent $evt

  # TODO: Memory management for the event object.

  return $rc
}

