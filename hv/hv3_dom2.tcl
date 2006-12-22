#
# Contains the following:
#
#     ::hv3::dom::pretty_print_script
#         Proc to reformat javascript code for readability.
#
#     ::hv3::dom::get_inner_html
#         Retrieve the "innerHTML" value of a node.
#

namespace eval ::hv3::dom {

  proc NodeToHtml {node} {
    set tag [$node tag]
    if {$tag eq ""} {
      return [$node text -pre]
    } else {
      set inner [get_inner_html $node]
      return "<$tag>$node</$tag>"
    }
  }

  proc get_inner_html {node} {
    if {[$node tag] eq ""} {error "$node is not an HTMLElement"}

    set ret ""
    foreach child [$node children] {
      append ret [NodeToHtml $node]
    }
    return $ret
  }

  proc set_inner_html {hv3 node newHtml} {
    if {[$node tag] eq ""} {error "$node is not an HTMLElement"}

    set children [$node children]
    $node remove $children
    foreach child $children {
      $child destroy
    }

    set children [[$hv3 html] fragment $newHtml]
    $node insert $children
  }

}
