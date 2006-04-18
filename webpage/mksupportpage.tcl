
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
	[CSSREF background-attachment] {} \
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
	[CSSREF height] {} \
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
	are supported except for pseudo-elements. Any 	
	declaration that includes any of the following is ignored:
</p>
<ul>
	<li> :first-child
	<li> :first-letter
	<li> :first-line
	<li> :lang
</ul>
}



SECTION {HTML Parsing Support}

P {
  <p>This section describes the some of the special processing that
     Tkhtml does to support HTML.

  <p style="text-align:center"><b>Implicit Elements</b>

  <p>In any Html document, the &lt;html&gt;, &lt;head&gt; and
     &lt;body&gt; elements are always present, even if not explicity
     opened or closed in the html text.

  <p style="text-align:center"><b>Implicit Closing Tags</b>

  <p>This sub-section describes the cases where the Html parser infers
     the presence of implicit closing tags and inserts them before 
     generating the document tree. Tkhtml can detect implicit closing 
     tags in five cases:

  <dl>
     <dt>Empty elements:
     <dd>The &lt;br&gt;, &lt;area&gt;, &lt;link&gt;, &lt;img&gt;, 
         &lt;param&gt;, &lt;hr&gt;, &lt;base&gt;, &lt;meta&gt; and 
         &lt;input&gt; elements are always empty, they cannot contain 
         any other elements.

     <dt>Inline content only elements:
     <dd>The &lt;p&gt; and &lt;dt&gt; elements may only contain inline 
         content. They are implicitly closed by any element that does not
         generate inline content (according to html).

     <dt>Flow content only elements:
     <dd>The &lt;th&gt;, &lt;td&gt;, &lt;p&gt; and &lt;dt&gt; elements may 
         only contain flow content. They are implicitly closed by any 
         element that does not generate flow content (according to html).

     <dt>Table-cell content only elements:
     <dd>The &lt;tr&gt; element may only contain &lt;th&gt; or &lt;td&gt; 
         elements.

     <dt>Text content only elements:
     <dd>The &lt;option&gt; element may only contain text, not markup tags.
  </dl>


  <p style="text-align:center"><b>Table Support</b>

  <p>Table support is a curious mix of support for CSS and Html at the 
     moment. The CSS display types 'table', 'table-row' and 'table-cell' are 
     recognized for laying out document sub-trees as tables. The default 
     stylesheet assigns these values to the display properties of Html 
     &lt;table&gt;, &lt;tr&gt;, &lt;td&gt; and &lt;th&gt; elements.

  <p>Tkhtml checks for and respects the "rowspan" and "cellspan" attributes 
     on any element with the display property set to 'table-cell'. There is 
     no way to asign a row or column span via CSS (at least not with Tkhtml).

  <p>There is no support for the &lt;caption&gt;, &lt;thead&gt;, 
     &lt;tfoot&gt;, &lt;tbody&gt;, &lt;colgroup&gt; and &lt;col&gt; tags,
     these are ignored by the parser.

  <p>The Tkhtml layout engine simply ignores any children of an element with 
     display property 'table' that do not have their display property set 
     to 'table-row'. Children of 'table-row' elements that are not of type
     'table-cell' are similarly ignored. This is incorrect, the CSS2 
     specification requires anonymous boxes to be inserted where such 
     elements are missing. The Tkhtml html parser compensates for this by 
     transforming each sub-tree rooted at a &lt;table&gt; element 
     according to the following rules:

  <ol>
    <li>Children of &lt;table&gt; elements other than &lt;tr&gt;,  
        &lt;td&gt; and &lt;th&gt; are moved in the tree to become left-hand 
        siblings of the &lt;table&gt; element.

    <li>Should there exist one or more &lt;td&gt; or &lt;th&gt; children of
        the &lt;table&gt; element, then one or more &lt;tr&gt; elements is
        inserted into the tree. Each new &lt;tr&gt; element is a child of
        the &lt;table&gt; element.

    <li>Children of &lt;tr&gt; elements that are themselves children 
        of &lt;table&gt; elements other than &lt;td&gt; and &lt;th&gt; are
        also moved in the tree to become left-hand siblings of the 
        &lt;table&gt; element.
  </ol>
 
  <p>Logically, steps are performed in the order above. For example, 
     parsing the following Html fragment:

  <pre class=code>
&lt;table&gt;
  &lt;div&gt;Text A&lt;/div&gt;
  &lt;tr&gt; &lt;div&gt;Text B&lt;/div&gt;
    &lt;td&gt; Text C
  &lt;/tr&gt;
  &lt;td&gt; Text D
  &lt;td&gt; Text E
&lt;/table&gt;
  </pre>
  
  <p>Produces the tree that one would expect from:

  <pre class=code>
&lt;div&gt;Text A&lt;/div&gt;
&lt;div&gt;Text B&lt;/div&gt;
&lt;table&gt;
  &lt;tr&gt; 
    &lt;td&gt; Text C
  &lt;/tr&gt;
  &lt;tr&gt; 
    &lt;td&gt; Text D
    &lt;td&gt; Text E
  &lt;/tr&gt;
&lt;/table&gt;
  </pre>

}

SECTION {HTML Element Support}

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

# List of CSS 2.1 properties within scope for Tkhtml.
if 0 {

# Position related:
left right top bottom position

# Outlines
outline-color outline-style outline-width

# Maximum and minimum heights.
max-height max-width min-height min-width

# Tables:
border-collapse border-spacing caption-side empty-cells table-layout

# Bi-directional text:
unicode-bidi direction

# Interesting but not currently in scope:
clip overflow content counter-increment counter-reset 
cursor z-index quotes visibility

}


FINISH

