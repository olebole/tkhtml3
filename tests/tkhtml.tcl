
bind Html <Expose> {
    tk::HtmlExpose %W %x %y %w %h
}

proc ::tk::HtmlExpose {win x y w h} {
    after idle "$win layout widget $x $y $x $y $w $h"
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

    $win tree build
    $win style apply
    $win layout force

    set width [$win cget -width]
    set height [$win cget -height]

    $win layout widget 0 0 0 0 $width $height
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
    ]
    foreach {cmd script} $cmds {
        $widget command $cmd $script
    }

    # Initialise some state variables:
    $widget var update_pending 0

    return $widget
}
