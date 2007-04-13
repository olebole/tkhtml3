namespace eval hv3 { set {version($Id: hv3_dom.tcl,v 1.31 2007/04/13 11:44:43 danielk1977 Exp $)} 1 }

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

    # Mouse events:
    foreach e [list onclick onmouseout onmouseover \
        onmouseup onmousedown onmousemove ondblclick
    ] {
      # $myHv3 Subscribe $e [mymethod event $e]
    }

    $self reset
  }

  destructor { 
    catch { $myLogData destroy }
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
        set myWindow [::hv3::DOM::Window %AUTO% $self -see $mySee -hv3 $myHv3]
        $mySee global $myWindow 
      }

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
        # $self Log "Dispatched script request - $handle" "" "" ""
      } else {
        return [$self scriptCallback $attr "" $script]
      }
    }
    return ""
  }

  method noscript {attr script} {
    if {[::hv3::dom::use_scripting] && $mySee ne ""} {
      return ""
    } else {
      return $script
    }
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

    set name [$self NewFilename]
    set rc [catch {$mySee eval -file $name $script} msg]

    set attributes ""
    foreach {a v} $attr {
      append attributes " [htmlize $a]=\"[htmlize $v]\""
    }
    set title "<SCRIPT$attributes> $downloadHandle"
    $myLogData Log $title $name $script $rc $msg

    $myHv3 write continue
  }

  method javascript {script} {
    set msg ""
    if {[::hv3::dom::use_scripting] && $mySee ne ""} {
      set name [$self NewFilename]
      set rc [catch {$mySee eval -file $name $script} msg]
      # $self Log "javascript: name=$name" $script $rc $msg
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
      # $self Log "$node $event" [$mySee tostring $eventobj] $rc $msg
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
}

snit::widget ::hv3::dom::logwin {
  hulltype toplevel

  # Internal widgets:
  variable myFileList ""
  variable myCode ""
  variable myCodeTitle ""

  variable myInput ""
  variable myOutput ""

  # ::hv3::dom::logdata object
  variable myData

  constructor {data} {
    
    set myData $data

    panedwindow ${win}.pan -orient horizontal
    panedwindow ${win}.pan.right -orient vertical

    set myFileList [::hv3::scrolled listbox ${win}.pan.files]
    $myFileList configure -bg white
    bind $myFileList <<ListboxSelect>> [mymethod PopulateText]

    frame ${win}.pan.right.top 
    set myCode [::hv3::scrolled ::hv3::text ${win}.pan.right.top.code]
    set myCodeTitle [::hv3::label ${win}.pan.right.top.label]
    pack $myCodeTitle -fill x
    pack $myCode -fill both -expand 1
    $myCode configure -bg white
    $myCode tag configure linenumber -foreground darkblue

    frame ${win}.pan.right.bottom 
    set myOutput [::hv3::scrolled ::hv3::text ${win}.pan.right.bottom.output]
    set myInput  [::hv3::text ${win}.pan.right.bottom.input -height 3]
    $myInput configure -bg white
    $myOutput configure -bg white -state disabled
    bind $myInput <Return> [list after idle [mymethod Evaluate]]
    $myOutput tag configure commandtext -foreground darkblue

    pack $myInput -fill x -side bottom
    pack $myOutput -fill both -expand 1

    ${win}.pan add ${win}.pan.files -width 200
    ${win}.pan add ${win}.pan.right

    ${win}.pan.right add ${win}.pan.right.top  -height 300 -width 600
    ${win}.pan.right add ${win}.pan.right.bottom -height 250

    pack ${win}.pan -fill both -expand 1

    bind ${win} <Escape> [list destroy ${win}]

    focus $myInput
    $myInput insert end "help"
    $self Evaluate
  }

  method Populate {} {
    $myFileList delete 0 end

    foreach ls [$myData GetList] {
      set name [$ls cget -name] 
      set rc [$ls cget -rc] 
      $myFileList insert end $name
  
      if {$rc} {
        $myFileList itemconfigure end -foreground red
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

      set script [$ls cget -script]
      set N 1
      foreach line [split $script "\n"] {
        $myCode insert end [format "% 5d   " $N] linenumber
        $myCode insert end "$line\n"
        incr N
      }
    }
    $myCode configure -state disabled
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

    if     {$nWord>=1 && [string first $zWord javascript]==0} {
      # Command "javascript"
      #
      #     Evaluate a javascript script.
      set js [string trim [string range $script $nWord end]]
      set res [$myData Evaluate $js]
      $myOutput insert end "javascript: $js\n" commandtext
      $myOutput insert end "    [string trim $res]\n"
    } \
    elseif {$nWord>=1 && [string first $zWord result]==0} {
      # Command "result"
      #
      #     Retrieve the result for previously evaluated <script> block.
      set arg [lindex $script 1]
      
      $myOutput insert end "result: $arg\n" commandtext
    } else {
      # Command "help"
      #
      #     Print debugger usage instructions
      
      $myOutput insert end "help:" commandtext
      $myOutput insert end {
        help
        javascript JAVASCRIPT...
        result BLOBID

    Unambiguous prefixes of the above commands are also accepted.}
      $myOutput insert end "\n"
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
proc ::hv3::dom::use_scripting {} {
  set r [expr [::hv3::dom::have_scripting]&&$::hv3::dom::use_scripting_option]
  return $r
}

#set ::hv3::dom::reformat_scripts_option 0
set ::hv3::dom::use_scripting_option 1
set ::hv3::dom::reformat_scripts_option 1

