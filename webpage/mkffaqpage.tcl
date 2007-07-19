
source [file join [file dirname [info script]] common.tcl]

addPageSection "Download"            download
addPageSection "Source Code/Hacking" source
addPageSection "More Information"    info

proc VERSION {} {
  if {[info exists ::env(VERSION)]} {return $::env(VERSION)}
  return "alpha-14"
}

set idx 1

proc Q {id Question Answer} {
  global idx

  append ::BODY "<h2 id=\"$id\">${idx}. $Question</h2>"
  append ::BODY $Answer
  append ::TOC "<li>${idx}. <a href=\"#${id}\">$Question</a>"
  incr idx
}

Q hv3_polipo {What is this hv3_polipo?} {
  <p>
    hv3_polipo is a very slightly modified version of the standard
    polipo program by Juliusz Chroboczek, available at 
    <a href="http://www.pps.jussieu.fr/~jch/software/polipo/">
    http://www.pps.jussieu.fr/~jch/software/polipo/</a>. The modifications
    are designed to make sure that no hv3_polipo processes are
    left running if hv3 crashes or is terminated by the operating system.
    The patch used to create the custom version is available 
    <a href="hv3_polipo.patch">here</a>.
  </p>
  <p>
    If building Hv3 from source code, you probably want to obtain hv3_polipo
    as well. The starkit and other pre-built packages 
    <a href="hv3.html#download">available here</a>  already include 
    pre-compiled versions.
  </p>
}

puts [subst {

<html>
<head>
<link rel="stylesheet" href="tkhtml_tcl_tk.css">
<title>tkhtml.tcl.tk FFAQ</title>
</head>
<body>

[getTabs 4]

<div id="body">
<h1>tkhtml.tcl.tk FFAQ</h1>
<div id="text">
<ul style="list-style-type:none">
  $::TOC
</ul>
$::BODY

}]

