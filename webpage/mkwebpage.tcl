#!/usr/bin/tclsh
#
# Construct the web page for tkhtml
#
# @(#) $Id: mkwebpage.tcl,v 1.38 2007/10/03 10:06:39 danielk1977 Exp $
#

source [file join [file dirname [info script]] common.tcl]

proc header {} {
  puts {
    <html>
    <head>
    <link rel="stylesheet" href="tkhtml_tcl_tk.css">
    <title>tkhtml.tcl.tk</title>
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

h 1 {tkhtml.tcl.tk}

p {

<p>
This website hosts the tkhtml.tcl.tk project, an experiment in creating
modern web browser components based on the Tcl/Tk platform. Currently
this consists of two pieces of software and their accompanying 
documentation:
</p>
<table>
  <tr><td style="padding:0 10px">
       <a href="hv3.html" class="bigred">Hv3</a>
   <td>Hv3 is a cross-platform web browser with support for modern
       web standards like HTML, CSS, HTTP and ECMAScript (a.k.a. javascript).

  <tr><td height=5>

  <tr><td style="padding:0 10px">
       <a href="tkhtml.html" class="bigred">Tkhtml3</a>
       <td>Tkhtml3 is a Tk widget that displays content formatted according
       to the HTML and CSS standards. Tkhtml3 is not an end-user application,
       it is for Tcl programmers who wish to embed a standards-compliant
       HTML/CSS implementation in their applications.
</table>

<p>
  There is a mailing list for Hv3 and Tkhtml3 hosted by Google Groups.
  You can join the mailing list, view the archive and post new messages 
  by clicking here:
  <blockquote>
    <a href="http://groups.google.com/group/tkhtml3">
    The Tkhtml3/Hv3 mailing list</a>
  </blockquote>
<p>
  Bug reports, enhancement requests and the project changelog are managed
  by a CVStrac installation. There is also a wiki where users can 
  contribute content. Access CVStrac here:
<blockquote>
  <a href="http://tkhtml.tcl.tk/cvstrac/index">
    CVSTrac - Bug reports, Enhancement requests, Changelog and Wiki
  </a>
</blockquote>

}

h 2 {Documents}

p {
<ul>
  <li> A 
    <a href="http://www.tcl.tk/community/tcl2007/papers/Dan_Kennedy/file___localhost_...tcl2006_tkhtml3_tcl2006.pdf">paper describing Tkhtml3</a> (www.tcl.tk) 
    was presented at the 2006 Tcl conference. Even though it is a little
    out of date, this is the best general introduction to Tkhtml3 programming
    available. Prospective users should read this paper for a general 
    overview, then proceed to the tkhtml(n) man page to absorp the details.
  <li> An early 
    <a href="requirements.html">requirements specification</a> from way 
    back in 2005. This is no longer really relevant, but it's amusing
    in it's own way.
</ul>
}

h 2 {Source Code}

p {
  <p>The source code for Tkhtml3 and Hv3 is bundled together as a single
     project for source code management purposes. It can be obtained 
     either by downloading a release tarball, or via anonymous CVS.

  <p>Download the <a href="tkhtml3-alpha-16.tar.gz">
      source code for the latest release (alpha 16)</a>.

  <p>Or to obtain the lastest source-code from cvs, use the following 
     procedure (from an x-term or command prompt):

    <ol>
      <li> Log in with the following command:
    <blockquote>
<pre>cvs -d :pserver:anonymous@tkhtml.tcl.tk:/tkhtml login</pre>
    </blockquote>

      <li> You will be prompted for a password. Use "<tt>anonymous</tt>".
      <li> Obtain the lastest version 3 source code:
    <blockquote>
<pre>cvs -d :pserver:anonymous@tkhtml.tcl.tk:/tkhtml checkout htmlwidget</pre>
    </blockquote>
    </ol>

}

h 2 {Participation}

p {
  tkhtml.tcl.tk is an open-source project, and so requires community
  participation to succeed. All are welcome! Here are some of the ways 
  you can participate:

<table>
  <tr><td valign=top style="font-weight:bold;padding:0 10px;white-space:nowrap">
       Using Hv3 <td style="padding:0">
    <a href="hv3.html">Download Hv3</a> and browse the web with it 
    for a while. Report any bugs, problems or incompatibilities that you
    encounter. Make some suggestions for improvements.
  <tr><td height=5>
  <tr><td valign=top style="font-weight:bold;padding:0 10px;white-space:nowrap">
       Using Tkhtml3 <td style="padding:0">
    Write a program that uses Tkhtml3, or embed it into an existing
    program. Comment on your experience doing so and report any bugs.
  <tr><td height=5>
  <tr><td valign=top style="font-weight:bold;padding:0 10px;white-space:nowrap">
       Join The Mailing List<td style="padding:0">
    Join the <a href="http://groups.google.com/group/tkhtml3">
    Tkhtml3/Hv3 mailing list</a> hosted at Google Groups to discuss 
    Tkhtml3 or Hv3.
  <tr><td height=5>
  <tr><td valign=top style="font-weight:bold;padding:0 10px;white-space:nowrap">
       Help Out With The Website<td style="padding:0">
    As you can see, the website isn't up to much at the moment (ironic eh?).
    If you would like to help change that, or if you can help by building
    a mac osx build, please get in touch.
  <tr><td height=5>
  <tr><td valign=top style="font-weight:bold;padding:0 10px;white-space:nowrap">
       Join The Development Team<td style="padding:0">
    If you can program in C, Tcl or javascript, then you are welcome to
    join the development team. Pick an aspect of Tkhtml3 or Hv3 you want to
    improve, post a message to the mailing list, and go from there. 
</table>

}

h 2 {Contacts}

p {
  It is best to join the 
  <a href="http://groups.google.com/group/tkhtml3">mailing list</a> and 
  post messages there. That way there is an archive of the message.
  Alternatively, you can send mail to one of the following contacts:
  <blockquote>
    <a href="mailto:danielk1977@gmail.com">danielk1977@gmail.com</a> (Dan -
    current maintainer)
    <br>
    <a href="mailto:drh@hwaci.com">drh@hwaci.com</a> (Richard)
  </blockquote>
}



output_page

