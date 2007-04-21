namespace eval hv3 { set {version($Id: hv3_dom.tcl,v 1.38 2007/04/21 08:54:48 danielk1977 Exp $)} 1 }

#--------------------------------------------------------------------------
# Global interfaces in this file:
#
#     [::hv3::dom::have_scripting]
#         This method returns true if scripting is available, otherwise 
#         false. Scripting is available if the command [::see::interp]
#         is available (see file hv3see.c).
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
# Class ::hv3::dom
#
#     set dom [::hv3::dom %AUTO% $hv3]
#
#     $dom script   ATTR SCRIPT
#     $dom noscript ATTR SCRIPT
#
#     $dom javascript SCRIPT
#     $dom event EVENT NODE
#     $dom reset
#
#     $dom destroy
#
#  Debugging aids:
#     $dom javascriptlog
#     $dom eventreport NODE
#
#
snit::type ::hv3::dom {
  variable mySee ""

  # Boolean option. Enable this DOM implementation or not.
  option -enable -default 0

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

  variable myNextCodeblockNumber 1


  constructor {hv3 args} {
    set myHv3 $hv3

    set myLogData [::hv3::dom::logdata %AUTO% $self]

    $self configurelist $args
    $self reset
  }

  destructor { 
    catch { $myLogData destroy }
  }


  # Return true if the Tclsee extension is available and 
  # the user has foolishly enabled it.
  method HaveScripting {} {
    return $options(-enable)
  }

  method reset {} {

    # Delete the old interpreter and the various objects, if they exist.
    # They may not exist, if this is being called from within the
    # object constructor or scripting is disabled.
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

    if {[$self HaveScripting]} {
      # Set up the new interpreter with the global "Window" object.
      set mySee [::see::interp]
      set myWindow [::hv3::DOM::Window %AUTO% $self -see $mySee -hv3 $myHv3]
      $mySee global $myWindow 

      # Reset the debugger.
      $self LogReset
    }
  }


  method NewFilename {} {
    return "blob[incr myNextCodeblockNumber]"
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
    if {$mySee ne ""} {
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
        # $self Log "Dispatched script request - $handle" "" "" ""
      } else {
        return [$self scriptCallback $attr "" $script]
      }
    }
    return ""
  }

  # Script handler for <noscript> elements. If javascript is enabled,
  # do nothing (meaning don't process the contents of the <noscript>
  # block). On the other hand, if js is disabled, feed the contents
  # of the <noscript> block back to the parser using the same
  # Tcl interface used for document.write().
  #
  method noscript {attr script} {
    if {$mySee ne ""} {
      return ""
    } else {
      [$myHv3 html] write text $script
    }
  }
  
  # If a <SCRIPT> element has a "src" attribute, then the [script]
  # method will have issued a GET request for it. This is the 
  # successful callback.
  #
  method scriptCallback {attr downloadHandle script} {
    if {$downloadHandle ne ""} { 
      $downloadHandle destroy 
    }

    if {$::hv3::dom::reformat_scripts_option} {
      set script [string map {"\r\n" "\n"} $script]
      set script [string map {"\r" "\n"} $script]
      set script [::see::format $script]
    }

    set name [$self NewFilename]
    set rc [catch {$mySee eval -file $name $script} msg]

    set attributes ""
    foreach {a v} $attr {
      append attributes " [htmlize $a]=\"[htmlize $v]\""
    }
    set title "<SCRIPT$attributes>"
    $myLogData Log $title $name $script $rc $msg

    $myHv3 write continue
  }

  method javascript {script} {
    set msg ""
    if {$mySee ne ""} {
      set name [$self NewFilename]
      set rc [catch {$mySee eval -file $name $script} msg]
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
    if {$mySee eq ""} {return ""}

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
      if {$rc} {
        set name [string map {blob error} [$self NewFilename]]
        $self Log "$node $event event" $name "event-handler" $rc $msg
      }
    }
  }

  # This method is called by the ::hv3::mousemanager object to 
  # dispatch a mouse-event into DOM country.
  #
  method mouseevent {event node x y args} {
    if {$mySee eq ""} {return 1}

    # This can happen if the node is deleted by a DOM event handler
    # invoked by the same logical GUI event as this DOM event.
    #
    if {"" eq [info commands $node]} {return 1}

    if {$node eq ""} {
      set Node [lindex [$myWindow Get document] 1]
    } else {
      set Node [$self node_to_dom $node]
    }

    set rc [catch {
      ::hv3::dom::dispatchMouseEvent $self $event $Node $x $y $args
    } msg]
    if {$rc} {
      set name [string map {blob error} [$self NewFilename]]
      $myLogData Log "$node $event event" $name "event-handler" $rc $msg
      $myLogData Popup
    }
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
  variable myLogData

  # This variable contains the current javascript debugging log in HTML 
  # form. It is appended to by calls to [Log] and truncated to an
  # empty string by [LogReset]. If the debugging window is displayed,
  # the contents are identical to that of this variable.
  #
  variable myLogDocument ""

  method Log {heading script rc result} {
    $myLogData Log $heading $script $rc $result
    return
  }


  method LogReset {} {
    $myLogData Reset
    return
  }

  method javascriptlog {} {
    $myLogData Popup
    return
  }

  # Called by the tree-browser to get event-listener info for the
  # javascript object associated with the specified tkhtml node.
  #
  method eventdump {node} {
    if {$mySee eq ""} {return ""}
    set Node [$self node_to_dom $node]
    $Node eventdump
  }
}

#-----------------------------------------------------------------------
# ::hv3::dom::logdata
# ::hv3::dom::logscript
#
#     Javascript debugger state.
#
# ::hv3::dom::logwin
#
#     Toplevel window widget that implements the javascript debugger.
#
snit::type ::hv3::dom::logscript {
  option -rc      -default ""
  option -heading -default "" 
  option -script  -default "" 
  option -result  -default "" 
  option -name    -default "" 
}

snit::type ::hv3::dom::logdata {
  variable myDom ""
  variable myLogScripts [list]
  variable myWindow ""

  constructor {dom} {
    set myDom $dom
    set myWindow ".[string map {: _} $self]_logwindow"
  }

  method Log {heading name script rc result} {
    set ls [::hv3::dom::logscript %AUTO% \
      -rc $rc -name $name -heading $heading -script $script -result $result
    ]
    lappend myLogScripts $ls
  }

  method Reset {} {
    foreach ls $myLogScripts {
      $ls destroy
    }
    set myLogScripts [list]
  }

  method Popup {} {
    if {![winfo exists $myWindow]} {
      ::hv3::dom::logwin $myWindow $self
    } 
    wm state $myWindow normal
    raise $myWindow
    $myWindow Populate
  }

  destructor {
    $self Reset
  }


  method GetList {} {
    return $myLogScripts
  }

  method Evaluate {script} {
    set res [$myDom javascript $script]
    return $res
  }

  method BrowseToNode {node} {
    ::HtmlDebug::browse [$myDom hv3] $node
  }
}

snit::widget ::hv3::dom::searchbox {

  variable myLogwin 

  constructor {logwin} {
    ::hv3::label ${win}.label
    ::hv3::scrolled listbox ${win}.listbox

    set myLogwin $logwin

    pack ${win}.label   -fill x
    pack ${win}.listbox -fill both -expand true

    ${win}.listbox configure -background white
    bind ${win}.listbox <<ListboxSelect>> [mymethod Select]
  }

  method Select {} {
    set idx  [lindex [${win}.listbox curselection] 0]
    set link [${win}.listbox get $idx]
    set link [string range $link 0 [expr [string first : $link] -1]]
    $myLogwin GotoCmd -silent $link
    ${win}.listbox selection set $idx
  }

  method Search {str} {
    ${win}.listbox delete 0 end

    set nHit 0
    foreach ls [$myLogwin GetList] {
      set blobid [$ls cget -name]
      set iLine 0
      foreach line [split [$ls cget -script] "\n"] {
        incr iLine
        if {[string first $str $line]>=0} {
          ${win}.listbox insert end "$blobid $iLine: $line"
          incr nHit
        }
      }
    }

    return $nHit
  }
}

snit::widget ::hv3::dom::stacktrace {

  variable myLogwin ""

  constructor {logwin} {
    ::hv3::label ${win}.label
    ::hv3::scrolled listbox ${win}.listbox

    set myLogwin $logwin

    pack ${win}.label -fill x
    pack ${win}.listbox -fill both -expand true

    ${win}.listbox configure -background white
    bind ${win}.listbox <<ListboxSelect>> [mymethod Select]
  }

  method Select {} {
    set idx  [lindex [${win}.listbox curselection] 0]
    set link [${win}.listbox get $idx]
    $myLogwin GotoCmd -silent $link
    ${win}.listbox selection set $idx
  }

  method Populate {title stacktrace} {
    ${win}.label configure -text $title
    ${win}.listbox delete 0 end
    foreach {blobid lineno calltype callname} $stacktrace {
      ${win}.listbox insert end "$blobid $lineno"
    }
  }
}

snit::widget ::hv3::dom::logwin {
  hulltype toplevel

  # Internal widgets from left-hand pane:
  variable myFileList ""                            ;# Listbox with file-list
  variable mySearchbox ""                           ;# Search results
  variable myStackList ""                           ;# Stack trace widget.

  variable myCode ""
  variable myCodeTitle ""

  variable myInput ""
  variable myOutput ""

  # ::hv3::dom::logdata object
  variable myData

  # Index in $myFileList of currently displayed file:
  variable myCurrentIdx 0

  # Current point in stack trace.
  variable myTraceFile ""
  variable myTraceLineno ""

  variable myStack ""

  constructor {data} {
    
    set myData $data

    panedwindow ${win}.pan -orient horizontal
    panedwindow ${win}.pan.right -orient vertical

    set nb [::hv3::tile_notebook ${win}.pan.left]
    set myFileList [::hv3::scrolled listbox ${win}.pan.left.files]

    set mySearchbox [::hv3::dom::searchbox ${win}.pan.left.search $self]
    set myStackList [::hv3::dom::stacktrace ${win}.pan.left.stack $self]

    $nb add $myFileList  -text "Files"
    $nb add $mySearchbox -text "Search"
    $nb add $myStackList -text "Stack"

    $myFileList configure -bg white
    bind $myFileList <<ListboxSelect>> [mymethod PopulateText]

    frame ${win}.pan.right.top 
    set myCode [::hv3::scrolled ::hv3::text ${win}.pan.right.top.code]
    set myCodeTitle [::hv3::label ${win}.pan.right.top.label]
    pack $myCodeTitle -fill x
    pack $myCode -fill both -expand 1
    $myCode configure -bg white
    $myCode tag configure linenumber -foreground darkblue
    $myCode tag configure tracepoint -background skyblue
    $myCode tag configure stackline -background wheat

    frame ${win}.pan.right.bottom 
    set myOutput [::hv3::scrolled ::hv3::text ${win}.pan.right.bottom.output]
    set myInput  [::hv3::text ${win}.pan.right.bottom.input -height 3]
    $myInput configure -bg white
    $myOutput configure -bg white -state disabled
    bind $myInput <Return> [list after idle [mymethod Evaluate]]
    $myOutput tag configure commandtext -foreground darkblue

    pack $myInput -fill x -side bottom
    pack $myOutput -fill both -expand 1

    ${win}.pan add ${win}.pan.left -width 200
    ${win}.pan add ${win}.pan.right

    ${win}.pan.right add ${win}.pan.right.top  -height 300 -width 600
    ${win}.pan.right add ${win}.pan.right.bottom -height 250

    pack ${win}.pan -fill both -expand 1

    bind ${win} <Escape> [list destroy ${win}]

    focus $myInput
    $myInput insert end "help"
    $self Evaluate
  }

  method GetList {} { return [$myData GetList] }

  method Populate {} {
    $myFileList delete 0 end

    # Populate the "Files" list-box.
    #
    foreach ls [$myData GetList] {
      set name    [$ls cget -name] 
      set heading [$ls cget -heading] 
      set rc   [$ls cget -rc] 

      $myFileList insert end "$name - $heading"

      if {$name eq $myTraceFile} {
        $myFileList selection set end
      }
  
      if {$rc} {
        $myFileList itemconfigure end -foreground red -selectforeground red
      }
    }
  }

  method PopulateText {} {
    $myCode configure -state normal
    $myCode delete 0.0 end
    set idx [lindex [$myFileList curselection] 0]
    if {$idx ne ""} {
      set ls [lindex [$myData GetList] $idx]
      $myCodeTitle configure -text [$ls cget -heading]

      set name [$ls cget -name]
      set stacklines [list]
      foreach {n l X Y} $myStack {
        if {$n eq $name} { lappend stacklines $l }
      }

      set script [$ls cget -script]
      set N 1
      foreach line [split $script "\n"] {
        $myCode insert end [format "% 5d   " $N] linenumber
        if {[$ls cget -name] eq $myTraceFile && $N == $myTraceLineno} {
          $myCode insert end "$line\n" tracepoint
        } elseif {[lsearch $stacklines $N] >= 0} {
          $myCode insert end "$line\n" stackline
        } else {
          $myCode insert end "$line\n"
        }
        incr N
      }
      set myCurrentIdx $idx
    }

    # The following line throws an error if no chars are tagged "tracepoint".
    catch { $myCode yview -pickplace tracepoint.first }
    $myCode configure -state disabled
  }

  # This is called when the user issues a [result] command.
  #
  method ResultCmd {cmd} {
    # The (optional) argument is one of the blob names. If
    # it is not specified, use the currently displayed js blob 
    # as a default.
    #
    set arg [lindex $cmd 0]
    if {$arg eq ""} {
      set idx $myCurrentIdx
      if {$idx eq ""} {set idx [llength [$myData GetList]]}
    } else {
      set idx 0
      foreach ls [$myData GetList] {
        if {[$ls cget -name] eq $arg} break
        incr idx
      }
    }
    set ls [lindex [$myData GetList] $idx]
    if {$ls eq ""} {
      error "No such file: \"$arg\""
    }

    # Echo the command to the output panel.
    #
    $myOutput insert end "result: [$ls cget -name]\n" commandtext

    $myOutput insert end "   rc    : [$ls cget -rc]\n"
    if {[$ls cget -rc] && [lindex [$ls cget -result] 0] eq "JS_ERROR"} {
      set res [$ls cget -result]
      set msg [lindex $res 1]
      set stacktrace [lrange $res 2 end]
      set stack ""
      foreach {file lineno a b} $stacktrace {
        set stack "-> $file:$lineno $stack"
      }
      $myOutput insert end "   result: $msg\n"
      $myOutput insert end "   stack:  [string range $stack 3 end]\n"

      set blobid ""
      set r [$ls cget -result]
      regexp {(blob[[:digit:]]*):([[:digit:]]*):} $r X blobid lineno

      if {$blobid ne ""} {
        set stacktrace [linsert $stacktrace 0 $blobid $lineno "" ""]
      }

      if {[llength $stacktrace] > 0} {
        set blobid [lindex $stacktrace 0]
        set lineno [lindex $stacktrace 1]
        set myStack $stacktrace
        $myStackList Populate "Result of [$ls cget -name]:" $stacktrace
        [winfo parent $myStackList] select $myStackList
      }

      if {$blobid ne ""} {
        $self GotoCmd -silent [list $blobid $lineno]
      }
      
    } else {
      $myOutput insert end "   result: [$ls cget -result]\n"
    }
  }
 
  # This is called when the user issues a [clear] command.
  #
  method ClearCmd {cmd} {
    $myOutput delete 0.0 end
  }

  # This is called when the user issues a [javascript] command.
  #
  method JavascriptCmd {cmd} {
    set js [string trim $cmd]
    set res [$myData Evaluate $js]
    $myOutput insert end "javascript: $js\n" commandtext
    $myOutput insert end "    [string trim $res]\n"
  }

  method GotoCmd {cmd {cmd2 ""}} {
    if {$cmd2 ne ""} {set cmd $cmd2}
    set blobid [lindex $cmd 0]
    set lineno [lindex $cmd 1]
    if {$lineno eq ""} {
      set lineno 1
    }

    if {$cmd2 eq ""} {
      $myOutput insert end "goto: $blobid $lineno\n" commandtext
    }

    set idx 0
    set ok 0
    foreach ls [$myData GetList] {
      if {[$ls cget -name] eq $blobid} {set ok 1 ; break}
      incr idx
    }

    if {!$ok} {
      $myOutput insert end "        No such blob: $blobid"
      return
    }

    set myTraceFile $blobid
    set myTraceLineno $lineno
    if {$cmd2 eq ""} {
      $self Populate
    } else {
      $myFileList selection clear 0 end
      $myFileList selection set $idx
    }
    $self PopulateText
  }

  method ErrorCmd {cmd} {
  }

  method SearchCmd {cmd} {
    set n [$mySearchbox Search $cmd]
    ${win}.pan.left select $mySearchbox
    $myOutput insert end "search: $cmd\n" commandtext
    $myOutput insert end "    $n hits.\n"
  }

  method TreeCmd {node} {
    $myOutput insert end "tree: $node\n" commandtext
    $myData BrowseToNode $node
  }

  method Evaluate {} {
    set script [string trim [$myInput get 0.0 end]]
    $myInput delete 0.0 end
    $myInput mark set insert 0.0

    set idx [string first " " $script]
    if {$idx < 0} {
      set idx [string length $script]
    }
    set zWord [string range $script 0 [expr $idx-1]]
    set nWord [string length $zWord]

    $myOutput configure -state normal

    set cmdlist [list \
      js         1 "JAVASCRIPT..."        JavascriptCmd \
      result     1 "BLOBID"               ResultCmd     \
      clear      1 ""                     ClearCmd      \
      goto       1 "BLOBID ?LINE-NUMBER?" GotoCmd       \
      search     1 "STRING"               SearchCmd     \
      tree       1 "NODE"                 TreeCmd       \
    ]
    set done 0
    foreach {cmd nMin NOTUSED method} $cmdlist {
      if {$nWord>=$nMin && [string first $zWord $cmd]==0} {
        $self $method [string trim [string range $script $nWord end]]
        set done 1
        break
      }
    }
    
    if {!$done} {
        # Command "help"
        #
        #     Print debugger usage instructions
        $myOutput insert end "help:\n" commandtext
        foreach {cmd NOTUSED help NOTUSED} $cmdlist {
        $myOutput insert end "          $cmd $help\n"
      }
      set x "Unambiguous prefixes of the above commands are also accepted.\n"
      $myOutput insert end "        $x"
    }

    $myOutput yview -pickplace end
    $myOutput insert end "\n"
    $myOutput configure -state disabled
  }
}
#-----------------------------------------------------------------------

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

#set ::hv3::dom::reformat_scripts_option 0
set ::hv3::dom::reformat_scripts_option 1

