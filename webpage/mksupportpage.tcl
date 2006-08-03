
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
    P <table border=1> <tr> <th colspan=2> $title
    foreach {a b} $args {
        P <tr> <td> $a <td width="100%"> $b
    }
    P </table>
}
proc CSSREF {section property {class {support}}} {
    # set cssdoc file:///home/dan/work/tkhtml/docs/css_1.0/css1_spec.html
    set cssdoc http://www.w3.org/TR/CSS21/
    set ref ${cssdoc}${section}.html#propdef-${property}
    return "<a href=\"$ref\" class=\"$class\">$property</a>"
}

set ::section_counter 1
proc SECTION {title} {
    set name "part$::section_counter"
    P <a name="$name"><h2>$title</h2></a>
    addPageSection $title $name
    incr ::section_counter
}

set ::title ""
proc START {title} {
    set ::html_head [subst {
        <html>
        <head>
          <title>$title</title>
          <link rel="stylesheet" href="tkhtml_tcl_tk.css">
        </head>
        <body>
    }]
    set ::title $title
}

proc FINISH {} {
    puts $::html_head
    puts [getTabs 1]
    puts [subst {
        <div id="body">
        <h1>$::title</h1>
    }]
    puts [getToc]
    puts {<div id="text">}
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
	2.1 and HTML 4.01 (todo) specifications.
</p>
}

SECTION "CSS Property Support"

P {
<p>
	The tables in this section compare CSS property support in Tkhtml with
	the <a href="http://www.w3.org/TR/CSS21/">CSS level 2.1</a>
	specification. Property names in blue are supported, those in grey 
	are unsupported.
</p>
}

SUPPORTTABLE {Font Properties} \
	[CSSREF fonts font-family] {
		Standard families "cursive" and "fantasy" are only
		available if the underlying font system used by Tk supports
		them.  
        } \
	[CSSREF fonts font-style] {
		Values 'italic' and 'oblique' map to "-slant italic"
		and 'normal' maps to "-slant roman".
	} \
	[CSSREF fonts font-variant nosupport] {No support.} \
	[CSSREF fonts font-weight] {
		Values 'bold', 'bolder' and numbers greater than 550
		map to "-weight bold", everything else maps to 
		"-weight normal".
	} \
	[CSSREF fonts font-size] {} \
	[CSSREF fonts font] {
		Supported except for font-variant values ('small-caps').
	}

SUPPORTTABLE {Color and Background Properties} \
	[CSSREF colors color] {} \
	[CSSREF colors background-color] {} \
	[CSSREF colors background-image] {Not supported on inline elements.} \
	[CSSREF colors background-repeat] {} \
	[CSSREF colors background-attachment] {} \
	[CSSREF colors background-position] {} \
	[CSSREF colors background] {}

SUPPORTTABLE {Text Properties} \
	[CSSREF text word-spacing nosupport] {No support.} \
	[CSSREF text letter-spacing nosupport] {No support.} \
	[CSSREF text text-decoration] {
		Value 'blink' is not supported. Also, multiple decorations
		(e.g. an underline and an overline) are not supported.
	} \
	[CSSREF text vertical-align] {
		No support for the following values: 'middle', 'top', 'bottom', 
		'text-top'or 'text-bottom'. Values 'baseline', 'sub' and 
		'super', or any percentage or length work.
        } \
	[CSSREF text text-transform nosupport] {No support.} \
	[CSSREF text text-align] {} \
	[CSSREF text text-indent] {} \
	[CSSREF text white-space] {
		No support on inline elements (correct for CSS1, incorrect 
		for later revisions).
	}

SUPPORTTABLE {Box Properties} \
	[CSSREF box margin] {
		Properties 'margin-top', 'margin-right', 'margin-bottom' and 
		'margin-left' are also supported. 
	} \
	[CSSREF box padding] {
		Properties 'padding-top', 'padding-right', 'padding-bottom' and 
		'padding-left' are also supported. 
	} \
	[CSSREF box border-width] {} \
	[CSSREF box border-style] {
		All border styles apart from 'none' (i.e. 'dashed', 
		'groove' etc.) are currently rendered as solid lines. This
		is legal according to the spec, but it's sub-optimal.
        } \
	[CSSREF box border-color] {} \
	[CSSREF box border] {
		Properties 'border-top', 'border-right', 'border-bottom' and
		'border-left' are also supported. Also 'border-top-color',
		'border-left-width' and other such variants.
	} 

SUPPORTTABLE {Visual Rendering Properties} \
	[CSSREF visuren display] {
		Not all values are supported. Currently supported values are
		'inline', 'block', 'list-item', 'table', 'table-row',
		'table-cell', 'none' and 'inherit'. The following are 
		handled as 'block': 'run-in', 'inline-block', 'table-caption'.
        } \
	[CSSREF visudet width] {} \
	[CSSREF visudet height] {} \
	[CSSREF visuren float] {} \
	[CSSREF visuren clear] {} \
	[CSSREF visudet line-height] {Supported on block level elements only} \
	[CSSREF visudet min-width nosupport] {No support.} \
	[CSSREF visudet max-width nosupport] {No support.} \
	[CSSREF visudet min-height nosupport] {No support.} \
	[CSSREF visudet max-height nosupport] {No support.} \
	[CSSREF visuren position] {
		Positioning modes 'static', 'relative', 'fixed' and 'absolute' 
		are all supported.
        } \
	[CSSREF visuren left] {} \
	[CSSREF visuren right] {} \
	[CSSREF visuren top] {} \
	[CSSREF visuren bottom] {} \
	[CSSREF visuren z-index nosupport] {No support.} \
	[CSSREF visuren unicode-bidi nosupport] {
		Tkhtml does not yet support bi-directional text. So the
		properties "unicode-bidi" and "direction" are both ignored.
	} \
	[CSSREF visuren direction nosupport] {No support.}

SUPPORTTABLE {Visual Effects Properties} \
	[CSSREF visufx overflow nosupport] {No support.} \
	[CSSREF visufx clip nosupport] {No support.} \
	[CSSREF visufx visibility nosupport] {No support.}

SUPPORTTABLE {Table Properties} \
	[CSSREF tables border-collapse nosupport] {No support.} \
	[CSSREF tables border-spacing] {}  \
	[CSSREF tables caption-side nosupport] {No support.}    \
	[CSSREF tables empty-cells nosupport] {No support.}     \
	[CSSREF tables table-layout nosupport] {No support.}

SUPPORTTABLE {User Interface Properties} \
	[CSSREF ui cursor nosupport] {No support.} \
	[CSSREF ui outline nosupport] {No support.} \
	[CSSREF ui outline-width nosupport] {No support.} \
	[CSSREF ui outline-color nosupport] {No support.} \
	[CSSREF ui outline-style nosupport] {No support.}

SUPPORTTABLE {Generated Content Properties}                         \
	[CSSREF generate list-style-type] {}                        \
	[CSSREF generate list-style-image] {}                       \
	[CSSREF generate list-style-position] {}                    \
	[CSSREF generate list-style] {}                             \
	[CSSREF generate content nosupport] {No support.}           \
	[CSSREF generate counter-increment nosupport] {No support.} \
	[CSSREF generate counter-reset nosupport] {No support.}     \
	[CSSREF generate quotes nosupport] {No support.}

# List of CSS 2.1 properties considered out of scope for Tkhtml.
set outofscopes {
    azimuth cue-after cue-before cue elevation pause-after pause-before
    pause pitch-range pitch play-during richness speak-header speak-numeral
    speak-punctuation speak speech-rate stress voice-family volume

    orphans page-break-after page-break-before page-break-inside widows
}
P {
  <p>
    The following CSS 2.1 properties are currently considered to be
    outside of Tkhtml's scope, as they only apply to aural or paged 
    document rendering:
  </p>
}
P <table style="margin:0px"><tr><td valign=top><ul>
set ii 1
foreach property $outofscopes {
    P <li>$property
    incr ii
    if {$ii % 10 == 0} {P </ul></td><td valign=top><ul>}
}
P </table>

SECTION {CSS Selector Support}

P {
<p>
	Essentially, all CSS 2.1 selectors are supported except for
	pseudo-elements. Any declaration that includes any of the following is
	ignored:
</p>
<ul>
	<li> :first-child
	<li> :first-letter
	<li> :first-line
	<li> :lang
	<li> :after
	<li> :before
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

FINISH

