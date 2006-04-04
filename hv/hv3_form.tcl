
###########################################################################
# hv3_form.tcl --
#
#     This file contains code to implement the cosmetic forms functionality.
#     The public interface to this file is the command:
#
#         form_init HTML getcmd
#
#     The following Tk widgets are used for form elements:
#
#         <input>            -> button|radiobutton|menubutton|entry|image
#         <button>           -> button|image
#         <select>           -> menubutton
#         <textarea>         -> text
#
#     We attempt to baseline align the button, entry, radiobutton and 
#     menubutton widgets.
#

package require Itcl
namespace import itcl::class

#--------------------------------------------------------------------------
# An instance of the following class is instantiated for each <form> 
# element in the document that has at least one descendant element 
# that creates a control.
class HtmlForm {

  # The HtmlForm::instance common variable is an array. It maps from
  # a Tkhtml node-handle (always a <form> element) to the corresponding
  # HtmlForm object.
  public common instance

  private variable myControls    ;# List of HtmlControl objects
  private variable myNode        ;# Tkhtml node-handle for <form> element
  private variable myGetcmd      ;# Command to process a GET request.

  # HtmlForm::constructor
  constructor {node getcmd} {
    set myControls [list]
    set myNode $node
    set myGetcmd $getcmd
    set instance($myNode) $this
  }

  # HtmlForm::constructor
  destructor {
    unset instance($myNode)
  }

  # HtmlForm::add_control
  method add_control {control} {
    lappend myControls $control
  }

  # HtmlForm::del_control
  method del_control {control} {
    set n [lsearch $myControls $control]
    if {$n >= 0} {
      set myControls [lreplace $myControls $n $n]
    }
    if {[llength $myControls] == 0} {
      itcl::delete object $this
    }
  }

  # HtmlForm::submit
  method submit {} {
    set form_values [list]

    # Find the set of successful controls. A control is successful iff:
    #
    #     1. The name attribute was present and not an empty string, and
    #     2. The [success] method of the control object returns true.
    foreach control $myControls {
      set name [$control name]
      if {$name!="" && [$control success]} {
        set value [string map {" " +} [$control value]]
        lappend form_values "${name}=${value}"
      }
    }
  
    # Encode the form action and values into a URI
    set    uri [$myNode attr -default "" action]
    append uri ?
    append uri [join $form_values &]

    # Submit the request
    eval [concat $myGetcmd $uri]
  }
}
# End of HtmlForm
#--------------------------------------------------------------------------

proc find_form {control getcmd node} {
  # set name [$node attr -default "" name]
  # if {$name==""} return ""

  set form ""
  for {set N $node} {$N!="" && [$N tag]!="form"} {set N [$N parent]} {}
  if {$N!=""} {
    if {![info exists ::HtmlForm::instance($N)]} {
      HtmlForm #auto $N $getcmd
    }
    set form $::HtmlForm::instance($N) 
    $form add_control $control
  }
  return $form
}

#--------------------------------------------------------------------------
class HtmlControl {
  private {
    variable myName
  }

  protected {
    variable myForm
    variable myWidget
  }

  constructor {} {}

  destructor {
    catch {$myForm del_control $this}
    destroy $myWidget
  }

  method name {} {
    return $myName
  }

  method init {getcmd node} {
    set myName [$node attr -default "" name]
    set myForm [find_form $this $getcmd $node]
    $node replace $myWidget                    \
      -configurecmd [list $this configure]     \
      -deletecmd    [list itcl::delete object $this]
  }

  # HtmlControl methods that will be implemented by derived classes.
  method success {}
  method value {}
  method configure {props}

  proc font_to_offset {w font} {
    set descent [font metrics $font -descent]
    set ascent  [font metrics $font -ascent]
    set drop [expr ([winfo reqheight $w] + $descent - $ascent) / 2]
    return $drop
  }
}

#--------------------------------------------------------------------------
# Now there are 6 classes that inherit from HtmlControl defined:
#
#     HtmlEntry
#     HtmlButton
#     HtmlRadioButton
#     HtmlComboBox
#     HtmlText
#
# An instance of one of the above classes represents a single form control
# replaced by a Tk widget of the same name. Each class overrides the 
# following HtmlControl methods (as well as the constructor):
#
#      success
#      configure
#      value
#

class HtmlEntry {
  inherit HtmlControl

  constructor {HTML getcmd node} {
    set myWidget ${HTML}.[string map {: _} $node]

    entry $myWidget
    $myWidget configure -width [$node attr -default 20 size]
    $myWidget insert 0 [$node attr -default "" value]

    if {[$node attr -default "" type]=="password"} {
      $myWidget configure -show *
    }

    init $getcmd $node
    if {$myForm != ""} {
      bind $myWidget <KeyPress-Return> [list $myForm submit]
    }
  }

  method configure {props} {
    array set p $props
    $myWidget configure -font $p(font)
    # $myWidget configure -foreground $p(color)
    # $myWidget configure -background $p(background-color)
    $myWidget configure -background white
    $myWidget configure -borderwidth 1

    if {[info exists p(width)]} {
      set charwidth [expr $p(width) / [font measure $p(font) o]]
      $myWidget configure -width $charwidth
    }

    return [font_to_offset $myWidget $p(font)]
  }

  # An entry control is always successful. The current contents of
  # the entry box are used as the form value.
  method success {} {return 1}
  method value   {} {return [$myWidget get]}
}

class HtmlButton {
  inherit HtmlControl

  constructor {HTML getcmd node} {
    set myWidget ${HTML}.[string map {: _} $node]
    set type [$node attr -default "" type]

    button $myWidget
    $myWidget configure -pady 0
    if {$type=="file"} {
      $myWidget configure -text "Select File..."
    } else {
      $myWidget configure -text [$node attr -default ? value]
    }

    set isSubmit 0
    if {$type=="submit"} {set isSubmit 1}

    init $getcmd $node
    if {$myForm != "" && $isSubmit} {
      $myWidget configure -command [list $myForm submit]
    }
  }

  method configure {props} {
    array set p $props
    $myWidget configure -font $p(font)
    $myWidget configure -activeforeground $p(color)
    $myWidget configure -foreground $p(color)
    $myWidget configure -borderwidth 1
    return [font_to_offset $myWidget $p(font)]
  }

  method success {} {return 1}
  method value   {} {return [$myWidget cget -text]}
}

class HtmlRadioButton {
  inherit HtmlControl

  variable myVarName
  variable myValue

  common myVarCounter 0

  constructor {widget HTML getcmd node} {
    set myWidget ${HTML}.[string map {: _} $node]
    set myVarName "::HtmlRadioButton::var[incr myVarCounter]"

    $widget $myWidget 
    init $getcmd $node

    set    myVarName "::HtmlRadioButton::var[string map {: _} $myForm]"
    append myVarName [$node attr -default "" name]
    set myValue [$node attr -default "" value]

    if {$widget == "radiobutton"} {
      $myWidget configure -value $this -variable $myVarName
    } else {
      $myWidget configure -onvalue $this -offvalue "" -variable $myVarName
      $myWidget deselect
    }

    if {![info exists $myVarName] || ![catch {$node attr checked}]} {
      $myWidget select
    }
  }

  method configure {props} {
    array set p $props
    $myWidget configure -offrelief flat
    $myWidget configure -padx 0
    $myWidget configure -pady 0
    $myWidget configure -foreground $p(color)
    $myWidget configure -background $p(background-color)
    $myWidget configure -activeforeground $p(color)
    $myWidget configure -activebackground $p(background-color)
    return [font_to_offset $myWidget $p(font)]
  }

  # A radio or check button is successful if it is checked.
  method success {} {return [expr {[set $myVarName]==$this}]}
  method value   {} {return $myValue}
}

class HtmlComboBox {
  inherit HtmlControl

  variable myLabels [list]
  variable myValues [list]

  constructor {HTML getcmd node} {
    # Figure out a list of options for the drop-down list.
    set options [list]
    foreach child [$node children] {
      if {[$child tag] == "option"} {
        if {[catch {set label [$node attr label]}]} {
          set t [lindex [$child children] 0]
          set label [$t text]
        }
        if {[catch {set value [$node attr value]}]} {
          set t [lindex [$child children] 0]
          set value [$t text]
        }
        lappend myLabels $label
        lappend myValues $value

        if {![catch {$child attr selected}] || [llength $myLabels]==1} {
          set idx [expr [llength $myLabels] - 1]
        } 
      }
    }

    # Set up the combobox widget. 
    set myWidget ${HTML}.[string map {: _} $node]
    combobox::combobox $myWidget 
    $myWidget configure -listvar [itcl::scope myLabels]
    $myWidget configure -editable false
    $myWidget configure -height 0
    $myWidget configure -maxheight [$node attr -default 10 size]
    $myWidget select $idx

    # Initialize the base class stuff
    init $getcmd $node
  }

  method configure {props} {
    array set p $props
    $myWidget configure -font $p(font)
    $myWidget configure -width 0
    return [font_to_offset $myWidget $p(font)]
  }

  method success {} {return 1}
  method value   {} {return [lindex $myValues [$myWidget curselection]]}
}
#--------------------------------------------------------------------------

proc formHandleInput {HTML getcmd node} {
  switch [$node attr -default unused type] {
    image    {return}
    reset    {return}
    hidden   {return}

    file     { HtmlButton #auto $HTML $getcmd $node }
    button   { HtmlButton #auto $HTML $getcmd $node }
    submit   { HtmlButton #auto $HTML $getcmd $node }

    radio    { HtmlRadioButton #auto radiobutton $HTML $getcmd $node }
    checkbox { HtmlRadioButton #auto checkbutton $HTML $getcmd $node }

    default  { HtmlEntry #auto $HTML $getcmd $node }
  }
}

proc form_init {HTML getcmd} {
    $HTML handler node input  [list formHandleInput $HTML $getcmd]
    $HTML handler node select [list HtmlComboBox #auto $HTML $getcmd]
}


#
#     The following HTML elements create document nodes that are replaced with
#     form controls:
#
#         <!ELEMENT INPUT    - O EMPTY> 
#         <!ELEMENT BUTTON   - - (%flow;)* -(A|%formctrl;|FORM|FIELDSET)>
#         <!ELEMENT SELECT   - - (OPTGROUP|OPTION)+> 
#         <!ELEMENT TEXTAREA - - (#PCDATA)> 
#         <!ELEMENT ISINDEX  - O EMPTY> 
#
#     This module registers node handler scripts with client html widgets for
#     these five element types.
#
#         <input>            -> button|radiobutton|menubutton|entry|image
#         <button>           -> button|image
#         <select>           -> combobox
#         <textarea>         -> text
#
# type = text|password|checkbox|radio|submit|reset|file|hidden|image|button
#
# <button>
# type = submit|button|reset
#
# <select>
#
# <textarea>
#
# <isindex>

