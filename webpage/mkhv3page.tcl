source [file join [file dirname [info script]] common.tcl]

addPageSection "Download"            download
addPageSection "Source Code/Hacking" source
addPageSection "More Information"    info

proc VERSION {} {
  if {[info exists ::env(VERSION)]} {return $::env(VERSION)}
  return "alpha-16"
}

if {[string first alpha [VERSION]] == 0} {
  set files_comment ""
} else {
  set files_comment [subst -novariables {
    <p>
    The files offered for download above are not built from any specific
    released version of Tkhtml. Instead, they are updated whenever the
    source code is considered stable enough (some projects call these 
    "nightly" builds). In practice this means once every couple of days.
    The binaries currently offered were last updated at 
    <b>[clock format [clock seconds]]</b>.
  }]
}

puts [subst -novariables {

<html>
<head>
<link rel="stylesheet" href="tkhtml_tcl_tk.css">
<title>Html Viewer 3 - Tkhtml3 Web Browser</title>
</head>
<body>

[getTabs 3]

<div id="body">
<h1>Hv3 - Tcl/Tk Web Browser</h1>
[getToc]
<div id="text">

<p>
  Html Viewer 3 (hv3) is a powerful yet minimalist web browser that uses
  Tkhtml3 as a rendering engine and 
  <a href="http://www.adaptive-enterprises.com.au/~d/software/see/">
  SEE (Simple ECMAScript Engine)</a> to interpret scripts. The application
  itself is written in Tcl. Currently it is at alpha stage. Please try it 
  out, then report bugs or make suggestions.

<p>
  Instructions for obtaining source code, reporting bugs, requesting
  enhancements, joining the mailing list and contacting the authors
  may be found on <a href="index.html">the front page of this site</a>.
</p>

<h2><a name="download">Download</a></h2>
  <table style="margin-left:2em; margin-right:2em; margin-top:0">

<!-- A black line. We should be able to style the <tr> blocks for
     this effect, but that hits a Gecko bug. And (of course) the
     corresponding IE6 bug.
-->
<tr><td colspan=3><div style="border-bottom: 1px solid black;padding: 0">

<tr><td valign=top>Windows<td width=10>
<td>
    <p><a href="hv3-win32-[VERSION].exe">hv3-win32-[VERSION].exe</a> -
    Executable for win32 platforms.

    <p><a href="hv3-win32-[VERSION].kit">hv3-win32-[VERSION].kit</a> - 
    Starkit package. 
    To use this you also require a tclkit runtime from Equi4 software
    (<a href="http://www.equi4.com">http://www.equi4.com</a>). If you're 
    not sure what this means, grab the executable file above instead.

<tr><td colspan=3><div style="border-bottom: 1px solid black;padding: 0">
<tr><td valign=top>Generic&nbsp;Linux<td>
<td>
    <p><a href="hv3-linux-[VERSION].gz">
        hv3-linux-[VERSION].gz</a> - Gzip'd executable for
    linux x86 platforms. Everything is staticly linked in, so there are no
    dependencies. To use this, download the file, gunzip it, set the
    permissions to executable and run it. i.e. execute the following commands
    from a terminal window:

<pre>
  wget http://tkhtml.tcl.tk/hv3-linux-[VERSION].gz
  gunzip hv3-linux-[VERSION].gz
  chmod 755 hv3-linux-[VERSION]
  ./hv3-linux-[VERSION]
</pre>

    <p><a href="hv3-linux-[VERSION].kit">hv3-linux-[VERSION].kit</a> - 
    Starkit package. 
    To use this you also require a tclkit runtime from Equi4 software
    (<a href="http://www.equi4.com">http://www.equi4.com</a>). If you're 
    not sure what this means, grab the gzip'd executable file above instead.

<tr><td colspan=3><div style="border-bottom: 1px solid black;padding: 0">
<tr><td valign=top>Puppy&nbsp;Linux<td>
<td>
    <p><a href="hv3-[VERSION].pet">hv3-[VERSION].pet</a> - Pupget package
    for Puppy linux 
    (<a href="http://www.puppyos.com">http://www.puppyos.com</a>). 
    This won't work with other linux distributions. For anything other 
    than puppy or puppy derivitives, use the generic linux package above.

<tr><td colspan=3><div style="border-bottom: 1px solid black;padding: 0">
</table>

    [set ::files_comment]

  <p>
    Detailed version information for any hv3 build may be found by
    visiting the internal URI "home://about/" (i.e. type "home://about/" 
    into the location entry field at the top of the window).
  </p>

<h2><a name="source">Source Code and Hacking</a></h2>

  <p>
    A goal of Hv3 development is that the source code should be accessible;
    it should be easy to hack on. For those wishing to experiment with
    modifying Hv3 source code, there are two options:

    <ol> 
      <li> By downloading one of the starkits available above and editing
           the Tcl code.
      <li> By building the whole system from scratch.
    </ol>
  </p>

  <p>
    The Windows and "Generic Linux" *.kit files available for download
    above are constructed using starkit technology. This means you
    can "unwrap" them, modify the source code therein, and run the
    modified version. The <a href="http://www.equi4.com/starkit/started.html">
    "Getting started with Starkits and Tclkit"</a> page over at 
    <a href="http://www.equi4.com">equi4.com</a> provides a step
    by step example of doing this.
  </p>

  <p>
    To build the system from scratch, first obtain the 
    <a href="index.html#part2"> latest sources</a> for Tkhtml3 and Hv3.
    Detailed instructions for building the required components (Tkhtml3,
    <a href="ffaq.html#hv3_polipo">hv3_polipo</a> and Tclsee) and running the
    Hv3 script are found in the 
    <a href="http://tkhtml.tcl.tk/cvstrac/fileview?f=htmlwidget/COMPILE.txt">
    COMPILE.txt</a> file of the source code distribution.
  </p>

<h2><a name="info">More Information</a></h2>
  <p>
    There is more information related to Hv3 to be found in the tkhtml.tcl.tk
    <a href="ffaq.html">FFAQ</a>.
  </p>

</div>
</div>
</body>
</html>
}]

