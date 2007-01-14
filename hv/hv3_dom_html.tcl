
#--------------------------------------------------------------------------
# DOM Level 1 Html
#
# This file contains the Hv3 implementation of the DOM Level 1 Html. Where
# possible, Hv3 tries hard to be compatible with W3C and Gecko. Gecko
# is pretty much a clean super-set of W3C for this module.
#
#-------------------------------------------------------------------------

namespace eval ::hv3::dom::compiler {

  # element_attr --
  #
  #     element_attr NAME ?OPTIONS?
  #
  #         -attribute ATTRIBUTE-NAME          (default NAME)
  #         -readonly                          (make the attribute readonly)
  #
  proc element_attr {name args} {

    set readonly 0
    set attribute $name

    # Process the arguments to [element_attr]:
    for {set ii 0} {$ii < [llength $args]} {incr ii} {
    }

    # The Get code.
    dom_get $name [subst -novariables {
      $self HTMLElement_getAttributeString [set attribute] ""
    }]

    # Create the Put method (unless the -readonly switch was passed).
    if {!$readonly} {
      dom_put $name val [subst -novariables {
        $self HTMLElement_putAttributeString [set name] $val
      }]
    }
  }

}

#-------------------------------------------------------------------------
# DOM Type HTMLElement (Node -> Element -> HTMLElement)
#
set BaseList {Element WidgetNode Node NodePrototype EventTarget}
::hv3::dom::type HTMLElement $BaseList {
  element_attr id
  element_attr title
  element_attr lang
  element_attr dir
  element_attr className -attribute class

  dom_snit {
    method HTMLElement_getAttributeString {name def} {
      set val [$options(-nodehandle) attribute -default $def $name]
      list string $val
    }
    method HTMLElement_putAttributeString {name val} {
      $options(-nodehandle) attribute $name $val
      return ""
    }
  }
}

