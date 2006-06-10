set {::hv3::version($Id: main.tcl,v 1.6 2006/06/10 12:32:27 danielk1977 Exp $)} 1

package require starkit
starkit::startup
set ::HV3_STARKIT 1
source [file join [file dirname [info script]] hv3_main.tcl] 

