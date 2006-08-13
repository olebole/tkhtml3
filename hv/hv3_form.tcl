namespace eval hv3 { set {version($Id: hv3_form.tcl,v 1.26 2006/08/13 11:41:39 danielk1977 Exp $)} 1 }

###########################################################################
# hv3_form.tcl --
#
#     This file contains code to implement Html forms for Tkhtml based
#     browsers. The only requirement is that no other code register for
#     node-handler callbacks for <input>, <button> <select> or <textarea> 
#     elements. The application must provide this module with callback
#     scripts to execute for GET and POST form submissions.
#
#     The public interface to this file is the command:
#
#         ::hv3::forms_module_init HTML GETCMD POSTCMD
#

source [file join [file dirname [info script]] combobox.tcl]

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
#         <input>            -> button|radiobutton|combobox|entry|image
#         <button>           -> button|image
#         <select>           -> combobox
#         <textarea>         -> text
#
# <input>
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
#

#--------------------------------------------------------------------------
# WIDGETS
#
#     The following Tk widgets are used for form elements:
#
#         <input>            -> button|radiobutton|combobox|entry|image
#         <button>           -> button|image
#         <select>           -> combobox
#         <textarea>         -> text
#
#     An attempt to baseline align the button, entry, radiobutton and 
#     combobox widgets is made.
#
# METHOD
#
#     Each replaced element is replaced by a snit widget wrapper around
#     the displayed Tk widget. Each widget wrapper supports the following
#     methods:
#
#         name         - Return the value of the Html "name" attribute.
#         success      - Return true if the control is successful.
#         value        - Return the value of the control.
#         configure    - Called by Tkhtml as the -configurecmd callback.
#

snit::widget ::hv3::control {

  variable  myControlNode
  variable  myWidget ""

  variable  mySuccess 1            ;# Value returned by [success]
  variable  myValue   ""           ;# Value returned by [value]

  variable  myRadioVarname ""      ;# Used by radiobuttons only

  # True if the calculated value of the 'width' property should be used.
  variable  myUsePixelWidth 1

  option -submitcmd -default ""

  constructor {node args} {
    set myControlNode $node

    set tag [$myControlNode tag]
    switch -- $tag {

      input {
        # An <INPUT> element can create a variety of different control types,
        # depending on the value of the "tag" attribute. The default value
        # of "tag" is "text".
        switch [string tolower [$node attr -default text type]] {
          text     { $self CreateEntryWidget 0 }
          password { $self CreateEntryWidget 1 }
          checkbox { $self CreateCheckboxWidget }
          radio    { $self CreateRadioWidget }
          submit   { $self CreateButtonWidget submit }
          image    { #TODO }
          reset    { #TODO }
          button   { #TODO }
          hidden   { set myValue [$myControlNode attr -default "" value] }
          file     { #TODO }
          default  { #TODO }
        }
      }

      textarea {
        # A <TEXTAREA> element is replaced by a Tk text widget.
        $self CreateTextWidget
      }

      select {
        # A <SELECT> element is replaced by a Tk combobox widget.
        $self CreateComboboxWidget
      }

      button {
	# A <BUTTON> element is not replaced by any widget. All formatting is
	# done via CSS. See the text that class ::hv3::formmanager adds to the
	# Tkhtml default stylesheet for details.
        #
	# Even though there is no associated Tk widget, an ::hv3::control
	# object is still created for the <BUTTON> element, to manage details
	# associated with form submission. When the <BUTTON> is invoked, the
	# ::hv3::formmanager invokes the [invokebutton] method of this object.
        
        # TODO
      }
    }

    if {$myWidget ne ""} {
      pack $myWidget -expand 1 -fill both
    }

    $hull configure -borderwidth 0 -pady 0 -padx 0

    $self configurelist $args
  }

  destructor { 
    if {$myRadioVarname ne ""} {
      unset -nocomplain $myRadioVarname
    }
  }

  method CreateTextWidget {} {
    set myWidget [text ${win}.widget]
    set cols [$myControlNode attr -default 60 cols]
    set rows [$myControlNode attr -default 10 rows]

    $myWidget configure -height $rows 
    $myWidget configure -width $cols
    set contents ""
    foreach child [$myControlNode children] {
      append contents [$child text -pre]
    }
    $myWidget insert 0.0 $contents

    $myWidget configure -borderwidth 0
    $myWidget configure -selectborderwidth 0
    $myWidget configure -highlightthickness 0
    $myWidget configure -background white
  }

  # Create a standard Tk entry widget for this control. The argument is
  # true if this is a password entry field, in which case the contents are
  # visually obscured.
  #
  method CreateEntryWidget {isPassword} {
    set myWidget [entry ${win}.widget]
    $myWidget configure -textvar [myvar myValue]
    $myWidget configure -background white

    $myWidget configure -borderwidth 0
    $myWidget configure -selectborderwidth 0
    $myWidget configure -highlightthickness 0

    # If this is a password entry field, obscure it's contents
    if {$isPassword} { $myWidget configure -show * }

    # Html Attributes:
    #
    #     Attribute     Default      Usage
    #     --------------------------------
    #     size          20           width of field in characters
    #     value         ""           initial contents of field
    #
    $myWidget configure -width [$myControlNode attr -default 20 size]
    set myValue                [$myControlNode attr -default "" value]

    # Pressing enter in an entry widget submits the form.
    bind $myWidget <KeyPress-Return> [mymethod Submit]
  }

  # Create a standard Tk button widget for this control. The argument may
  # be any of:
  #
  #     submit
  #     file
  #     ...
  #
  method CreateButtonWidget {variant} {
    set myWidget [button ${win}.widget]

    $myWidget configure -pady 0 

    # Determine the text to use for the button label. If this is a
    # file-select button, then the text is always "Select File...".
    # Otherwise, it is the value of the Html "value" attribute. If no such
    # attribute is defined, enigmaticly use "?" as the label.
    switch -- $variant {
      file    { set labeltext "Select File..."                       }
      default { set labeltext [$myControlNode attr -default ? value] }
    }
    $myWidget configure -text $labeltext

    # Configure an action for when the button is pushed.
    switch -- $variant {
      submit  { 
        set mySuccess 0
        set cmd [mymethod Submit] 
      }
      default { set cmd "" }
    }
    $myWidget configure -command $cmd

    set myValue [$myControlNode attr -default "" value]
  }

  method CreateCheckboxWidget {} {
    set myWidget [checkbutton ${win}.widget]
    $myWidget configure -variable [myvar mySuccess]
    set mySuccess [expr [catch {$myControlNode attr checked}] ? 0 : 1]
    set myValue [$myControlNode attr -default "" value]
  }
 
  method CreateComboboxWidget {} {
    # Figure out a list of options for the drop-down list. This block 
    # sets up two local list variables, $labels and $values. The $labels
    # list stores the options from which the user may select, and the $values
    # list stores the corresponding form control values.
    set idx 0
    set maxlen 5
    set labels [list]
    set values [list]
    foreach child [$myControlNode children] {
      if {[$child tag] == "option"} {

        # If the Html "selected" attribute is defined, this option is 
        # initially selected. If "selected" is not defined for any
        # option, the first is initially selected.
        if {![catch {$child attr selected}]} {
          set idx [expr [llength $labels]]
        } 

        # If the element has text content, this is used as the default
	# for both the label and value of the entry (used if the Html
	# attributes "value" and/or "label" are not defined.
	set contents ""
        catch {
          set t [lindex [$child children] 0]
          set contents [$t text]
        }

        # Append entries to both $values and $labels
        set     label  [$child attr -default $contents label]
        set     value  [$child attr -default $contents value]
        lappend labels $label
        lappend values $value

        set len [string length $label]
        if {$len > $maxlen} {set maxlen $len}
      }
    }

    # Set up the combobox widget. 
    set myWidget [combobox::combobox ${win}.widget]
    eval [concat [list $myWidget list insert 0] $labels]
    $myWidget configure -command   [mymethod ComboboxChanged $values]
    $myWidget select $idx
    set myValue [lindex $values $idx]

    # Set the width and height of the combobox. Prevent manual entry.
    if {[set height [llength $values]] > 10} { set height 10 }
    $myWidget configure -width  $maxlen
    $myWidget configure -height $height
    $myWidget configure -editable false
  }

  method ComboboxChanged {values args} {
    set myValue [lindex $values [$myWidget curselection]]
  }

  method CreateRadioWidget {} {
    set myWidget [radiobutton ${win}.widget]
    set myRadioVarname ::hv3::radiobutton_[$self name]
    set myValue [$myControlNode attr -default "" value]

    if { 
      ([catch {$myControlNode attr checked}] ? 0 : 1) ||
      ![info exists $myRadioVarname]
    } {
      set $myRadioVarname $myValue
    }

    $myWidget configure -value $myValue
    $myWidget configure -variable $myRadioVarname
    $myWidget configure -padx 0 -pady 0
    $myWidget configure -borderwidth 0
    $myWidget configure -highlightthickness 0
  }

  # Submit the form this control belongs to.
  method Submit {} {
    # The control that submits the form is always successful
    set mySuccess 1

    set cmd $options(-submitcmd)
    if {$cmd ne ""} {
      eval $cmd
    }
  }

  method dump {} {
    set type [$myControlNode attr -default "" type]
    if {$type ne ""} {
      set type " type=$type"
    }
    return "<[$myControlNode tag] name=[$self name]$type>"
  }

  # This method is called during form submission to determine the name of the
  # control. It returns the value of the Html "name" attribute, or failing that
  # an empty string.
  #
  method name {} {
    return [$myControlNode attr -default "" name]
  }

  # Return the current value of the control.
  #
  method value {} {
    if {[$myControlNode tag] eq "textarea"} {
      return [$myWidget get 0.0 end]
    }
    return $myValue
  }

  # Return true if the control is successful, or false otherwise.
  #
  method success {} {
    if {[$self name] eq ""} {
      # A control without a name is never successful.
      return 0
    }

    if {$myRadioVarname ne ""} {
      set res [expr \
        {[set $myRadioVarname] eq [$myControlNode attr -default "" value]}
      ]
      return $res
    }

    return $mySuccess
  }

  # This method is invoked by Tkhtml as the -configurecmd callback for this
  # control. The argument is a serialized array of property-value pairs, as
  # described in the Tkhtml man page along with the [node replace] command.
  #
  method configurecmd {values} {
    if {$myWidget eq ""} return

    set class [winfo class $myWidget]

    array set v $values
    if {$class eq "Checkbutton" || $class eq "Radiobutton"} {
      catch { $myWidget configure -bg $v(background-color) }
      catch { $myWidget configure -highlightbackground $v(background-color) }
      catch { $myWidget configure -activebackground $v(background-color) }
      catch { $myWidget configure -highlightcolor $v(background-color) }
    }

    # catch { $myWidget configure -font $v(font) }

    set font [$myWidget cget -font]
    set descent [font metrics $font -descent]
    set ascent  [font metrics $font -ascent]
    set drop [expr ([winfo reqheight $myWidget] + $descent - $ascent) / 2]
    return $drop
  }
}

snit::type ::hv3::form {
  variable myFormNode 
  variable myControls [list] 

  option -getcmd  -default ""
  option -postcmd -default ""

  constructor {node} {
    set myFormNode $node
  }

  destructor {
    foreach control $myControls {
      # destroy $control
    }
  }

  method add_control {control} {
    $control configure -submitcmd [mymethod submit]
    lappend myControls $control
  }

  method submit {} {
    puts "FORM SUBMIT:"
    set data [list]
    foreach control $myControls {
      set success [$control success]
      set name    [$control name]
      if {$success} {
        set value [$control value]
        puts "    Control \"$name\" is successful: \"$value\""
        lappend data $name $value
      } else {
        puts "    Control \"$name\" is unsuccessful"
      }
    }

    set encdata [eval [linsert $data 0 ::http::formatQuery]]
    puts "Submitting: $encdata"

    set action [$myFormNode attr -default "" action]
    set method [string toupper [$myFormNode attr -default get method]]
    switch -- $method {
      GET     { set script $options(-getcmd) }
      POST    { set script $options(-postcmd) }
      ISINDEX { 
        set script $options(-getcmd) 
        set encdata [::http::formatQuery [[lindex $myControls 0] value]]
      }
      default { set script "" }
    }

    if {$script ne ""} {
      set exec [concat $script [list $action $encdata]]
      puts $exec
      eval $exec
    }
  }

  method dump {} {
    set action [$myFormNode attr -default "" action]
    set method [$myFormNode attr -default "" method]
    set ret "[string toupper $method] $action\n"
    foreach control $myControls {
      append ret "        [$control dump]\n"
    }
    return $ret
  }
}

snit::type ::hv3::formmanager {

  option -getcmd  -default ""
  option -postcmd -default ""

  variable myHv3
  variable myHtml
  variable myForms -array [list]

  constructor {hv3 args} {
    $self configurelist $args
    set myHv3  $hv3
    set myHtml [$myHv3 html]
    $myHtml handler node input     [mymethod control_handler]
    $myHtml handler node textarea  [mymethod control_handler]
    $myHtml handler node select    [mymethod control_handler]

    $myHtml handler script isindex [list ::hv3::isindex_handler $hv3]
  }

  method form_handler {node} {
    set myForms($node) [::hv3::form %AUTO% $node]
    $myForms($node) configure -getcmd $options(-getcmd)
    $myForms($node) configure -postcmd $options(-postcmd)
  }

  method control_handler {node} {
    set name [string map {: _} $node]
    set control [::hv3::control ${myHtml}.control_${name} $node]

    for {set n $node} {$n ne ""} {set n [$n parent]} {
      if {[$n tag] eq "form"} {
        if {![info exists myForms($n)]} {
          $self form_handler $n
        }
        $myForms($n) add_control $control
        break
      }
    }

    $node replace $control                         \
        -configurecmd [list $control configurecmd] \
        -deletecmd    [list destroy $control]
  }

  destructor {
    $self reset
  }

  method reset {} {
    foreach form [array names myForms] {
      $myForms($form) destroy
    }
    array unset myForms
  }

  method dumpforms {} {
    foreach form [array names myForms] {
      puts [$myForms($form) dump]
    }
  }
}

# ::hv3::isindex_handler
#
#     This proc is registered as a Tkhtml script-handler for <isindex> 
#     elements. An <isindex> element is essentially an entire form
#     in and of itself.
#
#     Example from HTML 4.01:
#         The following ISINDEX declaration: 
#
#              <ISINDEX prompt="Enter your search phrase: "> 
#
#         could be rewritten with INPUT as follows: 
#
#              <FORM action="..." method="post">
#                  <P> Enter your search phrase:<INPUT type="text"> </P>
#              </FORM>
#
proc ::hv3::isindex_handler {hv3 attr script} {
  set a(prompt) ""
  array set a $attr


  set loc [::hv3::uri %AUTO% [$hv3 location]]
  set LOCATION "[$loc cget -scheme]://[$loc cget -authority]/[$loc cget -path]"
  set PROMPT   $a(prompt)
  $loc destroy

  return [subst {
    <hr>
    <form action="$LOCATION" method="ISINDEX">
      <p>
        $PROMPT
        <input type="text">
      </p>
    </form>
    <hr>
  }]
}

