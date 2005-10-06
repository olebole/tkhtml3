
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
  cache_init
}

proc bgerror {args} {
  puts "BGERROR: $args"
}

# cache_init, cache_store, cache_query, cache_fetch --
#
#         cache_init
#         cache_store URL DATA
#         cache_query URL
#         cache_fetch URL
#
#     A tiny API to implement a primitive web cache.
#
proc cache_init {} {
  sqlite3 dbcache :memory:
  .html var cache dbcache
  [.html var cache] eval {CREATE TABLE cache(url PRIMARY KEY, data BLOB);}
}
proc cache_store {url data} {
  set sql {REPLACE INTO cache(url, data) VALUES($url, $data);}
  [.html var cache] eval $sql
}
proc cache_query {url} {
  set sql {SELECT count(*) FROM cache WHERE url = $url}
  return [[.html var cache] one $sql]
}
proc cache_fetch {url} {
  set sql {SELECT data FROM cache WHERE url = $url}
  return [[.html var cache] one $sql]
}

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

    pack .vscroll -fill y -side right
    pack .status -fill x -side bottom 
    # pack .hscroll -fill x -side bottom
    pack $HTML -fill both -expand true

    $HTML configure -yscrollcommand {.vscroll set}
    .hscroll configure -command "$HTML xview"
    $HTML configure -xscrollcommand {.hscroll set}
    .vscroll configure -command "$HTML yview"

if 0 {
    bind $HTML <Motion> "update_status %x %y"
    bind $HTML <ButtonPress> "click %x %y"
}
    bind $HTML <KeyPress-q> exit
    bind $HTML <KeyPress-Q> exit

if 0 {
    $HTML handler script style "handle_style_script"
    $HTML handler node link "handle_link_node"
    $HTML handler node a "handle_a_node"
}

    $HTML handler script style "handle_style_script"
    $HTML handler node img "handle_img_node"

    focus $HTML
}

###########################################################################
#
# "Gui" routines:
#
# Global vars:
#
#     gui_replaced_images
set gui_replaced_images [list]

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
  .html style -id author.0 $script
}

# gui_goto
#
#         gui_goto DOC
#
#     Commence the process of loading the document at url $doc.
proc gui_goto {doc} {
  set url [url_resolve $doc -setbase]
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

if {[llength $argv] != 1} {
  puts stderr "Usage: $argv0 <url>"
  exit -1
}

gui_build
gui_init_globals
gui_goto [lindex $argv 0]

