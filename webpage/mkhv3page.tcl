
source [file join [file dirname [info script]] common.tcl]

addPageSection "Overview"     overview
addPageSection "Binaries"     binaries
addPageSection "Source Code"  source
addPageSection "Future Plans" future

puts [subst -novariables {

<html>
<head>
<link rel="stylesheet" href="tkhtml_tcl_tk.css">
<title>Html Viewer 3 - Tkhtml test application</title>
</head>
<body>

[getTabs 3]

<div id="body">
<h1>Html Viewer 3 - Tkhtml Test Application</h1>
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
    Hv3 is not yet as sophisticated as most popular web browsers. Most
    notably, it does not support either javascript or plugins (although
    it can run most "tclets" created for the 
    <a href="http://www.tcl.tk/software/plugin/">tcl plugin<a>). As
    well as formatting HTML/CSS documents, hv3 supports FRAMESET documents,
    tabs, HTML forms and HTTP cookies.
  </p>
  <p>
    <a href="screenshot1.gif">
      <img class=screenshot align=left src="screenshot1_small.gif">
    </a>

    By itself, hv3 can connect to remote servers to retrieve documents
    specified by http URIs using the built-in Tcl http package. However
    much better performance can be obtained by installing the custom
    http proxy program hv3_polipo (also available for download from
    this site).  Once installed, hv3 starts and stops hv3_polipo automatically.
    See below for installation instructions.
  </p>
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

<h2><a name="binaries">Binary Installation</a></h2>
  <p>
    <a href="screenshot2.gif">
      <img class=screenshot align=right src="screenshot2_small.gif">
    </a>
    Binary packages are provided for Windows and Linux on x86 only. To
    install a binary package, download the following:
    <ol>
      <li> The hv3 starkit (required),
      <li> A Tclkit runtime for your platform (required),
      <li> The custom web proxy program hv3_polipo (optional).
    </ol>
  <p>
    Some users may already have a Tclkit runtime. Although only the
    linked files below have been tested, any reasonably modern version 
    of Tclkit should work.
  </p>
  <p>
    The files offered for download below are not built from any specific
    released version of Tkhtml. Instead, they are updated whenever the
    source code is considered stable enough (some projects call these 
    "nightly" builds). In practice this means once every couple of days.
    The binaries currently offered were last updated at:
  </p>

  <p> <b>TODO: FILL THIS IN BY HAND</b> </p>

  <p>
    Detailed version information for any hv3 build may be found by
    visiting the internal URI "about:" (i.e. type "about:" into the 
    location entry field at the top of the window). Cut'n'pastin'
    this information into bug reports can be a big help.
  </p>

  <table border=1>
    <tr><th>Windows<th>Linux
    <tr><td>
           <ol style="list-style-position: inside; margin: 0; padding: 1em">
               <li><a href="hv3_img_w32.kit">hv3_img_w32.kit</a>
               <li><a href="http://www.equi4.com/pub/tk/8.4.13/tclkit-win32.upx.exe">tclkit-w32.upx.exe</a>
               <li><a href="hv3_polipo.exe">hv3_polipo.exe</a>
           </ol>
        <td>
           <ol style="list-style-position: inside; margin: 0; padding: 1em">
               <li><a href="hv3_img.kit">hv3_img.kit</a>
               <li><a href="http://www.equi4.com/pub/tk/8.5a4/tclkit-linux-x86-xft.gz">tclkit-linux-x86-xft.gz</a>
               <li><a href="hv3_polipo">hv3_polipo</a>
           </ol>
  </table>

  <p style="text-align:center"><b>Linux Specific Instructions</b></p>
  <p>
    After downloading the starkit and tclkit runtime (files 1 and 2), Linux
    users must decompress the tclkit runtime, change it's permissions to
    be executable and execute it with the path of to the starkit as the first
    argument. For example:
  </p>

  <pre>
      $ gunzip ./tclkit-linux-x86-xft.gz
      $ chmod 755 ./tclkit-linux-x86-xft
      $ ./tclkit-linux-x86-xft ./hv3_img.kit
  </pre>

  <p>
    To use the custom web proxy program hv3_polipo, simply download it, 
    make sure it is executable and put the executable in the same directory
    as the starkit file (hv3_img.kit).
  </p>

  <p>
    Linux users who are trusting sorts try consider cutting and paste the
    following block of text into a command terminal to download, unpack and 
    run hv3 without typing a whole lot of commands:
  </p>

  <pre>
    mkdir /tmp/test_hv3
    cd /tmp/test_hv3
    wget http://tkhtml.tcl.tk/hv3_img.kit
    wget http://tkhtml.tcl.tk/hv3_polipo
    wget http://www.equi4.com/pub/tk/8.5a4/tclkit-linux-x86-xft.gz
    gunzip ./tclkit-linux-x86-xft.gz
    chmod 755 ./tclkit-linux-x86-xft
    chmod 755 ./hv3_polipo
    ./tclkit-linux-x86-xft ./hv3_img.kit
  </pre>

  <p style="text-align:center"><b>Windows Specific Instructions</b></p>
  <p>
    After downloading the starkit and tclkit runtime (files 1 and 2), 
    right-click on the starkit file (hv3_img_w32.kit) and select 
    "Open With..." from the menu. The click "Browse" and choose
    the tclkit runtime (tclkit-win32-upx.exe) with the file selection
    dialog.
  </p>

  <p>
    To use the custom web proxy program hv3_polipo.exe, download it and put it
    in the same directory as the starkit file (hv3_img_w32.kit).
  </p>
 
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

<h2><a name="future">Future Plans</a></h2>
  <p>
    Currently, the plan is for hv3 to develop alongside Tkhtml to serve
    as it's primary test harness, but not to work on more complex browser
    functions, such as javascrpt, plugins or https. But this is primarily due
    to lack of resources. Anyone interested in volunteering to patch, work on
    or fork the hv3 source code is encouraged to do so. Please 
    <a href="index.html#part3">get in touch</a>!
  </p>

</div>
</div>
</body>
</html>
}]

