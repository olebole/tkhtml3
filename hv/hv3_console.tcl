namespace eval hv3 { set {version($Id: hv3_console.tcl,v 1.1 2007/10/27 12:06:52 hkoba Exp $)} 1 }

# -*- mode: tcl; tab-width: 8 -*-
#
# Make sure [::console show] is available.
#

if {![catch {package require tclreadline}]} {
    proc ::console method {
	if {[info exists ::tclreadline::_in_loop]} return
	set ::tclreadline::_in_loop 1
	switch -- $method {
	    show {
		after idle tclreadline::Loop
	    }
	    default {
		error "Not implemented: console $method"
	    }
	}
    }
} else {
    namespace eval ::hv3::tinyconsole {
	namespace export console

	variable buffer ""
	proc NS args { namespace code $args }
	proc K {x y} { set x }

	variable prompt "\n% "

	proc prompt {} {
	    variable prompt
	    puts -nonewline stdout [subst $prompt]
	}

	proc console {method args} {
	    switch -- $method {
		show {
		    prompt
		    set chan stdin
		    fileevent $chan readable [NS listener $chan]
		}
		default {
		    error "Not implemented: console $method"
		}
	    }
	}

	proc listener chan {
	    append buffer [gets $chan]
	    if {[info complete $buffer]} {
		puts -nonewline [uplevel #0 [K $buffer [set buffer ""]]]
		prompt
	    }
	}
    }

    namespace import ::hv3::tinyconsole::console
}
