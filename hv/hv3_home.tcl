namespace eval hv3 { set {version($Id: hv3_home.tcl,v 1.7 2006/11/22 07:34:24 danielk1977 Exp $)} 1 }

# Register the about: scheme handler with ::hv3::protocol $protocol.
#
proc ::hv3::about_scheme_init {protocol} {
  set dir $::hv3::maindir
  $protocol schemehandler about [list ::hv3::about_request]
}

proc ::hv3::about_request {downloadHandle} {
  set tkhtml_version [::tkhtml::version]
  set hv3_version ""
  foreach version [array names ::hv3::version] {
    set t [string trim [string range $version 4 end-1]]
    append hv3_version "$t\n"
  }

  set html [subst {
    <html> <head> </head> <body>
    <h1>Tkhtml Source Code Versions</h1>
    <pre>$tkhtml_version</pre>
    <h1>Hv3 Source Code Versions</h1>
    <pre>$hv3_version</pre>
    </body> </html>
  }]

  $downloadHandle append $html
  $downloadHandle finish
}

# Register the home: scheme handler with ::hv3::protocol $protocol.
#
proc ::hv3::home_scheme_init {hv3 protocol} {
  set dir $::hv3::maindir
  $protocol schemehandler home [list ::hv3::home_request $protocol $hv3 $dir]
}

# When a URI with the scheme "home:" is requested, this proc is invoked.
#
proc ::hv3::home_request {http hv3 dir downloadHandle} {
  set fname [string range [$downloadHandle cget -uri] 8 end]
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
  return
  set html [$hv3 html]

  foreach node [$html search {span[widget]}] {
    switch [$node attr widget] {
      radio_noproxy {
        set widget [radiobutton ${html}.radio_noproxy]
        $widget configure -variable ::hv3_home_radio -value noproxy
      }
      radio_configured_proxy {
        set widget [radiobutton ${html}.radio_configured_proxy]
        $widget configure -variable ::hv3_home_radio -value proxy
      }
      entry_host {
        set widget [entry ${html}.entry_host -textvar ::hv3_home_host]
        bind $widget <KeyPress>        ::hv3::home_entervalue
        bind $widget <KeyPress-Return> ::hv3::home_set_proxy
        lappend ::hv3::home_widgets $widget
      }
      entry_port {
        set widget [entry ${html}.entry_port -textvar ::hv3_home_port]
        bind $widget <KeyPress-Return> ::hv3::home_set_proxy
        lappend ::hv3::home_widgets $widget
      }
    }
    $node replace $widget                                  \
        -deletecmd    [list ::hv3::home_delwidget $widget] \
        -configurecmd [list ::hv3::home_configure $widget]
  }
  ::hv3::home_configurewidgets
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

proc ::hv3::home_set_proxy {args} {
  switch $::hv3_home_radio {
    proxy {
      set val normal
      ::http::config -proxyhost $::hv3_home_host
      ::http::config -proxyport $::hv3_home_port
      set ::hv3::home_entervalue_color black
    }
    noproxy {
      ::http::config -proxyhost ""
      ::http::config -proxyport ""
    }
  }
  ::hv3::home_configurewidgets
  ::http::config -useragent {Mozilla/5.0 Gecko/20050513}
}

proc ::hv3::home_configurewidgets {} {
  set state normal
  if {$::hv3_home_radio eq "noproxy"} {set state disabled}
  foreach widget $::hv3::home_widgets {
    $widget configure -state $state -foreground $::hv3::home_entervalue_color
  }
}

proc ::hv3::home_entervalue {args} {
  if {$::hv3::home_entervalue_color eq "red"} return
  set ::hv3::home_entervalue_color red
  foreach widget $::hv3::home_widgets {
    $widget configure -foreground $::hv3::home_entervalue_color
  }
}

proc ::hv3::home_delwidget {widget} {
  set idx [lsearch $::hv3::home_widgets $widget]
  if {$idx >= 0} {
    set ::hv3::home_widgets [lreplace $::hv3::home_widgets $idx $idx]
  }
  destroy $widget
}

set ::hv3::home_widgets [list]
set ::hv3::home_entervalue_color black
set ::hv3_home_radio proxy
set ::hv3_home_host localhost
set ::hv3_home_port 8123

# ::hv3::home_set_proxy
# trace add variable ::hv3_home_radio write ::hv3::home_set_proxy

