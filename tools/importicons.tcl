package require base64

set dir /Users/dan/tmp/tango-icon-theme-0.8.1/32x32/

set A(hv3_previmg)   actions/go-previous.png
set A(hv3_nextimg)   actions/go-next.png
set A(hv3_stopimg)   actions/process-stop.png
set A(hv3_newimg)    actions/tab-new.png
set A(hv3_reloadimg) actions/view-refresh.png
set A(hv3_homeimg)   actions/go-home.png
set A(hv3_bugimg)    actions/mail-message-new.png

puts "proc color_icons {} {"
foreach {key value} [array get A] {
  set fd [open [file join $dir $value]]
  fconfigure $fd -translation binary -encoding binary
  set data [read $fd]
  close $fd

  puts "image create photo $key -data {"
  puts [base64 -mode encode $data]
  puts "}"
}
puts "}"
