
#
# This is the main.tcl file used to create a starkit from the hv.tcl
# application.
#

package require starkit
if {[starkit::startup] eq "sourced"} return

if {[llength $argv] == 0} {
    set argv [file join [file dirname [info script]] index.html]
}

package require app-hv3

