
namespace eval hv3 { set {version($Id: hv3_polipo.tcl,v 1.7 2006/09/14 15:50:57 danielk1977 Exp $)} 1 }

# This file contains code to control a single instance of the 
# external program "hv3_polipo" that may be optionally used by
# hv3 as a web proxy.
#
# hv3_polipo is a slightly modified version of "polipo" by 
# (TODO: Juliusz full name) (TODO: polipo website).
#

namespace eval ::hv3::polipo {
  variable g

  # The port to listen on.
  set g(port) 8123

  # The path to the polipo binary.
  set g(binary) ""

  # The file-handle to the polipo's stdout. 
  set g(filehandle) ""

  # The file-handle to the polipo keepalive connection
  set g(keepalive) ""

  # Text of the log.
  set g(log) ""

  proc print {string} {
    variable g
    append g(log) $string
    if {[winfo exists .polipo]} {
      .polipo.text insert end $string
    }
  }

  # Initialise this sub-system. This proc sets the g(binary) variable -
  # the path to "polipo".
  #
  proc init {} {
    variable g

    set dir [file dirname [file normalize [info script]]]
    set prog hv3_polipo
    if {$::tcl_platform(platform) eq "windows"} {
      set prog hv3_polipo.exe
    }

    set locations [list $dir [file dirname $dir] [pwd]]
    catch {
      set locations [concat $locations [split $::env(PATH) :]]
    }
    foreach loc $locations {
      set g(binary) [file join $loc $prog]
      if {[file executable $g(binary)]} {
        print "Using binary \"$g(binary)\"\n"
        break
      }
      set g(binary) ""
    }
  }

  # Popup the gui log window.    
  proc popup {} {
    variable g
    if {![winfo exists .polipo]} {
      toplevel .polipo

      ::hv3::scrolled ::hv3::text .polipo.text -width 400 -height 250
      ::hv3::button .polipo.button -text "Restart Polipo" 

      pack .polipo.button -side bottom -fill x
      pack .polipo.text -fill both -expand true
      .polipo.button configure -command [namespace code restart]

      if {[string length $g(log)] > 10240} {
        set g(log) [string range $g(log) end-5120 end]
      }
      .polipo.text insert end $g(log)
    }
    raise .polipo
  }

  proc polipo_stdout {} {
    variable g
    if {[eof $g(filehandle)]} {
      set s "ERROR: Polipo failed."
      stop
      popup
    } else {
      set s [gets $g(filehandle)]
      if {$g(keepalive) eq "" && [scan $s "polipo port is %d" g(port)] == 1} {
        set g(keepalive) ""
        set g(keepalive) [socket localhost $g(port)]
      }
    }
    if {$s ne ""} { print "$s\n" }
  }

  # Stop any running polipo instance. If polipo is not running, this
  # proc is a no-op.
  proc stop {} {
    variable g
    catch {close $g(filehandle)}
    catch {close $g(keepalive)}
    set g(filehandle) ""
    set g(keepalive) ""
  }

  # (Re)start the polipo process. This proc blocks until polipo has
  # been successfully (re)started.
  proc restart {} {
    variable g

    # Make sure any previous polipo instance is finished.
    stop

    if {$g(binary) eq ""} {
      print "ERROR: No hv3_polipo binary found.\n"
      return
    }

    # Kick off polipo.
    set cmd "|{$g(binary)} dontCacheRedirects=true"
    if {$::tcl_platform(platform) eq "unix"} {
      append cmd " |& cat"
      # set cmd "|{$g(binary)} diskCacheRoot=/home/dan/cache |& cat"
    }
    set fd [open $cmd r]
    fconfigure $fd -blocking 0
    fconfigure $fd -buffering none
    fileevent $fd readable [namespace code polipo_stdout]

    # Wait until the keepalive connection is established.
    set g(filehandle) $fd
    if {$g(keepalive) eq ""} {
      vwait ::hv3::polipo::g(keepalive)
    }

    # Log a fun and friendly message.
    if {$g(keepalive) ne ""} {
      print "INFO:  Polipo (re)started successfully.\n"
      catch {
        ::http::config -proxyhost 127.0.0.1
        ::http::config -proxyport $g(port)
      }
    } 
  }
}

::hv3::polipo::init
::hv3::polipo::restart
#::hv3::polipo::popup

