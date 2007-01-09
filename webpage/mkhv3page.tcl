
source [file join [file dirname [info script]] common.tcl]

addPageSection "Overview"     overview
addPageSection "Binaries"     binaries
addPageSection "Source Code"  source
addPageSection "Future Plans" future

puts [subst -novariables {

<html>
<head>
<link rel="stylesheet" href="tkhtml_tcl_tk.css">
<title>Html Viewer 3 - Tkhtml3 Web Browser</title>
</head>
<body>

[getTabs 3]

<div id="body">
<h1>Html Viewer 3 - Tkhtml3 Web Browser</h1>
[getToc]
<div id="text">

<p>
  Html Viewer 3 (hv3) is a minimalist web browser that uses Tkhtml. Currently
  it is at early alpha stage. Please try it out, then report bugs or make
  suggestions. Hv3 shares Tkhtml's 
<a href="index.html#part3">mailing list, bug-tracking database
  and contact details
</a>.
</p>
<p>
  Using hv3 and reporting bugs with it and the underlying Tkhtml widget
  assists Tkhtml development greatly. Suggestions also gratefully received.
</p>

<h2><a name="overview">Overview</a></h2>
  <p>
    Hv3 is not yet as sophisticated as some popular web browsers. Most
    notably, it does not support either javascript or plugins (although
    it can run most "tclets" created for the 
    <a href="http://www.tcl.tk/software/plugin/">tcl plugin<a>). It
    does support the following:
  </p>

    <ul>
      <li>Formatting of regular HTML/CSS documents,
      <li>HTML Frameset documents,
      <li>HTML forms,
      <li>HTTP cookies,
      <li>HTTP "Location" and "Refresh" headers.
    </ul>

  <p>
    <a href="screenshot1.gif">
      <img class=screenshot align=left src="screenshot1_small.gif">
    </a>
  </p>
<h2><a name="binaries">Binary Installation</a></h2>
  <p>
    The files offered for download below are not built from any specific
    released version of Tkhtml. Instead, they are updated whenever the
    source code is considered stable enough (some projects call these 
    "nightly" builds). In practice this means once every couple of days.
    The binaries currently offered were last updated at:
  </p>

  <p> <b>[clock format [clock seconds]]</b>

  <p>
    Detailed version information for any hv3 build may be found by
    visiting the internal URI "about:" (i.e. type "about:" into the 
    location entry field at the top of the window). Cut'n'pastin'
    this information into bug reports can be a big help.
  </p>

  <table border=1 cellpadding=5 style="margin-left:2em; margin-right:2em">

<tr>
<td>Windows
<td><a href="hv3-win32.exe">hv3-win32.exe</a> - Executable for win32 platforms.

<tr>
<td>Generic Linux
<td>
    <p><a href="hv3-linux-x86.gz">hv3-linux-x86.gz</a> - Gzip'd executable for
    linux x86 platforms. Everything is staticly linked in, so there are no
    dependencies. To use this, download the file, gunzip it, set the
    permissions to executable and run it. i.e. execute the following commands
    from a terminal window:

<pre>
  wget http://tkhtml.tcl.tk/hv3-linux-x86.gz
  gunzip hv3-linux-x86.gz
  chmod 755 hv3-linux-x86
  ./hv3-linux-x86
</pre>

    <p><a href="hv3_img.kit">hv3_img.kit</a> - Starkit package. To use
    this you also require a tclkit runtime from Equi4 software
    (<a href="http://www.equi4.com">http://www.equi4.com</a>). If you're 
    not sure what this means, grab the file above instead.

<tr>
<td>Puppy Linux
<td>
    <p><a href="hv3-alpha-14.tgz">hv3-alpha-14.tgz</a> - Pupget package
    for Puppy linux 
    (<a href="http://www.puppyos.com">http://www.puppyos.com</a>). This
    probably won't work on other linux distributions.

</table>

<h2><a name="source">Source Code Installation</a></h2>
    
  <p>
    <a href="screenshot3.gif">
      <img class=screenshot align=left src="screenshot3_small.gif">
    </a>
    All source code for hv3 is included in the Tkhtml source distribution.
    To use hv3, first download, build and install Tkhtml from 
    <a href="index.html#part4">source code</a>. Then, run the file
    hv/hv3_main.tcl from the source distribution using the "wish" shell.
    Instructions for building Tkhtml are contained in the README file
    of the source distribution (it's a TEA compliant package).
  </p>
  <p>
    Installing Tkhtml creates a directory "Tkhtml3.0" in the Tcl lib
    directory. To uninstall, remove this directory and it's contents.
  </p>
  <p>
    Running hv3 without installing Tkhtml (it is alpha software after all)
    is more difficult. The hv3 scripts execute the command 
    \[package require Tkhtml\] to load the Tkhtml package, which only
    works if the package is already installed. If using a unix-like shell,
    one way to do this is a command like,
  </p>
<pre style="white-space: nowrap">
   echo "load ./libTkhtml3.0.so ; source ../htmlwidget/hv/hv3_main.tcl" | wish
</pre>
  <p>
    , assuming ./libTkhtml3.0.so is the path to the shared object built by
    the TEA-compliant build process and ../htmlwidget/hv3/hv3_main.tcl is
    the path to the hv/hv3_main.tcl file from the source distribution.
  </p>

<h2><a name="hv3_polipo">Hv3 Polipo</a></h2>
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
    as well. The binary packages above already include pre-compiled versions.

<h2><a name="future">Future Plans</a></h2>
  <p>
    Currently, the plan is for hv3 to develop alongside Tkhtml to serve
    as it's primary test harness, but not to immediately work on more complex
    browser functions, such as javascrpt or plugins. But this is primarily due
    to lack of resources. This said, Hv3 is already a useful web-browser
    in it's own right, not merely a proof of concept.
  </p>
  <p>
    Anyone interested in volunteering to patch, work on
    or fork the hv3 source code is encouraged to do so. Why not 
    <a href="index.html#part3">get in touch</a>?
  </p>

</div>
</div>
</body>
</html>
}]

