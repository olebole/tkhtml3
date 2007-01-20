namespace eval hv3 { set {version($Id: hv3_dom_html.tcl,v 1.4 2007/01/20 07:58:40 danielk1977 Exp $)} 1 }

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

  #--------------------------------------------------------------------
  # The completely non-standard offsetParent, offsetTop and offsetLeft.
  # TODO: Ref.
  #
  dom_snit {
    method HTMLElement_offsetParent {} {
      for {set N [$options(-nodehandle) par]} {$N ne ""} {set N [$N parent]} {
        set position [$N property position]
        if {$position ne "static"} break
      }
      return $N
    }
  }

  dom_get offsetParent { 
    set N [$self HTMLElement_offsetParent]
    if {$N eq ""} {
      list null
    } else {
      list object [$myDom node_to_dom $N]
    }
  }

  dom_get offsetLeft { 
    set hv3 [$myDom node_to_hv3 $options(-nodehandle)] 

    set bbox [$hv3 bbox $options(-nodehandle)]
    list number [lindex $bbox 0]
  }

  dom_get offsetTop { 
    set hv3 [$myDom node_to_hv3 $options(-nodehandle)] 
    set bbox [$hv3 bbox $options(-nodehandle)]
    # if {[lindex $bbox 1] < 0} {error "$bbox"}
    # set ret [list number [lindex $bbox 1]]
    
    set ptop 0
    set parent [$self HTMLElement_offsetParent]
    if {$parent ne ""} {
      set bbox [$hv3 bbox $parent]
      puts $bbox
      set ptop [lindex [$hv3 bbox $parent] 1]
    }

    set ret [list number [expr {[lindex $bbox 1] - $ptop}]]
    set ret
  }

  dom_get offsetHeight { 
    set hv3 [$myDom node_to_hv3 $options(-nodehandle)] 
    set bbox [$hv3 bbox $options(-nodehandle)]
    list number [expr {[lindex $bbox 3] - [lindex $bbox 1]}]
  }
  dom_get offsetWidth { 
    set hv3 [$myDom node_to_hv3 $options(-nodehandle)] 
    set bbox [$hv3 bbox $options(-nodehandle)]
    list number [expr {[lindex $bbox 2] - [lindex $bbox 0]}]
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

  dom_get value { list string [[$options(-nodehandle) replace] value] }
  dom_put -string value val { [$options(-nodehandle) replace] set_value $val }

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
#
#     http://api.kde.org/cvs-api/kdelibs-apidocs/khtml/html/classDOM_1_1HTMLTextAreaElement.html
#
::hv3::dom::type HTMLTextAreaElement HTMLElement {

  dom_get value { list string [[$options(-nodehandle) replace] value] }
  dom_put -string value val { [$options(-nodehandle) replace] set_value $val }

  dom_call focus  {THIS} { [$options(-nodehandle) replace] dom_focus }

  #-------------------------------------------------------------------------
  # The following are not part of the standard DOM. They are mozilla
  # extensions. KHTML implements them too. 
  #
  dom_get selectionEnd   { $self HTMLTextAreaElement_getSelection 1 }
  dom_get selectionStart { $self HTMLTextAreaElement_getSelection 0 }

  dom_snit {
    method HTMLTextAreaElement_getSelection {isEnd} {
      set t [[$options(-nodehandle) replace] get_text_widget]
      set sel [$t tag nextrange sel 0.0]
      if {$sel eq ""} {
        set ret [string length [$t get 0.0 insert]]
      } else {
        set ret [string length [$t get 0.0 [lindex $sel $isEnd]]]
      }
      list number $ret
    }
  }

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
# DOM Type HTMLTableElement (extends HTMLElement)
#
::hv3::dom::type HTMLTableElement HTMLElement {
  dom_todo caption
  dom_todo tHead
  dom_todo tFoot

  dom_todo rows

  dom_get -cache tBodies {
    set cmd [mymethod HTMLTableElement_getTBodies]
    list object [::hv3::DOM::HTMLCollection %AUTO% $myDom -nodelistcmd $cmd]
  }
  dom_snit {
    method HTMLTableElement_getTBodies {} {
      set tbodies [list] 
      foreach child [$options(-nodehandle) children] {
        if {[$child tag] eq "tbody"} { lappend tbodies $child }
      }
      set tbodies
    }
  }

  element_attr align
  element_attr bgColor -attribute bgcolor
  element_attr border
  element_attr cellPadding -attribute cellpadding
  element_attr cellSpacing -attribute cellspacing
  element_attr frame
  element_attr rules
  element_attr summary
  element_attr width

  dom_todo createTHead
  dom_todo deleteTHead
  dom_todo createTFoot
  dom_todo deleteTFoot
  dom_todo createCaption
  dom_todo deleteCaption
  dom_todo insertRow
  dom_todo deleteRow
}
# </HTMLTableElement>
#-------------------------------------------------------------------------
#-------------------------------------------------------------------------
# DOM Type HTMLTableSectionElement (extends HTMLElement)
#
#     This DOM type is used for HTML elements <TFOOT>, <THEAD> and <TBODY>.
#
::hv3::dom::type HTMLTableSectionElement HTMLElement {

  element_attr align
  element_attr ch -attribute char
  element_attr chOff -attribute charoff
  element_attr vAlign -attribute valign

  dom_get -cache rows {
    set cmd [mymethod HTMLTableSectionElement_getRows]
    list object [::hv3::DOM::HTMLCollection %AUTO% $myDom -nodelistcmd $cmd]
  }
  dom_snit {
    method HTMLTableSectionElement_getRows {} {
      set rows [list] 
      foreach child [$options(-nodehandle) children] {
        if {[$child tag] eq "tr"} { lappend rows $child }
      }
      set rows
    }
  }

  dom_todo insertRow
  dom_todo deleteRow
}

# </HTMLTableSectionElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLTableRowElement (extends HTMLElement)
#
#     This DOM type is used for HTML <TR> elements.
#
::hv3::dom::type HTMLTableRowElement HTMLElement {

  dom_todo rowIndex
  dom_todo sectionRowIndex

  dom_get -cache cells {
    set cmd [mymethod HTMLTableRowElement_getCells]
    list object [::hv3::DOM::HTMLCollection %AUTO% $myDom -nodelistcmd $cmd]
  }
  dom_snit {
    method HTMLTableRowElement_getCells {} {
      set cells [list] 
      foreach child [$options(-nodehandle) children] {
        set tag [$child tag]
        if {$tag eq "td" || $tag eq "th"} {lappend cells $child}
      }
      set cells
    }
  }

  element_attr align
  element_attr bgColor -attribute bgcolor
  element_attr ch -attribute char
  element_attr chOff -attribute charoff
  element_attr vAlign -attribute valign


  dom_todo insertCell
  dom_todo deleteCell
}

# </HTMLTableSectionElement>
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

  # HTML Tables related objects:
  array set TagToNodeTypeMap {
    table    ::hv3::DOM::HTMLTableElement
    tbody    ::hv3::DOM::HTMLTableSectionElement
    tfoot    ::hv3::DOM::HTMLTableSectionElement
    thead    ::hv3::DOM::HTMLTableSectionElement
    tr       ::hv3::DOM::HTMLTableRowElement
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

