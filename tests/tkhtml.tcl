
bind Html <Expose> {
    tk::HtmlExpose %W %x %y %w %h
}

bind Html <Configure> {
    tk::HtmlConfigure %W
}

bind Html <KeyPress-Down>  { %W yview scroll 1 units }
bind Html <KeyPress-Up>    { %W yview scroll -1 units }
bind Html <KeyPress-Right> { %W xview scroll 1 units }
bind Html <KeyPress-Left>  { %W xview scroll -1 units }
bind Html <KeyPress-Next>  { %W yview scroll 1 pages }
bind Html <KeyPress-Prior> { %W yview scroll -1 pages }

# Important variables:
#
#     The following variables are stored in the widget dictionary using the
#     [<widget> var] command:
#     
#         "x" - The number of pixels scrolled in the x-direction.
#         "y" - The number of pixels scrolled in the y-direction.
#

# <Configure> event
#
#     According to the docs a <Configure> event is "sent to a window
#     whenever its size, position, or border width changes, and sometimes
#     when it has changed position in the stacking order". So presumably we
#     need to redo the whole layout here.
#
proc ::tk::HtmlConfigure {win} {
    $win update
}

# <Expose> event
#
proc ::tk::HtmlExpose {win x y w h} {
    $win damage $x $y $w $h
}
        
# If either the -yscrollcommand or -xscrollcommand option is defined, then
# make a scrollbar update callback now.
#
proc ::tk::HtmlScrollbarCb {win} {
    set yscrollcommand [$win cget -yscrollcommand]
    set xscrollcommand [$win cget -xscrollcommand]
    if {$yscrollcommand != ""} {
        eval [concat $yscrollcommand [$win yview]]
    }
    if {$xscrollcommand != ""} {
        eval [concat $xscrollcommand [$win xview]]
    }
}

# <widget> xview
# <widget> xview moveto FRACTION
# <widget> xview scroll NUMBER WHAT
#
# <widget> yview
# <widget> yview moveto FRACTION
# <widget> yview scroll NUMBER WHAT
#
#     This is an implementation of the standard Tk widget xview and yview
#     commands. The second argument, $axis, is always "x" or "y",
#     indicating if this is an xview or yview command. The variable length
#     $args parameter that follows contains 0 or more options:
#
proc ::tk::HtmlView {win axis args} {

    # Calculate some variables according to whether this is an xview or
    # yview command:
    #
    # $layout_len - Size of the virtual canvas the widget is a viewport into.
    # $offscreen_len - Number of pixels above or to the left of the viewport.
    # $screen_len - Number of pixels in the viewport.
    #
    if {$axis == "x"} {
        set layout_len [lindex [$win layout size] 0]
        set offscreen_len [$win var x]
        set screen_len [winfo width $win]
    } else {
        set layout_len [lindex [$win layout size] 1]
        set offscreen_len [$win var y]
        set screen_len [winfo height $win]
    }

    # If this is a query, not a "scroll" or "moveto" command, then just
    # return the required values. The first value is the fraction of the
    # virtual canvas that is to the left or above the viewport. The second
    # is the fraction of the virtual canvas that lies above the bottom (or
    # to the left of the right hand side) of the viewport.
    #
    if {[llength $args] == 0} {
        set ret [list \
                [expr double($offscreen_len) / double($layout_len)] \
                [expr double($screen_len+$offscreen_len)/double($layout_len)] \
        ]
        return $ret
    }

    # This must be a "moveto" or "scroll" command (or an error). This block
    # sets local variable $newval to the new value for widget variables "x"
    # or "y", after the scroll or move is performed.
    #
    set cmd [lindex $args 0]
    if {$cmd == "moveto"} {
        if {[llength $args] != 2} {
            set e "wrong # args: should be \"$win $axis"
            append e "view moveto fraction"
            error $e
        }
        set newval [expr int(double($layout_len) * [lindex $args 1])]

    } elseif {$cmd == "scroll"} {
        if {[llength $args] != 3} {
            set e "wrong # args: should be \"$win $axis"
            append e "view scroll number what"
            error $e
        }

        set number [lindex $args 1]
        set what [lindex $args 2]

        if {$what == "pages"} {
            set incr [expr (10 * $screen_len) / 9]
        } else {
            set scrollIncr "-$axis"
            append scrollIncr scrollincrement
            set incr [$win cget $scrollIncr]
        }

        set incr [expr int(double($incr) * double($number))]
        set newval [expr $offscreen_len + $incr]
    } else {
        error "unknown option \"$cmd\": must be moveto or scroll"
    }

    # When we get here, $newval is set to the new value of the "offscreen
    # to the left/top" portion of the layout, in pixels. Fix this value so
    # that we don't scroll past the start or end of the virtual canvas.
    #
    if {$newval > ($layout_len - $screen_len)} {
       set newval [expr $layout_len - $screen_len]
    }
    if {$newval < 0} {
        set newval 0
    }
    $win var $axis $newval

    set diff [expr $newval - $offscreen_len]
    set adiff [expr int(abs($diff))]
    set w [winfo width $win]
    set h [winfo height $win]

    if {$adiff > 0} {
        if {$adiff < $screen_len} {
            if {$axis == "x"} {
                $win layout scroll $diff 0
            } else {
                $win layout scroll 0 $diff
            }

            if {$diff < 0} {
                if {$axis == "x"} {
                    $win damage 0 0 $adiff $h
                } else {
                    $win damage 0 0 $w $adiff
                }
            } else {
                if {$axis == "x"} {
                    $win damage [expr $w - $adiff] 0 $adiff $h
                } else {
                    $win damage 0 [expr $h - $adiff] $w $adiff
                }
            }

        } else {
            $win damage 0 0 $w $h
        }

        $win scrollbar_cb
    }
}

# <widget> default_style <style>
#
#     Load the default stylesheet into the widget. Without the default
#     stylesheet, most constructs will have no effect on the rendering.
#
#     The default stylesheet is stored in the same directory as this file
#     with the filename "$style.css". The first time it is loaded, it is
#     cached in the widget variable "default_style_$style".
#
proc ::tk::HtmlDefaultStyle {win stylename} {
    set varname "default_style_$stylename"
    set notloaded [catch {$win var $varname} style]
    if {$notloaded} {
        set filename [file join [file dirname [info script]] $stylename.css]  
        set nosuchfile [catch {open $filename} fd]
        if {$nosuchfile} {
            $win var $varname ""
        } else {
            set style [read $fd]
            $win var $varname $style
        }
    }
    $win style parse agent.0 $style
}

proc ::tk::HtmlDoUpdate {win} {
    $win var update_pending 0

    set width [winfo width $win]
    set height [winfo height $win]

    $win tree build
    $win style apply

    $win layout force -width $width
    $win layout widget 0 0 0 0 $width $height

    $win scrollbar_cb
}

# <widget> update
#
#     Schedule an update of the widget display for the next idle time.
#
proc ::tk::HtmlUpdate {win} {
    if {![$win var update_pending]} {
         $win var update_pending 1
         after idle "::tk::HtmlDoUpdate $win"
    }
}

# <widget> damage x y width height
#
proc ::tk::HtmlDamage {win x y w h} {
    if {![$win var update_pending]} {
        set xc [expr [$win var x] + $x]
        set yc [expr [$win var y] + $y]
        after idle "$win layout widget $xc $yc $x $y $w $h"
    }
}

# <widget> parse HTML-TEXT
#
#     Append the html-text to the document the widget currently contains.
#     Also schedule a redraw for the next idle-time.
#
proc ::tk::HtmlParse {win htmltext} {
    $win read $htmltext
    $win update
}

# html PATH ?OPTIONS?
#
#     This procedure is a wrapper around the widget creation proc "html"
#     implemented in C. The widget is created as normal by calling the C
#     function. Then the widget commands implemented in Tcl in this file
#     are added to the interface.
#
rename html htmlinternal
proc html {args} {
    # Create the C-level widget.
    set widget [eval [concat htmlinternal $args]]

    # Add the commands defined in Tcl to the widget ensemble.
    set cmds [list \
        default_style [list ::tk::HtmlDefaultStyle $widget] \
        parse         [list ::tk::HtmlParse        $widget] \
        update        [list ::tk::HtmlUpdate       $widget] \
        damage        [list ::tk::HtmlDamage       $widget] \
        xview         [list ::tk::HtmlView         $widget x] \
        yview         [list ::tk::HtmlView         $widget y] \
        scrollbar_cb  [list ::tk::HtmlScrollbarCb  $widget] \
    ]
    foreach {cmd script} $cmds {
        $widget command $cmd $script
    }

    # Initialise some state variables:
    $widget var update_pending 0
    $widget var x 0
    $widget var y 0

    # Set up a NULL callback to handle the <script> tag. The default
    # behaviour is just to throw away the contents of the <script> markup -
    # more complex behaviour could be added by overiding this handler.
    $widget handler script script #

    return $widget
}
