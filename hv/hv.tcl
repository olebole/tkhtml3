#
# This script implements the "hv" application.  Type "hv FILE" to
# view the file as HTML.
# 
wm title . {HTML File Viewer}
wm iconname . {HV}

if {[info command html]==""} {
  foreach f {
    ./tkhtml.so
    /usr/lib/tkhtml.so
    /usr/local/lib/tkhtml.so
    ./tkhtml.dll
  } {
    if {[file exists $f]} {
      if {[catch {load $f Tkhtml}]==0} break
    }
  }
}

set HtmlTraceMask 0
set file {}
foreach a $argv {
  if {[regexp {^debug=} $a]} {
    scan $a "debug=0x%x" HtmlTraceMask
  } else {
    set file $a
  }
}

image create photo biggray -data {
    R0lGODdhPAA+APAAALi4uAAAACwAAAAAPAA+AAACQISPqcvtD6OctNqLs968+w+G4kiW5omm
    6sq27gvH8kzX9o3n+s73/g8MCofEovGITCqXzKbzCY1Kp9Sq9YrNFgsAO///
}
image create photo smgray -data {
    R0lGODdhOAAYAPAAALi4uAAAACwAAAAAOAAYAAACI4SPqcvtD6OctNqLs968+w+G4kiW5omm
    6sq27gvH8kzX9m0VADv/
}

frame .mbar -bd 2 -relief raised
pack .mbar -side top -fill x
menubutton .mbar.help -text File -underline 0 -menu .mbar.help.m
pack .mbar.help -side left -padx 5
set m [menu .mbar.help.m]
$m add command -label Open -underline 0 -command Load
$m add command -label Refresh -underline 0 -command Refresh
$m add separator
$m add command -label Exit -underline 1 -command exit

frame .h
pack .h -side top -fill both -expand 1
html .h.h \
  -yscrollcommand {.h.vsb set} \
  -xscrollcommand {.f2.hsb set} \
  -padx 5 \
  -pady 9 \
  -formcommand FormCmd \
  -imagecommand ImageCmd \
  -scriptcommand ScriptCmd \
  -bg white -tablerelief flat

proc FormCmd {n cmd args} {
  # puts "FormCmd: $n $cmd $args"
  switch $cmd {
    input {
      set w [lindex $args 0]
      label $w -image smgray
    }
  }
}
proc ImageCmd {args} {
  set fn [lindex $args 0]
  if {[catch {image create photo -file $fn} img]} {
    global HtmlTraceMask
    if {$HtmlTraceMask==0} {
      tk_messageBox -icon error -message $img -type ok
    }
    return biggray
  } else {
    global Images
    set Images($img) 1
    return $img
  }
}
proc ScriptCmd {args} {
  # puts "ScriptCmd: $args"
}
proc HrefBinding {x y} {
  set new [.h.h href $x $y]
  if {$new!=""} {
    LoadFile $new
  }
}
bind .h.h.x <1> {HrefBinding %x %y}
pack .h.h -side left -fill both -expand 1
scrollbar .h.vsb -orient vertical -command {.h.h yview}
pack .h.vsb -side left -fill y

frame .f2
pack .f2 -side top -fill x
frame .f2.sp -width [winfo reqwidth .h.vsb] -bd 2 -relief raised
pack .f2.sp -side right -fill y
scrollbar .f2.hsb -orient horizontal -command {.h.h xview}
pack .f2.hsb -side top -fill x

#proc FontCmd {args} {
#  puts "FontCmd: $args"
#  return {Times 12}
#}
#proc ResolverCmd {args} {
#  puts "Resolver: $args"
#  return [lindex $args 0]
#}

set lastDir [pwd]
proc Load {} {
  set filetypes {
    {{Html Files} {.html .htm}}
    {{All Files} *}
  }
  global lastDir htmltext
  set f [tk_getOpenFile -initialdir $lastDir -filetypes $filetypes]
  if {$f!=""} {
    LoadFile $f
    set lastDir [file dirname $f]
  }
}

# Clear the screen.
#
proc Clear {} {
  global Images
  .h.h clear
  foreach img [array names Images] {
    image delete $img
  }
  catch {unset Images}
}

# Read a file
#
proc ReadFile {name} {
  if {[catch {open $name r} fp]} {
    tk_messageBox -icon error -message $fp -type ok
    return {}
  } else {
    return [read $fp [file size $name]]
  }
}

# Load a file into the HTML widget
#
proc LoadFile {name} {
  set html [ReadFile $name]
  if {$html==""} return
  Clear
  global LastFile
  set LastFile $name
   .h.h config -base $name
  .h.h parse $html
}

# Refresh the current file.
#
proc Refresh {} {
  global LastFile
  if {![info exists LastFile]} return
  LoadFile $LastFile
}

update
if {$file!=""} {
  LoadFile $file
}
