
set       ::aBrowser {Hv3 Mozilla}
array set ::aResult {}
array set ::anOutstanding {}
set       ::zStatus {}

set ::template {
HTTP/1.1 200 OK
Content-type: text/html
Cache-Control: no-cache

<HTML>
  <SCRIPT>
    function runtest () {
      var res = browsertest()

      var form = "<FORM action=\"/next\" method=\"GET\" id=\"testform\">"
      form += "<INPUT type=hidden name=\"testid\" value=\"%TESTID%\"></INPUT>"
      form += "<INPUT type=hidden name=\"result\" id=\"testresult\"></INPUT>"
      form += "</FORM>"

      document.body.innerHTML = form

      document.getElementById("testresult").value = res
      document.getElementById("testform").submit()
    }
  </SCRIPT>
<BODY onload="runtest()">

  %TESTBODY%

</BODY>
}

set ::template2 {
HTTP/1.1 200 OK
Content-type: text/html
Cache-Control: no-cache

<HTML>
<BODY> 

  Tests finished. <A href="/">Click here</A> to rerun.

</BODY>
}

proc log {args} {
  .text.t insert end "[join $args]\n"
  .text.t yview end
}

proc listen_for_connections {} {
  socket -server new_connection -myaddr 127.0.0.1 8080
}

proc new_connection {channel clientaddr clientport} {
  fconfigure $channel -translation crlf

  # Read the request line:
  set request [gets $channel]

  # Read HTTP headers until we figure out which browser this is.
  #
  while {[set line [gets $channel]] ne ""} {
    set idx [string first : $line]
    if {$idx > 0} {
      set hdr [string range $line 0 [expr {$idx-1}]]
      switch -exact -- [string tolower $hdr] {
        user-agent {
          foreach browser $::aBrowser {
            if {[string first $browser $line] >= 0} {
              log "Connection from $browser"
              set zBrowser $browser
              break
            }
          }
          # If we couldn't identify the browser, drop the connection.
          #
          if {![info exists zBrowser]} {
            log $line
            log "Failed to identify browser. Disconnecting."
            close $channel
            return
          }
        }
      }
    }
    # log $line
  }

  set zPath [lindex [split $request " "] 1]
  if {$zPath eq "/"} {
    # Send the first test to the browser.
    #
    set ::anOutstanding($zBrowser) [llength $::tests]
    send_test $channel 0
    set_status
  } elseif {[string first /next $zPath] == 0} {
    set idx [string first ? $zPath]
    set fields [string range $zPath [expr {$idx+1}] end]
    
    foreach field [split $fields &] {
      foreach {name value} [split $field =] break
      set $name $value
    }

    set ::aResult($zBrowser,$testid) [::tkhtml::decode $result]
    log "$zBrowser,$testid  \"[::tkhtml::decode $result]\""

    send_test $channel [expr {$testid+1}]
    incr ::anOutstanding($zBrowser) -1
    set_status
  }

  close $channel
}

proc send_test {channel testid} {
  if {$testid == [llength $::tests]} {
    puts -nonewline $channel [string trim $::template2]
  } else {
    set map [list %TESTID% $testid %TESTBODY% [lindex $::tests $testid 1]]
    puts -nonewline $channel [string map $map [string trim $::template]]
  }
}

proc setup_gui {} {
  frame  .gotos
  button .gotos.hv3 -text "Signal Hv3" -command [list send hv3_main.tcl {
    [gui_current hv3] goto http://localhost:8080/ -cachecontrol no-cache
  }]
  button .gotos.firefox -text "Signal Firefox" -command [list exec \
    firefox -remote "openurl(http://localhost:8080/,new-tab)"
  ]
  pack .gotos.hv3 -side left
  pack .gotos.firefox -side left

  frame     .text
  text      .text.t
  scrollbar .text.s -orient vertical
  .text.t configure -yscrollcommand [list .text.s set] 
  .text.s configure -command        [list .text.t yview] 

  frame  .buttons
  button .buttons.quit   -command press_quit   -text "Quit"
  button .buttons.report -command press_report -text "Report"
  button .buttons.clear  -command [list .text.t delete 0.0 end] -text "Clear"
  button .buttons.reload -command press_reload -text "Reload"
  label  .buttons.status -textvariable ::zStatus

  pack .buttons.quit -side left
  pack .buttons.report -side left
  pack .buttons.clear -side left
  pack .buttons.reload -side right
  pack .buttons.status -side left -fill x

  pack .text.t -side left -fill both -expand true
  pack .text.s -side left -fill y

  pack .gotos -side top -fill x
  pack .buttons -side bottom -fill x
  pack .text -side top -fill both -expand true
}

proc press_quit {} {
  exit
}

proc press_report {} {
  .text.t delete 0.0 end
  set nMatch 0

  for {set ii 0} {$ii < [llength $::tests]} {incr ii} {

    set result_list [list]
    foreach {k v} [array get ::aResult "*,$ii"] {lappend result_list $v}
    set result_list [lsort $result_list]
    if {
      [llength $result_list] != [llength $::aBrowser] ||
      [lindex $result_list 0] ne [lindex $result_list end]
    } {
      log "Test [lindex $::tests $ii 0] Failed: "
      foreach browser $::aBrowser {
        set res NR
        if {[info exists ::aResult($browser,$ii)]} {
          set res "\"$::aResult($browser,$ii)\""
        }
        log [format "    % -10s %s" "$browser:" $res]
      }
    } else {
      incr nMatch
    }
  }

  log ""
  log "$nMatch tests were successful"
  log "[expr [llength $::tests] - $nMatch] tests failed"
}

proc set_status {} {
  set z "Outstanding requests: "
  foreach browser $::aBrowser {
    if {![info exists ::anOutstanding($browser)]} {
      set ::anOutstanding($browser) [llength $::tests]
    }
    append z "$browser - $::anOutstanding($browser)    "
  }
  set ::zStatus $z
}

set ::tests [list]
proc browsertest {name code} {
  lappend ::tests [list $name $code]
}
proc do_browser_test {name args} {

  # Argument processing:
  #
  set opts(-html)     ""
  array set opts $args
  if {![info exists opts(-javascript)]} {
    error "Missing mandatory -javascript option"
  }
  foreach option [array names opts] {
    switch -- $option {
      -browsers     {}
      -timeout      {}
      -html         {}
      -javascript   {}
      -expected     {}
      default {
        error "Unknown option: $option"
      }
    }
  }

  browsertest $name "
    <SCRIPT>
      function browsertest () { $opts(-javascript) }
    </SCRIPT>
    $opts(-html)
  "
}

setup_gui

proc press_reload {} {
  .text.t delete 0.0 end
  array unset ::anOutstanding
  array unset ::aResult
  set ::tests [list]
  foreach zFile $::argv {
    set nTest [llength $::tests]
    source $zFile
    log "Loaded [expr [llength $::tests]-$nTest] from $zFile"
  }

  log "Loaded 2 internal warmbody tests."
  do_browser_test warmbody-1 -javascript { return "hello" }
  do_browser_test warmbody-2 -javascript { return "world" }

  set_status
}

press_reload
listen_for_connections

