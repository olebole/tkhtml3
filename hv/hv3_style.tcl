
###########################################################################
# hv3_style.tcl --
#
#     This file contains code to implement stylesheet functionality.
#     The public interface to this file are the commands:
#
#         style_init HTML
#         style_newdocument HTML
#

#--------------------------------------------------------------------------
# Global variables section
set ::hv3_style_count 0

#--------------------------------------------------------------------------

proc style_init {HTML} {
    $HTML handler node link    "styleHandleLink $HTML"
    $HTML handler script style "styleHandleStyle $HTML"
}

proc style_newdocument {HTML} {
    set ::hv3_style_count 0
}

# styleHandleStyle --
#
#     styleHandleStyle HTML SCRIPT
#
proc styleHandleStyle {HTML script} {
  set id author.[format %.4d [incr ::hv3_style_count]]
  styleCallback $HTML $id $script
}

# styleCallback --
#
#     styleCallback HTML ID STYLE-TEXT
#
proc styleCallback {HTML id style} {
  $HTML style -id $id -importcmd [list styleImport $HTML $id] $style
}

# styleImport --
#
#     styleImport HTML PARENTID URL
#
proc styleImport {HTML parentid url} {
    set id ${parentid}.[format %.4d [incr ::hv3_style_count]]
    set url [url_resolve $url]
    url_fetch $url -id $url -script [list styleCallback $HTML $id]
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
            url_fetch $url -id $url -script [list styleCallback $HTML $id]
        }
    }
}

