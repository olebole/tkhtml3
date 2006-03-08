
package require tcltest
package require Tkhtml

proc sourcefile {file} {
  set fname [file join [file dirname [info script]] $file] 
  uplevel #0 [list source $fname]
}

tcltest::verbose {pass body error}

sourcefile tree.test

catch {
  destroy .
}

