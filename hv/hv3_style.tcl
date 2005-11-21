
###########################################################################
# hv3_style.tcl --
#
#     This file contains code to implement stylesheet functionality.
#     The public interface to this file are the commands:
#
#         style_init HTML
#         style_newdocument HTML
#

package require uri

#--------------------------------------------------------------------------
# Global variables section
set ::hv3_style_count 0

#--------------------------------------------------------------------------

# style_init --
#
#         style_init HTML
#
#     This is called just after the html widget ($HTML) is created. The two
#     handler commands are registered.
#
proc style_init {HTML} {
    $HTML handler node link    "styleHandleLink $HTML"
    $HTML handler script style "styleHandleStyle $HTML"
}

# style_newdocument --
#
#         style_newdocument HTML
#
#     This should be called before each new document begins loading (i.e. from
#     [gui_goto]).
#
proc style_newdocument {HTML} {
    set ::hv3_style_count 0
}

# styleHandleStyle --
#
#     styleHandleStyle HTML SCRIPT
#
proc styleHandleStyle {HTML script} {
  set id author.[format %.4d [incr ::hv3_style_count]]
  styleCallback $HTML [$HTML var url] $id $script
}

# styleUrl --
#
#     styleCallback BASE-URL URL
#
proc styleUrl {baseurl url} {
    set ret $url
    if {[::uri::isrelative $url]} {
        set ret "${baseurl}${url}"
    }
    return $ret
}

# styleCallback --
#
#     styleCallback HTML URL ID STYLE-TEXT
#
proc styleCallback {HTML url id style} {
    # Argument $url is the full URL of the stylesheet just loaded.
    if {[::uri::isrelative $url]} {
        error {assert($url is relative)}
    }

    array set u [::uri::split $url]
    regexp -expanded {^(.*/)[^/]*$} $u(path) dummy u(path)
    set baseurl [eval [concat ::uri::join [array get u]]]

    $HTML style \
        -id $id \
        -importcmd [list styleImport $HTML $id] \
        -urlcmd [list styleUrl $baseurl] \
        $style
}

# styleImport --
#
#     styleImport HTML PARENTID URL
#
proc styleImport {HTML parentid url} {
    set id ${parentid}.[format %.4d [incr ::hv3_style_count]]
    set url [url_resolve $url]
    url_fetch $url -id $url -script [list styleCallback $HTML $url $id]
}

# styleHandleLink --
#
#     styleHandleLink HTML NODE
#
proc styleHandleLink {HTML node} {
    if {[$node attr rel] == "stylesheet"} {
        # Check if the media is Ok. If so, download and apply the style.
        set media [$node attr -default "" media]
        if {$media == "" || [regexp all $media] || [regexp screen $media]} {
            set id author.[format %.4d [incr ::hv3_style_count]]
            set url [url_resolve [$node attr href]]
            url_fetch $url -id $url -script [list styleCallback $HTML $url $id]
        }
    }
}

