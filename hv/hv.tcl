#
# This script implements the "hv" application.  Type "hv FILE" to
# view the file as HTML.
#

wm title . {HTML File Viewer}
wm iconname . {HV}

image create photo gray -data {
    R0lGODdhIwAnAPcAAMDAwPgAwP8AAb8AQG/wALVAAQEWAEAIAPDUE0D3ABb/AAi/AFA1AU+y
    ABYBAAhAANys0EdJRxYWFggICMhUgDL39gj//0C/vw4EYMAA9gEA/0AAv1DIbwEytQAIAQBA
    QFDw8EhAQFDwUE9AT4Bk1PhPR/8WFr8ICDEb/v1bRwIAFkAACE4EUEgATxYAFggACGjwnjlA
    vQ4WAQgIQMEEMHr4AAv/AEC/AIwEDUoAABIAAEAAAADwLQBAAAAWAAQIAFgQFHb4SBL/Fgy/
    CFg1vHay9hIB/whAv+gEbrkAjQ8AB0hAvRYWAVAEMEgAABYAAAgAAMjwDTJAAAgWAJwoLfj4
    AP//AL+/AJQ1HLWySAQBFkBACAPovABH9gAW/wAIvyCooEf3jRb/Bwi/CEwE8AIAQAAAFgAA
    CMjIgzIyAEBABbjwUPhAT8TwML9AAAQWAJhkEEZPABYWAAgIACAbLkdbAEwEEgIAAAAAAARY
    yPn49v///7+/v901TnSylQgBBwhACGwcGEdIcA4WFQDYAQD3AAD/AAC/AHgEXfkAx/8AEr8A
    CMjI2O8y9hgI/8jwNTJAiAgWB0AICHTw1PlAbf8WFXhkJPlP/P8W/78Iv8gA2O8A+RgA/wgA
    vzTw3/lAfP8WBcBQDadPAAcWAACgAABJAAAIAAHIJAAy/AAI/wBAvyMOJADA/AAB/ydSSAAB
    +QAA/wAAvwAkAABKAABQAAFPAHwsAPn4AHgxAPn9AP8CADQaJPlK/PycUKf4Twf/FsgK4O8A
    RxgAFiPsyAAAMgAAQCduDgAAwABAAQAFQACnUgD4ASQKZPwASP8AFgBABAD4AABv8AC1QAAB
    FgBACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
    AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
    AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACwAAAAAIwAnAAAIPQABCBxIsKDBgwgTKlzIsKHD
    hxAjSpxIsaLFixgzatzIsaPHjyBDihxJsqTJkyhTqlzJsqXLlzBjypx5MiAAO///
}

frame .mbar -bd 2 -relief raised
pack .mbar -side top -fill x
menubutton .mbar.help -text Help -underline 0 -menu .mbar.help.m
pack .mbar.help -side left -padx 5
set m [menu .mbar.help.m]
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
  -bg white

proc FormCmd {n cmd args} {
  puts "FormCmd: $n $cmd $args"
  switch $cmd {
    input {
      set w [lindex $args 0]
      button $w -text $w -command [format {puts {%s}} $args]
    }
  }
}
proc ImageCmd {args} {
  set fn [lindex $args 0]
  if {[catch {image create photo -file $fn} img]} {
    tk_messageBox -icon error -message $img -type ok
    return gray
  } else {
    return $img
  }
}
proc HrefBinding {x y} {
  puts "Href $x $y: [.h.h href $x $y]"
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

proc FontCmd {args} {
  puts "FontCmd: $args"
  return {Times 12}
}
proc ResolverCmd {args} {
  puts "Resolver: $args"
  return [lindex $args 0]
}

set lastDir [pwd]
proc Load {} {
  set filetypes {
    {{Html Files} {.html .htm}}
    {{All Files} *}
  }
  global lastDir htmltext
  set f [tk_getOpenFile -initialdir $lastDir -filetypes $filetypes]
  if {$f!=""} {
    if {[catch {open $f r} fp]} {
      tk_messageBox -icon error -message $fp -type ok
    } else {
      set htmltext [read $fp [file size $f]]
      close $fp
      .h.h config -base file:$f
    }
    set lastDir [file dirname $f]
  }
}
proc Parse {n {pr 0}} {
  global htmltext
  set toparse [string range $htmltext 0 [expr $n-1]]
  set htmltext [string range $htmltext $n end]
  if {$pr} {puts "Parsing: [list $toparse]"}
  .h.h parse $toparse
}
proc SlowParseAll {} {
  global htmltext
  while {[string length $htmltext]>0} {
    Parse 1 1
    update
  }
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
  .h.h clear
  .h.h config -base $name
  .h.h parse $html
}

update
if {[llength $argv]>0} {
  LoadFile [lindex $argv 0]
}
