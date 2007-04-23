namespace eval hv3 { set {version($Id: hv3_dom_html.tcl,v 1.11 2007/04/23 17:31:16 danielk1977 Exp $)} 1 }

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

  dom_todo title
  dom_todo referrer
  dom_todo domain
  dom_todo URL

  dom_todo open
  dom_todo close

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
    catch { [$options(-hv3) html] write text $str } msg
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
    set elementId [string map [list "\x22" "\x5C\x22"] $elementId]
    set selector [subst -nocommands {[id="$elementId"]}]
    set node [lindex [$options(-hv3) search $selector] 0]
    if {$node ne ""} {
      return [list object [$myDom node_to_dom $node]]
    }
    return null
  }

  #-------------------------------------------------------------------------
  # HTMLDocument.getElementsByName() method. (DOM level 1)
  #
  # Return a NodeList of the elements whose "name" value is set to
  # the supplied argument. This is similar to the 
  # Document.getElementsByTagName() method in hv3_dom_core.tcl.
  #
  dom_call -string getElementsByName {THIS elementName} {
    set name [string map [list "\x22" "\x5C\x22"] $elementName]
    set cmd [list $options(-hv3) search [subst -nocommands {[name="$name"]}]]
    set nl [::hv3::DOM::NodeList %AUTO% $myDom -nodelistcmd $cmd]
    list transient $nl
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
    set obj [::hv3::DOM::Location %AUTO% $myDom -hv3 $options(-hv3)] 
    list object $obj
  }
  dom_put -string location value { 
    set location [lindex [$self Get location] 1]
    $location Location_assign $value
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
    # of widget nodes. 
    #
    # Otherwise look for nodes with the "name" or "id" attribute set 
    # to the queried attribute name. If a single node is found, return
    # it directly. If more than one have matching names or ids, a NodeList
    # containing the matches is returned.
    #
    if {[string is double $property]} {
      set res [$self HTMLCollection_item $property]
    } else {
      set res [
        HTMLCollection_getNodeHandlesByName $options(-nodelistcmd) $property
      ]
      set nRet [llength $res]
      if {$nRet==0} {
        set res ""
      } elseif {$nRet==1} {
        set res [list object [$myDom node_to_dom [lindex $res 0]]]
      } else {
        set getnodes [namespace code [list \
          HTMLCollection_getNodeHandlesByName $options(-nodelistcmd) $property
        ]]
        set obj [::hv3::DOM::NodeList %AUTO% $myDom -nodelistcmd $getnodes]
        set res [list object $obj]
      }
    }

    return $res
  }

  dom_snit { 

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
::hv3::dom::type HTMLElement Element {
  element_attr id
  element_attr title
  element_attr lang
  element_attr dir
  element_attr className -attribute class

  # The above is all that is defined by DOM Html Level 1 for HTMLElement.
  # Everything below this point is for compatibility with legacy browsers.
  #
  #   See Mozilla docs: http://developer.mozilla.org/en/docs/DOM:element
  #
  dom_todo localName
  dom_todo namespaceURI
  dom_todo prefix
  dom_todo textContent


  #----------------------------------------------------------------------
  # The HTMLElement.innerHTML property. This is not part of any standard.
  # See reference for the equivalent mozilla property at:
  #
  #     http://developer.mozilla.org/en/docs/DOM:element.innerHTML
  #
  dom_get innerHTML { 
    set res [$self HTMLElement_getInnerHTML]
    set res
  }
  dom_put -string innerHTML val { 
    $self HTMLElement_putInnerHTML $val
  }

  dom_snit {

    method HTMLElement_getInnerHTML {} {
      set str [HTMLElement_ChildrenToHtml $options(-nodehandle)]
      list string $str
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

    proc HTMLElement_offsetParent {node} {
      for {set N [$node parent]} {$N ne ""} {set N [$N parent]} {
        set position [$N property position]
        if {$position ne "static"} break
      }
      return $N
    }

    proc HTMLElement_nodeBox {dom node} {
      set hv3 [$dom node_to_hv3 $node]
      set bbox [$hv3 bbox $node]
      if {$bbox eq ""} {set bbox [list 0 0 0 0]}
      return $bbox
    }

    proc HTMLElement_offsetBox {dom node} {
      set bbox [HTMLElement_nodeBox $dom $node]

      set parent [HTMLElement_offsetParent $node]
      if {$parent ne ""} {
        set bbox2 [HTMLElement_nodeBox $dom $parent]
        set x [lindex $bbox2 0]
        set y [lindex $bbox2 1]
        lset bbox 0 [expr {[lindex $bbox 0] - $x}]
        lset bbox 1 [expr {[lindex $bbox 1] - $y}]
        lset bbox 2 [expr {[lindex $bbox 2] - $x}]
        lset bbox 3 [expr {[lindex $bbox 3] - $y}]
      }
      
      return $bbox
    }
  }

  dom_get offsetParent { 
    set N [HTMLElement_offsetParent $options(-nodehandle)]
    set ret null
    if {$N ne ""} { list object [$myDom node_to_dom $N] }
    set ret
  }
  
  # Implementation of Gecko/DHTML compatibility properties for 
  # querying layout geometry:
  #
  #  offsetLeft offsetTop offsetHeight offsetWidth
  #
  #    offsetHeight and offsetWidth are the height and width of the
  #    generated box, including borders.
  #
  #    offsetLeft and offsetTop are the offsets of the generated box
  #    within the box generated by the element returned by DOM
  #    method HTMLElement.offsetParent().
  #
  #  clientLeft clientTop clientHeight clientWidth
  #
  #    clientHeight and clientWidth are the height and width of the
  #    box generated by the node, including padding but not borders.
  #    clientLeft is the width of the generated box left-border. clientTop 
  #    is the generated boxes top border.
  #
  #    Note that there are complications with boxes that generate their
  #    own scrollbars if the scrollbar is on the left or top side. Since
  #    hv3 never does this, it doesn't matter.
  #
  #  scrollLeft scrollTop scrollHeight scrollWidth
  #
  #    For most elements, scrollLeft and scrollTop are 0, and scrollHeight
  #    and scrollWidth are equal to clientHeight and clientWidth.
  #
  #    For elements that generate their own scrollable boxes, (i.e. with 
  #    'overflow' property set to "scroll") scrollHeight and scrollWidth
  #    are the height and width of the scrollable areas. scrollLeft/scrollTop
  #    are the current scroll offsets.
  #
  dom_get offsetLeft { 
    list number [lindex [HTMLElement_offsetBox $myDom $options(-nodehandle)] 0]
  }
  dom_get offsetTop { 
    list number [lindex [HTMLElement_offsetBox $myDom $options(-nodehandle)] 1]
  }
  dom_get offsetHeight { 
    set bbox [HTMLElement_offsetBox $myDom $options(-nodehandle)]
    list number [expr {[lindex $bbox 3] - [lindex $bbox 1]}]
  }
  dom_get offsetWidth { 
    set bbox [HTMLElement_offsetBox $myDom $options(-nodehandle)]
    list number [expr {[lindex $bbox 2] - [lindex $bbox 0]}]
  }

  dom_get clientLeft {
    set bw [$options(-nodehandle) property border-left-width]
    list number [string range $bw 0 end-2]
  }
  dom_get clientTop {
    set bw [$options(-nodehandle) property border-top-width]
    list number [string range $bw 0 end-2]
  }
  dom_get clientHeight {
    set N $options(-nodehandle)
    set bbox [HTMLElement_nodeBox $myDom $N]
    set bt [string range [$N property border-top-width] 0 end-2]
    set bb [string range [$N property border-bottom-width] 0 end-2]
    list number [expr [lindex $bbox 3] - [lindex $bbox 1] - $bt - $bb]
  }
  dom_get clientWidth {
    set N $options(-nodehandle)
    set bbox [HTMLElement_nodeBox $myDom $N]
    set bt [string range [$N property border-left-width] 0 end-2]
    set bb [string range [$N property border-right-width] 0 end-2]
    list number [expr [lindex $bbox 2] - [lindex $bbox 0] - $bt - $bb]
  }

  # TODO: See comments above for what these are supposed to do.
  dom_get scrollTop    { list number 0 }
  dom_get scrollLeft   { list number 0 }
  dom_get scrollWidth  { $self Get clientWidth }
  dom_get scrollHeight { $self Get clientHeight }
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
#     <INPUT> elements. The really complex stuff for this object is 
#     handled by the forms manager code.
#
::hv3::dom::type HTMLInputElement HTMLElement {

  dom_todo defaultValue
  dom_todo form
  dom_todo accept
  dom_todo accessKey
  dom_todo align
  dom_todo alt
  dom_todo checked
  dom_todo maxLength
  dom_todo name
  dom_todo readOnly
  dom_todo size
  dom_todo src
  dom_todo tabIndex
  dom_todo useMap

  element_attr disabled
  element_attr type -readonly

  # According to DOM HTML Level 1, the HTMLInputElement.defaultChecked
  # is the HTML element attribute, not the current value of the form
  # control. Setting this attribute sets both the value of the form
  # control and the HTML attribute.
  #
  dom_get defaultChecked { 
    set c [$options(-nodehandle) attr -default 0 checked]
    list boolean $c
  }
  dom_put -string defaultChecked C { 
    set F [$options(-nodehandle) replace]
    $F dom_checked $C
    $options(-nodehandle) attr checked $C
  }

  # The HTMLInputElement.checked attribute on the other hand is the
  # current state of the form control. Writing to it does not change
  # the attribute on the underlying HTML element.
  #
  dom_get checked { 
    set F [$options(-nodehandle) replace]
    list boolean [$F dom_checked]
  }
  dom_put -string checked C { 
    set F [$options(-nodehandle) replace]
    $F dom_checked $C
  }

  # HTMLInputElement.value is the current form control value if
  # the "type" is one of "Text", "File" or "Password". Otherwise
  # it is the attribute on the underlying HTML element.
  #
  dom_get value {
    set SPECIAL [list text file password]
    set T [string tolower [$options(-nodehandle) attr -default text type]]
    if {[lsearch $SPECIAL $T]>=0} {
      set F [$options(-nodehandle) replace]
      list string [$F dom_value]
    } else {
      list string [$options(-nodehandle) attr -default "" value]
    }
  }
  dom_put -string value V { 
    set SPECIAL [list text file password]
    set T [string tolower [$options(-nodehandle) attr -default text type]]
    if {[lsearch $SPECIAL $T]>=0} {
      set F [$options(-nodehandle) replace]
      $F dom_value $V
    } else {
      $options(-nodehandle) attr checked $V
    }
  }

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

  dom_get type {
    # DOM Level 1 says: "This is the string "select-multiple" when the 
    # multiple attribute is true and the string "select-one" when false."
    # However since Hv3 does not support multiple-select controls, this
    # property is always set to "select-one".
    list string "select-one"
  }

  dom_get selectedIndex {
    list number [[$options(-nodehandle) replace] dom_selectionIndex]
  }
  dom_put -string selectedIndex value {
    [$options(-nodehandle) replace] dom_setSelectionIndex $value
  }

  dom_get value {
    # The value attribute is read-only for this element. It is set to
    # the string value that will be submitted by this control during
    # form submission.
    list string [[$options(-nodehandle) replace] value]
  }

  dom_todo length
  dom_todo form

  dom_snit {
    method HTMLSelectElement_getOptions {} {
      # Note: This needs to be merged with code in hv3_form.tcl.
      set ret [list]
      foreach child [$options(-nodehandle) children] {
        if {[$child tag] == "option"} {lappend ret $child}
      }
      set ret
    }
  }
  dom_get -cache options {
    set cmd [mymethod HTMLSelectElement_getOptions]
    list object [::hv3::DOM::HTMLCollection %AUTO% $myDom -nodelistcmd $cmd]
  }

  dom_todo disabled

  dom_get multiple {
    # In Hv3, this attribute is always 0. This is because Hv3 does not
    # support multiple-select controls.
    list number 0
  }

  element_attr name
  element_attr size
  element_attr tabIndex -attribute tabindex

  dom_todo form
  dom_todo disabled

  dom_todo add
  dom_todo remove
  dom_call blur   {THIS} { [$options(-nodehandle) replace] dom_blur }
  dom_call focus  {THIS} { [$options(-nodehandle) replace] dom_focus }

  #--------------------------------------------------------------------
  # Non-standard stuff starts here.
  dom_get * {
    set obj [lindex [$self Get options] 1]
    $obj Get $property
  }
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
  dom_todo form
  dom_todo defaultSelected
  dom_todo text
  dom_todo index
  dom_todo disabled
  dom_get label {
    # TODO: After writing this attribute, have to update data structures in
    # the hv3_forms module.
    list string [$self HTMLOptionElement_getLabelOrValue label]
  }
  dom_put -string label v {
    $options(-nodehandle) attr label $v
  }
  dom_todo selected

  dom_snit {
    method HTMLOptionElement_getLabelOrValue {attr} {
      # If the element has text content, this is used as the default
      # for both the label and value of the entry (used if the Html
      # attributes "value" and/or "label" are not defined.
      #
      # Note: This needs to be merged with code in hv3_form.tcl.
      set contents ""
      catch {
        set t [lindex [$child children] 0]
        set contents [$t text]
      }
      $options(-nodehandle) attribute -default $contents $attr
    }
  }

  dom_get value {
    list string [$self HTMLOptionElement_getLabelOrValue value]
  }
  dom_put -string value v {
    # TODO: After writing this attribute, have to update data structures in
    # the hv3_forms module.
    $options(-nodehandle) attr value $v
  }
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
# </HTMLTableRowElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLAnchorElement (extends HTMLElement)
#
#     This DOM type is used for HTML <A> elements.
#
::hv3::dom::type HTMLAnchorElement HTMLElement {
  element_attr accessKey -attribute accesskey
  element_attr charset
  element_attr coords
  element_attr href
  element_attr hreflang
  element_attr name
  element_attr rel
  element_attr shape
  element_attr tabIndex -attribute tabindex
  element_attr target
  element_attr type

  # Hv3 does not support keyboard focus on <A> elements yet. So these
  # two calls are no-ops for now.
  #
  dom_call focus {THIS} {list}
  dom_call blur {THIS} {list}
}

# </HTMLAnchorElement>
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
    a        ::hv3::DOM::HTMLAnchorElement
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

