namespace eval hv3 { set {version($Id: hv3_dom_core.tcl,v 1.9 2007/06/01 18:07:48 danielk1977 Exp $)} 1 }

#--------------------------------------------------------------------------
# DOM Level 1 Core
#
# This file contains the Hv3 implementation of the DOM Level 1 Core. Where
# possible, Hv3 tries hard to be compatible with W3C and Gecko. Gecko
# is pretty much a clean super-set of W3C for this module.
#
#-------------------------------------------------------------------------

#--------------------------------------------------------------------------
# The Node Prototype object
#
#     Currently, Hv3 implements the following types:
#
#         * Element nodes (1)        -> type HTMLElement
#         * Attribute nodes (2)      -> type Attr
#         * Text nodes (3)           -> type Text
#         * Document nodes (9)       -> type HTMLDocument
#
#     Probably also want to implement Document-fragment nodes at some
#     stage. And Comment too, but I bet we can get away without it :)
#    
::hv3::dom2::stateless NodePrototype {} {
  # Required by XML and HTML applications:
  dom_get ELEMENT_NODE                {list number 1}
  dom_get ATTRIBUTE_NODE              {list number 2}
  dom_get TEXT_NODE                   {list number 3}
  dom_get COMMENT_NODE                {list number 8}
  dom_get DOCUMENT_NODE               {list number 9}
  dom_get DOCUMENT_FRAGMENT_NODE      {list number 11}

  # Required by XML applications only:
  dom_get CDATA_SECTION_NODE          {list number 4}
  dom_get ENTITY_REFERENCE_NODE       {list number 5}
  dom_get ENTITY_NODE                 {list number 6}
  dom_get PROCESSING_INSTRUCTION_NODE {list number 7}
  dom_get DOCUMENT_TYPE_NODE          {list number 10}
  dom_get NOTATION_NODE               {list number 12}
}

#--------------------------------------------------------------------------
# This block contains default implementations of the methods and
# attributes in the Node interface (DOM Level 1 Core).
#
::hv3::dom2::stateless Node {} {

  dom_get nodeName        {error "Must be overridden ($property)"}
  dom_get nodeType        {error "Must be overridden ($property)"}

  # Node.nodeValue is null of all nodes except ATTRIBUTE and TEXT.
  # Also, technically CDATA_SECTION, COMMENT and PROCESSING_INSTRUCTION,
  # but these are not implemented in Hv3.
  #
  dom_get nodeValue       {list null}

  # Node.attributes is null for all nodes types except ELEMENT
  #
  dom_get attributes      {list null}

  # Default implementation is good enough for DOCUMENT nodes.
  #
  dom_get previousSibling {list null}
  dom_get nextSibling     {list null}

  # Default implementations for nodes that are not allowed children.
  # i.e. those of type ATTRIBUTE and TEXT.
  #
  dom_get parentNode {list null}
  dom_get firstChild {list null}
  dom_get lastChild  {list null}

  # Default implementation of childNodes() returns an empty NodeList 
  # containing no Nodes.
  #
  dom_get childNodes {
    list object [list ::hv3::DOM::NodeList $myDom list]
  }
  dom_call hasChildNodes {THIS} {list boolean false}

  dom_todo ownerDocument

  dom_call insertBefore {THIS newChild refChild} {
    error "DOMException HIERACHY_REQUEST_ERR"
  }
  dom_call replaceChild {THIS newChild oldChild} {
    error "DOMException HIERACHY_REQUEST_ERR"
  }
  dom_call removeChild {THIS oldChild} {
    error "DOMException HIERACHY_REQUEST_ERR"
  }
  dom_call appendChild {THIS newChild} {
    error "DOMException HIERACHY_REQUEST_ERR"
  }

  # Method to clone the node. Spec indicates that it is optional to
  # support this on for DOCUMENT nodes, hence the exception.
  #
  dom_call cloneNode {THIS} {error "DOMException NOT_SUPPORTED_ERR"}
}

::hv3::dom2::stateless Document {} {

  # The ::hv3::hv3 widget containing the document this DOM object
  # represents.
  #
  dom_parameter myHv3

  # Override the two mandatory Node methods.
  #
  dom_get nodeType {list number 9}          ;# Node.DOCUMENT_NODE -> 9
  dom_get nodeName {list string #document}

  # Override other Node methods. The default implementations of 
  # nextSibling(), previousSibling() and parentNode() are Ok, but the
  # properties for accessing child-nodes need to be implemented.
  #
  # The Document node only has one child -> the <HTML> node of the 
  # document tree.
  #
  dom_get childNodes {
    set cmd [list $myHv3 node]
    list object [list ::hv3::DOM::NodeList $myDom $cmd]
  }
  dom_get firstChild {list object [$myDom node_to_dom [$myHv3 node]]}
  dom_get lastChild  {list object [$myDom node_to_dom [$myHv3 node]]}

  # The document node always has exactly one child node.
  #
  dom_call hasChildNodes {THIS} {list boolean true}

  # dom_call replaceChild {THIS newChild oldChild}  {error "TODO"}
  # dom_call removeChild  {THIS oldChild}           {error "TODO"}
  # dom_call appendChild  {THIS newChild}           {error "TODO"}
  # dom_call insertBefore {THIS newChild refChild}  {error "TODO"}

  dom_todo insertBefore
  dom_todo replaceChild
  dom_todo removeChild 
  dom_todo appendChild  

  # For a Document node, the ownerDocument is null.
  #
  dom_get ownerDocument {list null}

  # End of Node interface overrides.
  #---------------------------------


  dom_todo doctype
  dom_todo implementation

  dom_get documentElement {
    list object [$myDom node_to_dom [$myHv3 node]]
  }

  dom_call -string createElement {THIS tagname} {
    set node [$myHv3 fragment "<$tagname>"]
    if {$node eq ""} {error "DOMException NOT_SUPPORTED_ERR"}
    list object [$myDom node_to_dom $node]
  }
  dom_call -string createTextNode {THIS data} {
    set escaped [string map {< &lt; > &gt;} $data]
    set node [$myHv3 fragment $escaped]
  }

  dom_todo createDocumentFragment
  dom_todo createTextNode
  dom_todo createComment
  dom_todo createAttribute

  #-------------------------------------------------------------------------
  # The Document.getElementsByTagName() method (DOM level 1).
  #
  dom_call -string getElementsByTagName {THIS tag} { 
    # TODO: Should check that $tag is a valid tag name. Otherwise, one day
    # someone is going to pass ".id" and wonder why all the elements with
    # the "class" attribute set to "id" are returned.
    #
    set cmd [list $myHv3 search $tag]
    set nl [list ::hv3::DOM::NodeList $myDom $cmd]
    list transient $nl
  }
}

#-------------------------------------------------------------------------
# This is not a DOM type. It contains code that is common to the
# DOM types "HTMLElement" and "Text". These are both wrappers around
# html widget node-handles, hence the commonality. The following 
# parts of the Node interface are implemented here:
#
#     Node.parentNode
#     Node.previousSibling
#     Node.nextSibling
#     Node.ownerDocument
#
::hv3::dom2::stateless WidgetNode {} {

  dom_parameter myNode

  # Retrieve the parent node.
  #
  dom_get parentNode {
    set parent [$myNode parent]
    if {$parent eq ""} {
      # Parent of the root of the document (the <HTML> node)
      # is the DOM HTMLDocument object.
      #
      set ret [list object [$myDom node_to_document $myNode]]
    } else {
      set ret [list object [$myDom node_to_dom $parent]]
    }
    set ret
  }

  # Retrieve the left and right sibling nodes.
  #
  dom_get previousSibling {WidgetNode_Sibling $myNode -1}
  dom_get nextSibling     {WidgetNode_Sibling $myNode +1}

  dom_get ownerDocument { 
    list object [$myDom node_to_document $myNode]
  }
}
namespace eval ::hv3::DOM {
  proc WidgetNode_Sibling {node dir} {
    set ret null
    set parent [$node parent]
    if {$parent ne ""} {
      set siblings [$parent children]
      set idx [lsearch $siblings $node]
      incr idx $dir
      if {$idx >= 0 && $idx < [llength $siblings]} {
        set ret [list object [$myDom node_to_dom [lindex $siblings $idx]]]
      }
    }
    set ret
  }
}

#-------------------------------------------------------------------------
# DOM Type Element (Node -> Element)
#
#     This object is never actually instantiated. HTMLElement (and other,
#     element-specific types) are instantiated instead.
#
set BaseList {ElementCSSInlineStyle WidgetNode Node NodePrototype EventTarget}
::hv3::dom2::stateless Element $BaseList {
  
  # Override parts of the Node interface.
  #
  dom_get nodeType {list number 1}           ;#     Node.ELEMENT_NODE -> 1
  dom_get nodeName {list string [string toupper [$myNode tag]]}

  dom_get childNodes {
    list object [list ::hv3::DOM::NodeList $myDom [list $myNode children]]
  }

  dom_get ownerDocument {
    error "TODO"
  }

  dom_call insertBefore {THIS newChild refChild}  {
    error "TODO"

    # TODO: Arg checking and correct error messages (excptions).
    # Children of an Element Node can be either Element or Text nodes.
    set new [[lindex $newChild 1] cget -nodehandle]

    if {[lindex $refChild 0] eq "object"} {
      set ref [[lindex $refChild 1] cget -nodehandle]
    } else {
      set ref [lindex [$myNode children] 0]
    }

    if {$ref ne ""} {
      $myNode insert -before $ref $new
    } else {
      $myNode insert $new
    }

    return ""
  }
  dom_call appendChild {THIS newChild} {
    error "TODO"

    set new [[lindex $newChild 1] cget -nodehandle]
    $myNode insert $new
    return ""
  }

  dom_call removeChild {THIS oldChild} {
    error "TODO"

    set old [[lindex $oldChild 1] cget -nodehandle]
    $myNode remove $old
    return ""
  }

  # dom_todo insertBefore
  dom_todo replaceChild


  # End of Node interface overrides.
  #---------------------------------

  # Element.tagName
  #
  #     DOM Level 1 HTML section 2.5.3 specifically says that the string
  #     returned for the tag-name property be in upper-case. Tkhtml3 should
  #     probably be altered to match this.
  #
  dom_get tagName {
    list string [string toupper [$myNode tag]]
  }

  dom_call -string getAttribute {THIS attr} {
    Element_getAttributeString $myNode $attr ""
  }
  dom_call -string setAttribute {THIS attr v} {
    Element_putAttributeString $myNode $attr $v
  }

  dom_todo removeAttribute
  dom_todo getAttributeNode
  dom_todo setAttributeNode
  dom_todo removeAttributeNode

  dom_call -string getElementsByTagName {THIS tagname} {
    set hv3 [$myDom node_to_hv3 $myNode]
    set cmd [list $hv3 search $tagname -root $myNode]
    set nl [list ::hv3::DOM::NodeList $myDom $cmd]
    list transient $nl
  }

  dom_todo normalize

}
namespace eval ::hv3::DOM {
  proc Element_getAttributeString {node name def} {
    set val [$node attribute -default $def $name]
    list string $val
  }
  proc Element_putAttributeString {node name val} {
    $node attribute $name $val
    return ""
  }
}

# element_attr --
#
#     This command can be used in the body of DOM objects that mixin class
#     Element.
#
#     element_attr NAME ?OPTIONS?
#
#         -attribute ATTRIBUTE-NAME          (default NAME)
#         -readonly                          (make the attribute readonly)
#
namespace eval ::hv3::dom::compiler {

  proc element_attr {name args} {

    set readonly 0
    set attribute $name

    # Process the arguments to [element_attr]:
    for {set ii 0} {$ii < [llength $args]} {incr ii} {
      set s [lindex $args $ii]
      switch -- $s {
        -attribute {
          incr ii
          set attribute [lindex $args $ii]
        }
        -readonly {
          set readonly 1
        }
        default {
          error "Bad option to element_attr: $s"
        }
      }
    }

    # The Get code.
    dom_get $name [subst -novariables {
      Element_getAttributeString $myNode [set attribute] ""
    }]

    # Create the Put method (unless the -readonly switch was passed).
    if {!$readonly} {
      dom_put -string $name val [subst -novariables {
        Element_putAttributeString $myNode [set attribute] $val
      }]
    }
  }
}
namespace eval ::hv3::dom2::compiler {

  proc element_attr {name args} {

    set readonly 0
    set attribute $name

    # Process the arguments to [element_attr]:
    for {set ii 0} {$ii < [llength $args]} {incr ii} {
      set s [lindex $args $ii]
      switch -- $s {
        -attribute {
          incr ii
          set attribute [lindex $args $ii]
        }
        -readonly {
          set readonly 1
        }
        default {
          error "Bad option to element_attr: $s"
        }
      }
    }

    # The Get code.
    dom_get $name [subst -novariables {
      Element_getAttributeString $myNode [set attribute] ""
    }]

    # Create the Put method (unless the -readonly switch was passed).
    if {!$readonly} {
      dom_put -string $name val [subst -novariables {
        Element_putAttributeString $myNode [set attribute] $val
      }]
    }
  }
}


#-------------------------------------------------------------------------
# DOM Type Text (Node -> Text)
#
set BaseList {WidgetNode Node NodePrototype EventTarget}
::hv3::dom2::stateless Text $BaseList {

  # Override parts of the Node interface.
  #
  dom_get nodeType  {list number 3}           ;#     Node.TEXT_NODE -> 3
  dom_get nodeName  {list string #text}
  dom_get nodeValue {list string [$myNode text -pre] }

  # TODO: This needs to be implemented.
  #
  dom_call cloneNode {THIS} {error "DOMException NOT_SUPPORTED_ERR"}

  # End of Node interface overrides.
  #---------------------------------
}
