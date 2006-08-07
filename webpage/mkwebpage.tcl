#!/usr/bin/tclsh
#
# Construct the web page for tkhtml
#
# @(#) $Id: mkwebpage.tcl,v 1.28 2006/08/07 12:49:07 danielk1977 Exp $
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

  puts [getTabs 0]

  puts {<div id="body">}
  puts $::TITLE
  puts [getToc]
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

  <p style="font-weight:bold; font-style:italic; text-align:center">
  <a href="hv3.html">
  Download a binary build of Tkhtml's web browser application, Hv3.
  </a>
  </p>
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
<ul style="list-style-position: inside">
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

h 2 {Current Status (Version 3)}

p {
  <a href="screenshot_acid2b.gif">
    <img align=right class=screenshot src="screenshot_acid2b_small.gif">
  </a>
  <a href="screenshot_acid2a.gif">
    <img align=right class=screenshot src=screenshot_acid2a_small.gif>
  </a>

  There is now an alpha release available for download in 
  <a href="#part4">source code form</a>. Nightly binary builds are 
  available as part of the starkit builds of the demo application, 
  <a href="hv3.html">hv3</a>.
}
p {
  At this stage both the alpha release and nightly builds are undoubtably still
  full of bugs. But it compiles and runs on both windows and linux, and the
  code and <a href="tkhtml.html">the widget documentation</a> have converged.
  The rendering engine is not yet feature complete by any means, but the
  majority of common HTML and CSS 2.1 constructs are supported. We hold
  our own against the 
  <a href="http://www.webstandards.org/files/acid2/test.html">
  acid2 test</a>. See <a href="support.html">this page</a> for a
  comparison of current capabilities against the CSS 2.1 specification.  
}
p {
  Please help by testing the alpha release and filing bug tickets 
  at the Tkhtml cvstrac site: <a href="http://tkhtml.tcl.tk/cvstrac/">
  http://tkhtml.tcl.tk/cvstrac/</a>.
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

p {
  Binary distributions of Tkhtml version 2.0 are available as part of 
  <a href="http://www.activestate.com/Products/ActiveTcl/">ActiveTcl</a>.
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


###########################################################################
h 2 {Source Code}

p {
  The current release of Tkhtml 3.0 is "alpha release 8", available for
  download <a href="tkhtml3-alpha-8.tar.gz">here</a>.
}

p {
  Alternatively, source code is always available via anonymous CVS (See 
  <a href="http://www.cyclic.com/">http://www.cyclic.com/</a> 
  for additional information on CVS.).  The following procedure creates a
  directory named "<tt>htmlwidget</tt>" and fills it with the latest 
  version of the sources:
}

p {
    <b>1.</b> Log in with the following command:
    <blockquote><pre>
  cvs -d :pserver:anonymous@tkhtml.tcl.tk:/tkhtml login
    </pre></blockquote>
}
  
p {
    <b>2.</b> You will be prompted for a password. Use "<tt>anonymous</tt>".
}
  
p {
    <b>3a.</b> Obtain the lastest version 3 source code:
    <blockquote><pre>
  cvs -d :pserver:anonymous@tkhtml.tcl.tk:/tkhtml checkout htmlwidget
    </pre></blockquote>
  
    <b>3b.</b> Or the version 2 source code:
    <blockquote><pre>
  cvs -d :pserver:anonymous@tkhtml.tcl.tk:/tkhtml checkout -D 2005-01-01 htmlwidget
    </pre></blockquote>
}
###########################################################################
output_page

