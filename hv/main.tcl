
package require starkit
starkit::startup
if {[llength $::argv] == 0} {
  set ::argv [list [file join [file dirname [info script]] index.html]]
}
source [file join [file dirname [info script]] hv3_main.tcl] 

