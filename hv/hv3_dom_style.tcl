namespace eval hv3 { set {version($Id: hv3_dom_style.tcl,v 1.6 2007/06/01 18:07:48 danielk1977 Exp $)} 1 }

#-------------------------------------------------------------------------
# DOM Level 2 Style.
#
# This file contains the Hv3 implementation of the DOM Level 2 Style
# specification.
#
#     ElementCSSInlineStyle        (mixed into Element interface)
#     CSSStyleDeclaration          (mixed into Element interface)
#
# TODO: The above statement is not really true at the moment. The
# CSSStyleDeclaration interface currently only supports DOM Level
# 0 stuff, not the standardised model. There is no reason that it
# cannot be supported, but it's not really in use anywhere yet so
# supporting it is not a high priority. 
#

::hv3::dom2::stateless ElementCSSInlineStyle {} {
  dom_get style {
    list object [list ::hv3::DOM::CSSStyleDeclaration $myDom $myNode]
  }
}

namespace eval ::hv3::dom2::compiler {
  proc style_property {css_prop {dom_prop ""}} {
    if {$dom_prop eq ""} {
      set dom_prop $css_prop
    }
    set getcmd "CSSStyleDeclaration_getStyleProperty \$myNode $css_prop"
    dom_get $dom_prop $getcmd

    set putcmd "CSSStyleDeclaration_setStyleProperty \$myNode $css_prop \$value"
    dom_put -string $dom_prop {value} $putcmd
  }
}

::hv3::dom2::stateless CSSStyleDeclaration {} {

  dom_parameter myNode

  style_property width width
  style_property height height
  style_property display display

  style_property position position
  style_property top top
  style_property left left
  style_property bottom bottom
  style_property right right

  style_property z-index zIndex

  style_property border-top-width borderTopWidth
  style_property border-right-width borderRightWidth
  style_property border-left-width borderLeftWidth
  style_property border-bottom-width borderBottomWidth

  dom_put -string border value {
    set style [$myNode attribute -default {} style]
    if {$style ne ""} {append style ";"}
    append style "border: $value"
    $myNode attribute style $style
  }
}

namespace eval ::hv3::DOM {
  proc CSSStyleDeclaration_getStyleProperty {node css_property} {
    set val [$node property -inline $css_property]
    list string $val
  }

  proc CSSStyleDeclaration_setStyleProperty {node css_property value} {
    array set current [$node prop -inline]

# if {$value eq "NaNpx"} "error NAN"

    if {$value ne ""} {
      set current($css_property) $value
    } else {
      unset -nocomplain current($css_property)
    }

    set style ""
    foreach prop [array names current] {
      append style "$prop:$current($prop);"
    }

    $node attribute style $style
  }
}

