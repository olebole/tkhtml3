
package require Tkhtml
package require tcltest
tcltest::verbose {pass body error}

catch {rename finish_test ""}

if {[catch {incr ::nested_test_count}]} {set ::nested_test_count 1}

proc finish_test {} {
  catch {
    destroy .h
  }
  incr ::nested_test_count -1
  if {$::nested_test_count == 0}  {
    catch {
      destroy .
      catch {::tkhtml::htmlalloc}
    }
  }
}


