namespace eval hv3 { set {version($Id: hv3_dom_containers.tcl,v 1.2 2007/06/05 15:34:14 danielk1977 Exp $)} 1 }

# This file contains the implementation of the two DOM specific
# container objects:
#
#     NodeList               (DOM Core 1)
#     HTMLCollection         (DOM HTML 1)
#
# Both are implemented as stateless js objects.
#


#-------------------------------------------------------------------------
# DOM class: (HTMLCollection)
#
# Supports the following javascript interface:
#
#     length
#     item(index)
#     namedItem(name)
#
# Also, a request for any property with a numeric name is mapped to a call
# to the item() method. A request for any property with a non-numeric name
# maps to a call to namedItem(). Hence, javascript references like:
#
#     collection[1]
#     collection["name"]
#     collection.name
#
# work as expected.
#
::hv3::dom2::stateless HTMLCollection {} {

  # There are several variations on the role this object may play in
  # DOM level 1 Html:
  #
  #     Document.getElementsByTagName()
  #     Element.getElementsByTagName()
  #
  #     HTMLDocument.getElementsByName()
  #
  #     HTMLDocument.images
  #     HTMLDocument.applets
  #     HTMLDocument.links
  #     HTMLDocument.forms
  #     HTMLDocument.anchors
  #
  #     HTMLFormElement.elements
  #     HTMLSelectElement.options
  #     HTMLMapElement.areas
  #
  #     HTMLTableElement.rows
  #     HTMLTableElement.tBodies
  #     HTMLTableSectionElement.rows
  #     HTMLTableRowElement.cells
  #
  #     Node.childNodes
  #     Node.attributes
  #
  dom_parameter nodelistcmd

  # HTMLCollection.length
  #
  dom_get length {
    list number [llength [eval $nodelistcmd]]
  }

  # HTMLCollection.item()
  #
  dom_call -string item {THIS index} {
    HTMLCollection_item $myDom $nodelistcmd $index
  }

  # HTMLCollection.namedItem()
  #
  dom_call -string namedItem {THIS name} {
    HTMLCollection_namedItem $myDom $nodelistcmd $name
  }

  # Handle an attempt to retrieve an unknown property.
  #
  dom_get * {

    # If $property looks like a number, treat it as an index into the list
    # of widget nodes. 
    #
    # Otherwise look for nodes with the "name" or "id" attribute set 
    # to the queried attribute name. If a single node is found, return
    # it directly. If more than one have matching names or ids, a NodeList
    # containing the matches is returned.
    #
    if {[string is double $property]} {
      set res [HTMLCollection_item $myDom $nodelistcmd $property]
    } else {
      set res [
        HTMLCollection_getNodeHandlesByName $nodelistcmd $property
      ]
      set nRet [llength $res]
      if {$nRet==0} {
        set res ""
      } elseif {$nRet==1} {
        set res [list object [$myDom node_to_dom [lindex $res 0]]]
      } else {
        set getnodes [namespace code [list \
          HTMLCollection_getNodeHandlesByName $nodelistcmd $property
        ]]
        set obj [list ::hv3::DOM::NodeList $myDom $getnodes]
        set res [list object $obj]
      }
    }

    return $res
  }
}

namespace eval ::hv3::DOM {
  proc HTMLCollection_getNodeHandlesByName {supersetcmd name} {
    set nodelist [eval $supersetcmd]
    set ret [list]
    foreach node $nodelist {
      if {[$node attr -default "" id] eq $name || 
        [$node attr -default "" name] eq $name} {
        lappend ret $node
      }
    }
    return $ret
  }
  
  proc HTMLCollection_item {dom nodelistcmd index} {
    set idx [format %.0f $index]
    set ret ""
    set node [lindex [eval $nodelistcmd] $idx]
    if {$node ne ""} {
      set ret [list object [$dom node_to_dom $node]]
    }
    set ret
  }
  
  proc HTMLCollection_namedItem {dom nodelistcmd name} {
    set nodelist [eval $nodelistcmd]
    
    foreach node $nodelist {
      if {[$node attr -default "" id] eq $name} {
        set domobj [$dom node_to_dom $node]
        return [list object $domobj]
      }
    }
    
    foreach node $nodelist {
      if {[$node attr -default "" name] eq $name} {
        set domobj [$dom node_to_dom $node]
        return [list object $domobj]
      }
    }
    
    return ""
  }
}



#-------------------------------------------------------------------------
# DOM Type NodeList
#
#     This object is used to store lists of child-nodes (property
#     Node.childNodes). There are three variations, depending on the
#     type of the Node that this object represents the children of:
#
#         * Document node (one child - the root (<HTML>) element)
#         * Element node (children based on html widget node-handle)
#         * Text or Attribute node (no children)
#
::hv3::dom2::stateless NodeList {} {

  # The following option is set to a command to return the html-widget nodes
  # that comprise the contents of this list. i.e. for the value of
  # the "Document.childNodes" property, this option will be set to
  # [$hv3 node], where $hv3 is the name of an ::hv3::hv3 widget (that
  # delagates the [node] method to the html widget).
  #
  dom_parameter myNodelistcmd

  dom_call -string item {THIS index} {
    if {![string is double $index]} { return null }
    set idx [expr {int($index)}]
    NodeList_item $myDom $myNodelistcmd $idx
  }

  dom_get length {
    list number [llength [eval $myNodelistcmd]]
  }

  # Unknown property request. If the property name looks like a number,
  # invoke the NodeList.item() method. Otherwise, return undefined ("").
  #
  dom_get * {
    if {[string is integer $property]} {
      NodeList_item $myDom $myNodelistcmd $property
    }
  }
}

namespace eval ::hv3::DOM {
  proc NodeList_item {dom nodelistcmd idx} {
    set children [eval $nodelistcmd]
    if {$idx < 0 || $idx >= [llength $children]} { return null }
    list object [$dom node_to_dom [lindex $children $idx]]
  }
}

