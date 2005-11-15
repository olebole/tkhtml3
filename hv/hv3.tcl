
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
sourcefile hv3_form.tcl
sourcefile hv3_style.tcl

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
    bind $HTML <ButtonPress-2> "handle_event rightclick %x %y"
    bind $HTML <KeyPress-q> hv3_exit
    bind $HTML <KeyPress-Q> hv3_exit

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

    .m.file add command -label Browser -command [list prop_browse $HTML]
    .m.file add separator
    .m.file add command -label Exit -command exit

    log_init $HTML
    image_init $HTML
    form_init $HTML
    style_init $HTML
}

#--------------------------------------------------------------------------
# handle_event E X Y
#
#     Handle an html window event. Argument E may be "motion", "click" or
#     "rightclick". X and Y are the window coordinates where the event
#     occured. For "motion" events, this means the position of the cursor
#     after the event.
#
proc handle_event {e x y} {

    # Calculate the (rough) maximum number of chars .status can hold.
    set pix [font measure [.status cget -font] -displayof .status xxxxxxxxxx]
    set chars [expr 9 * ([winfo width .status] / $pix)]

    if {$e == "click"} {
        catch {.prop_menu unpost}
    }

    set node [.html node $x $y]
    if {$e == "rightclick"} {
        prop_browse .html -node $node
    }

    set n $node
    for {} {$n != ""} {set n [$n parent]} {
      if {[$n tag] == "a" && 0 == [catch {set href [$n attr href]}]} {
        switch -- $e {
          motion {
            .status configure -text [string range $href 0 $chars]
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

      .status configure -text [string range $nodeid 0 $chars]
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

# handle_script_script
#
#     handle_script_script SCRIPT
proc handle_script_script {script} {
  return ""
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
  style_newdocument .html
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

# hv3_exit
#
#          hv3_exit
#
#     Exit the application.
proc hv3_exit {} {
    destroy .html 
    catch {destroy .prop.html}
    catch {::tk::htmlalloc}
    if {[llength [form_widget_list]] > 0} {
        puts "Leaked widgets: [form_widget_list]"
    }
    exit
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

