#
# This script should be run by the "hwish" program.  It is used to
# test the HTML widget.
#

wm title . {HTML Widget Test}
wm iconname . {HTML Widget Test}

frame .h
pack .h -side top -fill both -expand 1
html .h.h \
  -yscrollcommand {.h.vsb set} \
  -xscrollcommand {.f2.hsb set} \
  -padx 5 \
  -pady 9 \
  -bg white
pack .h.h -side left -fill both -expand 1
scrollbar .h.vsb -orient vertical -command {.h.h yview}
pack .h.vsb -side left -fill y

frame .f2
pack .f2 -side top -fill x
frame .f2.sp -width [winfo reqwidth .h.vsb] -bd 2 -relief raised
pack .f2.sp -side right -fill y
scrollbar .f2.hsb -orient horizontal -command {.h.h xview}
pack .f2.hsb -side top -fill x

frame .f3
pack .f3 -side top -fill x
button .f3.exit -text Exit -command exit
pack .f3.exit -side left
button .f3.load -text Load -command Load
pack .f3.load -side left
button .f3.parseall -text {Parse All} -command {Parse 100000000}
pack .f3.parseall -side left
button .f3.parse100 -text {Parse 100} -command {Parse 100}
pack .f3.parse100 -side left
button .f3.clear -text {Clear} -command {.h.h clear}
pack .f3.clear -side left


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
    }
    set lastDir [file dirname $f]
  }
}
proc Parse {n} {
  global htmltext
  set toparse [string range $htmltext 0 [expr $n-1]]
  set htmltext [string range $htmltext $n end]
  .h.h parse $toparse
}
