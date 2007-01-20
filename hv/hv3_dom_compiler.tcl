namespace eval hv3 { set {version($Id: hv3_dom_compiler.tcl,v 1.4 2007/01/20 07:58:40 danielk1977 Exp $)} 1 }

#--------------------------------------------------------------------------
# This file implements infrastructure used to create the Snit objects
# that implement the DOM objects. It adds the following command that
# may be used to declare DOM object classes, in the same way as 
# [::snit::type] is used to declare new Snit types:
#
#     ::hv3::dom::type TYPE-NAME BASE-TYPE-LIST BODY
#

package require snit

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
    proc dom_call {args} {
      if {[llength $args] == 3} {
        set isString 0
        foreach {property arg_list code} $args {}
      } elseif {[llength $args] == 4 && [lindex $args 0] eq "-string"} {
        set isString 1
        foreach {dummy property arg_list code} $args {}
      } else {
        error "Invalid args to dom_call: $args"
      }

      set get_code [subst -nocommands {
        list object [
          ::hv3::JavascriptObject %AUTO% [set myDom] \
              -call [mymethod call_$property] -callwithstrings $isString
        ]
      }]

      $::hv3::dom::CurrentType add_get 0 $property $get_code
      $::hv3::dom::CurrentType add_snit [list \
        method call_$property $arg_list $code
      ]
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

  method get {} { return [array get myGet] }
  method put {} { return [array get myPut] }
  method snit {} { return $mySnit }

  method CompilePut {mixins} {
    set Put {
        method Put {property value} {
          switch -- [set property] {
            $SWITCHBODY
            default {
              $NATIVE
            }
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

    set NATIVE [string trim {
puts "Native put on $self: $property -> $value"
      eval [$myDom see] $myNative Put $property [list $value]
    }]

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
          $NATIVE
          set result
        }
    }

    set NATIVE [string trim {
          if {$result eq ""} {
            set result [eval [$myDom see] $myNative Get $property]
          }
          
          if {($result eq "" || $result eq "undefined") && !$isExplicit} {
            $self Log_UndefinedProperty $property
          }
    }]

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

  method compile {mixins} {

    set Get [$self CompileGet $mixins]
    set Put [$self CompilePut $mixins]

    set Snit ""
    foreach t [concat $self $mixins] {
      append Snit [join [$t snit] "\n"]
      append Snit "\n"
    }

    set SnitCode {
      ::snit::type ::hv3::DOM::$myName {

        # Reference to the javascript object to store properties.
        variable myNative

        # Reference to the ::hv3::dom object
        variable myDom

        $CONSTRUCTOR
        $DESTRUCTOR
        $Get
        $Put
        $Snit

        $Log_UndefinedPropertyMethod
      }
    }

    set CONSTRUCTOR [string trim {
        constructor {dom args} {
          set myDom $dom
          set myNative [[$dom see] native]
          catch {$self configurelist $args}
        }
    }]
    set DESTRUCTOR [string trim {
        destructor {
          # Destroy objects returned by [dom_get -cache] methods.
          #
          foreach {name value} [array get myGetCache] {
            foreach {t v} $value {}
            if {[lindex $value 0] eq "object"} {
              [lindex $value 1] destroy
            }
          }

          # TODO Destroy the myNative object?
        }
    }]

    set Log_UndefinedPropertyMethod [subst -novariables {
        method Log_UndefinedProperty {prop} {
          # puts "Request for DOM property [set myName].$prop -> undefined"
          # puts "Request for DOM property $self.$prop -> undefined"
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

foreach class [concat \
    HTMLCollection HTMLElement HTMLDocument \
    [::hv3::dom::getHTMLElementClassList]   \
    Text NodePrototype NodeList             \
    MouseEvent UIEvent MutationEvent Event  \
    CSSStyleDeclaration                     \
] {
  eval [::hv3::dom::compile $class]
  # puts [::hv3::dom::compile $class]
}
::hv3::dom::cleanup


