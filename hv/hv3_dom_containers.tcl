namespace eval hv3 { set {version($Id: hv3_dom_containers.tcl,v 1.4 2007/06/10 10:33:41 danielk1977 Exp $)} 1 }

# This file contains the implementation of the two DOM specific
# container objects:
#
#     NodeList               (DOM Core 1)
#     HTMLCollection         (DOM HTML 1)
#
# Both are implemented as stateless js objects. There are two 
# implementations of NodeList - NodeListC (commands) and NodeListS
# (selectors). Also HTMLCollectionC and HTMLCollectionS
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
::hv3::dom2::stateless HTMLCollectionC {} {

  # There are several variations on the role this object may play in
  # DOM level 1 Html:
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
  dom_parameter nodelistcmd

  # HTMLCollection.length
  #
  dom_get length {
    list number [llength [eval $nodelistcmd]]
  }

  # HTMLCollection.item()
  #
  dom_call -string item {THIS index} {
    HTMLCollectionC_item $myDom $nodelistcmd $index
  }

  # HTMLCollection.namedItem()
  #
  dom_call -string namedItem {THIS name} {
    HTMLCollectionC_namedItem $myDom $nodelistcmd $name
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
      set res [HTMLCollectionC_item $myDom $nodelistcmd $property]
    } else {
      set res [
        HTMLCollectionC_getNodeHandlesByName $nodelistcmd $property
      ]
      set nRet [llength $res]
      if {$nRet==0} {
        set res ""
      } elseif {$nRet==1} {
        set res [list object [$myDom node_to_dom [lindex $res 0]]]
      } else {
        set getnodes [namespace code [list \
          HTMLCollectionC_getNodeHandlesByName $nodelistcmd $property
        ]]
        set obj [list ::hv3::DOM::NodeListC $myDom $getnodes]
        set res [list object $obj]
      }
    }

    return $res
  }
}

namespace eval ::hv3::DOM {
  proc HTMLCollectionC_getNodeHandlesByName {supersetcmd name} {
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
  
  proc HTMLCollectionC_item {dom nodelistcmd index} {
    set idx [format %.0f $index]
    set ret ""
    set node [lindex [eval $nodelistcmd] $idx]
    if {$node ne ""} {
      set ret [list object [$dom node_to_dom $node]]
    }
    set ret
  }
  
  proc HTMLCollectionC_namedItem {dom nodelistcmd name} {
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

::hv3::dom2::stateless HTMLCollectionS {} {

  # Name of the tkhtml widget to evaluate [$myHtml search] with.
  #
  dom_parameter myHtml

  # The following option is set to a CSS Selector to return the
  # nodes in that make up the contents of this NodeList.
  #
  dom_parameter mySelector

  # HTMLCollection.length
  #
  dom_get length {
    list number [$myHtml search $mySelector -length]
  }

  # HTMLCollection.item()
  #
  dom_call -string item {THIS index} {
    set node [$myHtml search $mySelector -index [expr {int($index)}]]
    if {$node ne ""} { list object [$myDom node_to_dom $node] }
  }

  # HTMLCollection.namedItem()
  #
  dom_call -string namedItem {THIS name} {
    set sel [format {%s[name="%s"]} $mySelector $name]
    set node [$myHtml search $sel -index 0]
    if {$node ne ""} { list object [$myDom node_to_dom $node] }
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
      set node [$myHtml search $mySelector -index [expr {int($property)}]]
      if {$node ne ""} { list object [$myDom node_to_dom $node] }
    } else {
      set name $property
      set s $mySelector
      set sel [format {%s[name="%s"],%s[id="%s"]} $s $name $s $name]

      set nNode [$myHtml search $sel -length]
      if {$nNode > 0} {
        if {$nNode == 1} {
          list object [$myDom node_to_dom [$myHtml search $sel]]
        } else {
          list object [list \
            ::hv3::DOM::NodeListC $myDom [list $myHtml search $sel]
          ]
        }
      }
    }
  }
}


#-------------------------------------------------------------------------
# DOM Type NodeListC
#
#     This object is used to store lists of child-nodes (property
#     Node.childNodes). There are three variations, depending on the
#     type of the Node that this object represents the children of:
#
#         * Document node (one child - the root (<HTML>) element)
#         * Element node (children based on html widget node-handle)
#         * Text or Attribute node (no children)
#
::hv3::dom2::stateless NodeListC {} {

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
    NodeListC_item $myDom $myNodelistcmd $idx
  }

  dom_get length {
    list number [llength [eval $myNodelistcmd]]
  }

  # Unknown property request. If the property name looks like a number,
  # invoke the NodeList.item() method. Otherwise, return undefined ("").
  #
  dom_get * {
    if {[string is integer $property]} {
      NodeListC_item $myDom $myNodelistcmd $property
    }
  }
}

namespace eval ::hv3::DOM {
  proc NodeListC_item {dom nodelistcmd idx} {
    set children [eval $nodelistcmd]
    if {$idx < 0 || $idx >= [llength $children]} { return null }
    list object [$dom node_to_dom [lindex $children $idx]]
  }
}

#-------------------------------------------------------------------------
# DOM Type NodeListS
#
#     This object is used to implement the NodeList collections
#     returned by getElementsByTagName() and kin. It is a wrapper
#     around [$html search].
#
::hv3::dom2::stateless NodeListS {} {

  # Name of the tkhtml widget to evaluate [$myHtml search] with.
  #
  dom_parameter myHtml

  # The following option is set to the root-node of the sub-tree
  # to search with $mySelector. An empty string means search the
  # whole tree.
  #
  dom_parameter myRoot

  # The following option is set to a CSS Selector to return the
  # nodes in that make up the contents of this NodeList.
  #
  dom_parameter mySelector

  dom_call -string item {THIS index} {
    if {![string is double $index]} { return null }
    NodeListS_item $myDom $myHtml $mySelector $myRoot [expr {int($index)}]
  }

  dom_get length {
    list number [$myHtml search $mySelector -length -root $myRoot]
  }

  # Unknown property request. If the property name looks like a number,
  # invoke the NodeList.item() method. Otherwise, return undefined ("").
  #
  dom_get * {
    if {[string is integer $property]} {
      NodeListS_item $myDom $myHtml $mySelector $myRoot $property
    }
  }
}

namespace eval ::hv3::DOM {
  proc NodeListS_item {dom html selector root idx} {
    set N [$html search $selector -index $idx -root $root]
    if {$N eq ""} {
      list null
    } else {
      list object [$dom node_to_dom $N]
    }
  }
}
