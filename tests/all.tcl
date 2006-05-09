
proc sourcefile {file} {
  set fname [file join [file dirname [info script]] $file] 
  uplevel #0 [list source $fname]
}
sourcefile common.tcl

sourcefile tree.test
sourcefile style.test
sourcefile dynamic.test
sourcefile options.test

catch { destroy . }

