namespace eval hv3 { set {version($Id: hv3_dom_compiler.tcl,v 1.13 2007/05/14 02:45:05 danielk1977 Exp $)} 1 }

#--------------------------------------------------------------------------
# This file implements infrastructure used to create the Snit objects
# that implement the DOM objects. It adds the following command that
# may be used to declare DOM object classes, in the same way as 
# [::snit::type] is used to declare new Snit types:
#
#     ::hv3::dom::type TYPE-NAME BASE-TYPE-LIST BODY
#

package require snit

proc ::hv3::dom::ToString {pSee js_value} {
  switch -- [lindex $js_value 0] {
    undefined {return "undefined"}
    null      {return "null"}
    object    {
      set val [$pSee [lindex $js_value 1] DefaultValue]
      if {[lindex $val 1] eq "object"} {error "DefaultValue is object"}
      return [::hv3::dom::ToString $val]
    }
  }

  # Handles "boolean", "number" and "string".
  return [lindex $js_value 1]
}

# This proc is used in place of a full [dom_object] for callable
# functions. i.e. the object returned by a Get on "document.write".
#
# Arguments:
#
#     $pSee     - Name of SEE interpreter command (returned by [::see::interp])
#     $isString - True to convert arguments to strings
#     $zScript  - Tcl script to invoke if this is a "Call"
#     $op       - SEE/Tcl op (e.g. "Call", "Get", "Put" etc.)
#     $args     - Args to SEE/Tcl op.
#
proc ::hv3::dom::TclCallableProc {pSee isString zScript op args} {
  switch -- $op {
    Get { return "" }

    Put { return "native" }

    Call {
      set THIS [lindex $args 0]
      if {$isString} {
        foreach js_value [lrange $args 1 end] { 
          lappend A [::hv3::dom::ToString $pSee $js_value] 
        }
      } else {
        set A [lrange $args 1 end]
      }
      return [eval $zScript [list $THIS] $A]
    }

    Construct {
      error "Cannot call this as a constructor"
    }

    Finalize {
      # A no-op. There is no state data.
      return
    }
  }

  error "Unknown method: $op"
}

proc ::hv3::dom::TclConstructable {pSee zScript op args} {
  switch -- $op {
    Get { return "" }

    Put { return "native" }

    Construct {
      return [eval $zScript $args]
    }

    Call {
      error "Cannot call this object"
    }

    Finalize {
      # A no-op. There is no state data.
      return
    }
  }

  error "Unknown method: $op"
}

#--------------------------------------------------------------------------
# DOM objects are defined using the following command:
#
#     ::hv3::dom::type TYPE-NAME BASE-TYPE-LIST BODY
#
# This is a wrapper around [::snit::type]. The following commands are 
# supported within the BODY block:
#
#     dom_get ?-cache? PROPERTY CODE
#
#     dom_propertylist ?-cache? CODE
#
#     dom_put          ?-strings? PROPERTY ARG-NAME CODE
#     dom_call         ?-strings? PROPERTY ARG-LIST CODE
#
#     dom_snit         CODE
#
#     dom_construct    CODE
#     dom_destruct     CODE
#     dom_finalize     CODE
#
namespace eval ::hv3::dom {

  ::variable BaseArray
  ::variable TypeArray
  ::variable CurrentType

  array set TypeArray ""
  set CurrentType ""

  proc type {type_name base_list body} {

    ::variable TypeArray
    ::variable BaseArray
    ::variable CurrentType

    if {![info exists TypeArray($type_name)]} {
      set TypeArray($type_name) [::hv3::dom::typecompiler %AUTO% $type_name]
      set BaseArray($type_name) $base_list 
    }

    set CurrentType $TypeArray($type_name)
    namespace eval compiler $body
    set CurrentType ""
  }

  namespace eval compiler {
    proc dom_snit {code} {
      $::hv3::dom::CurrentType add_snit $code
    }

    proc dom_todo {property} {
      dom_get $property [subst -nocommands {
        puts "TODO: [[set self] info type].$property"
        list
      }]
    }

    proc dom_call_todo {property} {
      dom_call $property {args} [subst -nocommands {
        puts "TODO: [[set self] info type].${property}()"
        list
      }]
    }

    proc dom_get {args} {
      if {[llength $args] == 2} {
        set isCache 0
        foreach {property code} $args {}
      } elseif {[llength $args] == 3 && [lindex $args 0] eq "-cache"} {
        set isCache 1
        foreach {dummy property code} $args {}
      } else {
        error "Invalid args to dom_get: $args"
      }

      $::hv3::dom::CurrentType add_get $isCache $property $code
    }

    # dom_put ?-string? PROPERTY ARG-NAME CODE
    #
    proc dom_put {args} {
      if {[llength $args] == 3} {
        set isString 0
        foreach {property arg_name code} $args {}
      } elseif {[llength $args] == 4 && [lindex $args 0] eq "-string"} {
        set isString 1
        foreach {dummy property arg_name code} $args {}
      } else {
        error "Invalid args to dom_put: $args"
      }

      $::hv3::dom::CurrentType add_put $isString $property $arg_name $code
    }

    # dom_call ?-string? PROPERTY ARG-LIST CODE
    #
    proc dom_call {args} {

      # Process arguments.
      if {[llength $args] == 3} {
        set isString 0
        foreach {property arg_list code} $args {}
      } elseif {[llength $args] == 4 && [lindex $args 0] eq "-string"} {
        set isString 1
        foreach {dummy property arg_list code} $args {}
      } else {
        error "Invalid args to dom_call: $args"
      }

      # Use [dom_get] to create the property in the javascript object.
      dom_get -cache $property [subst -nocommands {
        list object [list                                       \
          ::hv3::dom::TclCallableProc                           \
          [[set myDom] see] $isString [mymethod call_$property] \
        ]
      }]

      # Create a method in the Snit object to implement the function call.
      $::hv3::dom::CurrentType add_snit [list \
        method call_$property $arg_list $code
      ]

    }

    proc dom_construct {property arg_list code} {
      # Use [dom_get] to create the property in the javascript object.
      dom_get -cache $property [subst -nocommands {
        list object [list                                            \
          ::hv3::dom::TclConstructable                               \
          [[set myDom] see] [mymethod construct_$property]           \
        ]
      }]

      # Create a method in the Snit object to implement the function call.
      $::hv3::dom::CurrentType add_snit [list \
        method construct_$property $arg_list $code
      ]
    }

    proc dom_finalize {code} {
      $::hv3::dom::CurrentType add_finalizer $code
    }
  }

  proc reverse_foreach {var list body} {
    for {set ii [expr {[llength $list] - 1}]} {$ii >= 0} {incr ii -1} {
      uplevel [list set $var [lindex $list $ii]]
      uplevel $body
    }
  }

  # Figure out the base-class list for this type. The base-class list
  # should be in order from lowest to highest priority. i.e. if
  # constructing the following hierachy:
  #
  #             Node
  #              |
  #           Element
  #              |
  #         HTMLElement
  #
  # the base class list for HTMLElement should be {Node Element}.
  #
  proc getBaseList {domtype} {
    ::variable TypeArray
    ::variable BaseArray

    if {![info exists TypeArray($domtype)]} {
      error "No such DOM type: $domtype"
    }

    set base_list ""
    reverse_foreach base $BaseArray($domtype) {
      if {![info exists TypeArray($base)]} {
        error "No such DOM type: $base"
      }

      eval lappend base_list [getBaseList $base]
      lappend base_list $TypeArray($base)
    }
    return $base_list
  }

  proc compile {domtype} {

    ::variable TypeArray

    set base_list [getBaseList $domtype]

    set ret ""
    append ret [$TypeArray($domtype) compile $base_list]
    append ret "\n"

    return $ret
  }

  proc cleanup {} {
    ::variable TypeArray
    foreach {name type} [array get TypeArray] {
      $type destroy
    }
    array set TypeArray ""
  }
}

::snit::type ::hv3::dom::typecompiler {

  # Map of properties configured with a [dom_get]
  #
  #     property-name -> [list IS-CACHE CODE]
  #
  # where IS-CACHE is a boolean variable indicating whether or not
  # the -cache switch was specified.
  #
  variable myGet -array [list]

  # Map of properties configured with a [dom_put]
  #
  #     property-name -> [list IS-STRING ARG-NAME CODE]
  #
  # where IS-STRING is a boolean variable indicating whether or not
  # the -string switch was specified.
  #
  variable myPut -array [list]

  # Map of object methods. More accurately: Map of callable properties.
  #
  #     property-name -> [list IS-STRING ARG-LIST CODE]
  #
  # where IS-STRING is a boolean variable indicating whether or not
  # the -string switch was specified.
  #
  variable myCall -array [list]

  # List of snit blocks to add to the object definition
  #
  variable mySnit [list]
  variable myFinalizer [list]

  # Name of this type - i.e. "HTMLDocument".
  variable myName 
  method name {} {set myName}

  constructor {name} {
    set myName $name
  }

  method add_get {isCache property code} {
    set myGet($property) [list $isCache $code]
  }
  method add_put {isString property argname code} {
    set myPut($property) [list $isString $argname $code]
  }
  method add_snit {code} {
    lappend mySnit $code
  }
  method add_finalizer {code} {
    lappend myFinalizer $code
  }

  method get {} { return [array get myGet] }
  method put {} { return [array get myPut] }
  method snit {} { return $mySnit }
  method final {} { return $myFinalizer }

  method CompilePut {mixins} {
    set Put {
        method Put {property value} {
          $DEBUG
          switch -- [set property] {
            $SWITCHBODY
            default {
              $NATIVE
            }
          }
        }
    }

    set DEBUG ""
    # append DEBUG { puts "$self Put $property {$value}" }

    array set put_array ""
    foreach t [concat $mixins $self] {
      array set put_array [$t put]
    }

    set SWITCHBODY ""
    foreach {name value} [array get put_array] {
      foreach {isString argname code} $value {}
      if {$isString} {
        set put_code "set $argname \[\[\$myDom see\] tostring \$value\]"
      } else {
        set put_code "set $argname \$value\n"
      }
      append put_code "\n$code\n"
      append put_code return
      append SWITCHBODY "[list $name $put_code]\n            "
    }

    set NATIVE "return native"

    set Put [subst -nocommands $Put]
    return $Put
  }

  method CompileGet {mixins} {
    set Get {
        variable myGetCache -array ""
        method Get {property} {
          set isExplicit 1
          set result [switch -- [set property] {
            $SWITCHBODY
            default {
              set isExplicit 0
              list
              $DEFAULT
            }
          }]
          $DEBUG
          set result
        }
    }

    set DEBUG {}
    # append DEBUG { puts "$self Get $property -> $result" }

    set SWITCHBODY ""
    foreach t [concat $mixins $self] {
      array set get_array [$t get]
    }
    foreach {name value} [array get get_array] {
      if {$name ne "*"} {
        foreach {isCache code} $value {}
        if {$isCache} {
          set code [subst -nocommands {
              if {[info exists myGetCache($name)]} {
                set myGetCache($name)
              } else {
                set myGetCache($name) [$code]
              }
            }]
        }
        append SWITCHBODY "[list $name $code]\n            "
      }
    }
    set SWITCHBODY [string trim $SWITCHBODY]

    set DEFAULT ""
    if {[info exists myGet(*)]} {
      set DEFAULT [lindex $myGet(*) 1]
    }

    set Get [subst -nocommands $Get]
    return $Get
  }

  method CompileHasProperty {mixins} {
    set l [list]
    foreach t [concat $mixins $self] {
      foreach {key code} [$t get] {
        lappend l $key
      }
    }
    return [subst -nocommands {
      method HasProperty {property} {
        return [expr [lsearch {$l} [set property]]>=0]
      }
    }]
  }

  method compile {mixins} {

    set Get         [$self CompileGet $mixins]
    set Put         [$self CompilePut $mixins]
    set HasProperty [$self CompileHasProperty $mixins]

    set Snit ""
    foreach t [concat $self $mixins] {
      append Snit [join [$t snit] "\n"]
      append Snit "\n"
    }

    set Final ""
    foreach t [concat $self $mixins] {
      append Finalizer [join [$t final] "\n"]
      append Finalizer "\n"
    }

    set SnitCode {
      ::snit::type ::hv3::DOM::$myName {

        # Reference to the ::hv3::dom object
        variable myDom

        $CONSTRUCTOR
        $DESTRUCTOR
        $Get
        $Put
        $HasProperty
        $Snit

        method Final {} {
          $Final
        }
      }
    }

    set CONSTRUCTOR [string trim {
        constructor {dom args} {
          set myDom $dom
          catch {$self configurelist $args}
        }
    }]
    set DESTRUCTOR [string trim {
        method Finalize {} {
          puts "Finalize $self"
          $self destroy
        }
        destructor {
          # Call the user destructor(s).
          #
          $self Final

          # Move objects returned by [dom_get -cache] methods to 
          # to transient state. The garbage collector will clean 
          # them up eventually.
          #
          foreach {name value} [array get myGetCache] {
            foreach {t v} $value {}
            if {[lindex $value 0] eq "object"} {
              [$myDom see] make_transient [lindex $value 1]
            }
          }
        }
    }]

    set SnitCode [subst -nocommands $SnitCode]
    return $SnitCode
  }

  destructor {
  }
}

#
# Above this line is system code. Below are the declarations for the 
# objects that make up the DOM.
#############################################################################

source [file join [file dirname [info script]] hv3_dom_core.tcl]
source [file join [file dirname [info script]] hv3_dom_html.tcl]
source [file join [file dirname [info script]] hv3_dom_events.tcl]
source [file join [file dirname [info script]] hv3_dom_style.tcl]
source [file join [file dirname [info script]] hv3_dom_ns.tcl]
source [file join [file dirname [info script]] hv3_dom_xmlhttp.tcl]

set classlist [concat \
  HTMLCollection HTMLElement HTMLDocument \
  [::hv3::dom::getHTMLElementClassList]   \
  [::hv3::dom::getNSClassList]            \
  Text NodePrototype NodeList             \
  MouseEvent UIEvent MutationEvent Event  \
  CSSStyleDeclaration                     \
  XMLHttpRequest                          \
]

foreach class $classlist {
  eval [::hv3::dom::compile $class]
  # puts [::hv3::dom::compile $class]
}
#puts [::hv3::dom::compile HTMLElement]
::hv3::dom::cleanup

