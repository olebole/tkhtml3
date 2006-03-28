
package require Tkhtml
package require tcltest
tcltest::verbose {pass body error}

proc finish_test {} {
  catch {
    destroy .h
  }
}


