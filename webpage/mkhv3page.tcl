
source [file join [file dirname [info script]] common.tcl]

addPageSection "Overview"    overview
addPageSection "Screenshots" screenshots
addPageSection "Download"    download

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
  Html Viewer 3 (hv3) is a minimalist web browser that uses Tkhtml. 
</p>

<h2><a name="overview">Overview/Features</a></h2>


  <p>
    The target feature set for hv3 is:
  </p>
  <ul>
    <li> Support for http:// and file:// URIs.
    <li> Support for hyperlinks, and manual entry of URIs.
    <li> Support for anchors in documents (i.e. URIs like: index.html#part1).
    <li> Support for images, both as replaced objects and in other roles.
    <li> Support for html forms.
    <li> Support for http cookies.
    <li> Support for selecting text from the rendered document and copying 
         that text to the clipboard.
  </ul>

<h2><a name="screenshots">Screenshots</a></h2>
  <a href="screenshot1.gif"><img src="screenshot1_small.gif"></a>
  <a href="screenshot2.gif"><img src="screenshot2_small.gif"></a>
  <a href="screenshot3.gif"><img src="screenshot3_small.gif"></a>


<h2><a name="download">Download</a></h2>
  <p>
    The source code to hv3 is part of the Tkhtml source bundle and so is 
    available from the <a href="index.html#part4">same place</a>. Or, for
    linux or windows on x86, starkits are available:
  </p>
  <ul>
    <li><a href="hv3.kit">Linux starkit without Img package</a>
    <li><a href="hv3_img.kit">Linux starkit with Img package</a> 
        (handles more image types)
    <li><a href="hv3_w32.kit">Windows starkit without Img package</a>
    <li><a href="hv3_img_w32.kit">Windows starkit with Img package</a>
  </ul>
  <p>
    To use the starkit, download a tclkit runtime for your platform from 
    <a href="http://www.equi4.com/pub/tk/downloads.html">equi4.com</a>.
    I used <a href="http://www.equi4.com/pub/tk/8.5a4/tclkit-linux-x86-xft.gz">
    http://www.equi4.com/pub/tk/8.5a4/tclkit-linux-x86-xft.gz</a> with linux 
    because it takes advantage of fontconfig and xft.
  </p>
</div>
</div>
</body>
</html>
}]


