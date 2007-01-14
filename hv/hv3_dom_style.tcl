package require snit

namespace eval ::hv3::dom {
  # This array is a map between the DOM name of a CSS property
  # and the CSS name.
  array set CSS_PROPERTY_MAP [list \
    display display                \
    height  height                 \
    width   width                  \
  ]
}

#-----------------------------------------------------------------------
# ::hv3::dom::InlineStyle 
#
#     This Snit type implements a javascript element.style object, used to
#     provide access to the "style" attribute of an HTML element.
# 
set InlineStyleDefn {
  js_init {dom node} { 
    set myNode $node
  }
}
foreach p [array names ::hv3::dom::CSS_PROPERTY_MAP] {
  append InlineStyleDefn [subst -nocommands {
    js_get $p   { [set self] GetStyleProp $p }
    js_put $p v { [set self] PutStyleProp $p [set v] }
  }]
}
append InlineStyleDefn {

  variable myNode

  method GetStyleProp {prop} {
      list string [$myNode prop -inline $hv3::dom::CSS_PROPERTY_MAP($prop)]
  }

  method PutStyleProp {property js_value} {
    set value [[$self see] tostring $js_value]

    array set current [$myNode prop -inline]

    if {$value ne ""} {
      set current($::hv3::dom::CSS_PROPERTY_MAP($property)) $value
    } else {
      unset -nocomplain current($::hv3::dom::CSS_PROPERTY_MAP($property))
    }

    set style ""
    foreach prop [array names current] {
      append style "$prop : $current($prop); "
    }

    $myNode attribute style $style
  }

  js_finish {}
}
::snit::type ::hv3::dom::InlineStyle $InlineStyleDefn
unset InlineStyleDefn
#
# End of DOM class InlineStyle.
#-----------------------------------------------------------------------

