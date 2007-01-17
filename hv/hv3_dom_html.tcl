namespace eval hv3 { set {version($Id: hv3_dom_html.tcl,v 1.3 2007/01/17 10:15:12 danielk1977 Exp $)} 1 }

#--------------------------------------------------------------------------
# DOM Level 1 Html
#
# This file contains the Hv3 implementation of the DOM Level 1 Html. Where
# possible, Hv3 tries hard to be compatible with W3C and Gecko. Gecko
# is pretty much a clean super-set of W3C for this module.
#
# Interfaces defined in this file:
#
#     HTMLDocument
#     HTMLCollection
#     HTMLElement
#       HTMLFormElement
#       plus a truckload of other HTML***Element interfaces to come.
#
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLDocument (Node -> Document -> HTMLDocument)
#
# DOM level 1 interface (- sign means it's missing) in Hv3.
#
#     HTMLDocument.write(string)
#     HTMLDocument.writeln(string)
#     HTMLDocument.getElementById(string)
#     HTMLDocument.forms[]
#     HTMLDocument.anchors[]
#     HTMLDocument.links[]
#     HTMLDocument.applets[]
#     HTMLDocument.body
#     HTMLDocument.cookie
#
::hv3::dom::type HTMLDocument [list \
    Document               \
    DocumentEvent          \
    Node                   \
    NodePrototype          \
    EventTarget            \
] {

  # The document collections (DOM level 1)
  #
  #     HTMLDocument.images[] 
  #     HTMLDocument.forms[]
  #     HTMLDocument.anchors[]
  #     HTMLDocument.links[]
  #     HTMLDocument.applets[] 
  #
  # TODO: applets[] is supposed to contain "all the OBJECT elements that
  # include applets and APPLET (deprecated) elements in a document". Here
  # only the APPLET elements are collected.
  #
  dom_get -cache images   { $self HTMLDocument_Collection img }
  dom_get -cache forms    { $self HTMLDocument_Collection form }
  dom_get -cache applet   { $self HTMLDocument_Collection applet }
  dom_get -cache anchors  { $self HTMLDocument_Collection {a[name]} }
  dom_get -cache links    { $self HTMLDocument_Collection {area,a[href]} }
  dom_snit {
    method HTMLDocument_Collection {selector} {
      set cmd [list $options(-hv3) search $selector]
      list object [::hv3::DOM::HTMLCollection %AUTO% $myDom -nodelistcmd $cmd]
    }
  }

  #-------------------------------------------------------------------------
  # The HTMLDocument.write() and writeln() methods (DOM level 1)
  #
  dom_call -string write {THIS str} {
    catch { [$options(-hv3) html] write text $str }
    return ""
  }
  dom_call -string writeln {THIS str} {
    catch { [$options(-hv3) html] write text "$str\n" }
    return ""
  }

  #-------------------------------------------------------------------------
  # HTMLDocument.getElementById() method. (DOM level 1)
  #
  # This returns a single object (or NULL if an object of the specified
  # id cannot be found).
  #
  dom_call -string getElementById {THIS elementId} {
    set node [lindex [$options(-hv3) search "#$elementId"] 0]
    if {$node ne ""} {
      return [list object [$myDom node_to_dom $node]]
    }
    return null
  }

  #-----------------------------------------------------------------------
  # The HTMLDocument.cookie property (DOM level 1)
  #
  # The cookie property is a strange modeling. Getting and putting the
  # property are not related in the usual way (the usual way: calling Get
  # returns the value stored by Put).
  #
  # When setting the cookies property, at most a single cookie is added
  # to the cookies database. 
  #
  # The implementations of the following get and put methods interface
  # directly with the ::hv3::the_cookie_manager object. Todo: Are there
  # security implications here (in concert with the location property 
  # perhaps)?
  #
  dom_get cookie {
    list string [::hv3::the_cookie_manager Cookie [$options(-hv3) uri get]]
  }
  dom_put -string cookie value {
    ::hv3::the_cookie_manager SetCookie [$options(-hv3) uri get] $value
  }

  #-----------------------------------------------------------------------
  # The HTMLDocument.body property (DOM level 1)
  #
  dom_get body {
    set body [lindex [$options(-hv3) search body] 0]
    list object [$myDom node_to_dom $body]
  }

  #-----------------------------------------------------------------------
  # The "location" property (Gecko compatibility)
  #
  # Setting the value of the document.location property is equivalent
  # to calling "document.location.assign(VALUE)".
  #
  dom_get -cache location { 
    set obj [::hv3::dom::Location %AUTO% $myDom $options(-hv3)] 
    list object $obj
  }
  dom_put location value { 
    set location [lindex [$self Get location] 1]
    set assign [lindex [$location Get assign] 1]
    $assign Call THIS $value
  }

  #-------------------------------------------------------------------------
  # Handle unknown property requests.
  #
  # An unknown property may refer to certain types of document element
  # by either the "name" or "id" HTML attribute.
  #
  # 1: Have to find some reference for this behaviour...
  # 2: Maybe this is too inefficient. Maybe it should go to the 
  #    document.images and document.forms collections.
  #
  dom_get * {

    # Allowable element types.
    set tags [list form img]

    # Selectors to use to find document nodes.
    set nameselector [subst -nocommands {[name="$property"]}]
    set idselector   [subst -nocommands {[id="$property"]}]
 
    foreach selector [list $nameselector $idselector] {
      set node [lindex [$options(-hv3) search $selector] 0]
      if {$node ne "" && [lsearch $tags [$node tag]] >= 0} {
        return [list object [[$options(-hv3) dom] node_to_dom $node]]
      }
    }

    list
  }
}

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
::hv3::dom::type HTMLCollection {} {

  #
  # There are several variations on the role this object may play in
  # DOM level 1 Html:
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
  dom_snit {
    option -nodelistcmd -default ""
  }

  # HTMLCollection.length
  #
  dom_get length {
    return [list number [llength [eval $options(-nodelistcmd)]]]
  }

  # HTMLCollection.item()
  #
  dom_call -string item {THIS index} {
    $self HTMLCollection_item $index
  }

  # HTMLCollection.namedItem()
  #
  dom_call -string namedItem {THIS name} {
    $self HTMLCollection_namedItem $name
  }

  # Handle an attempt to retrieve an unknown property.
  #
  dom_get * {

    # If $property looks like a number, treat it as an index into the list
    # of widget nodes. Otherwise look for a node with the "name" or "id"
    # attribute set to the attribute name.
    if {[string is double $property]} {
      set res [$self HTMLCollection_item $property]
    } else {
      set res [$self HTMLCollection_namedItem $property]
    }

    return $res
  }

  dom_snit { 
    method HTMLCollection_item {index} {
      set idx [format %.0f $index]
      set ret ""
      set node [lindex [eval $options(-nodelistcmd)] $idx]
      if {$node ne ""} {
        set ret [list object [$myDom node_to_dom $node]]
      }
      set ret
    }

    method HTMLCollection_namedItem {name} {
      set nodelist [eval $options(-nodelistcmd)]
  
      foreach node $nodelist {
        if {[$node attr -default "" id] eq $name} {
          set domobj [$myDom node_to_dom $node]
          return [list object $domobj]
        }
      }
  
      foreach node $nodelist {
        if {[$node attr -default "" name] eq $name} {
          set domobj [$myDom node_to_dom $node]
          return [list object $domobj]
        }
      }
  
      return ""
    }
  }
}
# </HTMLCollection>
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
#
::hv3::dom::type HTMLElement Element {
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

  #----------------------------------------------------------------------
  # The HTMLElement.innerHTML property. This is not part of any standard.
  # See reference for the equivalent mozilla property at:
  #
  #     http://developer.mozilla.org/en/docs/DOM:element.innerHTML
  #
  dom_get innerHTML { list string [$self HTMLElement_getInnerHTML] }
  dom_put -string innerHTML val { 
    $self HTMLElement_putInnerHTML $val 
  }

  dom_snit {

    method HTMLElement_getInnerHTML {} {
      list string [HTMLElement_ChildrenToHtml $options(-nodehandle)]
    }

    proc HTMLElement_ChildrenToHtml {elem} {
      set ret ""
      foreach child [$elem children] {
        set tag [$child tag]
        if {$tag eq ""} {
          append ret [$child text -pre]
        } else {
          append ret "<$tag>"
          append ret [HTMLElement_ChildrenToHtml $child]
          append ret "</$tag>"
        }
      }
      return $ret
    }

    method HTMLElement_putInnerHTML {newHtml} {
      set node $options(-nodehandle)

      # Destroy the existing children (and their descendants)
      set children [$node children]
      $node remove $children
      foreach child $children {
        $child destroy
      }

      # Insert the new descendants, created by parsing $newHtml.
      set doc [$myDom node_to_document $node]
      set htmlwidget [[$doc cget -hv3] html]
      set children [$htmlwidget fragment $newHtml]
      $node insert $children
      return ""
    }
  }
}

#-------------------------------------------------------------------------
# DOM Type HTMLFormElement (extends HTMLElement)
#
::hv3::dom::type HTMLFormElement HTMLElement {

  # Various Get/Put string property/attributes.
  #
  element_attr name
  element_attr target
  element_attr method
  element_attr action
  element_attr acceptCharset -attribute acceptcharset
  element_attr enctype

  # The HTMLFormElement.elements array.
  #
  dom_get -cache elements {
    set cmd [subst -nocommands {[$options(-nodehandle) replace] controls}]
    list object [::hv3::DOM::HTMLCollection %AUTO% $myDom -nodelistcmd $cmd]
  }

  # Form control methods: submit() and reset().
  #
  dom_call submit {THIS} {
    set form [$options(-nodehandle) replace]
    $form submit ""
  }
  dom_call reset {THIS} {
    set form [$options(-nodehandle) replace]
    $form reset
  }

  # Unknown property handler. Delegate any unknown property requests to
  # the HTMLFormElement.elements object.
  #
  dom_get * {
    set obj [lindex [$self Get elements] 1]
    $obj Get $property
  }
}
# </HTMLFormElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLInputElement (extends HTMLElement)
#
::hv3::dom::type HTMLInputElement HTMLElement {

  dom_todo defaultValue
  dom_todo defaultChecked
  dom_todo form
  dom_todo accept
  dom_todo accessKey
  dom_todo align
  dom_todo alt
  dom_todo checked
  dom_todo disabled
  dom_todo maxLength
  dom_todo name
  dom_todo readOnly
  dom_todo size
  dom_todo src
  dom_todo tabIndex
  dom_todo type
  dom_todo useMap

  dom_get value             { list string [[$options(-nodehandle) replace] value] }
  dom_put -string value val { [$options(-nodehandle) replace] set_value $val      }

  dom_call blur   {THIS} { [$options(-nodehandle) replace] dom_blur }
  dom_call focus  {THIS} { [$options(-nodehandle) replace] dom_focus }
  dom_call select {THIS} { [$options(-nodehandle) replace] dom_select }
  dom_call click  {THIS} { [$options(-nodehandle) replace] dom_click }
}
# </HTMLInputElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLSelectElement (extends HTMLElement)
#
::hv3::dom::type HTMLSelectElement HTMLElement {
}
# </HTMLSelectElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLTextAreaElement (extends HTMLElement)
#
::hv3::dom::type HTMLTextAreaElement HTMLElement {
}
# </HTMLTextAreaElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLButtonElement (extends HTMLElement)
#
::hv3::dom::type HTMLButtonElement HTMLElement {
}
# </HTMLButtonElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLOptGroupElement (extends HTMLElement)
#
::hv3::dom::type HTMLOptGroupElement HTMLElement {
}
# </HTMLOptGroupElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLOptionElement (extends HTMLElement)
#
::hv3::dom::type HTMLOptionElement HTMLElement {
}
# </HTMLOptionElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLLabelElement (extends HTMLElement)
#
::hv3::dom::type HTMLLabelElement HTMLElement {
}
# </HTMLLabelElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLFieldSetElement (extends HTMLElement)
#
::hv3::dom::type HTMLFieldSetElement HTMLElement {
}
# </HTMLFieldSetElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLLegendElement (extends HTMLElement)
#
::hv3::dom::type HTMLLegendElement HTMLElement {
}
# </HTMLLegendElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# Element/Text Node Factory:
#
#     This block implements a factory method called by the ::hv3::dom
#     object to transform html-widget node handles into DOM objects.
#
namespace eval ::hv3::dom {

  ::variable TagToNodeTypeMap

  array set TagToNodeTypeMap {
    ""       ::hv3::DOM::Text
  }

  # HTML Forms related objects:
  array set TagToNodeTypeMap {
    form     ::hv3::DOM::HTMLFormElement
    button   ::hv3::DOM::HTMLButtonElement
    input    ::hv3::DOM::HTMLInputElement
    select   ::hv3::DOM::HTMLSelectElement
    textarea ::hv3::DOM::HTMLTextAreaElement
    optgroup ::hv3::DOM::HTMLOptGroupElement
    option   ::hv3::DOM::HTMLOptionElement
    label    ::hv3::DOM::HTMLLabelElement
    fieldset ::hv3::DOM::HTMLFieldSetElement
    legend   ::hv3::DOM::HTMLLegendElement
  }

  proc getHTMLElementClassList {} {
    ::variable TagToNodeTypeMap
    set ret [list]
    foreach e [array names TagToNodeTypeMap] {
      lappend ret [string range $TagToNodeTypeMap($e) 12 end]
    }
    set ret
  }

  # Create a DOM HTMLElement or Text object in DOM $dom (type ::hv3::dom)
  # wrapped around the html-widget $node.
  #
  proc createWidgetNode {dom node} {
    ::variable TagToNodeTypeMap

    set tag [$node tag]

    set objtype ::hv3::DOM::HTMLElement
    catch {
      set objtype $TagToNodeTypeMap($tag)
    }

    $objtype %AUTO% $dom -nodehandle $node
  }
}
#-------------------------------------------------------------------------



