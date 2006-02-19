
source [file join [file dirname [info script]] common.tcl]

set ::html {}
set ::html_head {}

proc P {args} {
    foreach a $args {
        append ::html "$a "
    }
    append ::html "\n"
}

proc SUPPORTTABLE {title args} {
    P <table border=1> <tr> <th colspan=3> $title
    foreach {a b} $args {
        P <tr> <td> $a <td> $b
    }
    P </table>
}
proc CSSREF {property {class {support}}} { 
    # set cssdoc file:///home/dan/work/tkhtml/docs/css_1.0/css1_spec.html
    set cssdoc http://www.w3.org/TR/CSS1
    set ref ${cssdoc}#${property}
    return "<a href=\"$ref\" class=\"$class\">$property</a>"
}

set ::section_counter 1
proc SECTION {title} {
    set name "part$::section_counter"
    P <a name="$name"><h2>$title</h2></a>
    addPageSection $title $name
    incr ::section_counter
}

proc START {title} {
    set ::html_head [subst {
        <html>
        <head>
          <title>$title</title>
          <link rel="stylesheet" href="tkhtml_tcl_tk.css">
        </head>
        <body>
    }]
    P [subst {
        <div id="body">
        <h1>$title</h1>
        <div id="text">
    }]
}

proc FINISH {} {
    puts $::html_head
    puts [getSideBoxes]
    puts $::html
    puts {
        </div>
        </div>
        </body>
        </html>
    }
}

###########################################################################

START "Tkhtml CSS and HTML Support"

P {
<p>
	Ultimately, Tkhtml aims to support those aspects of HTML 4.01 and CSS
	2.1 that apply to the parsing and visual rendering of documents. But,
	as you may have surmised, that is a work in progress. This document
	describes the current situation in terms of the CSS properties, CSS
	selectors, HTML attributes and HTML tags supported.
</p><p>
	This document currently tracks the CVS version against the CSS 
	1.0 and HTML 4.01 (todo) specifications.
</p>
}

SECTION "CSS Property Support"

P {
<p>
	The tables in this section compare CSS property support in Tkhtml with
	the <a href="http://www.w3.org/TR/CSS1">CSS level 1</a> specification.
	Property names in blue are supported, those in grey are unsupported. 
</p>
}

SUPPORTTABLE {Font Properties} \
	[CSSREF font-family] {
		Standard families "cursive" and "fantasy" are only
		available if the underlying font system used by Tk supports
		them.  
        } \
	[CSSREF font-style] {
		Values 'italic' and 'oblique' map to "-slant italic"
		and 'normal' maps to "-slant roman".
	} \
	[CSSREF font-variant nosupport] {} \
	[CSSREF font-weight] {
		Values 'bold', 'bolder' and numbers greater than 550
		map to "-weight bold", everything else maps to "-weight
		normal".
	} \
	[CSSREF font-size] {} \
	[CSSREF font] {Supported except for font-variant values ('small-caps').}

SUPPORTTABLE {Color and Background Properties} \
	[CSSREF color] {} \
	[CSSREF background-color] {} \
	[CSSREF background-image] {Not supported on inline elements.} \
	[CSSREF background-repeat] {} \
	[CSSREF background-attachment nosupport] {
		There is no way to position a background relative to the
		viewport at the moment, all background images are relative to
		the containing block. This will wait until there's time to
		look at the CSS2 'position' property.
	} \
	[CSSREF background-position] {} \
	[CSSREF background] {}

SUPPORTTABLE {Text Properties} \
	[CSSREF word-spacing nosupport] {} \
	[CSSREF letter-spacing nosupport] {} \
	[CSSREF text-decoration] {
		Value 'blink' is not supported. Also, multiple decorations
		(e.g. an underline and an overline) are not supported.
	} \
	[CSSREF vertical-align] {
		No support for the following values: 'middle', 'top', 'bottom', 
		'text-top'or 'text-bottom'. Values 'baseline', 'sub' and 
		'super', or any percentage or length work.
        } \
	[CSSREF text-transform nosupport] {} \
	[CSSREF text-align] {} \
	[CSSREF text-indent nosupport] {} \
	[CSSREF line-height] {Supported on block level elements only}

SUPPORTTABLE {Box Properties} \
	[CSSREF margin] {
		Properties 'margin-top', 'margin-right', 'margin-bottom' and 
		'margin-left' are also supported. 
	} \
	[CSSREF padding] {
		Properties 'padding-top', 'padding-right', 'padding-bottom' and 
		'padding-left' are also supported. 
	} \
	[CSSREF border-width] { \
		Properties 'border-top-width', 'border-right-width',
		'border-bottom-width' and 'border-left-width' are also
		supported. 
	} \
	[CSSREF border-style] {
		All border styles apart from 'none' (i.e. 'dashed', 
		'groove' etc.) are currently rendered as solid lines. This
		is legal according to the spec, but it's sub-optimal.
        } \
	[CSSREF border-color] {} \
	[CSSREF border] {
		Properties 'border-top', 'border-right', 'border-bottom' and
		'border-left' are also supported. 
	} \
	[CSSREF width] {} \
	[CSSREF height nosupport] {} \
	[CSSREF float] {} \
	[CSSREF clear] {} 

SUPPORTTABLE {Classification Properties} \
	[CSSREF display] {} \
	[CSSREF white-space] {
		No support on inline elements (correct for CSS1, incorrect 
		for later revisions)
	} \
	[CSSREF list-style-type] {} \
	[CSSREF list-style-image] {} \
	[CSSREF list-style-position] {} \
	[CSSREF list-style] {} 

SECTION {CSS Selector Support}

P {
<p>
	Essentially, all CSS1 selectors (and all CSS 2.1 for that matter) 
	are supported except for pseudo-classes and elements. Any 	
	declaration that includes any of the following is ignored:
</p>
<ul>
	<li> :first-child
	<li> :first-letter
	<li> :first-line
	<li> :active
	<li> :hover
	<li> :focus
	<li> :visited
	<li> :lang
</ul>
<p>
	The pseudo-class ":link" is handled in the same way as "a[href]" 
	(matches any &lt;a&gt; element that has an href element).
</p>
}



SECTION {HTML Support}

if 0 {

# List of HTML attributes:
abbr
accept-charset
accept
accesskey
action
align
alt
archive
axis
background
bgcolor
border
cellpadding
char
charoff
charset
checked
cite
class
classid
clear
code
codebase
codetype
color
cols
colspan
compact
content
coords
data
datetime
declare
defer
dir
disabled
enctype
face
for
frame
frameborder
headers
height
href
hreflang
hspace
http-equiv
id
ismap
label
lang
language
link
longdesc
longdesc
marginheight
marginwidth
maxlength
media
method
multiple
name
nohref
noresize
noshade
nowrap
object
profile
prompt
readonly
rel
rev
rows
rowspan
rules
scheme
scope
scrolling
selected
shape
size
span
src
standby
start
style
summary
tabindex
target
text
title
type
usemap
valign
value
valuetype
version
vlink
vspace
width
}



FINISH

