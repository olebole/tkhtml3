
proc FileToDefine {file define} {
  set fd [open $file]
  append ret "#define $define \\"
  while {![eof $fd]} {
      set line [string map [list \" \\\" \\ \\\\] [gets $fd]]
      append ret "\n        \"$line\\n\" \\"
  }
  append ret "\n\n\n"

  return $ret
}

proc VersionsToDefine {glob define} {
  set ret "#define $define \\\n"
  foreach file [glob $glob] {
    set fd [open $file]
    set contents [read $fd]
    close $fd
    if {[regexp {\$Id: mkdefaultstyle.tcl,v 1.3 2006/06/10 12:38:38 danielk1977 Exp $]*)\$} $contents dummy match]} {
      append ret "    \"[string trim $match]\\n\" \\\n"
    }
  }
  return $ret
}

set css_file    [file join [file dirname [info script]] .. tests html.css]
set tcl_file    [file join [file dirname [info script]] .. tests tkhtml.tcl]
set quirks_file [file join [file dirname [info script]] quirks.css]
set src_files   [file join [file dirname [info script]] {*.c}]

puts ""
puts [FileToDefine $tcl_file      HTML_DEFAULT_TCL]
puts [FileToDefine $css_file      HTML_DEFAULT_CSS]
puts [FileToDefine $quirks_file   HTML_DEFAULT_QUIRKS]
puts [VersionsToDefine $src_files HTML_SOURCE_FILES]

