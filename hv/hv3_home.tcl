
proc ::hv3::home_scheme_init {hv3 protocol} {
  set dir [file dirname [info script]] 
  $protocol schemehandler home [list ::hv3::home_request $protocol $hv3 $dir]
}

# When a URI with the scheme "home:" is requested, this proc is invoked.
proc ::hv3::home_request {http hv3 dir downloadHandle} {
  set fname [string range [$downloadHandle uri] 8 end]
  if {$fname eq ""} {
      set fname index.html
      after idle [list ::hv3::home_after_idle $http $hv3]
  }
  set fd   [open [file join $dir $fname]]
  set data [read $fd]
  close $fd
  $downloadHandle append $data
  $downloadHandle finish
}

proc ::hv3::home_after_idle {http hv3} {
  trace remove variable ::hv3_home_radio write [list ::hv3::home_set_proxy $http $hv3]
  trace remove variable ::hv3_home_port  write [list ::hv3::home_set_proxy $http $hv3]
  trace remove variable ::hv3_home_proxy write [list ::hv3::home_set_proxy $http $hv3]

  set html [$hv3 html]
  set ::hv3_home_host [$http cget -proxyhost]
  set ::hv3_home_port [$http cget -proxyport]

  foreach node [$html search {span[widget]}] {
    switch [$node attr widget] {
      radio_noproxy {
        set widget [radiobutton ${html}.radio_noproxy]
        $widget configure -variable ::hv3_home_radio -value 1
      }
      radio_configured_proxy {
        set widget [radiobutton ${html}.radio_configured_proxy]
        $widget configure -variable ::hv3_home_radio -value 2
      }
      entry_host {
        set widget [entry ${html}.entry_host -textvar ::hv3_home_host]
      }
      entry_port {
        set widget [entry ${html}.entry_port -textvar ::hv3_home_port]
      }
    }
    $node replace $widget                               \
        -deletecmd [list destroy $widget]               \
        -configurecmd [list ::hv3::home_configure $widget]
  }
 
  set val 2
  if {$::hv3_home_host eq "" && $::hv3_home_port eq ""} {
    set val 1
    set ::hv3_home_host localhost
    set ::hv3_home_port 8123
  }

  trace add variable ::hv3_home_radio write [list ::hv3::home_set_proxy $http $hv3]
  trace add variable ::hv3_home_port  write [list ::hv3::home_set_proxy $http $hv3]
  trace add variable ::hv3_home_proxy write [list ::hv3::home_set_proxy $http $hv3]

  set ::hv3_home_radio $val
}

proc ::hv3::home_configure {widget values} {
  array set v $values
  set class [winfo class $widget]

  if {$class eq "Checkbutton" || $class eq "Radiobutton"} {
    catch { $widget configure -background          $v(background-color) }
    catch { $widget configure -highlightbackground $v(background-color) }
    catch { $widget configure -activebackground    $v(background-color) }
    catch { $widget configure -highlightcolor      $v(background-color) }
    $widget configure -padx 0 -pady 0
  }
  catch { $widget configure -font $v(font) }

  $widget configure -borderwidth 0
  $widget configure -highlightthickness 0
  catch { $widget configure -selectborderwidth 0 } 

  set font [$widget cget -font]
  set descent [font metrics $font -descent]
  set ascent  [font metrics $font -ascent]
  set drop [expr ([winfo reqheight $widget] + $descent - $ascent) / 2]
  return $drop
}

proc ::hv3::home_set_proxy {http hv3 args} {
  switch $::hv3_home_radio {
    1 {
      $http configure -proxyhost "" -proxyport ""
      set val disabled
    }
    2 {
      $http configure -proxyhost $::hv3_home_host -proxyport $::hv3_home_port
      set val normal
    }
  }

  set html [$hv3 html]
  foreach widget [list ${html}.entry_host ${html}.entry_port] {
    $widget configure -state $val
  }
}

