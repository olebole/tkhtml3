
package require Tkhtml
package require tcltest
tcltest::verbose {pass body error}

proc sourcefile {file} {
  set fname [file join [file dirname [info script]] $file] 
  uplevel #0 [list source $fname]
}

sourcefile tree.test
sourcefile style.test
sourcefile dynamic.test

catch {
  destroy .
}

