
package provide app-hv3 2.0

catch { memory init on }

# Load packages.
set auto_path [concat . $auto_path]
package require Tkhtml 
package require Tk
package require http 
package require uri
package require dns
package require sqlite3

# If possible, load package "Img". Without it the script can still run,
# but won't be able to load many image formats.
catch { package require Img }

# Source the other script files that are part of this application. The file
# hv3_common.tcl must be sourced first, because other files use the
# [swproc] construction it provides to declare procedures.
#
proc sourcefile {file} {
  source [file join [file dirname [info script]] $file] 
}
sourcefile hv3_common.tcl
sourcefile hv3_url.tcl

###########################################################################
# Global data:
#
# The following variables are stored in the widget dictionary:
#
#     $baseurl              # The current base URI
#     $uri                  # The current document URI
#     $cache                # Name of sqlite3 handle for cache db
#
proc gui_init_globals {} {
  .html var baseurl "file:///[pwd]/"
  .html var url {}
}

proc bgerror {args} {
  puts "BGERROR: $args"
}

###########################################################################
#
# "Gui" routines:
#
# Global vars:
#
#     gui_replaced_images
set gui_replaced_images [list]
set gui_style_count 0

# gui_build --
#
#     This procedure is called once at the start of the script to build
#     the GUI used by the application. It also sets up the callbacks
#     supplied by this script to help the widget render html.
#
#     It populates the top-level frame "." with the following widgets:
#
#         .html
#         .status
#         .goto
#
proc gui_build {} {
    set HTML [html .html]
    scrollbar .vscroll -orient vertical
    scrollbar .hscroll -orient horizontal
    label .status -height 1 -anchor w
    entry .entry

    pack .entry -fill x -side top 
    bind .entry <KeyPress-Return> {gui_goto [.entry get]}
    pack .vscroll -fill y -side right
    pack .status -fill x -side bottom 
    # pack .hscroll -fill x -side bottom
    pack $HTML -fill both -expand true

    $HTML configure -yscrollcommand {.vscroll set}
    .hscroll configure -command "$HTML xview"
    $HTML configure -xscrollcommand {.hscroll set}
    .vscroll configure -command "$HTML yview"

    bind $HTML <Motion> "handle_event motion %x %y"
    bind $HTML <ButtonPress> "handle_event click %x %y"
    bind $HTML <KeyPress-q> exit
    bind $HTML <KeyPress-Q> exit

    $HTML handler node link "handle_link_node"
    $HTML handler script style "handle_style_script"
    $HTML handler node img "handle_img_node"

    focus $HTML
}

proc handle_event {e x y} {

  set n [.html node $x $y]
  for {} {$n != ""} {set n [$n parent]} {
    if {[$n tag] == "a" && [$n attr href] != ""} {
      switch -- $e {
        motion {
          .status configure -text [$n attr href]
          . configure -cursor hand2
        }
        click  {gui_goto [$n attr href]}
      }
      break
    }
  }

  if {$e == "motion" && $n == ""} {
    .status configure -text ""
    . configure -cursor ""
  }
}

# handle_img_node_cb
#
#     handle_img_node_cb NODE IMG-DATA
#
proc handle_img_node_cb {node imgdata} {
  set img [image create photo -data $imgdata]
  $node replace $img
  .html update
}

# handle_img_node
#
#     handle_img_node NODE
proc handle_img_node {node} {
  set src [$node attr src]
  if {$src == ""} return
  set url [url_resolve $src]
  lappend ::gui_replaced_images $node $url
}

# handle_style_script
#
#     handle_style_script SCRIPT
proc handle_style_script {script} {
  set id author.[format %.4d [incr ::gui_style_count]]
  .html style -id $id $script
}

# handle_style_cb
#
#     handle_style_cb ID STYLE-TEXT
proc handle_style_cb {id style} {
  .html style -id $id $style
  .html update
}

# handle_link_node
#
#     handle_link_node NODE
proc handle_link_node {node} {
  if {[$node attr rel] == "stylesheet"} {

    # Check if the media is Ok. If so, download and apply the style.
    set media [$node attr media]
    if {$media == "" || [regexp all $media] || [regexp screen $media]} {
      set id author.[format %.4d [incr ::gui_style_count]]
      set url [url_resolve [$node attr href]]
      url_fetch $url -id $url -script [list handle_style_cb $id]
    }
  }
}

# gui_goto
#
#         gui_goto DOC
#
#     Commence the process of loading the document at url $doc.
proc gui_goto {doc} {
  .html reset
  .html update
  update

  set url [url_resolve $doc -setbase]
  .entry delete 0 end
  .entry insert 0 $url

  .html var url $url
  url_fetch $url -id $url -script [list gui_parse $url]
}

# gui_parse 
#
#         gui_parse DOC TEXT
#
#     Append the text TEXT to the current document. Argument DOC
#     is the URL from whence the new document data was received. If this
#     is different from the current URL, then clear the widget before
#     loading the text.
#
proc gui_parse {doc text} {
  set ::gui_style_count 0
  .html parse $text
  update

  foreach {node url} $::gui_replaced_images {
    url_fetch $url -script [list handle_img_node_cb $node]
  }
  set ::gui_replaced_images [list]
}

# gui_log
#
#         gui_log MSG
#
#     Log a message to the log file (stdout).
#
proc gui_log {msg} {
    puts $msg
}

swproc main {url {cache :memory:}} {
  gui_build
  gui_init_globals
  cache_init $cache
  gui_goto $url
}

eval [concat main $argv]

