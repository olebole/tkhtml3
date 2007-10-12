namespace eval hv3 { set {version($Id: hv3_dom_compiler.tcl,v 1.33 2007/10/12 08:20:06 danielk1977 Exp $)} 1 }

#--------------------------------------------------------------------------
# This file implements infrastructure used to create the [proc] definitions
# that implement the DOM objects. It adds the following command that
# may be used to declare DOM object classes, in the same way as 
# [::snit::type] is used to declare new Snit types:
#
#     ::hv3::dom2::stateless TYPE-NAME BASE-TYPE-LIST BODY
#

namespace eval ::hv3::dom::code {}
namespace eval ::hv3::DOM::docs {}

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
        set A [list]
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

    Events { return }
    Scope  { return }
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

    Events { return }
  }

  error "Unknown method: $op"
}

proc Indent {iIndent str} {
  set in  "\n[string repeat { } [expr {-1 * $iIndent}]]"
  set out "\n[string repeat { } $iIndent]"
  string map [list $in $out] $str
}

proc AutoIndent {iIndent str} {
return $str
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
      set level 2
    } else {
      set level 1
    }

    uplevel $level {set SELF}
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
  ::variable DocBuffer
  ::variable Docs

  ::variable ClassList ""

  array set TypeArray ""
  set CurrentType ""

  proc evalcode {code} {
    #puts $code
    eval $code
  }

  proc stateless2 {type_name body} {
    set compiler2::parameter ""
    set compiler2::default_value ""
    set compiler2::finalize ""
    array unset compiler2::get_array
    array unset compiler2::put_array
    array unset compiler2::call_array

    # Compile the documentation for this object. This step is optional.
    doccompiler::clean
    namespace eval doccompiler $body
    set documentation [doccompiler::make $type_name]
    evalcode [list \
       proc ::hv3::DOM::docs::${type_name} {} [list return $documentation]
    ]

    namespace eval compiler2 $body

    set GET [array get compiler2::get_array]
    foreach {zProp val} [array get compiler2::call_array] {
      foreach {isString call_args zCode} $val {}

      set procname ::hv3::DOM::${type_name}.${zProp}
      set arglist [concat myDom $compiler2::parameter $call_args]
      set proccode [list proc $procname $arglist $zCode]
      evalcode $proccode

      if {$isString < 0} {
        set calltype TclConstructable
        set isString ""
      } else {
        set calltype TclCallableProc
      }

      lappend GET $zProp [string map [list \
        %CALLTYPE% $calltype               \
        %ISSTRING% $isString               \
        %PROCNAME% $procname               \
        %PARAM% $compiler2::parameter    \
      ] {
        list cache transient [list \
          ::hv3::dom::%CALLTYPE%   \
            [$myDom see] %ISSTRING% [list %PROCNAME% $myDom %PARAM%]
        ]
      }]
    }

    set PUT [list]
    foreach {zProp val} [array get compiler2::put_array] {
      foreach {isString zArg zCode} $val {}

      if {$isString} {
        set Template {
          set %ARG% [[$myDom see] tostring [lindex $args 1]]
          %CODE%
        }
      } else {
        set Template {
          set %ARG% [lindex $args 1]
          %CODE%
        }
      }
      lappend PUT $zProp [string map       \
          [list %ARG% $zArg %CODE% $zCode] \
          $Template
      ]
    }

    set arglist [list myDom $compiler2::parameter Method args]
    set proccode [list \
      proc ::hv3::DOM::$type_name $arglist [string map [list \
        %GET% $GET %PUT% $PUT \
        %DEFAULTVALUE% $compiler2::default_value \
        %FINALIZE% $compiler2::finalize \
        %DOCUMENTATION% $documentation  \
      ] {
        switch -exact -- $Method {
          Get {
            switch -exact -- [lindex $args 0] {
              %GET%
            }
          }
          Put {
            switch -exact -- [lindex $args 0] {
              %PUT%
            }
          }
          HasProperty {
          }
          DefaultValue {
            %DEFAULTVALUE%
          }
          Finalize {
            %FINALIZE%
          }
          Events {
          }
          Scope {
          }
        }
      }
    ]]

    evalcode $proccode
  }

  namespace eval compiler2 {

    variable parameter
    variable default_value
    variable finalize

    variable get_array
    variable put_array
    variable call_array

    proc dom_parameter {zParam} {
      variable parameter
      set parameter $zParam
    }

    proc dom_default_value {zDefault} {
      variable default_value
      set default_value $zDefault
    }

    proc dom_finalize {zScript} {
      variable finalize
      set finalize $zScript
    }

    proc check_for_is_string {isStringVar argsVar} {
      upvar $isStringVar isString
      upvar $argsVar args

      set isString 0
      if {[lindex $args 0] eq "-string"} {
        set isString 1
        set args [lrange $args 1 end]
      }
    }

    # dom_call ?-string? PROPERTY ARG-LIST CODE
    #
    proc dom_call {args} {
      variable call_array
      check_for_is_string isString args
      if {[llength $args] != 3} {
        set shouldbe "\"dom_call ?-string? PROPERTY ARG-NAME CODE\""
        error "Invalid arguments to dom_call - should be: $shouldbe" 
      }
      foreach {zMethod zArgs zCode} $args {}
      set call_array($zMethod) [list $isString $zArgs $zCode]
    }

    proc dom_call_todo {zProc} {}

    # dom_construct PROPERTY ARG-LIST CODE
    #
    proc dom_construct {args} {
      variable call_array
      if {[llength $args] != 3} {
        set shouldbe "\"dom_construct ?-string? PROPERTY ARG-NAME CODE\""
        error "Invalid arguments to dom_construct - should be: $shouldbe" 
      }
      foreach {zMethod zArgs zCode} $args {}
      set call_array($zMethod) [list -1 $zArgs $zCode]
    }

    # dom_get PROPERTY CODE
    #
    proc dom_get {zProperty zScript} {
      variable get_array
      set get_array($zProperty) $zScript
    }

    # dom_put ?-string? PROPERTY ARG-NAME CODE
    #
    proc dom_put {args} {
      variable put_array
      check_for_is_string isString args
      if {[llength $args] != 3} {
        set shouldbe "\"dom_put ?-string? PROPERTY ARG-NAME CODE\""
        error "Invalid arguments to dom_put - should be: $shouldbe" 
      }
      foreach {zProperty zArg zCode} $args {}
      set put_array($zProperty) [list $isString $zArg $zCode]
    }

    proc dom_events {zCode} {
    }

    proc -- {args} {}
    proc XX {args} {}
    proc Ref {args} {}
  }


  namespace eval doccompiler {

    variable get_array
    variable put_array
    variable call_array

    variable current_xx ""
    variable xx_array

    proc dom_parameter     {args} {}
    proc dom_default_value {args} {}
    proc dom_finalize      {args} {}
    proc dom_call_todo    {zProc} {}
    proc dom_construct    {args} {}
    proc dom_events    {args} {}

    proc dom_get  {zProperty args} {
      variable get_array
      variable docbuffer

      set get_array($zProperty) $docbuffer
      set docbuffer ""
    }
    proc dom_put  {args} {
      variable put_array
      if {[lindex $args 0] eq "-string"} {
        set put_array([lindex $args 1]) 1
      } else {
        set put_array([lindex $args 0]) 1
      }
    }

    # dom_call -string method args ...
    proc dom_call {args} {
      variable call_array
      variable docbuffer

      if {[lindex $args 0] eq "-string"} {
        set args [lrange $args 1 end]
      }
      set method  [lindex $args 0]
      set arglist [lindex $args 1]

      set call_array($method) [list $docbuffer [lrange $arglist 1 end]]
      set docbuffer ""
    }

    proc -- {args} {
      variable docbuffer
      if {[llength $args] == 0} {
        append docbuffer <p>
      } else {
        if {$docbuffer eq ""} {append docbuffer <p>}
        append docbuffer [join $args " "]
        append docbuffer "\n"
      }
      return ""
    }
    proc XX {args} {
      variable current_xx
      variable xx_array
      variable docbuffer

      if {[llength $args] != 0} {
        set current_xx [join $args " "]
      }
      set xx_array($current_xx) $docbuffer
      set docbuffer ""
    }
    proc Ref {ref {text ""}} {
      if {$text eq ""} {set text $ref}
      subst {<A href="${ref}">${text}</A>}
    }

    proc clean {} {
      variable get_array
      variable put_array
      variable call_array
      variable docbuffer
      variable current_xx
      variable xx_array

      set docbuffer ""
      set current_xx ""
      array unset get_array
      array unset put_array
      array unset call_array
      array unset xx_array
      array set xx_array [list "" "<I>TODO: Class documentation</I>"]
    }

    proc make {classname} {
      variable get_array
      variable put_array
      variable call_array
      variable xx_array

      set properties "<TR><TD colspan=3><H2>Properties</H2>"
      set iStripe 0
      foreach {zProp} [lsort [array names get_array]] {
        set docs $get_array($zProp)
        set readwrite ""
        if {[info exists put_array($zProp)]} {
          set readwrite "<I>r/w</I>"
        }
        append properties "<TR class=stripe${iStripe}>
          <TD class=spacer> 
          <TD class=\"property\"><B>$zProp</B>
          <TD>$readwrite
          <TD width=100%>$docs
        "
        set iStripe [expr {($iStripe+1)%2}]
      }

      set methods "<TR><TD colspan=3><H2>Methods</H2>"
      set iStripe 0
      foreach {zProp} [lsort [array names call_array]] {
        set data $call_array($zProp)
        foreach {docs arglist} $data {break}
        set zArglist [join $arglist ", "]
        append methods "<TR class=stripe${iStripe}>
          <TD class=spacer> 
          <TD class=\"method\" colspan=2><B>${zProp}</B>(${zArglist})
          <TD width=100%>$docs
        "
        set iStripe [expr {($iStripe+1)%2}]
      }

      set Docs [string map [list       \
          %CLASSNAME%  $classname      \
          %OVERVIEW%   $xx_array()     \
          %PROPERTIES% $properties     \
          %METHODS%    $methods        \
      ] {
        <LINK rel="stylesheet" href="home://dom/style.css">
        <TITLE>Class %CLASSNAME%</TITLE>
        <H1>DOM Class %CLASSNAME%</H1>
        <DIV class=overview> %OVERVIEW% </DIV>
        <TABLE>
          %PROPERTIES%
          %METHODS%
        </TABLE>
      }]

      return $Docs
    }
  }

  proc stateless {type_name base_list body} {

    ::variable TypeArray
    ::variable BaseArray
    ::variable CurrentType

    set mappings [list]
    foreach v [info vars ::hv3::dom::code::*] {
      set name [string range $v [expr [string last : $v]+1] end]
      lappend mappings %${name}% [set $v]
    }
    set body [string map $mappings $body]

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

      -- This property is not yet implemented. For the moment it is a
      -- placeholder that always contains null.
      dom_get $property [subst -nocommands {
        puts "TODO: $type.$property"
        list
      }]
    }

    proc dom_call_todo {property} {
      set type [$::hv3::dom2::CurrentType name]

      -- This method is not yet implemented. For the moment it is a
      -- placeholder that has no effect and always returns null.
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
      FlushDocBuffer $property
      $::hv3::dom2::CurrentType add_get $property $code
    }

    proc dom_finalize {code} {
      $::hv3::dom2::CurrentType add_finalizer $code
    }

    proc dom_events {code} {
      $::hv3::dom2::CurrentType add_events $code
    }
    proc dom_scope {code} {
      $::hv3::dom2::CurrentType add_scope $code
    }

    proc Ref {ref {text ""}} {
      if {$text eq ""} {set text $ref}
      subst {<A href="#${ref}">${text}</A>}
    }

    proc FlushDocBuffer {{property {}}} {
      if {[info exists ::hv3::dom2::DocBuffer]} {
        if {[string range $::hv3::dom2::DocBuffer end-2 end] eq "<p>"} {
          set ::hv3::dom2::DocBuffer [
            string range $::hv3::dom2::DocBuffer 0 end-3
          ]
        }
        set name [$::hv3::dom2::CurrentType name]
        if {$property ne ""} {append name ".$property"}
        append ::hv3::dom2::Docs($name) $::hv3::dom2::DocBuffer
        unset ::hv3::dom2::DocBuffer
      }
    }
    proc -- {args} {
      if {[llength $args]==1 
         && [string range [lindex $args 0] 0 4] eq "http:"
      } {
        set uri [lindex $args 0]
        append ::hv3::dom2::DocBuffer [subst {
          <DIV class=uri><A href="$uri">$uri</A></DIV>
        }]
        return
      } elseif {[llength $args]==0 || ![info exists ::hv3::dom2::DocBuffer]} {
        append ::hv3::dom2::DocBuffer <p>
        append ::hv3::dom2::DocBuffer [join $args]
      } else {
        append ::hv3::dom2::DocBuffer " "
        append ::hv3::dom2::DocBuffer [join $args]
      }
    }

    proc noisy {args} {
      set code [lindex $args end]
      set code [subst -nocommands {
        puts "CALL: [info level 0]"
        set res [$code]
        puts "RETURN [set res]"
        set res
      }]
      lset args end $code
      eval $args
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

      FlushDocBuffer $property
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

  # Return the text for a Tcl [proc] implementing the object.
  #
  proc compile {domtype} {
    ::variable TypeArray

    set base_list [getBaseList $domtype]

    set ret ""
    append ret [$TypeArray($domtype) compile $base_list]
    append ret "\n"

    return $ret
  }

  # Return some HTML text describing the named object.
  #
  proc document {domtype} {
    ::variable TypeArray
    $TypeArray($domtype) document [getBaseList $domtype]
  }

  proc classlist {} {
    ::variable TypeArray
    array names TypeArray
  }

  proc cleanup {} {
    ::variable TypeArray
    ::variable BaseArray
    ::variable CurrentType
    foreach {name type} [array get TypeArray] {
      $type destroy
    }
    unset -nocomplain TypeArray
    unset -nocomplain BaseArray
    unset -nocomplain CurrentType
    unset -nocomplain Docs
    unset -nocomplain DocBuffer
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

  # Code for the "Events" method.
  #
  variable myEvents ""

  # Code for the "Scope" method.
  #
  variable myScope ""

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
  method add_events {code} {
    set myEvents $code
  }
  method add_scope {code} {
    set myScope $code
  }

  method call {}   { return [array get myCall] }
  method get {}    { return [array get myGet] }
  method put {}    { return [array get myPut] }
  method snit {}   { return $mySnit }
  method final {}  { return $myFinalizer }
  method param {}  { return $myParam }
  method events {} { return $myEvents }
  method scope {}  { return $myScope }

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
    foreach t [concat $mixins $self] {
      foreach {k v} [$t get] {
        if {![info exists put_array($k)]} {
          set put_array($k) {0 value {error "Read-only property"}}
        }
      }
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
          list cache transient [list \
            ::hv3::dom::TclConstructable \
            [[set myDom] see] [list $procname [set myDom] $SET_PARAMS]
          ]
        }]
      } else {
        set code [subst -nocommands {
          list cache transient [list \
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
    if {[info exists myGet(*)]} {
      return [subst -nocommands {
        return [expr {[eval [SELF] Get [set args]] ne ""}]
      }]
    } else {

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
  }

  method CompileEvents {mixins} {
    set L [concat $mixins $self]   
    set code ""
    for {set ii [llength $L]} {$code eq "" && $ii > 0} {incr ii -1} {
      set C [lindex $L [expr {$ii-1}]]
      set code [$C events]
    }
    return $code
  }
  method CompileScope {mixins} {
    set L [concat $mixins $self]   
    set code ""
    for {set ii [llength $L]} {$code eq "" && $ii > 0} {incr ii -1} {
      set C [lindex $L [expr {$ii-1}]]
      set code [$C scope]
    }
    return $code
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
    set Events      [AutoIndent 6 [$self CompileEvents $mixins]]
    set Scope       [AutoIndent 6 [$self CompileScope $mixins]]

    set Final ""
    foreach t [concat $self $mixins] {
      append Finalizer [join [$t final] "\n"]
      append Finalizer "\n"
    }

    if {$myDefaultValue eq ""} {
      set myDefaultValue {list string [SELF]}
    }

    set param_list [$self ParamList $mixins]
#puts "$myName has [llength $param_list] params: $param_list"
    set SetStateVar ""
    if {[lsearch -exact $param_list myStateArray]>=0} {
      set SetStateVar {upvar #0 $myStateArray state}
    }

    set selflist "list ::hv3::DOM::$myName \[set myDom\] "
    foreach param $param_list {
      append selflist [subst -nocommands {[set $param]}]
      append selflist " "
    }

    set arglist [concat myDom $param_list Method args]
    set Code [AutoIndent 0 {
      proc ::hv3::DOM::$myName {$arglist} {
        $SetStateVar
        set SELF [$selflist]
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
          Events {
            $Events
          }
          Scope {
            $Scope
          }
        }
      }
    }]

    set Code [subst -nocommands $Code]
    append Code "\n"
    append Code [join $myExtraCode "\n"]
    return $Code
  }


  method GetDocs {{property ""}} {
    set name [$self name]
    if {$property ne ""} {append name ".$property"}
    if {[info exists ::hv3::dom2::Docs($name)]} {
      return $::hv3::dom2::Docs($name)
    }
    if {$property eq ""} {return ""}
    return "<SPAN class=nodocs>No docs available.</SPAN>"
  }

  method document {mixins} {
    # Big heading: The name of the DOM class:
    #
    append ret "<A name=$myName><H1>$myName</H1></A>\n"

    set d [$self GetDocs]
    if {$d ne ""} { append ret "<P>$d</P>" }

    # The list of implemented interfaces:
    #
    if {[llength $mixins] > 0} {
      append ret "<H2>Inheritance</H2><UL>"
      foreach mixin $mixins {
        set name [$mixin name]
        append ret "<LI><A href=#${name}>${name}</A>"
      }
      append ret "</UL>"
    }

    # The list of properties
    #
    array set property_array [$self get]
    array set put_array [$self put]
    set props [array names property_array]
    if {[llength $props] > 0} {
      append ret {<H2>Properties</H2><TABLE border=1>}
      foreach k [lsort -command ::hv3::dom2::DocSorter $props] {
        set mode read-only
        if {[info exists put_array($k)]} {set mode read/write}
        set docs [$self GetDocs $k]
        if {$k eq "*"} {set k {Other Properties} }
        append ret "<TR><TH>$k<TD class=mode><I>$mode</I><TD>$docs"
      }
      append ret "</TABLE>"
    }

    # The list of methods
    #
    array set call_array [$self call]
    set calls [array names call_array]
    if {[llength $calls] > 0} {
      append ret {<H2>Methods</H2><TABLE border=1>}
      foreach k [lsort -command ::hv3::dom2::DocSorter $calls] {
        foreach {isConstructor isString lArg zBody} $call_array($k) {}
        set param_list [list]
        foreach param [lrange $lArg 1 end] {
          lappend param_list [lindex $param 0]
        }
        set params [join $param_list ", "]
        if {!$isConstructor} {
          set docs [$self GetDocs $k]
          append ret {<TR><TH>}
          append ret "${k}(${params})"
          append ret "<TD>$docs"
        }
      }
      append ret "</TABLE>"
    }

    set ret
  }
}

proc ::hv3::dom2::DocSorter {a b} {
  if {$a eq "*"} {return +1}
  if {$b eq "*"} {return -1}
  return [string compare $a $b]
}

