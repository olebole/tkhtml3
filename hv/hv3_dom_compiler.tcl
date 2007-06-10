namespace eval hv3 { set {version($Id: hv3_dom_compiler.tcl,v 1.19 2007/06/10 10:33:41 danielk1977 Exp $)} 1 }

#--------------------------------------------------------------------------
# This file implements infrastructure used to create the [proc] definitions
# that implement the DOM objects. It adds the following command that
# may be used to declare DOM object classes, in the same way as 
# [::snit::type] is used to declare new Snit types:
#
#     ::hv3::dom2::stateless TYPE-NAME BASE-TYPE-LIST BODY
#

proc ::hv3::dom::ToString {pSee js_value} {
  switch -- [lindex $js_value 0] {
    undefined {return "undefined"}
    null      {return "null"}
    object    {
      return [$pSee tostring $js_value]
#      set val [eval [lindex $js_value 1] DefaultValue]
#      if {[lindex $val 1] eq "object"} {error "DefaultValue is object"}
#      return [::hv3::dom::ToString $pSee $val]
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

# This proc is used in place of a full [dom_object] for callable
# functions. i.e. the object returned by a Get on the "window.Image"
# property requested by the javascript "new Image()".
#
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

proc Indent {iIndent str} {
  set in  "\n[string repeat { } [expr {-1 * $iIndent}]]"
  set out "\n[string repeat { } $iIndent]"
  string map [list $in $out] $str
}

proc AutoIndent {iIndent str} {
  set white 0
  regexp {\n( *)[^[:space:]]} $str DUMMY white
  set iIndent2 [expr $iIndent - [string length $white]]
  Indent $iIndent2 $str
}

namespace eval ::hv3::DOM {
  # Return the current javascript object command.
  #
  proc SELF {} {
    set values [info level -1]
    set procname [lindex $values 0]

    # Black magic so that [SELF] works in [dom_call] methods.
    #
    set idx [string first _call_ $procname]
    if {$idx > 0} {
      set values [lindex [info level -2] 3]
      set procname [string range $procname 0 [expr $idx-1]]
      lset values 0 $procname
    }

    set arglist [info args $procname]
    set iIdx [lsearch -exact $arglist Method]
    lrange $values 0 $iIdx
  }
}

#--------------------------------------------------------------------------
# Stateless DOM objects are defined using the following command:
#
#     ::hv3::dom2::stateless TYPE-NAME BASE-TYPE-LIST BODY
#
# Also:
#
#     ::hv3::dom2::compile TYPE-NAME
#     ::hv3::dom2::cleanup
#
# This generates code to implement the object using [proc]. The following are
# supported within the BODY block:
#
#     dom_parameter     NAME
#
#     dom_get           PROPERTY CODE
#
#     dom_put           ?-strings? PROPERTY ARG-NAME CODE
#     dom_call          ?-strings? PROPERTY ARG-LIST CODE
#
#     dom_todo          PROPERTY
#     dom_call_todo     PROPERTY
#
#     dom_default_value CODE
#
#
namespace eval ::hv3::dom2 {

  ::variable BaseArray
  ::variable TypeArray
  ::variable CurrentType

  array set TypeArray ""
  set CurrentType ""

  proc stateless {type_name base_list body} {

    ::variable TypeArray
    ::variable BaseArray
    ::variable CurrentType

    if {![info exists TypeArray($type_name)]} {
      set TypeArray($type_name) [::hv3::dom2::typecompiler %AUTO% $type_name]
      set BaseArray($type_name) $base_list 
    }

    set CurrentType $TypeArray($type_name)
    namespace eval compiler $body
    set CurrentType ""
  }

  namespace eval compiler {

    proc dom_todo {property} {
      set type [$::hv3::dom2::CurrentType name]
      dom_get $property [subst -nocommands {
        puts "TODO: $type.$property"
        list
      }]
    }

    proc dom_call_todo {property} {
      set type [$::hv3::dom2::CurrentType name]
      dom_call $property {args} [subst -nocommands {
        puts "TODO: $type.${property}()"
        list
      }]
    }

    proc dom_parameter {name} {
      $::hv3::dom2::CurrentType add_parameter $name
    }

    proc dom_default_value {code} {
      $::hv3::dom2::CurrentType add_default_value $code
    }

    proc dom_get {property code} {
      $::hv3::dom2::CurrentType add_get $property $code
    }

    proc dom_finalize {code} {
      $::hv3::dom2::CurrentType add_finalizer $code
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

      $::hv3::dom2::CurrentType add_put $isString $property $arg_name $code
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

      $::hv3::dom2::CurrentType add_call $property $isString $arg_list $code
    }

    proc dom_construct {property arg_list code} {
      $::hv3::dom2::CurrentType add_construct $property $arg_list $code
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

# Each stateless object declaration
#
::snit::type ::hv3::dom2::typecompiler {

  # Map of properties configured with a [dom_get]
  #
  #     property-name -> CODE
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
  #     property-name -> [list IS-CONSTRUCTOR IS-STRING ARG-LIST CODE]
  #
  # where IS-STRING is a boolean variable indicating whether or not
  # the -string switch was specified. IS-CONSTRUCTOR is true for a
  # constructor ([dom_construct]) and false for a regular method 
  # ([dom_call]).
  #
  variable myCall -array [list]

  # List of snit blocks to add to the object definition
  #
  variable myFinalizer [list]

  # List of parameters that will be passed to the object proc.
  #
  variable myParam [list]

  # Code for the [DefaultValue] method.
  #
  variable myDefaultValue ""

  # List of [proc] declarations used for [dom_call].
  #
  variable myExtraCode ""

  # Name of this type - i.e. "HTMLDocument".
  #
  variable myName 
  method name {} {set myName}

  constructor {name} {
    set myName $name
  }

  method add_get {property code} {
    set myGet($property) $code
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
  method add_parameter {parameter} {
    lappend myParam $parameter
  }
  method add_call {zName isString lArg zCode} {
    set myCall($zName) [list 0 $isString $lArg $zCode]
  }
  method add_construct {zName lArg zCode} {
    set myCall($zName) [list 1 0 $lArg $zCode]
  }
  method add_default_value {code} {
    set myDefaultValue $code
  }

  method call {} { return [array get myCall] }
  method get {} { return [array get myGet] }
  method put {} { return [array get myPut] }
  method snit {} { return $mySnit }
  method final {} { return $myFinalizer }
  method param {} { return $myParam }

  method CompilePut {mixins} {

    set Put {
      set value [lindex [set args] 1]
      switch -exact -- [lindex [set args] 0] {
        $SWITCHBODY
        default {
          $NATIVE
        }
      }
    }

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
    set Get [AutoIndent 0 {
      set property [lindex [set args] 0]
      set res [switch -exact -- [set property] {
        $SWITCHBODY
        default {
          list
          $DEFAULT
        }
      }]
      set res
    }]

    set SWITCHBODY ""
    foreach t [concat $mixins $self] {
      array set get_array [$t get]
    }
    foreach t [concat $mixins $self] {
      array set call_array [$t call]
    }

    # Add [switch] cases for properties declared with [dom_get].
    #
    foreach {name code} [array get get_array] {
      if {$name ne "*"} {
        set code [string trim [AutoIndent 4 $code]]
        append SWITCHBODY [subst -nocommands [AutoIndent 2 {
          $name {
            $code
          }
        }]]
      }
    }

    # Add [switch] cases for properties declared with [dom_call].
    #
    set param_list [$self ParamList $mixins]
    set SetStateVar ""
    if {[lsearch -exact $param_list myStateArray]>=0} {
      set SetStateVar {upvar #0 $myStateArray state}
    }

    foreach {name value} [array get call_array] {
      if {[info exists get_array($name)]} continue
      foreach {isConstructor isString lArg zBody} $value {}

      set zBody "\n${SetStateVar}\n${zBody}"

      set arglist [concat myDom $param_list $lArg]
      if {$isConstructor} {
        set procname ::hv3::DOM::${myName}_construct_${name}
      } else {
        set procname ::hv3::DOM::${myName}_call_${name}
      }
      lappend myExtraCode [list proc $procname $arglist $zBody]

      set SET_PARAMS ""
      foreach param $param_list {
        append SET_PARAMS " \$$param"
      }

      if {$isConstructor} {
        set code [subst -nocommands {
          list object [list \
            ::hv3::dom::TclConstructable \
            [[set myDom] see] [list $procname [set myDom] $SET_PARAMS]
          ]
        }]
      } else {
        set code [subst -nocommands {
          list object [list \
            ::hv3::dom::TclCallableProc \
            [[set myDom] see] $isString [list $procname [set myDom] $SET_PARAMS]
          ]
        }]
      }

      set code [string trim [AutoIndent 2 $code]]
      append SWITCHBODY [AutoIndent 2 [subst -nocommands {
        $name {
          $code
        }
      }]]
    }

    set SWITCHBODY [string trim $SWITCHBODY]

    set DEFAULT ""
    if {[info exists myGet(*)]} {
      set DEFAULT $myGet(*)
    }

    set Get [subst -nocommands $Get]
    return $Get
  }

  method CompileHasProperty {mixins} {
    set l [list]
    foreach t [concat $mixins $self] {
      foreach {key code} [concat [$t call] [$t get]] {
        lappend l $key
      }
    }
    return [subst -nocommands {
      return [expr [lsearch {$l} [lindex [set args] 0]]>=0]
    }]
  }

  method ParamList {mixins} {
    set ret [list]
    foreach t [concat $mixins $self] {
      set ret [concat $ret [$t param]]
    }
    set ret
  }

  method compile {mixins} {

    set Get         [AutoIndent 6 [$self CompileGet $mixins]]
    set Put         [AutoIndent 6 [$self CompilePut $mixins]]
    set HasProperty [AutoIndent 6 [$self CompileHasProperty $mixins]]

    set Final ""
    foreach t [concat $self $mixins] {
      append Finalizer [join [$t final] "\n"]
      append Finalizer "\n"
    }

    if {$myDefaultValue eq ""} {
      set myDefaultValue {list string [SELF]}
    }

    set param_list [$self ParamList $mixins]
    set SetStateVar ""
    if {[lsearch -exact $param_list myStateArray]>=0} {
      set SetStateVar {upvar #0 $myStateArray state}
    }
    set arglist [concat myDom $param_list Method args]
    set Code [AutoIndent 0 {
      proc ::hv3::DOM::$myName {$arglist} {
        $SetStateVar
        switch -exact -- [set Method] {
          Get {
            $Get
          }
          Put {
            $Put
          }
          HasProperty {
            $HasProperty
          }
          DefaultValue {
            $myDefaultValue
          }

          Finalize {
            $Finalizer
          }
        }
      }
    }]

    set Code [subst -nocommands $Code]
    append Code "\n"
    append Code [join $myExtraCode "\n"]
    return $Code
  }

}


