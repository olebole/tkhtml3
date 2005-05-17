
set USAGE [string trimleft [subst {

$argv0 <test-directory>

Test directory should contain one or more document files with the extension
".html". For each ".html" file, there may be a matching ".jpg" or ".gif"
file.

Each html file is rendered to a canvas widget, and the results compared to
the jpg or gif file, if any. If the rendering matches the image, then
proceed to the next html file. If there is no image file, or the rendering
does not match it, then present the user with a GUI to inspect the rendered
document. If the user indicates the rendering is correct, then an image
file is created in the test directory.

}]]

catch {memory init on}

# Required packages
set auto_path [concat . $auto_path]
package require Tkhtml
catch {
  package require Img
}

# Procedure to return the contents of a file-system entry
proc readFile {fname} {
  set ret {}
  catch {
    set fd [open $fname]
    set ret [read $fd]
    close $fd
  }
  return $ret
}

proc tree_to_html {indent tree} {
  set in [string repeat " " $indent]
  if {[regexp {^TEXT} $tree]} {
    puts -nonewline "$in"
    puts $tree
  } else {
    set tag [lindex $tree 0]
    puts "$in<$tag>"
    foreach child [lindex $tree 1] {
      tree_to_html [expr $indent + 2] $child
    }
    puts "$in</$tag>"
  }
}

proc scriptcommand {line_number tag tarargs script} {
  if {$tag=="style"} {
    append ::STYLE_TEXT $script
    puts $script
  }
  return ""
}

proc stylecmd {style} {
  append ::STYLE_TEXT $style
  append ::STYLE_TEXT "\n"
  return ""
}

proc scriptcmd {script} {
  return ""
}

proc linkcmd {node} {
  set rel [$node attr rel]
  if {[string compare -nocase $rel stylesheet]==0} {
    set href [$node attr href]
    set filename [file join $::BASE $href]
    lappend ::STYLESHEET_FILES $filename
  }
}

set HTML .h
proc main {document css} {

  html $::HTML

  $::HTML handler script script dummycmd
  $::HTML handler script style stylecmd
  $::HTML handler node link linkcmd

  set ::STYLESHEET_FILES {}
  set ::STYLE_TEXT {}
  set parsetime [time {
      $::HTML parse $document
      $::HTML tree build
      $::HTML style parse $css
      while {[llength $::STYLESHEET_FILES]>0} {
        set ss [lindex $::STYLESHEET_FILES 0]
        set ::STYLESHEET_FILES [lrange $::STYLESHEET_FILES 1 end]
        $::HTML style parse [readFile $ss]
      }
      $::HTML style parse $::STYLE_TEXT
  }]
  puts "Parse time [lrange $parsetime 0 1]"

  $::HTML style parse { 
    img    { -tkhtml-replace: tcl(replace_img) }
    object { -tkhtml-replace: tcl(replace_img) }
    input  { -tkhtml-replace: tcl(replace_input) }
    select { -tkhtml-replace: tcl(replace_select) }
  }

  set s [$::HTML style syntax_errs]
  puts "$s syntax errors in style sheet"

  set styletime [time {
      $::HTML style apply
  }]
  puts "Style time [lrange $styletime 0 1]"
}

set W 800
proc redraw {{w 0}} {
  if {$w==0} {
    set w $::W
  } else {
    set ::W $w
  } 
  set codetime [time {$::HTML layout force -width $w -win .c}]
  puts "Layout time [lrange $codetime 0 1]"
  set tclizetime [time {set code [$::HTML layout primitives]}]
  puts "Tclize time [lrange $tclizetime 0 1]"
  set drawtime [time {draw_to_canvas $code .c 0 0}]
  puts "Draw time   [lrange $drawtime 0 1]"

  set i [$::HTML layout image]
  puts "Image is $i"
  $i write out.jpg -format jpeg
}

proc replace_img {node} {
  if {[$node tag]=="object"} {
    set filename [file join $::BASE [$node attr data]]
  } else {
    set filename [file join $::BASE [$node attr src]]
  }
  if [catch { set img [image create photo -file $filename] } msg] {
    puts "Error: $msg"
    error $msg
  } 
  return $img
}

set CONTROL 0
proc replace_input {node} {
  set tkname ".control[incr ::CONTROL]"
  set width [$node attr width]
  if {$width==""} {
    set width 20
  }

  switch -exact [$node attr type] {
    image {
      return [replace_img $node]
    }
    hidden {
      return ""
    }
    checkbox {
      return [checkbutton $tkname]
    }
    radio {
      return [checkbutton $tkname]
    }
    submit {
      return [button $tkname -text Submit]
    }
    default {
      entry $tkname -width $width
      return $tkname
    }
  }
  return ""
}

proc replace_select {node} {
  set tkname ".control[incr ::CONTROL]"
  button $tkname -text Select
  return $tkname
}

proc draw_origin {x y} {
  upvar X X
  upvar Y Y
  upvar C C

  incr X $x
  incr Y $y
}

proc draw_text {x y font color string} {
  upvar X X
  upvar Y Y
  upvar C C

  incr x $X
  incr y $Y

  # The Y coordinate supplied by the layout code is for the baseline of the
  # text item. The canvas widget doesn't support this, so decrement Y by
  # the font metric 'ascent' value and anchor the nw corner of the text to
  # simulate it.
  set ascent [font metrics $font -ascent]
  incr y [expr -1*$ascent]

  $C create text $x $y -font $font -fill $color -text $string -anchor nw 
}

proc draw_quad {x1 y1 x2 y2 x3 y3 x4 y4 color} {
  upvar X X
  upvar Y Y
  upvar C C

  foreach v {x1 x2 x3 x4} {incr $v $X}
  foreach v {y1 y2 y3 y4} {incr $v $Y}
  $C create polygon $x1 $y1 $x2 $y2 $x3 $y3 $x4 $y4 -fill $color
}

proc draw_image {x y image} {
  upvar X X
  upvar Y Y
  upvar C C

  incr x $X
  incr y $Y
  $C create image $x $y -image $image -anchor nw
}

proc draw_window {x y window} {
  upvar X X
  upvar Y Y
  upvar C C

  incr x $X
  incr y $Y
  $C create window $x $y -window $window -anchor nw
}

proc draw_background {color} {
  upvar X X
  upvar Y Y
  upvar C C

  $C configure -background $color
}

proc draw_to_canvas {code c x y} {
  $c delete all

  set X $x
  set Y $y
  set C $c

  foreach instruction $code {
    # puts $instruction
    eval $instruction
  }

  set scrollregion [$c bbox all]
  if {[llength $scrollregion]==4} {
    lset scrollregion 0 0
    lset scrollregion 1 0
    $c configure -scrollregion $scrollregion
  }
  puts "Scrollregion: $scrollregion"
}

proc new_document {{r 1}} {
  catch {destroy $::HTML}
  if {[llength $::DOCS]==0} exit
  set ::BASE [file dirname [lindex $::DOCS 0]]
  main [readFile [lindex $::DOCS 0]] $::CSS
  set ::DOCS [lrange $::DOCS 1 end]
  if {$r} redraw
}

wm geometry . 800x600

frame .buttons
button .buttons.correct    -text Incorrect -command new_document
button .buttons.incorrect  -text Correct   -command new_document

pack .buttons.correct .buttons.incorrect -side left
pack .buttons -side bottom -fill x

scrollbar .s -orient vertical
scrollbar .s2 -orient horizontal
canvas .c -background white
pack .s -side right -fill y
pack .s2 -side bottom -fill x
pack .c -fill both -expand true

.c configure -yscrollcommand {.s set}
.c configure -xscrollcommand {.s2 set}
.s configure -command {.c yview}
.s2 configure -command {.c xview}

bind .c <Configure> {redraw [expr %w - 5]}
bind .c <KeyPress-Down> {.c yview scroll 1 units} 
bind .c <KeyPress-Up> {.c yview scroll -1 units} 
focus .c

set cssfile [file join [file dirname [info script]] html.css]
set CSS [readFile $cssfile]

set arg [lindex $argv 0]
if {[file isdirectory $arg]} {
  set DOCS [lsort [glob [file join [lindex $argv 0] *.html]]]
} else {
  set DOCS $arg
}

new_document 0
