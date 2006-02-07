#!/usr/bin/tclsh
#
# Construct the web page for tkhtml
#
# @(#) $Id: mkwebpage.tcl,v 1.18 2006/02/07 13:31:31 danielk1977 Exp $
#

source [file join [file dirname [info script]] common.tcl]

proc header {} {
  puts {
    <html>
    <head>
    <link rel="stylesheet" href="tkhtml_tcl_tk.css">
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
set ::BODY {}    ;# Body of html document is built up in this variable

proc p {text} {
  append ::BODY <p>
  append ::BODY $text
  append ::BODY </p>
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
    addPageSection $text $name
  }
  append $var $text
  append $var </h$level>
}

proc output_page {} {
  header

  puts [getSideBoxes]

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

p {
  "Tkhtml" is a Tcl/Tk widget that displays HTML. Tkhtml is implemented in C.
  It is a true widget, not a metawidget implemented using the Text or Canvas
  widgets of the Tcl/Tk core. 
}
p {
  There are two versions of Tkhtml, version 2.0, which has not changed
  significantly for several years, and version 3, currently still under
  development. Unless otherwise specified, all information on this site
  pertains to version 3. The interfaces supported by Tkhtml versions 2.0 
  and 3 are not at all similar.
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

h 2 {Current Status (Version 3)}

p {
  This is currently at alpha stage. The rendering engine is not yet feature
  complete by any means, but the majority of common HTML and CSS 1.0
  constructs are supported. See 
  <a href="http://tkhtml.tcl.tk/cvstrac/wiki?p=CssOne">this page</a> for a
  list of known defects. Nothing dynamic is supported yet (i.e. changing
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
  Binary distributions of Tkhtml version 2.0 are available as part of 
  <a href="http://www.activestate.com/Products/ActiveTcl/">ActiveTcl</a>.
}
p {
  Binary distributions of Tkhtml version 3.0 are available as part of 
  the <a href="hv3.html">hv3 demo application</a> starkit.
}
###########################################################################
output_page

