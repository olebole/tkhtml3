
#--------------------------------------------------------------------------
# DOM Level 1 Html
#
# This file contains the Hv3 implementation of the DOM Level 1 Html. Where
# possible, Hv3 tries hard to be compatible with W3C and Gecko. Gecko
# is pretty much a clean super-set of W3C for this module.
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

  #-------------------------------------------------------------------------
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
  dom_get -cache images   { $self CollectionObject 0 img }
  dom_get -cache forms    { $self CollectionObject 0 form }
  dom_get -cache applet   { $self CollectionObject 0 applet }
  dom_get -cache anchors  { $self CollectionObject 0 {a[name]} }
  dom_get -cache links    { $self CollectionObject 0 {area,a[href]} }

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

  dom_get -cache style { 
    list object [::hv3::dom::InlineStyle %AUTO% $myDom $options(-nodehandle)]
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




