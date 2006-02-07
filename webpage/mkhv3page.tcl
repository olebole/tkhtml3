
source [file join [file dirname [info script]] common.tcl]

addPageSection "Overview" overview
addPageSection "Screenshots" screenshots
addPageSection "Download" download

puts [subst -novariables {

<html>
<head>
<link rel="stylesheet" href="tkhtml_tcl_tk.css">
<title>Html Viewer 3 - Tkhtml test application</title>
</head>
<body>

[getSideBoxes]

<div id="body">
<h1>Html Viewer 3 - Tkhtml Test Application</h1>
<div id="text">

<p>
  Html Viewer 3 (hv3) is a minimalist browser script that uses
  Tkhtml. The idea is to have just enough functionality to test Tkhtml,
  but it's not supposed to be usable as a real browser. It may be
  useful as an offline html viewer. 
</p>


<h2><a name="overview">Overview</a></h2>
  <p>
    The target feature set for hv3 is:
  </p>
  <ul>
    <li> Support for http:// and file:// URIs.
    <li> Ability to load a new document, either by clicking on a hyper-link 
         or by entering a URI.
    <li> Support for anchors in documents (i.e. URIs like: index.html#part1).
    <li> Support for images, both as replaced objects and in other roles.
    <li> Support for widgets as replaced objects for form elements. No support
         for submitting forms etc., this functionality is purely cosmetic.
    <li> Support for selecting text from the rendered document and copying 
         that text to the clipboard.
  </ul>
  <p>
  </p>
  <p>
    Start the program and type a file:// or http:// URI into the address bar 
    at the top of the GUI window. A single URI can also be specified as a 
    command line argument.
  </p>
  <p>
    Hv3 can be used to view online documents accessible via http. To do so, 
    it is recommended that the user run a cacheing http proxy (i.e. squid) 
    locally. This is because hv3 does not implement any kind of object or DNS 
    cache, so connecting directly to the internet can be tediously slow.
    Hv3 is hard-coded to use a web proxy running on localhost, port 
    3128. Grep for "3128" in the Tcl source if you wish to change this.
    Or remove the -proxyhost and -proxyport lines altogether if you really
    do not wish to start a web proxy.
  </p>
  <p>
    If it is not already running, squid can be started on most linux 
    installations using a command like:
  </p>
  <pre>
      squid -N -D -d 10
  </pre>


<h2><a name="screenshots">Screenshots</a></h2>
  <a href="screenshot1.gif"><img src="screenshot1_small.gif"></a>
  <a href="screenshot2.gif"><img src="screenshot2_small.gif"></a>
  <a href="screenshot3.gif"><img src="screenshot3_small.gif"></a>


<h2><a name="download">Download</a></h2>
  <p>
    The source code to hv3 is part of the Tkhtml source bundle and so is 
    available from the <a href="index.html#part4">same place</a>. Or, for
    linux-x86, starkits are available:
  </p>
  <ul>
    <li><a href="hv3.kit">Starkit without Img package</a>
    <li><a href="hv3_img.kit">Starkit with Img package</a> 
        (handles more image types)
  </ul>
  <p>
    <b>TODO:</b> Starkits for windows. Or make these ones cross-platform.
  </p>
  <p>
    To use the starkit, download a tclkit runtime for your platform from 
    <a href="http://www.equi4.com/pub/tk/downloads.html">equi4.com</a>.
    I used <a href="http://www.equi4.com/pub/tk/8.5a4/tclkit-linux-x86-xft.gz">
    http://www.equi4.com/pub/tk/8.5a4/tclkit-linux-x86-xft.gz</a> because
    it takes advantage of fontconfig and xft.
  </p>
</div>
</div>
</body>
</html>
}]


