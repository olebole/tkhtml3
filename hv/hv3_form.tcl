namespace eval hv3 { set {version($Id: hv3_form.tcl,v 1.36 2006/09/07 14:53:39 danielk1977 Exp $)} 1 }

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
#     these five element types. The <isindex> element (from ancient times) is
#     handled specially, by transforming it to an equivalent HTML4 form.
#
#         <input>       -> button|radiobutton|checkbutton|combobox|entry|image
#         <button>      -> button|image
#         <select>      -> combobox
#         <textarea>    -> text
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

# ::hv3::fileselect
#
snit::widget ::hv3::fileselect {

  component myButton
  component myEntry

  option -font {Helvetica 14}
  delegate option -text to myButton

  delegate option -highlightthickness to hull

  constructor {args} {
    set myEntry [entry ${win}.entry -width 30]
    set myButton [button ${win}.button -command [mymethod Browse]]

    $myEntry configure -highlightthickness 0
    $myEntry configure -borderwidth 0
    $myEntry configure -bg white

    $myButton configure -highlightthickness 0
    $myButton configure -pady 0

    pack $myButton -side right
    pack $myEntry -fill both -expand true
    $self configurelist $args
  }

  method Browse {} {
    set file [tk_getOpenFile]
    if {$file ne ""} {
      $myEntry delete 0 end
      $myEntry insert 0 $file
    }
  }

  method success {} {
    set fname [${win}.entry get]
    if {$fname ne ""} {
      return 1
    }
    return 0
  }
  method value {} {
    set fname [${win}.entry get]
    if {$fname ne ""} {
      set fd [open $fname]
      fconfigure $fd -encoding binary -translation binary
      set data [read $fd]
      close $fd
      return $data
    }
    return ""
  }
  method filename {} {
    set fname [${win}.entry get]
    return [file tail $fname]
  }
}

#--------------------------------------------------------------------------
# ::hv3::control
#
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
#     combobox widgets is made. (Note that <isindex> is not mentioned
#     here because it is transformed to an <input> element by the 
#     [::hv3::isindex_handler] script handler proc below.
#
# INTERFACE
#
#     Each replaced element is replaced by an instance of the 
#     ::hv3::control widget. ::hv3::control supports the following
#     public interface:
#
#         -submitcmd
#             If not set to an empty string, the value of this option
#             is evaluated as a Tcl command when the control determines
#             that the form it belongs to should be submitted (i.e. when
#             a submit button is clicked on, <enter> is pressed in
#             a text field etc.)
#
#         [name] 
#             Return the value of the Html "name" attribute associated
#             with the control.
#
#         [success] 
#             Return true if the control is currently "successful".
#
#         [value] 
#             Return current the value of the control.
#
#         [configurecmd] 
#             Called by Tkhtml as the -configurecmd callback.
#
#         [dump]
#             Debugging only. Return a string rep. of the object.
#
#
snit::widget ::hv3::control {

  # The document node corresponding to the element that created this 
  # control (i.e. the <input>).
  variable  myControlNode

  # The widget for this control. One of the following types:
  #
  #     button
  #     entry
  #     ::combobox::combobox
  #     text
  #     checkbox
  #     radiobutton
  #     ::hv3::fileselect
  #
  variable  myWidget ""
  variable  myWidgetIsSmart 0

  variable  mySuccess 1            ;# Value returned by [success]
  variable  myValue   ""           ;# Value returned by [value]

  # If this is a radiobutton widget, the name of the -variable var.
  variable  myRadioVarname ""      ;# Used by radiobuttons only

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
          file     { $self CreateFileselectWidget }
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
    set contents ""
    foreach child [$myControlNode children] {
      append contents [$child text -pre]
    }
    $myWidget insert 0.0 $contents

    $myWidget configure -borderwidth 0
    $myWidget configure -pady 0
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

    # Borders are specified by CSS and drawn by the html widget. So
    # disable the entry widget's built-in border.
    $myWidget configure -borderwidth 0
    $myWidget configure -selectborderwidth 0
    $myWidget configure -highlightthickness 0

    # If this is a password entry field, obscure it's contents
    if {$isPassword} { $myWidget configure -show * }

    # Set the default width of the widget to 20 characters. Unless there
    # is no size attribute and the CSS 'width' property is set to "auto",
    # this will be overidden.
    $myWidget configure -width 20

    # The "value" attribute, if any, is used as the initial contents
    # of the entry widget.
    set myValue [$myControlNode attr -default "" value]

    # Pressing enter in an entry widget submits the form.
    bind $myWidget <KeyPress-Return> [mymethod Submit]
  }

  method CreateFileselectWidget {} {
    set myWidget [::hv3::fileselect ${win}.widget]
    set myWidgetIsSmart 1
    $myWidget configure -text Browse...
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
    catch { $myWidget configure -tristatevalue EWLhwEUGHWZAZWWZE }
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

    # If the $myWidget object has a [value] method, use it.
    if {$myWidgetIsSmart} { return [$myWidget value] }

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

    # If the $myWidget object has a [success] method, use it.
    if {$myWidgetIsSmart} { return [$myWidget success] }

    if {$myRadioVarname ne ""} {
      set res [expr \
        {[set $myRadioVarname] eq [$myControlNode attr -default "" value]}
      ]
      return $res
    }

    return $mySuccess
  }

  method filename {} {
    if {$myWidgetIsSmart} { return [$myWidget filename] }
    return ""
  }

  # This method is invoked by Tkhtml as the -configurecmd callback for this
  # control. The argument is a serialized array of property-value pairs, as
  # described in the Tkhtml man page along with the [node replace] command.
  #
  method configurecmd {values} {
    if {$myWidget eq ""} return

    set class [winfo class $myWidget]

    # If the widget has a -highlightthickness option, set it to 0.
    if {$class ne "Combobox"} {
      catch { $myWidget configure -highlightthickness 0 }
    }

    array set v $values
    if {$class eq "Checkbutton" || $class eq "Radiobutton"} {
      catch { $myWidget configure -bg $v(background-color) }
      catch { $myWidget configure -highlightbackground $v(background-color) }
      catch { $myWidget configure -activebackground $v(background-color) }
      catch { $myWidget configure -highlightcolor $v(background-color) }
    }

    catch { $myWidget configure -font $v(font) }

    set font [$myWidget cget -font]
    if {$class eq "Text" || $class eq "Entry"} {
        set drop [font metrics $font -descent]
    } else {
        set descent [font metrics $font -descent]
        set ascent  [font metrics $font -ascent]
        set drop [expr ([winfo reqheight $myWidget] + $descent - $ascent) / 2]
    }
    return $drop
  }
}

::snit::type ::hv3::clickcontrol {
  variable myNode ""
  variable mySuccess 0

  option -submitcmd -default ""

  constructor {node} {
    set myNode $node
  }
 
  method value {}   { return [$myNode attr -default "" value] }
  method name {}    { return [$myNode attr -default "" name] }
  method success {} { return $mySuccess }

  method submit {} {
    # The control that submits the form is successful
    set mySuccess 1
    set cmd $options(-submitcmd)
    if {$cmd ne ""} {
      eval $cmd
    }
  }

  method configurecmd {values} {}

  method dump {values} {
    return "TODO"
  }
}

#-----------------------------------------------------------------------
# ::hv3::format_query
#
#     This command is intended as a replacement for [::http::formatQuery].
#     It does the same thing, except it allows the following characters
#     to slip through unescaped:
#
#         - _ . ! ~ * ' ( )
#
#     as well as the alphanumeric characters (::http::formatQuery only
#     allows the alphanumeric characters through).
#
#     QUOTE FROM RFC2396:
#
#     2.3. Unreserved Characters
#     
#        Data characters that are allowed in a URI but do not have a reserved
#        purpose are called unreserved.  These include upper and lower case
#        letters, decimal digits, and a limited set of punctuation marks and
#        symbols.
#     
#           unreserved  = alphanum | mark
#     
#           mark        = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
#     
#        Unreserved characters can be escaped without changing the semantics
#        of the URI, but this should not be done unless the URI is being used
#        in a context that does not allow the unescaped character to appear.
#
#     END QUOTE
#
#     So in a way both versions are correct. But some websites (yahoo.com)
#     do not work unless we allow the extra characters through unescaped.
#
proc ::hv3::format_query {args} {
  set result ""
  set sep ""
  foreach i $args {
    append result $sep [::hv3::escape_string $i]
    if {$sep eq "="} {
      set sep &
    } else {
      set sep =
    }
  }
  return $result
}
set ::hv3::escape_map ""
proc ::hv3::escape_string {string} {
  if {$::hv3::escape_map eq ""} {
    for {set i 0} {$i < 256} {incr i} {
      set c [format %c $i]
      if {$c ne "-" && ![string match {[a-zA-Z0-9_.!~*'()]} $c]} {
        set map($c) %[format %.2x $i]
      }
    }
    set ::hv3::escape_map [array get map]
  }

  set converted [string map $::hv3::escape_map $string]
  return $converted
}
#-----------------------------------------------------------------------

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
    # puts "FORM SUBMIT:"
    set data [list]
    foreach control $myControls {
      set success [$control success]
      set name    [$control name]
      if {$success} {
        set value [$control value]
        # puts "    Control \"$name\" is successful: \"$value\""
        lappend data $name $value
      } else {
        # puts "    Control \"$name\" is unsuccessful"
      }
    }

    # Now encode the data, depending on the enctype attribute of the
    set enctype [$myFormNode attr -default "" enctype]
    if {[string match -nocase *multipart* $enctype]} {
      # Generate a pseudo-random boundary string. The key here is that
      # if this exact string actually appears in any form control values,
      # the form submission will fail. So generate something nice and
      # long to minimize the odds of this happening.
      set bound "-----Submitted_by_Hv3_[clock seconds].[pid].[expr rand()]"

      set querytype "multipart/form-data ; boundary=$bound"
      set querydata ""
      set CR "\r\n"
      foreach control $myControls {
        if {[$control success]} {

          set name  [$control name]
          set value [$control value]
          set filename [$control filename]

          append querydata "--${bound}$CR"
          append querydata "Content-Disposition: form-data; name=\"${name}\""
          if { $filename ne "" } {
            append querydata "; filename=\"$filename\""
          }
          append querydata "$CR$CR"
          append querydata "${value}$CR"
        }
      }
      append querydata "--${bound}--$CR"
    } else {
      set querytype "application/x-www-form-urlencoded"
      set querydata [eval [linsert $data 0 ::hv3::format_query]]
    }

    set action [$myFormNode attr -default "" action]
    set method [string toupper [$myFormNode attr -default get method]]
    switch -- $method {
      GET     { set script $options(-getcmd) }
      POST    { set script $options(-postcmd) }
      ISINDEX { 
        set script $options(-getcmd) 
        set querydata [::hv3::format_query [[lindex $myControls 0] value]]
      }
      default { set script "" }
    }

    if {$script ne ""} {
      set exec [concat $script [list $action $querytype $querydata]]
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

# ::hv3::formmanager
#
#     Each hv3 mega-widget has a single instance of the following type
#     It contains the logic and state required to manager any HTML forms
#     contained in the local document. This type implements the "manager"
#     events interface:
#
#         $formmanager press X Y
#         $formmanager release X Y
#         $formmanager motion X Y
#    
snit::type ::hv3::formmanager {

  option -getcmd  -default ""
  option -postcmd -default ""

  # Each time the parser sees a <form> tag, the created node-handle
  # is added to the end of the following list variable. Each time
  # a </form> is encountered, the end element (if any) is removed.
  variable myFormStack ""

  # Map from node-handle to ::hv3::clickcontrol object for all clickable
  # form controls currently managed by this form-manager.
  variable myClickControls -array [list]
  variable myClicked ""

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

    $myHtml handler parse form [mymethod FormHandler start]
    $myHtml handler parse /form [mymethod FormHandler end]

    bind $myHv3 <ButtonPress-1>   "+[mymethod press %x %y]"
    bind $myHv3 <ButtonRelease-1> "+[mymethod release %x %y]"
  }

  # FormHandler
  #
  #     A Tkhtml parse-handler for <form> and </form> tags.
  method FormHandler {i node offset} {
    switch -- $i {
      start {
        lappend myFormStack $node
      }
      end {
        set myFormStack [lrange $myFormStack 0 end-1]
      }
    }
  }

  method control_handler {node} {

    set name [string map {: _} $node]
  
    set tag  [$node tag]
    set type [$node attr -default "" type]
    if {$tag eq "input" && $type eq "image"} {
      $node override [list -tkhtml-replacement-image [$node attr src]]
      set control [::hv3::clickcontrol %AUTO% $node]
      set myClickControls($node) $control
    } elseif {$tag eq "input" && $type eq "submit"} {
      set control [::hv3::clickcontrol %AUTO% $node]
      set myClickControls($node) $control
    } else {
      set control [::hv3::control ${myHtml}.control_${name} $node]
    }

    set N [lindex $myFormStack end]
    if {$N ne ""} {
      if {![info exists myForms($N)]} {
        set myForms($N) [::hv3::form %AUTO% $N]
        $myForms($N) configure -getcmd $options(-getcmd)
        $myForms($N) configure -postcmd $options(-postcmd)
      }
      $myForms($N) add_control $control
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
    array unset myClickControls
    set myFormStack [list]
  }

  method dumpforms {} {
    foreach form [array names myForms] {
      puts [$myForms($form) dump]
    }
  }

  # The "manager" events interface - [press], [release] and [motion].
  #
  method press {x y} {
    set N [lindex [$myHv3 node $x $y] end]
    while {$N ne ""} {
      if {[info exists myClickControls($N)]} {
        set myClicked $N
        return
      }
      set N [$N parent]
    }
  }
  method release {x y} {
    set N [lindex [$myHv3 node $x $y] end]
    while {$N ne ""} {
      if {$N eq $myClicked} {
        $myClickControls($N) submit
        break
      }
      set N [$N parent]
    }
    set myClicked ""
  }
  method motion {X Y} {
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

