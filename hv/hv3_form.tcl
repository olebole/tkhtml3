
###########################################################################
# hv3_form.tcl --
#
#     This file contains code to implement the cosmetic forms functionality.
#     The public interface to this file are the commands:
#
#         form_init HTML
#         form_widget_list
#
#     The following Tk widgets are used for form elements:
#
#     <input>            -> button|radiobutton|menubutton|entry|image
#     <button>           -> button|image
#     <select>           -> menubutton
#     <textarea>         -> text
#

#--------------------------------------------------------------------------
# Global variables section
 
# A list of all the widgets we have created but not destroyed. This is used to
# make sure the widget code invokes the -deletecmd callbacks correctly.
#
set ::hv3_form_widgets [list]
#--------------------------------------------------------------------------

proc form_init {HTML} {
    $HTML handler node input    [list formHandleInput $HTML]
    $HTML handler node select   [list formHandleSelect $HTML]
    $HTML handler node textarea [list formHandleTextarea $HTML]
    $HTML handler node button   [list formHandleButton $HTML]
}

# Procedure to access $::hv3_form_widgets from outside of this file.
#
proc form_widget_list {} {
    return $::hv3_form_widgets
}

proc nodeToWidgetName {HTML node} {
    return ${HTML}.[string map {: _} $node]
}

# formHandleSelect --
#
#     formHandleSelect HTML NODE
#
proc formHandleSelect {HTML node} {
    set winname [nodeToWidgetName $HTML $node]

    # Figure out a list of options for the menubutton.
    set options [list]
    for {set ii 0} {$ii < [$node nChild]} {incr ii} {
        set child [$node child $ii]
        if {[$child tag] == "option"} {
            set label ???
            catch {
                if {[catch {set label [$node attr label]}]} {
                    set t [$child child 0]
                    set label [$t text]
                }
            }
            lappend options $label
        }
    }

    set winmenu ${winname}.menu
    set win [menubutton $winname -menu $winmenu -text [lindex $options 0]]
    $win configure -indicatoron 1 -relief raised
    menu $winmenu -tearoff 0
    foreach o $options {
        $winmenu add command -label $o -command "$win configure -text \"$o\""
    }

    $node replace $win \
        -configurecmd [list form_config $win menubutton] \
        -deletecmd    [list formDeleteSelect $win]

    # Add the new widget to the global list.
    lappend ::hv3_form_widgets $win
    lappend ::hv3_form_widgets $winmenu
}

proc formDeleteSelect {win} {
    form_delete ${win}.menu
    form_delete $win
}

# formHandleTextarea --
#
#     formHandleTextarea HTML NODE
#
proc formHandleTextarea {HTML node} {
}

# formHandleButton --
#
#     formHandleButton HTML NODE
#
proc formHandleButton {HTML node} {
}

# formHandleInput --
#
#     formHandleInput HTML NODE
#
proc formHandleInput {HTML node} {
    set winname [nodeToWidgetName $HTML $node]

    set type [string tolower [$node attr -default text type]]
    switch -- $type {
        text     {set widget entry}
        password {set widget entry}

        submit   {set widget button}
        button   {set widget button}
        file     {set widget button}

        radio    {set widget radiobutton}
        checkbox {set widget checkbutton}

        image    {return}
        reset    {return}
        hidden   {return}
        default  {return}
    }

    set win [$widget $winname]

    switch -- $widget {
        checkbutton {
            set widget radiobutton
        }
        entry {
            set w [$node attr -default 20 size]
            $win configure -width $w
            if {$type == "password"} {
                $win configure -show *
            }
        }
        button {
            if {$type == "file"} {
                $win configure -text "Choose File..."
            } else {
                $win configure -text [$node attr -default ? value]
            }
        }
        radiobutton {
            $win configure -value $node -variable [$node attr name]
            set ::[$node attr name] $node
        }
    }

    $node replace $win \
        -configurecmd [list form_config $win $widget] \
        -deletecmd    [list form_delete $win]

    # Add the new widget to the global list.
    lappend ::hv3_form_widgets $win
}

proc form_config {w widget props} {
    array set p $props

    switch -- $widget {
        button {
            $w configure -font $p(font)
            $w configure -activeforeground $p(color)
            $w configure -foreground $p(color)
        }
        entry {
            $w configure -font $p(font)
            $w configure -foreground $p(color)
            $w configure -background $p(background-color)
        }
        radiobutton {
            $w configure -offrelief flat
            $w configure -padx 0
            $w configure -pady 0

            $w configure -foreground $p(color)
            $w configure -background $p(background-color)
            $w configure -activeforeground $p(color)
            $w configure -activebackground $p(background-color)
        }
    }
}

#
# form_delete --
#
#         form_delete WIN
#
#     This procedure is used as the -deletecmd callback when code in this file
#     replaces a document node with a Tk window. It destroys the window 
#     and updates the ::hv3_form_widgets list (used for internal accounting 
#     of allocated widgets, see above).
#
proc form_delete {w} {
    set idx [lsearch -exact $::hv3_form_widgets $w]
    if {$idx < 0} {
        lappend ::hv3_form_widgets "Bogus delete of $w"
    }
    set ::hv3_form_widgets [lreplace $::hv3_form_widgets $idx $idx]
    destroy $w
}

