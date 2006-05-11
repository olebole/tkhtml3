
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

package require Itcl
namespace import itcl::class
source [file join [file dirname [info script]] combobox.tcl]


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
    # $myWidget configure -background $p(background-color)
    $myWidget configure -activeforeground $p(color)
    # $myWidget configure -activebackground $p(background-color)
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

      isindex {
        # TODO
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

  variable myHtml
  variable myForms -array [list]

  constructor {htmlwidget args} {
    $self configurelist $args
    set myHtml $htmlwidget
    $myHtml handler node input    [mymethod control_handler]
    $myHtml handler node textarea [mymethod control_handler]
    $myHtml handler node select   [mymethod control_handler]
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

