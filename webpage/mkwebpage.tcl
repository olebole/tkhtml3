#!/usr/bin/tclsh
#
# Construct the web page for tkhtml
#
# @(#) $Id: mkwebpage.tcl,v 1.14 2005/10/06 12:01:53 danielk1977 Exp $
#

proc header {} {
  puts {
    <html>
    <head>
    <style>

body {
    background: #EEEEFF;
}
#body {
    top: 2ex;
    right: 1ex;
    position: absolute;
    left: 23ex;
    padding: 0 3ex 0 3ex;
}

#toc {
    position: absolute;
    left: 1ex;
    top: 2ex;
    width: 20ex;
}
#toc ul {
    margin: 1ex;
    padding: 0;
}
#toc li {
    display: block;
    margin: 1em 0;
    padding: 0;
}
#text {
    padding: 3ex;
}
#toc,#text {
    border: solid 1px;
    background: #DDDDFF;
}

h1,h2 {
    background: #000088;
    color: white;
    padding-left: 2ex;
}

h1 {
    margin-bottom: 0;
    margin-top: 0;
}
h2 {
    margin-top: 2em;
}

    </style>
    <title>An HTML Widget For Tk</title>
    </head>
    <body>
  }
}

proc footer {} {
  puts {
    </body>
    </html>
  }
}

set ::TITLE {}   ;# The title
set ::LINKS {}   ;# Links are added to this variable.
set ::BODY {}    ;# Body of html document is built up in this variable

proc p {text} {
  append ::BODY <p>
  append ::BODY $text
  append ::BODY </p>
}

proc link {caption href} {
  append ::LINKS "<li><a href=\"#$href\">$caption</a></li>"
}

set ::H 0
proc h {level text} {
  set var ::BODY
  if {$level==1} {
    set var ::TITLE
  }

  append $var <h$level>
  if {$level==2} {
    set name "part[incr ::H]"
    append ::BODY "<a name=\"$name\"></a>"
    link $text $name
  }
  append $var $text
  append $var </h$level>
}

proc output_page {} {
  header
  puts {<div id="toc"><ul>}
  puts $::LINKS
  puts {</ul></div>}
  puts {<div id="body">}
  puts $::TITLE
  puts {<div id="text">}
  puts $::BODY
  puts {</div>}
  puts {</div>}
  footer
}
###########################################################################
# Document content is below this line.

h 1 {An HTML Widget For Tcl/Tk}
append ::BODY "<p><i>Last update: [clock format [clock seconds]]</i></p>"

p {
  "Tkhtml" is a Tcl/Tk widget that displays HTML. Tkhtml is implemented in C.
  It is a true widget, not a metawidget implemented using the Text or Canvas
  widgets of the Tcl/Tk core. The current version of Tkhtml 2.0, which has
  not changed significantly for several years. This page houses that version
  and is also home to the new version, currently at 3.alpha-1.
}

p {
  Tkhtml was created by D. Richard Hipp, and has since been enhanced by 
  Peter MacDonald while working on his 
  <a href="http://www.browsex.com">browsex web browser</a>.
}

p {
   The changes for version 3 and in particular all of the work on style
   sheets, has been done by Dan Kennedy. Dan has been able to work full-time
   on the project for several months thanks to the financial support of 
   <a href="http://www.eolas.com">Eolas Technologies, Inc.</a>.
}

h 2 {Current Status (Version 2.0)}

p {
  The current version of Tkhtml is more or less compatible with the HTML 3.2
  standard. The majority of documents on the web today are rendered 
  acceptably, however results are sometimes less attractive than when using 
  a more modern rendering engine, like Gecko or KHTML. This is particularly
  true when stylesheets are in use. Having said that, some advantages of
  Tkhtml over other similar widgets are:
<ul>
<li>It runs very fast and uses little memory.</li>
<li>It supports smooth scrolling.</li>
<li>It supports text wrap-around on images and tables.</li>
<li>It has a full implementation of tables. Complex pages (such as 
     <a href="http://www.scriptics.com/">http://www.scriptics.com/</a>)
     are displayed correctly.</li>
<li>It has an API that allows applications to provide configurable support 
     for applets, scripts and forms via callbacks.</li>
</ul>
}

p { 
  Tkhtml 2.0 can be used with Tcl/Tk8.0 or later. Shared libraries use 
  the TCL stubs mechanism, so you should be able to load Tkhtml with 
  any version of "wish" beginning with 8.0.6.
}

h 2 {Revitalisation Status (Version 3)}

p {
  This is currently at alpha stage. The rendering engine is not yet feature
  complete by any means, but the majority of common HTML and CSS 1.0
  constructs are supported. See 
  <a href="http://tkhtml.tcl.tk/cvstrac/wiki?p=CssOne">this page</a> for a
  list of know defects. Nothing dynamic is supported yet (i.e. changing
  the color of something when the mouse floats over it etc.).
}

p {
  The widget itself also needs some work, there is no selection support and
  the widget doesn't support many of the standard options (for example
  -pady or -borderwidth).
}

p { 
  The current plan is to continue to upgrade and modernise Tkhtml over the
  coming months. Some of the main goals of this are:
<ul>
<li>Support (where applicable) for the HTML 4.01, XHTML 1.1 and CSS 2.1 
    standards.</li>
<li>To provide interfaces functionally equivalent to the W3C DOM interfaces
    (so that a full DOM implementation could be built on top of them).</li>
<li>Modernisation of the internals of the widget to take advantage of 
    Tcl and Tk APIs introduced since the current code was written. </li>
<li>Eventual inclusion in the Tk core.</li>
</ul>
}

p {
  More detail is available in the <a href="requirements.html">
  Tkhtml revitalization requirements draft</a>.
}

p {
  Being an open-source project, if you have some time to spare you can help!
  For example, if you wanted to you could volunteer to:
<ul>
<li>Assist with programming the C code for the widget.</li>
<li>Write a TCL application to help test the code as it evolves.</li>
<li>Help with designing and reviewing the new interface.</li>
<li>Improve internal or external documentation.</li>
<li>Create a better design for this webpage</li>
<li>Help with the interpretation of the HTML/CSS specifications (it would 
    be great to have someone who knows these well to act as a consultant,
    even if your time is very limited).</li>
</ul>
}

p {
  Any help you can provide will be gratefully received. Send email to 
  one of the contacts below if you are interested. Don't be shy!
}

h 2 {Contacts/Support}

p {
  You can view a log of changes, create trouble tickets, or read or
  enter Wiki about TkHTML by visiting the CVSTrac server at:
  <blockquote>
  <a href="http://tkhtml.tcl.tk/cvstrac">http://tkhtml.tcl.tk/cvstrac</a> 
  </blockquote>
}

p {
  There is a mailing list hosted by Yahoo groups. You can sign up
  or review the archive at:
  <blockquote>
    <a href="http://groups.yahoo.com/group/tkhtml/">
    http://groups.yahoo.com/group/tkhtml/</a>
  </blockquote>
}

p {
  If you want to help, then you can send mail to one of the following 
  contacts (who are both subscribed to and read the mailing list).
  <blockquote>
  <a href="mailto:danielk1977@yahoo.com">danielk1977@yahoo.com</a> (Dan)
  <br>
  <a href="mailto:drh@hwaci.com">drh@hwaci.com</a> (Richard)
  </blockquote>
}

h 2 {Source Code}

p {
  Version 3.alpha-1 source code is available 
  <a href="tkhtml-3.alpha-1.tar.gz">here</a>.
}

p {
  Alternatively, use anonymous CVS (See <a href="http://www.cyclic.com/">
  http://www.cyclic.com/</a> for additional information on CVS.). This is
  the only way to get version 2. Login as follows: 
}

p {
  <blockquote><pre>
cvs -d :pserver:anonymous@tkhtml.tcl.tk:/tkhtml login
  </pre></blockquote>
}

p {
  You will be prompted for a password.  Use "<tt>anonymous</tt>".  After
  you get logged in successfully, you can check out the source tree
  like this:
}

p {
  <blockquote><pre>
cvs -d :pserver:anonymous@tkhtml.tcl.tk:/tkhtml checkout htmlwidget
  </pre></blockquote>
}

p {
  <b>Update:</b> The above command is used to get the latest copy of
  the sources from cvs (version 3). But recent changes haven't been tested
  well enough for use in the wild yet. To obtain a more reliable copy, use:
}
p {
  <blockquote><pre>
cvs -d :pserver:anonymous@tkhtml.tcl.tk:/tkhtml checkout -D 2005-01-01 htmlwidget
  </pre></blockquote>
}

p {
  This command creates a directory named "<tt>htmlwidget</tt>" and
  fills it with the latest version of the sources.
}

h 2 {Binaries}

p {
  The following binaries are available. To quickly test the capabilities of 
  the widget on Linux or Windows x86, download and run one of the executable 
  versions of the "Html Viewer" app.  The viewer app comes with a few
  built-in demo documents, or can load a document from the local file-system.
}

append ::BODY <ul>
foreach {file desc} {
  tkhtml.tar.gz   {A tarball containing all the latest source code}
  hv.tcl          {The "Html Viewer" example application}
  spec.html       {A raw specification of how the tkhtml widget works}
  tkhtml.so       {Shared library suitable for use on Linux}
  tkhtml.dll      {A DLL suitable for use on Windows95/98/NT/2K}
  hv.exe           {Windows version of the "Html Viewer" version 2.0}
  hv.linux-x86-xft {Linux x86 version of the "Html Viewer" version 2.0}
  hv3.exe          {Windows version of the "Html Viewer" version 3.alpha-1}
  hv3-linux-x86    {Linux x86 version of the "Html Viewer" version 3.alpha-1}
} {
  if {![file readable $file]} continue
  lappend SendList $file
  append ::BODY "<li><p><a href=\"$file\">$file</a><br>"
  append ::BODY "Description: $desc<br>"
  append ::BODY "Size: [file size $file] bytes<br>"
  append ::BODY "Last modified: [clock format [file mtime $file]]"
  if {![catch {exec strings $file | grep {$Id: }} ident]} {
    append ::BODY "<br>Version information:"
    append ::BODY "<pre>\n$ident</pre>"
  }
  append ::BODY "</p></li>\n"
}
append ::BODY </ul>

###########################################################################
output_page

