
source [file join [file dirname [info script]] common.tcl]

addPageSection "Download"            download
addPageSection "Source Code/Hacking" source
addPageSection "More Information"    info

proc VERSION {} {
  if {[info exists ::env(VERSION)]} {return $::env(VERSION)}
  return "alpha-15"
}

set idx 1

proc Q {id Question Answer} {
  global idx

  append ::BODY "<h2 id=\"$id\">${idx}. $Question</h2>"
  append ::BODY $Answer
  append ::TOC "<li>${idx}. <a href=\"#${id}\">$Question</a>"
  incr idx
}

Q statefile {How can I save my options/cookies/history-list etc.?} {
  <p>
    By default, Hv3 saves absolutely no data to the file system.
    After the Hv3 window has been closed, it is not possible for anyone else
    using the same computer to discover which websites you have visited or
    logged in to. 
  </p>
  <p>
    However, although this preserves your privacy, sometimes it is not
    conveniant. If a "state file" is enabled, Hv3 stores the following 
    data to a file on disk: 

  <ul>
  <li> HTTP cookies.
  <li> The list of visited URIs (used for auto-completion in the location 
       bar and for coloring visited hyperlinks).
  <li> The values of the settings configured in the "Options" pull-down menu,
       except for the "Hide GUI" option.
  <li> User bookmarks.
  </ul>
<p>
  To use a state file, specify the "-statefile" option as part of the command
  line used to start Hv3. The -statefile option is used to specify a file on
  disk used to persistently store various elements of the browser application
  state. Windows users may need to create a "batch file" to achieve this. For
  example, assuming that the Hv3 binary is named
  "hv3-linux-nightly-07_0723" and you wish to use the file
  "/home/dan/hv3_state.db" as the statefile, the full command line would be:
</p>

<pre>
    hv3-linux-nightly-07_0723 -statefile /home/dan/hv3_state.db
</pre>
<p>
  Because the state file is actually an <a href="http://www.sqlite.org">
  SQLite</a> database, there is no problem with two or more Hv3 processes
  using the same state file simultaneously. Bookmarked and configuration
  settings are propagated between instances automatically.
</p>

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

