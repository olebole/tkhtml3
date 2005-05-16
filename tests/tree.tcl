
set auto_path [concat . $auto_path]
package require Tkhtml

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

proc print_tree {node {indent 0}} {
  if {[$node tag]=="text"} {
    if {[regexp {^ *$} [$node text]]==0} {
      puts -nonewline [string repeat " " $indent]
      puts [$node text]
    }
  } else {
    puts -nonewline [string repeat " " $indent]
    puts "<[$node tag]>"
    for {set i 0} {$i < [$node nChildren]} {incr i} {
      print_tree [$node child $i] [expr $indent+2]
    }
    puts -nonewline [string repeat " " $indent]
    puts "</[$node tag]>"
  }
}

html .h
.h parse [readFile [lindex $argv 0]]
.h tree build
set root [.h tree root]
print_tree $root

exit


