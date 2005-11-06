
proc image_savefile {HTML} {
  set t [time {set img [$HTML layout image]}]
  # puts "IMAGE-TIME: $t"
  $img write tkhtml.gif
  image delete $img
}

proc image_savetest {HTML} {
  set url [$HTML var url]
  puts "SAVING $url..."

  catch {
    [.html var cache] eval {CREATE TABLE tests(url PRIMARY KEY, data BLOB);}
  }
  set t [time {set img [$HTML layout image]}]
  # puts "IMAGE-TIME: $t"

  set data [$img data -format ppm]
  image delete $img
  [$HTML var cache] eval {REPLACE INTO tests VALUES($url, $data);}
  puts "DONE"
}

proc image_runtests {HTML} {
  set db [.html var cache]
  $db eval {SELECT oid as id, url, data FROM tests} v {
    gui_goto $v(url)
    after 100 {set ::hv3_runtests_var 0}
    vwait ::hv3_runtests_var

    set t [time {set img [$HTML layout image]}]
    # puts "IMAGE-TIME: ($url) $t"
    set newdata [$img data -format ppm]
    image delete $img

    if {$data != $newdata} {
      puts "TEST FAILURE: ($url) -> $v(id).ppm"
      set fd [open $id.ppm w]
      fconfigure $fd -encoding binary
      fconfigure $fd -translation binary
      puts -nonewline $fd $newdata
      close $fd
    } else {
        puts "TEST SUCCESSFUL: ($v(url)) $v(id)"
    }
  }

  puts "RUNTESTS FINISHED"
}

proc image_800x600 {} {
    wm geometry . 800x600
}

