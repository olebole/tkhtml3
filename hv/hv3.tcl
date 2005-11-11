
package provide app-hv3 2.0

catch { memory init on }

# Load packages.
set auto_path [concat . $auto_path]
package require Tkhtml 3.0
package require Tk
package require http 
package require uri
package require dns
package require sqlite3

# If possible, load package "Img". Without it the script can still run,
# but won't be able to load many image formats.
if {[catch { package require Img }]} {
  puts "WARNING: Failed to load package Img"
}

# Source the other script files that are part of this application.
#
proc sourcefile {file} {
  set fname [file join [file dirname [info script]] $file] 
  uplevel #0 [list source $fname]
}
sourcefile hv3_url.tcl
sourcefile hv3_image.tcl
sourcefile hv3_log.tcl
sourcefile hv3_nav.tcl
sourcefile hv3_prop.tcl

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

    frame .entry
    entry .entry.entry
    button .entry.clear -text {Clear ->} -command {.entry.entry delete 0 end}

    pack .entry.clear -side left
    pack .entry.entry -fill both -expand true
    pack .entry -fill x -side top 
    bind .entry.entry <KeyPress-Return> {gui_goto [.entry.entry get]}

    pack .vscroll -fill y -side right
    pack .status -fill x -side bottom 
    # pack .hscroll -fill x -side bottom
    pack $HTML -fill both -expand true

    $HTML configure -yscrollcommand {.vscroll set}
    .hscroll configure -command "$HTML xview"
    $HTML configure -xscrollcommand {.hscroll set}
    .vscroll configure -command "$HTML yview"

    bind $HTML <Motion>        "handle_event motion %x %y"
    bind $HTML <ButtonPress-1> "handle_event click %x %y"
    bind $HTML <KeyPress-q> exit
    bind $HTML <KeyPress-Q> exit

    bind $HTML <ButtonPress-4> "$HTML yview scroll -2 units"
    bind $HTML <ButtonPress-5> "$HTML yview scroll 2 units"

    $HTML handler node link "handle_link_node"
    $HTML handler script style "handle_style_script"
    $HTML handler node img "handle_img_node"
    $HTML handler script script "handle_script_script"

    focus $HTML

    ###########################################################################
    # Build the main window menu.
    #
    . config -menu [menu .m]
    .m add cascade -label {File} -menu [menu .m.file]
    foreach f [list \
        [file join $::tcl_library .. .. bin tkcon] \
        [file join $::tcl_library .. .. bin tkcon.tcl]
    ] {
        if {[file exists $f]} {
            catch {
                uplevel #0 "source $f"
                package require tkcon
                .m.file add command -label Tkcon -command {tkcon show}
            }
            break
        }
    }

    .m.file add command -label Browser -command [list prop_updateBrowser $HTML]
    .m.file add separator
    .m.file add command -label Exit -command exit

    log_init $HTML
    image_init $HTML
}

proc handle_event {e x y} {
  if {$e == "click"} {
    catch {.prop_menu unpost}
  }

  set node [.html node $x $y]
  set n $node

  for {} {$n != ""} {set n [$n parent]} {
    if {[$n tag] == "a" && 0 == [catch {set href [$n attr href]}]} {
      switch -- $e {
        motion {
          .status configure -text $href
          . configure -cursor hand2
        }
        click  "gui_goto $href"
      }
      break
    }
  }

  if {$n == ""} {
      . configure -cursor ""
      set n $node

      set nodeid ""
      while {$n != ""} {
          if {[$n tag] == ""} {
              set nodeid [string range [$n text] 0 20]
          } else {
              set nodeid "<[$n tag]>$nodeid"
          }
          set n [$n parent]
      }
      .status configure -text $nodeid

      if {$e == "click"} {real_puts "($x, $y)"}
    }
}

# handle_img_node_cb
#
#     handle_img_node_cb NODE IMG-DATA
#
proc handle_img_node_cb {node imgdata} {
  catch {
    set img [image create photo -data $imgdata]
    $node replace $img
  }
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

# handle_script_script
#
#     handle_script_script SCRIPT
proc handle_script_script {script} {
  return ""
}

# handle_style_cb
#
#     handle_style_cb ID STYLE-TEXT
proc handle_style_cb {id style} {
  .html style -id $id $style
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
  # update

  set url [url_resolve $doc -setbase]
  .entry.entry delete 0 end
  .entry.entry insert 0 $url

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
  # update

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

swproc main {{cache :memory:} {doclist ""}} {
  gui_build
  gui_init_globals
  cache_init $cache
  nav_init $cache -doclist $doclist
}

##########################################################################
# Utility procedures designed for use in Tkcon:
#
#     hv3_findnode
#     hv3_nodetostring
#
proc findNode {predicate N nodevar} {
  set ret [list]
  set $nodevar $N
  catch {eval "if {$predicate} {lappend ret $N}"}
  for {set i 0} {$i < [$N nChild]} {incr i} {
      set ret [concat $ret [findNode $predicate [$N child $i] $nodevar]]
  }
  return $ret
}

swproc hv3_findnode {predicate {nodevar N}} {
  set ret [list]
  set N [.html node]

  return [findNode $predicate $N $nodevar]
}

proc hv3_nodetostring {N} {
    if {[$N tag] == ""} {
        return "\"[string range [$N text] 0 20]\""
    }
    set d "<[$N tag]"
    foreach {a v} [$N attr] {
        append d " $a=\"$v\""
    }
    append d ">"
    return $d
}

proc hv3_nodeprint {N} {
    set stack [list]
    for {set n $N} {$n != ""} {set n [$n parent]} {
        lappend stack [hv3_nodetostring $n]
    }

    set ret ""
    set indent 0
    for {set i [expr [llength $stack] - 1]} {$i >= 0} {incr i -1} {
        append ret [string repeat " " $indent]
        append ret [lindex $stack $i]
        append ret "\n"
        incr indent 4
    }

    return $ret;
}

eval [concat main $argv]

