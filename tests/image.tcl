
catch {memory init on}

proc usage {} {
  set prog $::argv0

  puts stderr [subst {
    $prog <html-document1> ?<html-document2>....?
    $prog -file <filename>

This program renders html documents to jpeg images. If the second syntax
above is used, then <filename> must be the name of a text file containing
the names of one or more html document files, each seperated by a newline
character. Otherwise the documents rendered are those specified directly on
the command line.

When invoked, the TKHTML_TESTDIR environment variable must be set to the
name of a directory. This directory is used by the program to store images
previously rendered. The idea is that if the user has previously inspected
and approved of the rendering of a document, then the image is saved and
may be used to verify rendering of the same document at a later stage.
Thus, automated test suites for the layout engine may be accomplished. It's
unfortunate that moving caches between machines etc. will probably generate
false-negatives, due to differences in font configuration.

}]

  exit -1
}

# Load Tkhtml and if possible the Img package. The Img package is required
# for most image files formats used by web documents. Also to write jpeg
# files.
#
set auto_path [concat . $auto_path]
package require Tkhtml
catch {
  package require Img
}


# Set the global variable ::TESTDIR to the value of the cache directory. If
# the environment variable is not set, invoke the usage message proc.
#
if {![info exists env(TKHTML_TESTDIR)]}       usage
if {![file isdirectory $env(TKHTML_TESTDIR)]} usage
set TESTDIR $env(TKHTML_TESTDIR)

proc shift {listvar} {
  upvar $listvar l
  set ret [lindex $l 0]
  set l [lrange $l 1 end]
  return $ret
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

# Procedure to handle text inside a <style> tag.
proc stylecmd {style} {
  append ::STYLE_TEXT $style
  append ::STYLE_TEXT "\n"
  return ""
}

# Procedure to handle a <link> tag that imports a stylesheet.
proc linkcmd {node} {
  set rel [$node attr rel]
  if {[string compare -nocase $rel stylesheet]==0} {
    set href [$node attr href]
    set filename [file join $::BASE $href]
    lappend ::STYLESHEET_FILES $filename
  }
}

proc load_document {css document} {

  set ::STYLESHEET_FILES {}
  set ::STYLE_TEXT {}
  set parsetime [time {
      $::HTML parse $document
      $::HTML tree build
      $::HTML style parse agent $css
      while {[llength $::STYLESHEET_FILES]>0} {
        set ss [lindex $::STYLESHEET_FILES 0]
        set ::STYLESHEET_FILES [lrange $::STYLESHEET_FILES 1 end]
        $::HTML style parse author [readFile $ss]
      }
      $::HTML style parse author $::STYLE_TEXT
  }]

  $::HTML style parse author.1 { 
    img    { -tkhtml-replace: tcl(replace_img) }
    object { -tkhtml-replace: tcl(replace_img) }
    input  { -tkhtml-replace: tcl(replace_input) }
    select { -tkhtml-replace: tcl(replace_select) }
  }

  set styletime [time {
      $::HTML style apply
  }]
  puts -nonewline "Parse [lrange $parsetime 0 1] Style [lrange $styletime 0 1]"
}

# Procedure to handle <input> and <object> tags.
proc replace_img {node} {
  if {[$node tag]=="object"} {
    set filename [file join $::BASE [$node attr data]]
  } else {
    set filename [file join $::BASE [$node attr src]]
  }
  if [catch { set img [image create photo -file $filename] } msg] {
    # puts "Error: $msg"
    error $msg
  } 
  return $img
}

# Procedure to handle <input> tags.
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

# Procedure to handle <select> tags
proc replace_select {node} {
  set tkname ".control[incr ::CONTROL]"
  button $tkname -text Select
  return $tkname
}

proc docname_to_imgname {docname} {
  return [file join $::TESTDIR [string map {{ } _ / _} $docname].png]
}

proc compare_document_image {docname} {
  $::HTML layout force
  set layouttime [time {set img [$::HTML layout image]}]
  puts " Layout [lrange $layouttime 0 1]"
  set filename [docname_to_imgname $docname]
  $img write tmp.png -format png
  image delete $img

  set data [readFile tmp.png]
  set data2 [readFile $filename]
  if {$data2==""} {
    return NOIMAGE
  }
  if {$data2==$data} {
    return MATCH
  }
  return NOMATCH
}

proc correct {docname img} {
  set filename [docname_to_imgname $docname]
  catch {
    file delete -force $filename
  }
  $img write $filename -format png
  set ::CONTINUEFLAG 1
}
proc incorrect {docname img} {
  set ::CONTINUEFLAG 1
}

wm geometry . 800x600

set ::HTML [html .h]
$::HTML handler script script dummycmd
$::HTML handler script style stylecmd
$::HTML handler node link linkcmd

set ::DOCUMENT_LIST $argv
set ::DEFAULT_CSS [readFile [file join [file dirname [info script]] html.css]]

frame .buttons
button .buttons.correct    -text Correct
button .buttons.incorrect  -text Incorrect
button .buttons.oldimage  -text {Old Image}
button .buttons.newimage  -text {New Image}

pack .buttons.correct .buttons.incorrect -side left
pack .buttons.oldimage .buttons.newimage -side right
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

bind .c <KeyPress-Down> {.c yview scroll 1 units} 
bind .c <KeyPress-Up> {.c yview scroll -1 units} 
focus .c

foreach document $::DOCUMENT_LIST {
  set ::BASE [file dirname $document]
  load_document $::DEFAULT_CSS [readFile $document]
  set res [compare_document_image $document]

  if {$res=="MATCH"} {
      puts "$document - MATCH"
  }
  if {$res=="NOIMAGE"} {
      .c delete all
      set img [$::HTML layout image]
      .c create image 0 0 -anchor nw -image $img
      catch {
        .c configure -scrollregion [.c bbox all]
      }
      .buttons.correct configure -command "correct $document $img"
      .buttons.incorrect configure -command "incorrect $document $img"
      .buttons.oldimage configure -state disabled
      .buttons.newimage configure -state disabled
      vwait ::CONTINUEFLAG
  }
  if {$res=="NOMATCH"} {
      set img [$::HTML layout image]
      set imgold [image create photo -file [docname_to_imgname $document]]

      .c delete all
      .c create image 0 0 -anchor nw -image $img
      catch { .c configure -scrollregion [.c bbox all] }

      .buttons.correct configure -command "correct $document $img"
      .buttons.incorrect configure -command "incorrect $document $img"
      .buttons.oldimage configure -state normal -command [subst -nocommands {
         .c delete all
         .c create image 0 0 -anchor nw -image $imgold
         catch { .c configure -scrollregion [.c bbox all] }
      }]
      .buttons.newimage configure -state normal -command [subst -nocommands {
         .c delete all
         .c create image 0 0 -anchor nw -image $img
         catch { .c configure -scrollregion [.c bbox all] }
      }]
      vwait ::CONTINUEFLAG
      .c delete all
      image delete $img
      image delete $oldimg
  }

  $::HTML clear
}

exit

