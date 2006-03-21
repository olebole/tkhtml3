
catch { memory init on }

# Load packages.
if {[info exists auto_path]} {
    set auto_path [concat . $auto_path]
}
package require Tk
package require Tkhtml 3.0

# If possible, load package "Img". Without it the script can still run,
# but won't be able to load many image formats.
if {[catch { package require Img }]} {
  puts "WARNING: Failed to load package Img"
}

# Source the other script files that are part of this application.
#
proc sourcefile {file} {
  set fname [file join [file dirname [info script]] $file] 
  uplevel #0 [list source $fname]
}

sourcefile snit.tcl
sourcefile hv3_log.tcl
sourcefile hv3_prop.tcl
sourcefile hv3_form.tcl
sourcefile combobox.tcl
sourcefile hv3.tcl

#--------------------------------------------------------------------------
# gui_build --
#
#     This procedure is called once at the start of the script to build
#     the GUI used by the application. It assumes a hv3 object ".hv3"
#     has already been created, but not [pack]ed.
#
proc gui_build {} {
  global HTML

  frame .entry
  entry .entry.entry
  button .entry.clear -text {Clear ->} -command {.entry.entry delete 0 end}

  pack .entry.clear -side left
  pack .entry.entry -fill both -expand true
  pack .entry -fill x -side top 
  bind .entry.entry <KeyPress-Return> {hv3Goto .hv3 [.entry.entry get]}

  pack .hv3 -fill both -expand true
  focus .hv3.html

  # Build the main window menu.
  . config -menu [menu .m]
  .m add cascade -label {File} -menu [menu .m.file]
  .m.file add command -label "Open File..." -command guiOpenFile
  .m.file add command -label Back -command guiBack -state disabled
  .m.file add separator
  foreach f [list \
    [file join $::tcl_library .. .. bin tkcon] \
    [file join $::tcl_library .. .. bin tkcon.tcl]
  ] {
    if {[file exists $f]} {
      catch {
        uplevel #0 "source $f"
        package require tkcon
        .m.file add command -label Tkcon -command {tkcon show}
      }
      break
    }
  }
  .m.file add command -label Browser -command [list HtmlDebug::browse $HTML]
  .m.file add separator

  .m.file add command -label "Select All" -command gui_select_all

  .m.file add separator
  .m.file add command -label Exit -command exit

  # Add the 'Font Size Table' menu
  .m add cascade -label {Font Size Table} -menu [menu .m.font]
  foreach {label table} [list \
    Normal {7 8 9 10 12 14 16} \
    Large  {9 10 11 12 14 16 18} \
    {Very Large}  {11 12 13 14 16 18 20} \
    {Extra Large}  {13 14 15 16 18 20 22} \
    {Recklessly Large}  {15 16 17 18 20 22 24}
  ] {
    .m.font add command -label $label -command [list \
      $HTML configure -fonttable $table
    ]
  }
}

proc gui_select_all {} {
  set n [.hv3.html node]
  .hv3.html select from $n
  while {[$n nChildren] > 0} {
    set n [$n child [expr [$n nChildren]-1]]
  }
  .hv3.html select to $n
}

proc guiOpenFile {} {
  set f [tk_getOpenFile -filetypes [list \
      {{Html Files} {.html}} \
      {{Html Files} {.htm}}  \
      {{All Files} *}
  ]]
  if {$f != ""} {
    hv3Goto .hv3 file://$f 
  }
}

#--------------------------------------------------------------------------
# Implementation of File->Back command.
#
#     Proc guiGotoCallback is registered as the -gotocallback with hv3
#     object .hv3. It maintains a URL history for the browser session in
#     global variable ::hv3_history_list
#    
#     Proc guiBack is invoked when the "Back" command is issued.

proc guiGotoCallback {url} {
  lappend ::hv3_history_list $url
  .entry.entry delete 0 end
  .entry.entry insert 0 $url
  if {[llength $::hv3_history_list]>1} {
    .m.file entryconfigure Back -state normal
  }
}

proc guiBack {} {
  if {[llength $::hv3_history_list]>1} {
    set newurl [lindex $::hv3_history_list end-1]
    set ::hv3_history_list [lrange $::hv3_history_list 0 end-2]
    hv3Goto .hv3 $newurl
  }
  if {[llength $::hv3_history_list]<=1} {
    .m.file entryconfigure Back -state disabled
  }
}

# End of implementation of File->Back command.
#--------------------------------------------------------------------------

snit::type Hv3HttpProtcol {

  option -proxyport -default 3128      -configuremethod _ConfigureProxy
  option -proxyhost -default localhost -configuremethod _ConfigureProxy

  variable myCookies -array [list]

  constructor {} {
    package require http
    $self _ConfigureProxy
  }

  method download {downloadHandle} {
    set uri [$downloadHandle uri]
    set finish [mymethod _DownloadCallback $downloadHandle]
    set append [mymethod _AppendCallback $downloadHandle]

    set headers ""
    set authority [$downloadHandle authority]
    if {[info exists myCookies($authority)]} {
      set headers "Cookie "
      foreach cookie $myCookies($authority) {
        lappend headers $cookie
      }
    }

    ::http::geturl $uri -command $finish -handler $append -headers $headers
  }

  # Configure the http package to use a proxy as specified by
  # the -proxyhost and -proxyport options on this object.
  #
  method _ConfigureProxy {} {
    ::http::config -proxyhost $options(-proxyhost)
    ::http::config -proxyport $options(-proxyport)
  }

  # Invoked when data is available from an http request. Pass the data
  # along to hv3 via the downloadHandle.
  #
  method _AppendCallback {downloadHandle socket token} {
    upvar \#0 $token state 
    set data [read $socket 2048]
    $downloadHandle append $data
    set nbytes [string length $data]
    return $nbytes
  }

  # Invoked when an http request has concluded.
  #
  method _DownloadCallback {downloadHandle token} {
    upvar \#0 $token state 

    if {[info exists state(meta)]} {
      foreach {name value} $state(meta) {
        if {$name eq "Set-Cookie"} {
          puts "COOKIE: $value"
          regexp {^[^ ]*} $value nv_pair
          lappend myCookies([$downloadHandle authority]) $nv_pair
        }
      }
      foreach {name value} $state(meta) {
        if {$name eq "Location"} {
          puts "REDIRECT: $value"
          $downloadHandle redirect $value
          $self download $downloadHandle
          return
        }
      }
    } 

    $downloadHandle append $state(body)
    $downloadHandle finish
  }
}

#
# Override the [exit] command to check if the widget code leaked memory
# or not before exiting.
#
rename exit tcl_exit
proc exit {args} {
  global HTML
  destroy $HTML 
  catch {destroy .prop$HTML}
  catch {::tkhtml::htmlalloc}
#  if {[llength [form_widget_list]] > 0} {
#    puts "Leaked widgets: [form_widget_list]"
#  }
puts [array names ::HtmlForm::instance]
  eval [concat tcl_exit $args]
}

# main URL
#
proc main {{doc index.html}} {
  hv3Init .hv3 -gotocallback guiGotoCallback

  set http_obj [Hv3HttpProtcol create %AUTO%]
  hv3RegisterProtocol .hv3 http [list $http_obj download]
  # hv3RegisterProtocol .hv3 http httpProtocol

  set ::HTML .hv3.html
  bind $::HTML <KeyPress-q> exit
  bind $::HTML <KeyPress-Q> exit
  gui_build
  log_init .hv3.html
  form_init .hv3.html [list hv3Goto .hv3]
  # set ::html_log_timer(LAYOUT) 1
  # set ::html_log_timer(STYLE) 1
  # set ::html_log_log(CALLBACK) 1
  hv3Goto .hv3 $doc
}
eval [concat main $argv]

