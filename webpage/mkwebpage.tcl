#!/usr/bin/tclsh
#
# Construct the web page for tkhtml
#
# @(#) $Id: mkwebpage.tcl,v 1.10 2005/03/11 14:03:31 danielk1977 Exp $
#

proc header {} {
  puts {
    <html>
    <head>
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
  puts $::TITLE
  puts {<div id="links"><ul>}
  puts $::LINKS
  puts {</ul></div>}
  puts $::BODY
  footer
}
###########################################################################
# Document content is below this line.

h 1 {An HTML Widget For Tcl/Tk}
append ::TITLE "<p><i>Last update: [clock format [clock seconds]]</i></p>"

p {
  "Tkhtml" is a Tcl/Tk widget that displays HTML. Tkhtml is implemented in C.
  It is a true widget, not a metawidget implemented using the Text or Canvas
  widgets of the Tcl/Tk core. The current version of Tkhtml 2.0, which has
  not changed significantly for several years. This page houses that version
  and is also home to the (currently embryonic) plans for a revitalisation.
}

p {
  Tkhtml was created by D. Richard Hipp, and has since been enhanced by 
  Peter MacDonald.
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
     for applets, scripts and forms via callbacks.
</ul>
}

p { 
  Tkhtml 2.0 can be used with Tcl/Tk8.0 or later. Shared libraries use 
  the TCL stubs mechanism, so you should be able to load Tkhtml with 
  any version of "wish" beginning with 8.0.6.
}

h 2 {Revitalisation Effort}

p { 
  The current plan is to upgrade and modernise Tkhtml over the coming 
  months. Some of the main goals of this are:
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
  To obtain the source code for Tkhtml, use anonymous CVS (See 
  <a href="http://www.cyclic.com/">http://www.cyclic.com/</a> for 
  additional information on CVS.) Login as follows:
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
  This command creates a directory named "<tt>htmlwidget</tt>" and
  fills it with the latest version of the sources.
}

h 2 {Binaries}

p {
  The following binaries are available. To quickly test the capabilities of 
  the widget on Linux or Windows x86, download and run one of the executable 
  versions of the "Html Viewer" app.  The viewer app comes with a few 
  built-in documents, or can load a document from the local file-system.
}

append ::BODY <ul>
foreach {file desc} {
  tkhtml.tar.gz   {A tarball containing all the latest source code}
  hv.tcl          {The "Html Viewer" example application}
  spec.html       {A raw specification of how the tkhtml widget works}
  tkhtml.so       {Shared library suitable for use on Linux}
  tkhtml.dll      {A DLL suitable for use on Windows95/98/NT/2K}
  hv.exe           {Executable version of the "Html Viewer" app for Windows}
  hv.linux-x86-xft {Executable version of the "Html Viewer" app for Linux}
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

