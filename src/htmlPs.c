#if TKHTML_PS
#define TCL_INTEGER_SPACE 24
/* 
 * So blatently ripped off from canvas, most comments are unchanged.
 * Peter MacDonald.  http://browsex.com
 *
 * tkCanvPs.c --
 *
 *	This module provides Postscript output support for canvases,
 *	including the "postscript" widget command plus a few utility
 *	procedures used for generating Postscript.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: htmlPs.c,v 1.7 2005/03/22 12:07:34 danielk1977 Exp $
 */

#include "tkInt.h"
#include "tkCanvas.h"
#include "tkPort.h"
#include <tcl.h>
#include <tk.h>

/*
 * See tkCanvas.h for key data structures used to implement canvases.
 */

/*
 * One of the following structures is created to keep track of Postscript
 * output being generated.  It consists mostly of information provided on
 * the widget command line.
 */

typedef struct TkPostscriptInfo {
    int x, y, width, height;	/* Area to print, in canvas pixel
				 * coordinates. */
    int x2, y2;			/* x+width and y+height. */
    char *pageXString;		/* String value of "-pagex" option or NULL. */
    char *pageYString;		/* String value of "-pagey" option or NULL. */
    double pageX, pageY;	/* Postscript coordinates (in points)
				 * corresponding to pageXString and
				 * pageYString. Don't forget that y-values
				 * grow upwards for Postscript! */
    char *pageWidthString;	/* Printed width of output. */
    char *pageHeightString;	/* Printed height of output. */
    double scale;		/* Scale factor for conversion: each pixel
				 * maps into this many points. */
    Tk_Anchor pageAnchor;	/* How to anchor bbox on Postscript page. */
    int rotate;			/* Non-zero means output should be rotated
				 * on page (landscape mode). */
    char *fontVar;		/* If non-NULL, gives name of global variable
				 * containing font mapping information.
				 * Malloc'ed. */
    char *colorVar;		/* If non-NULL, give name of global variable
				 * containing color mapping information.
				 * Malloc'ed. */
    char *colorMode;		/* Mode for handling colors:  "monochrome",
				 * "gray", or "color".  Malloc'ed. */
    int colorLevel;		/* Numeric value corresponding to colorMode:
				 * 0 for mono, 1 for gray, 2 for color. */
    char *fileName;		/* Name of file in which to write Postscript;
				 * NULL means return Postscript info as
				 * result. Malloc'ed. */
    char *channelName;		/* If -channel is specified, the name of
                                 * the channel to use. */
    Tcl_Channel chan;		/* Open channel corresponding to fileName. */
    Tcl_HashTable fontTable;	/* Hash table containing names of all font
				 * families used in output.  The hash table
				 * values are not used. */
    int prepass;		/* Non-zero means that we're currently in
				 * the pre-pass that collects font information,
				 * so the Postscript generated isn't
				 * relevant. */
    int prolog;			/* Non-zero means output should contain
				   the file prolog.ps in the header. */
    int noimages;
} TkPostscriptInfo;

#include "html.h"
static void HtmlPsOutText(Tcl_Interp *interp, HtmlBlock *pBlock );

double Html_PostscriptY(double y,TkPostscriptInfo* psinfo) {
  double ret=(((double)(psinfo->y2))-y);
  return ret; }
#define dHtml_PostscriptY(y,psinfo) (((double)((psinfo)->y2))-y)

/*
 * The table below provides a template that's used to process arguments
 * to the canvas "postscript" command and fill in TkPostscriptInfo
 * structures.
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_STRING, "-colormap", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, colorVar), 0},
    {TK_CONFIG_STRING, "-colormode", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, colorMode), 0},
    {TK_CONFIG_STRING, "-file", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, fileName), 0},
    {TK_CONFIG_STRING, "-channel", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, channelName), 0},
    {TK_CONFIG_STRING, "-fontmap", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, fontVar), 0},
    {TK_CONFIG_PIXELS, "-height", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, height), 0},
    {TK_CONFIG_ANCHOR, "-pageanchor", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, pageAnchor), 0},
    {TK_CONFIG_STRING, "-pageheight", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, pageHeightString), 0},
    {TK_CONFIG_STRING, "-pagewidth", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, pageWidthString), 0},
    {TK_CONFIG_STRING, "-pagex", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, pageXString), 0},
    {TK_CONFIG_STRING, "-pagey", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, pageYString), 0},
    {TK_CONFIG_BOOLEAN, "-prolog", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, prolog), 0},
    {TK_CONFIG_BOOLEAN, "-noimages", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, noimages), 0},
    {TK_CONFIG_BOOLEAN, "-rotate", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, rotate), 0},
    {TK_CONFIG_PIXELS, "-width", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, width), 0},
    {TK_CONFIG_PIXELS, "-x", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, x), 0},
    {TK_CONFIG_PIXELS, "-y", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TkPostscriptInfo, y), 0},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * The prolog data. Generated by str2c from prolog.ps
 * This was split in small chunks by str2c because
 * some C compiler have limitations on the size of static strings.
 * (str2c is a small tcl script in tcl's tool directory (source release))
 */
static CONST char * CONST  prolog[]= {
	/* Start of part 1 (2000 characters) */
	"%%BeginProlog\n\
50 dict begin\n\
\n\
% This is a standard prolog for Postscript generated by Tk's canvas\n\
% widget.\n\
% RCS: @(#) $Id: htmlPs.c,v 1.7 2005/03/22 12:07:34 danielk1977 Exp $\n\
\n\
% The definitions below just define all of the variables used in\n\
% any of the procedures here.  This is needed for obscure reasons\n\
% explained on p. 716 of the Postscript manual (Section H.2.7,\n\
% \"Initializing Variables,\" in the section on Encapsulated Postscript).\n\
\n\
/baseline 0 def\n\
/stipimage 0 def\n\
/height 0 def\n\
/justify 0 def\n\
/lineLength 0 def\n\
/spacing 0 def\n\
/stipple 0 def\n\
/strings 0 def\n\
/xoffset 0 def\n\
/yoffset 0 def\n\
/tmpstip null def\n\
\n\
% Define the array ISOLatin1Encoding (which specifies how characters are\n\
% encoded for ISO-8859-1 fonts), if it isn't already present (Postscript\n\
% level 2 is supposed to define it, but level 1 doesn't).\n\
\n\
systemdict /ISOLatin1Encoding known not {\n\
    /ISOLatin1Encoding [\n\
	/space /space /space /space /space /space /space /space\n\
	/space /space /space /space /space /space /space /space\n\
	/space /space /space /space /space /space /space /space\n\
	/space /space /space /space /space /space /space /space\n\
	/space /exclam /quotedbl /numbersign /dollar /percent /ampersand\n\
	    /quoteright\n\
	/parenleft /parenright /asterisk /plus /comma /minus /period /slash\n\
	/zero /one /two /three /four /five /six /seven\n\
	/eight /nine /colon /semicolon /less /equal /greater /question\n\
	/at /A /B /C /D /E /F /G\n\
	/H /I /J /K /L /M /N /O\n\
	/P /Q /R /S /T /U /V /W\n\
	/X /Y /Z /bracketleft /backslash /bracketright /asciicircum /underscore\n\
	/quoteleft /a /b /c /d /e /f /g\n\
	/h /i /j /k /l /m /n /o\n\
	/p /q /r /s /t /u /v /w\n\
	/x /y /z /braceleft /bar /braceright /asciitilde /space\n\
	/space /space /space /space /space /space /space /space\n\
	/space /space /space /space /space /space /space /space\n\
	/dotlessi /grave /acute /circumflex /tilde /macron /breve /dotaccent\n\
	/dieresis /space /ring /cedilla /space /hungarumlaut /ogonek /caron\n\
	/space /exclamdown /cent /sterling /currency /yen /brokenbar /section\n\
	/dieresis /copyright /ordfem",
	/* End of part 1 */

	/* Start of part 2 (2000 characters) */
	"inine /guillemotleft /logicalnot /hyphen\n\
	    /registered /macron\n\
	/degree /plusminus /twosuperior /threesuperior /acute /mu /paragraph\n\
	    /periodcentered\n\
	/cedillar /onesuperior /ordmasculine /guillemotright /onequarter\n\
	    /onehalf /threequarters /questiondown\n\
	/Agrave /Aacute /Acircumflex /Atilde /Adieresis /Aring /AE /Ccedilla\n\
	/Egrave /Eacute /Ecircumflex /Edieresis /Igrave /Iacute /Icircumflex\n\
	    /Idieresis\n\
	/Eth /Ntilde /Ograve /Oacute /Ocircumflex /Otilde /Odieresis /multiply\n\
	/Oslash /Ugrave /Uacute /Ucircumflex /Udieresis /Yacute /Thorn\n\
	    /germandbls\n\
	/agrave /aacute /acircumflex /atilde /adieresis /aring /ae /ccedilla\n\
	/egrave /eacute /ecircumflex /edieresis /igrave /iacute /icircumflex\n\
	    /idieresis\n\
	/eth /ntilde /ograve /oacute /ocircumflex /otilde /odieresis /divide\n\
	/oslash /ugrave /uacute /ucircumflex /udieresis /yacute /thorn\n\
	    /ydieresis\n\
    ] def\n\
} if\n\
\n\
% font ISOEncode font\n\
% This procedure changes the encoding of a font from the default\n\
% Postscript encoding to ISOLatin1.  It's typically invoked just\n\
% before invoking \"setfont\".  The body of this procedure comes from\n\
% Section 5.6.1 of the Postscript book.\n\
\n\
/ISOEncode {\n\
    dup length dict begin\n\
	{1 index /FID ne {def} {pop pop} ifelse} forall\n\
	/Encoding ISOLatin1Encoding def\n\
	currentdict\n\
    end\n\
\n\
    % I'm not sure why it's necessary to use \"definefont\" on this new\n\
    % font, but it seems to be important; just use the name \"Temporary\"\n\
    % for the font.\n\
\n\
    /Temporary exch definefont\n\
} bind def\n\
\n\
% StrokeClip\n\
%\n\
% This procedure converts the current path into a clip area under\n\
% the assumption of stroking.  It's a bit tricky because some Postscript\n\
% interpreters get errors during strokepath for dashed lines.  If\n\
% this happens then turn off dashes and try again.\n\
\n\
/StrokeClip {\n\
    {strokepath} stopped {\n\
	(This Postscript printer gets limitcheck overflows when) =\n\
	(stippling dashed lines;  lines will be printed solid instead.) =\n\
	[] 0 setdash strokepath} if\n\
    clip\n\
} bind def\n\
\n\
% d",
	/* End of part 2 */

	/* Start of part 3 (2000 characters) */
	"esiredSize EvenPixels closestSize\n\
%\n\
% The procedure below is used for stippling.  Given the optimal size\n\
% of a dot in a stipple pattern in the current user coordinate system,\n\
% compute the closest size that is an exact multiple of the device's\n\
% pixel size.  This allows stipple patterns to be displayed without\n\
% aliasing effects.\n\
\n\
/EvenPixels {\n\
    % Compute exact number of device pixels per stipple dot.\n\
    dup 0 matrix currentmatrix dtransform\n\
    dup mul exch dup mul add sqrt\n\
\n\
    % Round to an integer, make sure the number is at least 1, and compute\n\
    % user coord distance corresponding to this.\n\
    dup round dup 1 lt {pop 1} if\n\
    exch div mul\n\
} bind def\n\
\n\
% width height string StippleFill --\n\
%\n\
% Given a path already set up and a clipping region generated from\n\
% it, this procedure will fill the clipping region with a stipple\n\
% pattern.  \"String\" contains a proper image description of the\n\
% stipple pattern and \"width\" and \"height\" give its dimensions.  Each\n\
% stipple dot is assumed to be about one unit across in the current\n\
% user coordinate system.  This procedure trashes the graphics state.\n\
\n\
/StippleFill {\n\
    % The following code is needed to work around a NeWSprint bug.\n\
\n\
    /tmpstip 1 index def\n\
\n\
    % Change the scaling so that one user unit in user coordinates\n\
    % corresponds to the size of one stipple dot.\n\
    1 EvenPixels dup scale\n\
\n\
    % Compute the bounding box occupied by the path (which is now\n\
    % the clipping region), and round the lower coordinates down\n\
    % to the nearest starting point for the stipple pattern.  Be\n\
    % careful about negative numbers, since the rounding works\n\
    % differently on them.\n\
\n\
    pathbbox\n\
    4 2 roll\n\
    5 index div dup 0 lt {1 sub} if cvi 5 index mul 4 1 roll\n\
    6 index div dup 0 lt {1 sub} if cvi 6 index mul 3 2 roll\n\
\n\
    % Stack now: width height string y1 y2 x1 x2\n\
    % Below is a doubly-nested for loop to iterate across this area\n\
    % in units of the stipple pattern size, going up columns then\n\
    % acr",
	/* End of part 3 */

	/* Start of part 4 (2000 characters) */
	"oss rows, blasting out a stipple-pattern-sized rectangle at\n\
    % each position\n\
\n\
    6 index exch {\n\
	2 index 5 index 3 index {\n\
	    % Stack now: width height string y1 y2 x y\n\
\n\
	    gsave\n\
	    1 index exch translate\n\
	    5 index 5 index true matrix tmpstip imagemask\n\
	    grestore\n\
	} for\n\
	pop\n\
    } for\n\
    pop pop pop pop pop\n\
} bind def\n\
\n\
% -- AdjustColor --\n\
% Given a color value already set for output by the caller, adjusts\n\
% that value to a grayscale or mono value if requested by the CL\n\
% variable.\n\
\n\
/AdjustColor {\n\
    CL 2 lt {\n\
	currentgray\n\
	CL 0 eq {\n\
	    .5 lt {0} {1} ifelse\n\
	} if\n\
	setgray\n\
    } if\n\
} bind def\n\
\n\
% x y strings spacing xoffset yoffset justify stipple DrawText --\n\
% This procedure does all of the real work of drawing text.  The\n\
% color and font must already have been set by the caller, and the\n\
% following arguments must be on the stack:\n\
%\n\
% x, y -	Coordinates at which to draw text.\n\
% strings -	An array of strings, one for each line of the text item,\n\
%		in order from top to bottom.\n\
% spacing -	Spacing between lines.\n\
% xoffset -	Horizontal offset for text bbox relative to x and y: 0 for\n\
%		nw/w/sw anchor, -0.5 for n/center/s, and -1.0 for ne/e/se.\n\
% yoffset -	Vertical offset for text bbox relative to x and y: 0 for\n\
%		nw/n/ne anchor, +0.5 for w/center/e, and +1.0 for sw/s/se.\n\
% justify -	0 for left justification, 0.5 for center, 1 for right justify.\n\
% stipple -	Boolean value indicating whether or not text is to be\n\
%		drawn in stippled fashion.  If text is stippled,\n\
%		procedure StippleText must have been defined to call\n\
%		StippleFill in the right way.\n\
%\n\
% Also, when this procedure is invoked, the color and font must already\n\
% have been set for the text.\n\
\n\
/DrawText {\n\
    /stipple exch def\n\
    /justify exch def\n\
    /yoffset exch def\n\
    /xoffset exch def\n\
    /spacing exch def\n\
    /strings exch def\n\
\n\
    % First scan through all of the text to find the widest line.\n\
\n\
    /lineLength 0 def\n\
    strings {\n\
	stringwidth pop\n\
	dup lineLength gt {/lineLength exch def}",
	/* End of part 4 */

	/* Start of part 5 (1546 characters) */
	" {pop} ifelse\n\
	newpath\n\
    } forall\n\
\n\
    % Compute the baseline offset and the actual font height.\n\
\n\
    0 0 moveto (TXygqPZ) false charpath\n\
    pathbbox dup /baseline exch def\n\
    exch pop exch sub /height exch def pop\n\
    newpath\n\
\n\
    % Translate coordinates first so that the origin is at the upper-left\n\
    % corner of the text's bounding box. Remember that x and y for\n\
    % positioning are still on the stack.\n\
\n\
    translate\n\
    lineLength xoffset mul\n\
    strings length 1 sub spacing mul height add yoffset mul translate\n\
\n\
    % Now use the baseline and justification information to translate so\n\
    % that the origin is at the baseline and positioning point for the\n\
    % first line of text.\n\
\n\
    justify lineLength mul baseline neg translate\n\
\n\
    % Iterate over each of the lines to output it.  For each line,\n\
    % compute its width again so it can be properly justified, then\n\
    % display it.\n\
\n\
    strings {\n\
	dup stringwidth pop\n\
	justify neg mul 0 moveto\n\
	stipple {\n\
\n\
	    % The text is stippled, so turn it into a path and print\n\
	    % by calling StippledText, which in turn calls StippleFill.\n\
	    % Unfortunately, many Postscript interpreters will get\n\
	    % overflow errors if we try to do the whole string at\n\
	    % once, so do it a character at a time.\n\
\n\
	    gsave\n\
	    /char (X) def\n\
	    {\n\
		char 0 3 -1 roll put\n\
		currentpoint\n\
		gsave\n\
		char true charpath clip StippleText\n\
		grestore\n\
		char stringwidth translate\n\
		moveto\n\
	    } forall\n\
	    grestore\n\
	} {show} ifelse\n\
	0 spacing neg translate\n\
    } forall\n\
} bind def\n\
\n\
%%EndProlog\n\
",
	/* End of part 5 */

	NULL	/* End of data marker */
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		GetPostscriptPoints _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, double *doublePtr));
int
HtmlTk_PostscriptImage( Tk_Image image, Tcl_Interp *interp, Tk_Window tkwin,
    TkPostscriptInfo *psinfo, int x, int y, int width, int height, int prepass);


/*
 *--------------------------------------------------------------
 *
 * TkCanvPostscriptCmd --
 *
 *	This procedure is invoked to process the "postscript" options
 *	of the widget command for canvas widgets. See the user
 *	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

    /* ARGSUSED */
int
HtmlPostscript( HtmlWidget *htmlPtr, Tcl_Interp *interp, int argc,
    char **argv)
{
    int mpg=1;
    TkPostscriptInfo psInfo;
/*    Tk_PostscriptInfo oldInfoPtr; */
    int result;
#define STRING_LENGTH 400
    char string[STRING_LENGTH+1], *p;
    time_t now;
    size_t length;
    Tk_Window tkwin = htmlPtr->tkwin;
    int deltaX = 0, deltaY = 0;		/* Offset of lower-left corner of
					 * area to be marked up, measured
					 * in canvas units from the positioning
					 * point on the page (reflects
					 * anchor position).  Initial values
					 * needed only to stop compiler
					 * warnings. */
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    Tcl_DString buffer;
    CONST char * CONST *chunk;
    HtmlBlock *pB;

    /*
     *----------------------------------------------------------------
     * Initialize the data structure describing Postscript generation,
     * then process all the arguments to fill the data structure in.
     *----------------------------------------------------------------
     */

/*    oldInfoPtr = canvasPtr->psInfo;
    canvasPtr->psInfo = (Tk_PostscriptInfo) &psInfo; */
    psInfo.x = 0;
    psInfo.y = 0;
    psInfo.width = -1;
    psInfo.height = -1;
    psInfo.pageXString = NULL;
    psInfo.pageYString = NULL;
    psInfo.pageX = 72*4.25;
    psInfo.pageY = 72*5.5;
    psInfo.pageWidthString = NULL;
    psInfo.pageHeightString = NULL;
    psInfo.scale = 1.0;
    psInfo.pageAnchor = TK_ANCHOR_CENTER;
    psInfo.rotate = 0;
    psInfo.fontVar = NULL;
    psInfo.colorVar = NULL;
    psInfo.colorMode = NULL;
    psInfo.colorLevel = 0;
    psInfo.fileName = NULL;
    psInfo.channelName = NULL;
    psInfo.chan = NULL;
    psInfo.prepass = 0;
    psInfo.prolog = 1;
    Tcl_InitHashTable(&psInfo.fontTable, TCL_STRING_KEYS);
    result = Tk_ConfigureWidget(interp, tkwin,
	    configSpecs, argc-2, argv+2, (char *) &psInfo,
	    TK_CONFIG_ARGV_ONLY);
    if (result != TCL_OK) {
	goto cleanup;
    }

    if (psInfo.width == -1) {
/*	psInfo.width = Tk_Width(tkwin); */
	psInfo.width = htmlPtr->maxX;
    }
    if (psInfo.height == -1) {
        double dheight;
        psInfo.height = 72*11;
        if (psInfo.pageHeightString != NULL) {
	  if (GetPostscriptPoints(interp, psInfo.pageHeightString,
		&dheight) != TCL_OK) {
	    goto cleanup;
	  }
	  psInfo.height=(int)dheight;
        }
/*	psInfo.height = htmlPtr->maxY; */
/*	psInfo.height = Tk_Height(tkwin); */
    }
    psInfo.x2 = psInfo.x + psInfo.width;
    psInfo.y2 = psInfo.y + psInfo.height;

    if (psInfo.pageXString != NULL) {
	if (GetPostscriptPoints(interp, psInfo.pageXString,
		&psInfo.pageX) != TCL_OK) {
	    goto cleanup;
	}
    }
    if (psInfo.pageYString != NULL) {
	if (GetPostscriptPoints(interp, psInfo.pageYString,
		&psInfo.pageY) != TCL_OK) {
	    goto cleanup;
	}
    }
    if (psInfo.pageWidthString != NULL) {
	if (GetPostscriptPoints(interp, psInfo.pageWidthString,
		&psInfo.scale) != TCL_OK) {
	    goto cleanup;
	}
	psInfo.scale /= psInfo.width;
    } else if (psInfo.pageHeightString != NULL) {
	if (GetPostscriptPoints(interp, psInfo.pageHeightString,
		&psInfo.scale) != TCL_OK) {
	    goto cleanup;
	}
	psInfo.scale /= psInfo.height;
    } else {
	psInfo.scale = (72.0/25.4)*WidthMMOfScreen(Tk_Screen(tkwin));
	psInfo.scale /= WidthOfScreen(Tk_Screen(tkwin));
    }
    switch (psInfo.pageAnchor) {
	case TK_ANCHOR_NW:
	case TK_ANCHOR_W:
	case TK_ANCHOR_SW:
	    deltaX = 0;
	    break;
	case TK_ANCHOR_N:
	case TK_ANCHOR_CENTER:
	case TK_ANCHOR_S:
	    deltaX = -psInfo.width/2;
	    break;
	case TK_ANCHOR_NE:
	case TK_ANCHOR_E:
	case TK_ANCHOR_SE:
	    deltaX = -psInfo.width;
	    break;
    }
    switch (psInfo.pageAnchor) {
	case TK_ANCHOR_NW:
	case TK_ANCHOR_N:
	case TK_ANCHOR_NE:
	    deltaY = - psInfo.height;
	    break;
	case TK_ANCHOR_W:
	case TK_ANCHOR_CENTER:
	case TK_ANCHOR_E:
	    deltaY = -psInfo.height/2;
	    break;
	case TK_ANCHOR_SW:
	case TK_ANCHOR_S:
	case TK_ANCHOR_SE:
	    deltaY = 0;
	    break;
    }

    if (psInfo.colorMode == NULL) {
	psInfo.colorLevel = 2;
    } else {
	length = strlen(psInfo.colorMode);
	if (strncmp(psInfo.colorMode, "monochrome", length) == 0) {
	    psInfo.colorLevel = 0;
	} else if (strncmp(psInfo.colorMode, "gray", length) == 0) {
	    psInfo.colorLevel = 1;
	} else if (strncmp(psInfo.colorMode, "color", length) == 0) {
	    psInfo.colorLevel = 2;
	} else {
	    Tcl_AppendResult(interp, "bad color mode \"",
		    psInfo.colorMode, "\": must be monochrome, ",
		    "gray, or color", (char *) NULL);
	    goto cleanup;
	}
    }

    if (psInfo.fileName != NULL) {

        /*
         * Check that -file and -channel are not both specified.
         */

        if (psInfo.channelName != NULL) {
            Tcl_AppendResult(interp, "can't specify both -file",
                    " and -channel", (char *) NULL);
            result = TCL_ERROR;
            goto cleanup;
        }

        /*
         * Check that we are not in a safe interpreter. If we are, disallow
         * the -file specification.
         */

        if (Tcl_IsSafe(interp)) {
            Tcl_AppendResult(interp, "can't specify -file in a",
                    " safe interpreter", (char *) NULL);
            result = TCL_ERROR;
            goto cleanup;
        }
        
	p = Tcl_TranslateFileName(interp, psInfo.fileName, &buffer);
	if (p == NULL) {
	    goto cleanup;
	}
	psInfo.chan = Tcl_OpenFileChannel(interp, p, "w", 0666);
	Tcl_DStringFree(&buffer);
	if (psInfo.chan == NULL) {
	    goto cleanup;
	}
    }

    if (psInfo.channelName != NULL) {
        int mode;
        
        /*
         * Check that the channel is found in this interpreter and that it
         * is open for writing.
         */

        psInfo.chan = Tcl_GetChannel(interp, psInfo.channelName,
                &mode);
        if (psInfo.chan == (Tcl_Channel) NULL) {
            result = TCL_ERROR;
            goto cleanup;
        }
        if ((mode & TCL_WRITABLE) == 0) {
            Tcl_AppendResult(interp, "channel \"",
                    psInfo.channelName, "\" wasn't opened for writing",
                    (char *) NULL);
            result = TCL_ERROR;
            goto cleanup;
        }
    }
    
    /*
     *--------------------------------------------------------
     * Make a pre-pass over all of the items, generating Postscript
     * and then throwing it away.  The purpose of this pass is just
     * to collect information about all the fonts in use, so that
     * we can output font information in the proper form required
     * by the Document Structuring Conventions.
     *--------------------------------------------------------
     */

    psInfo.prepass = 1;

    for(pB=htmlPtr->firstBlock; pB; pB=pB->pNext){
        HtmlElement *pElem;
	if ((pB->left >= psInfo.x2) || (pB->right < psInfo.x)
		|| (pB->bottom >= psInfo.y2) || (pB->bottom < psInfo.y)) {
	    continue;
	}
	result = HtmlPostscriptProc(interp, htmlPtr, pB, 1, &psInfo);
	Tcl_ResetResult(interp);
	if (result != TCL_OK) {
	    /*
	     * An error just occurred.  Just skip out of this loop.
	     * There's no need to report the error now;  it can be
	     * reported later (errors can happen later that don't
	     * happen now, so we still have to check for errors later
	     * anyway).
	     */
	    break;
	}
    }
    psInfo.prepass = 0;

    /*
     *--------------------------------------------------------
     * Generate the header and prolog for the Postscript.
     *--------------------------------------------------------
     */

    if (psInfo.prolog) {
    Tcl_AppendResult(interp, "%!PS-Adobe-3.0 EPSF-3.0\n",
	    "%%Creator: Tk Canvas Widget\n", (char *) NULL);
#ifdef HAVE_PW_GECOS
    if (!Tcl_IsSafe(interp)) {
	struct passwd *pwPtr = getpwuid(getuid());	/* INTL: Native. */
	Tcl_AppendResult(interp, "%%For: ",
		(pwPtr != NULL) ? pwPtr->pw_gecos : "Unknown", "\n",
		(char *) NULL);
	endpwent();
    }
#endif /* HAVE_PW_GECOS */
    Tcl_AppendResult(interp, "%%Title: Window ",
	    Tk_PathName(tkwin), "\n", (char *) NULL);
    time(&now);
    Tcl_AppendResult(interp, "%%CreationDate: ",
	    ctime(&now), (char *) NULL);		/* INTL: Native. */
    if (!psInfo.rotate) {
	sprintf(string, "%d %d %d %d",
		(int) (psInfo.pageX + psInfo.scale*deltaX),
		(int) (psInfo.pageY + psInfo.scale*deltaY),
		(int) (psInfo.pageX + psInfo.scale*(deltaX + psInfo.width)
			+ 1.0),
		(int) (psInfo.pageY + psInfo.scale*(deltaY + psInfo.height)
			+ 1.0));
    } else {
	sprintf(string, "%d %d %d %d",
		(int) (psInfo.pageX - psInfo.scale*(deltaY + psInfo.height)),
		(int) (psInfo.pageY + psInfo.scale*deltaX),
		(int) (psInfo.pageX - psInfo.scale*deltaY + 1.0),
		(int) (psInfo.pageY + psInfo.scale*(deltaX + psInfo.width)
			+ 1.0));
    }
    Tcl_AppendResult(interp, "%%BoundingBox: ", string,
	    "\n", (char *) NULL);
    sprintf(string, "%%%%Pages: 1\n %%%%DocumentData: Clean7Bit\n", 
	htmlPtr->maxY/psInfo.height + 1);
    Tcl_AppendResult(interp, string, (char *) NULL);
    Tcl_AppendResult(interp, "%%Orientation: ",
	    psInfo.rotate ? "Landscape\n" : "Portrait\n", (char *) NULL);
    p = "%%DocumentNeededResources: font ";
    for (hPtr = Tcl_FirstHashEntry(&psInfo.fontTable, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	Tcl_AppendResult(interp, p,
		Tcl_GetHashKey(&psInfo.fontTable, hPtr),
		"\n", (char *) NULL);
	p = "%%+ font ";
    }
    Tcl_AppendResult(interp, "%%EndComments\n\n", (char *) NULL);

    /*
     * Insert the prolog
     */
    for (chunk=prolog; *chunk; chunk++) {
	Tcl_AppendResult(interp, *chunk, (char *) NULL);
    }


    if (psInfo.chan != NULL) {
	Tcl_Write(psInfo.chan, Tcl_GetStringResult(interp), -1);
	Tcl_ResetResult(htmlPtr->interp);
    }
    /*
     *-----------------------------------------------------------
     * Document setup:  set the color level and include fonts.
     *-----------------------------------------------------------
     */

    sprintf(string, "/CL %d def\n", psInfo.colorLevel);
    Tcl_AppendResult(interp, "%%BeginSetup\n", string,
	    (char *) NULL);
    for (hPtr = Tcl_FirstHashEntry(&psInfo.fontTable, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	Tcl_AppendResult(interp, "%%IncludeResource: font ",
		Tcl_GetHashKey(&psInfo.fontTable, hPtr), "\n", (char *) NULL);
    }
    Tcl_AppendResult(interp, "%%EndSetup\n\n", (char *) NULL);

    /*
     *-----------------------------------------------------------
     * Page setup:  move to page positioning point, rotate if
     * needed, set scale factor, offset for proper anchor position,
     * and set clip region.
     *-----------------------------------------------------------
     */

    Tcl_AppendResult(interp, "%%Page: 1 1\n", "save\n",
	    (char *) NULL);
    sprintf(string, "%.1f %.1f translate\n", psInfo.pageX, psInfo.pageY);
    Tcl_AppendResult(interp, string, (char *) NULL);
    if (psInfo.rotate) {
	Tcl_AppendResult(interp, "90 rotate\n", (char *) NULL);
    }
    sprintf(string, "%.4g %.4g scale\n", psInfo.scale, psInfo.scale);
    Tcl_AppendResult(interp, string, (char *) NULL);
    sprintf(string, "%d %d translate\n", deltaX - psInfo.x, deltaY);
    Tcl_AppendResult(interp, string, (char *) NULL);
    sprintf(string, "%d %.15g moveto %d %.15g lineto %d %.15g lineto %d %.15g",
	    psInfo.x,
	    Html_PostscriptY((double) psInfo.y, &psInfo),
	    psInfo.x2,
	    Html_PostscriptY((double) psInfo.y, &psInfo),
	    psInfo.x2, 
	    Html_PostscriptY((double) psInfo.y2, &psInfo),
	    psInfo.x,
	    Html_PostscriptY((double) psInfo.y2, &psInfo));
    Tcl_AppendResult(interp, string,
	" lineto closepath clip newpath\n", (char *) NULL);
    }
    if (psInfo.chan != NULL) {
	Tcl_Write(psInfo.chan, Tcl_GetStringResult(interp), -1);
	Tcl_ResetResult(htmlPtr->interp);
    }

    /*
     *---------------------------------------------------------------------
     * Iterate through all the items, having each relevant one draw itself.
     * Quit if any of the items returns an error.
     *---------------------------------------------------------------------
     */

    result = TCL_OK;
    while (1) {
     char *nxtpage = "showpage\n%%%%Page: %d %d\n%%%%BeginPageSetup"
"/pagelevel save def\n54 0 translate\n%%%%EndPageSetup"
"newpath 0 72 moveto 504 0 rlineto 0 648 rlineto -504 0 rlineto  closepath clip newpath\n";

      for(pB=htmlPtr->firstBlock; pB; pB=pB->pNext){

        HtmlElement *pElem;
	if ((pB->left >= psInfo.x2) || (pB->right < psInfo.x)
		|| (pB->bottom >= psInfo.y2) || (pB->bottom < psInfo.y)) {
	    continue;
	}
        Tcl_AppendResult(htmlPtr->interp, "gsave\n", (char *) NULL);
	result = HtmlPostscriptProc(interp, htmlPtr, pB, 0, &psInfo);
	if (result != TCL_OK) {
	    char msg[64 + TCL_INTEGER_SPACE];

	    sprintf(msg, "\n    (generating Postscript for item)");
	    Tcl_AddErrorInfo(interp, msg);
	    goto cleanup;
	}
	Tcl_AppendResult(interp, "grestore\n", (char *) NULL);
	if (psInfo.chan != NULL) {
	    Tcl_Write(psInfo.chan, Tcl_GetStringResult(interp), -1);
	    Tcl_ResetResult(interp);
	}
      }
      if (psInfo.y2<htmlPtr->maxY) {
        psInfo.y+=psInfo.height;
        psInfo.y2+=psInfo.height;
	mpg++;
	sprintf(string, nxtpage, mpg, mpg);
        Tcl_AppendResult(interp, string, (char *) NULL);
      } else break;
    }

    /*
     *---------------------------------------------------------------------
     * Output page-end information, such as commands to print the page
     * and document trailer stuff.
     *---------------------------------------------------------------------
     */

    if (psInfo.prolog) {
      Tcl_AppendResult(interp, "restore showpage\n\n",
	    "%%Trailer\nend\n%%EOF\n", (char *) NULL);
    }
    if (psInfo.chan != NULL) {
	Tcl_Write(psInfo.chan, Tcl_GetStringResult(interp), -1);
	Tcl_ResetResult(htmlPtr->interp);
    }

    /*
     * Clean up psInfo to release malloc'ed stuff.
     */

    cleanup:
    if (psInfo.pageXString != NULL) {
	HtmlFree(psInfo.pageXString);
    }
    if (psInfo.pageYString != NULL) {
	HtmlFree(psInfo.pageYString);
    }
    if (psInfo.pageWidthString != NULL) {
	HtmlFree(psInfo.pageWidthString);
    }
    if (psInfo.pageHeightString != NULL) {
	HtmlFree(psInfo.pageHeightString);
    }
    if (psInfo.fontVar != NULL) {
	HtmlFree(psInfo.fontVar);
    }
    if (psInfo.colorVar != NULL) {
	HtmlFree(psInfo.colorVar);
    }
    if (psInfo.colorMode != NULL) {
	HtmlFree(psInfo.colorMode);
    }
    if (psInfo.fileName != NULL) {
	HtmlFree(psInfo.fileName);
    }
    if ((psInfo.chan != NULL) && (psInfo.channelName == NULL)) {
	Tcl_Close(interp, psInfo.chan);
    }
    if (psInfo.channelName != NULL) {
        HtmlFree(psInfo.channelName);
    }
    Tcl_DeleteHashTable(&psInfo.fontTable);
/*    canvasPtr->psInfo = (Tk_PostscriptInfo) oldInfoPtr; */
    return result;
}

/* The rest of these just in here cause they are static in Tk. */
/*
 *--------------------------------------------------------------
 *
 * GetPostscriptPoints --
 *
 *	Given a string, returns the number of Postscript points
 *	corresponding to that string.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	screen distance is stored at *doublePtr;  otherwise
 *	TCL_ERROR is returned and an error message is left in
 *	the interp's result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
GetPostscriptPoints(
    Tcl_Interp *interp,		/* Use this for error reporting. */
    char *string,		/* String describing a screen distance. */
    double *doublePtr)		/* Place to store converted result. */
{
    char *end;
    double d;

    d = strtod(string, &end);
    if (end == string) {
	error:
	Tcl_AppendResult(interp, "bad distance \"", string,
		"\"", (char *) NULL);
	return TCL_ERROR;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    switch (*end) {
	case 'c':
	    d *= 72.0/2.54;
	    end++;
	    break;
	case 'i':
	    d *= 72.0;
	    end++;
	    break;
	case 'm':
	    d *= 72.0/25.4;
	    end++;
	    break;
	case 0:
	    break;
	case 'p':
	    end++;
	    break;
	default:
	    goto error;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    if (*end != 0) {
	goto error;
    }
    *doublePtr = d;
    return TCL_OK;
}

static int
Html_PostscriptFont(
    Tcl_Interp *interp,
    TkPostscriptInfo *psInfo,           /* Postscript Info. */
    Tk_Font tkfont)                     /* Information about font in which text
                                         * is to be printed. */
{
    TkPostscriptInfo *psInfoPtr = psInfo;
    char *end;
    char pointString[TCL_INTEGER_SPACE];
    Tcl_DString ds;
    int i, points;

    /*
     * First, look up the font's name in the font map, if there is one.
     * If there is an entry for this font, it consists of a list
     * containing font name and size.  Use this information.
     */

    Tcl_DStringInit(&ds);

    if (psInfoPtr->fontVar != NULL) {
        char *list, **argv;
        int argc;
        double size;
        char *name;

        name = Tk_NameOfFont(tkfont);
        list = Tcl_GetVar2(interp, psInfoPtr->fontVar, name, 0);
        if (list != NULL) {
            if (Tcl_SplitList(interp, list, &argc, &argv) != TCL_OK) {
                badMapEntry:
                Tcl_ResetResult(interp);
                Tcl_AppendResult(interp, "bad font map entry for \"", name,
                        "\": \"", list, "\"", (char *) NULL);
                return TCL_ERROR;
            }
            if (argc != 2) {
                goto badMapEntry;
            }
            size = strtod(argv[1], &end);
            if ((size <= 0) || (*end != 0)) {
                goto badMapEntry;
            }

            Tcl_DStringAppend(&ds, argv[0], -1);
            points = (int) size;

            HtmlFree((char *) argv);
            goto findfont;
        }
    }

    points = Tk_PostscriptFontName(tkfont, &ds);
    findfont:
    sprintf(pointString, "%d", points);
    Tcl_AppendResult(interp, "/", Tcl_DStringValue(&ds), " findfont ",
            pointString, " scalefont ", (char *) NULL);
    if (strncasecmp(Tcl_DStringValue(&ds), "Symbol", 7) != 0) {
        Tcl_AppendResult(interp, "ISOEncode ", (char *) NULL);
    }
    Tcl_AppendResult(interp, "setfont\n", (char *) NULL);
    Tcl_CreateHashEntry(&psInfoPtr->fontTable, Tcl_DStringValue(&ds), &i);
    Tcl_DStringFree(&ds);

    return TCL_OK;
}

static int
Html_PostscriptBitmap(
    Tcl_Interp *interp,
    Tk_Window tkwin,
    TkPostscriptInfo *psInfo,           /* Postscript info. */
    Pixmap bitmap,                      /* Bitmap for which to generate
                                         * Postscript. */
    int startX, int startY,                 /* Coordinates of upper-left corner
                                         * of rectangular region to output. */
    int width, int height)                  /* Height of rectangular region. */
{
    TkPostscriptInfo *psInfoPtr = psInfo;
    XImage *imagePtr;
    int charsInLine, x, y, lastX, lastY, value, mask;
    unsigned int totalWidth, totalHeight;
    char string[100];
    Window dummyRoot;
    int dummyX, dummyY;
    unsigned dummyBorderwidth, dummyDepth;

    if (psInfoPtr->prepass) {
        return TCL_OK;
    }

    /*
     * The following call should probably be a call to Tk_SizeOfBitmap
     * instead, but it seems that we are occasionally invoked by custom
     * item types that create their own bitmaps without registering them
     * with Tk.  XGetGeometry is a bit slower than Tk_SizeOfBitmap, but
     * it shouldn't matter here.
     */

    XGetGeometry(Tk_Display(tkwin), bitmap, &dummyRoot,
            (int *) &dummyX, (int *) &dummyY, (unsigned int *) &totalWidth,
            (unsigned int *) &totalHeight, &dummyBorderwidth, &dummyDepth);
    imagePtr = XGetImage(Tk_Display(tkwin), bitmap, 0, 0,
            totalWidth, totalHeight, 1, XYPixmap);
    Tcl_AppendResult(interp, "<", (char *) NULL);
    mask = 0x80;
    value = 0;
    charsInLine = 0;
    lastX = startX + width - 1;
    lastY = startY + height - 1;
    for (y = lastY; y >= startY; y--) {
        for (x = startX; x <= lastX; x++) {
            if (XGetPixel(imagePtr, x, y)) {
                value |= mask;
            }
            mask >>= 1;
            if (mask == 0) {
                sprintf(string, "%02x", value);
                Tcl_AppendResult(interp, string, (char *) NULL);
                mask = 0x80;
                value = 0;
                charsInLine += 2;
                if (charsInLine >= 60) {
                    Tcl_AppendResult(interp, "\n", (char *) NULL);
                    charsInLine = 0;
                }
            }
        }
        if (mask != 0x80) {
            sprintf(string, "%02x", value);
            Tcl_AppendResult(interp, string, (char *) NULL);
            mask = 0x80;
            value = 0;
            charsInLine += 2;
        }
    }
    Tcl_AppendResult(interp, ">", (char *) NULL);
    XDestroyImage(imagePtr);
    return TCL_OK;
}

int
Html_PostscriptColor(
    Tcl_Interp *interp,
    TkPostscriptInfo *psInfo,           /* Postscript info. */
    XColor *colorPtr)                   /* Information about color. */
{
    TkPostscriptInfo *psInfoPtr = psInfo;
    int tmp;
    double red, green, blue;
    char string[200];

    if (psInfoPtr->prepass) {
        return TCL_OK;
    }

    /*
     * If there is a color map defined, then look up the color's name
     * in the map and use the Postscript commands found there, if there
     * are any.
     */

    if (psInfoPtr->colorVar != NULL) {
        char *cmdString;

        cmdString = Tcl_GetVar2(interp, psInfoPtr->colorVar,
                Tk_NameOfColor(colorPtr), 0);
        if (cmdString != NULL) {
            Tcl_AppendResult(interp, cmdString, "\n", (char *) NULL);
            return TCL_OK;
        }
    }

    /*
     * No color map entry for this color.  Grab the color's intensities
     * and output Postscript commands for them.  Special note:  X uses
     * a range of 0-65535 for intensities, but most displays only use
     * a range of 0-255, which maps to (0, 256, 512, ... 65280) in the
     * X scale.  This means that there's no way to get perfect white,
     * since the highest intensity is only 65280 out of 65535.  To
     * work around this problem, rescale the X intensity to a 0-255
     * scale and use that as the basis for the Postscript colors.  This
     * scheme still won't work if the display only uses 4 bits per color,
     * but most diplays use at least 8 bits.
     */

    tmp = colorPtr->red;
    red = ((double) (tmp >> 8))/255.0;
    tmp = colorPtr->green;
    green = ((double) (tmp >> 8))/255.0;
    tmp = colorPtr->blue;
    blue = ((double) (tmp >> 8))/255.0;
    sprintf(string, "%.3f %.3f %.3f setrgbcolor AdjustColor\n",
            red, green, blue);
    Tcl_AppendResult(interp, string, (char *) NULL);
    return TCL_OK;
}

static int
Html_PostscriptStipple(
    Tcl_Interp *interp,
    Tk_Window tkwin,
    TkPostscriptInfo *psInfo,           /* Interpreter for returning Postscript
                                         * or error message. */
    Pixmap bitmap)                      /* Bitmap to use for stippling. */
{
    TkPostscriptInfo *psInfoPtr = psInfo;
    int width, height;
    char string[TCL_INTEGER_SPACE * 2];
    Window dummyRoot;
    int dummyX, dummyY;
    unsigned dummyBorderwidth, dummyDepth;

    if (psInfoPtr->prepass) {
        return TCL_OK;
    }

    /*
     * The following call should probably be a call to Tk_SizeOfBitmap
     * instead, but it seems that we are occasionally invoked by custom
     * item types that create their own bitmaps without registering them
     * with Tk.  XGetGeometry is a bit slower than Tk_SizeOfBitmap, but
     * it shouldn't matter here.
     */

    XGetGeometry(Tk_Display(tkwin), bitmap, &dummyRoot,
            (int *) &dummyX, (int *) &dummyY, (unsigned *) &width,
            (unsigned *) &height, &dummyBorderwidth, &dummyDepth);
    sprintf(string, "%d %d ", width, height);
    Tcl_AppendResult(interp, string, (char *) NULL);
    if (Html_PostscriptBitmap(interp, tkwin, psInfo, bitmap, 0, 0,
            width, height) != TCL_OK) {
        return TCL_ERROR;
    }
    Tcl_AppendResult(interp, " StippleFill\n", (char *) NULL);
    return TCL_OK;
}


static int
TextToPostscript(
    Tcl_Interp *interp,		/* Leave Postscript or error message here. */
    HtmlWidget * htmlPtr,
    HtmlBlock * pB,
    int prepass,		/* 1 means this is a prepass to collect */
    TkPostscriptInfo *psInfo)	/* font information; 0 means final Postscript*/
				 /* is being created. */
{
    HtmlElement * pElem=pB->base.pNext;
    /*TextItem *textPtr = (TextItem *) itemPtr; */
    int x, y;
    Tk_FontMetrics fm;
    char *justify;
    char buffer[500];
    XColor *color;
    Pixmap stipple;
    Tk_Font tkfont;
    color = htmlPtr->apColor[pElem->base.style.color];
/*    stipple = textPtr->stipple; */
    stipple = None;
    if (color == NULL || pElem->text.zText[0]==0)
	return TCL_OK;

    tkfont=HtmlGetFont(htmlPtr,pElem->base.style.font);
    if (Html_PostscriptFont(interp, psInfo, tkfont) != TCL_OK) {
	return TCL_ERROR;
    }
    if (prepass != 0) {
	return TCL_OK;
    }
    if (Html_PostscriptColor(interp, psInfo, color) != TCL_OK) {
	return TCL_ERROR;
    }
    if (stipple != None) {
	Tcl_AppendResult(interp, "/StippleText {\n    ",
		(char *) NULL);
	Html_PostscriptStipple(interp, htmlPtr->tkwin, psInfo, stipple);
	Tcl_AppendResult(interp, "} bind def\n", (char *) NULL);
    }

    sprintf(buffer, "%.15g %.15g [\n", (double)pB->left,
	    Html_PostscriptY((double)pB->top,psInfo));
    Tcl_AppendResult(interp, buffer, (char *) NULL);

/*    Tk_TextLayoutToPostscript(interp, textPtr->textLayout); */
    HtmlPsOutText(interp, pB);

    x = 0;  y = 0;  justify = "0";	/* lint. */
/*    switch (textPtr->anchor) {
	case TK_ANCHOR_NW:	x = 0; y = 0;	break;
	case TK_ANCHOR_N:	x = 1; y = 0;	break;
	case TK_ANCHOR_NE:	x = 2; y = 0;	break;
	case TK_ANCHOR_E:	x = 2; y = 1;	break;
	case TK_ANCHOR_SE:	x = 2; y = 2;	break;
	case TK_ANCHOR_S:	x = 1; y = 2;	break;
	case TK_ANCHOR_SW:	x = 0; y = 2;	break;
	case TK_ANCHOR_W:	x = 0; y = 1;	break;
	case TK_ANCHOR_CENTER:	x = 1; y = 1;	break;
    }
    switch (textPtr->justify) {
        case TK_JUSTIFY_LEFT:	justify = "0";	break;
	case TK_JUSTIFY_CENTER: justify = "0.5";break;
	case TK_JUSTIFY_RIGHT:  justify = "1";	break;
    } */

    Tk_GetFontMetrics(tkfont, &fm);
    sprintf(buffer, "] %d %g %g %s %s DrawText\n",
	    fm.linespace, x / -2.0, y / 2.0, justify,
	    ((stipple == None) ? "false" : "true"));
    Tcl_AppendResult(interp, buffer, (char *) NULL);

    return TCL_OK;
}

static void HtmlPsOutText(Tcl_Interp *interp, HtmlBlock *pBlock ) {
#define MAXUSE 128
  char buf[MAXUSE+10];
#ifdef NEWTK
  Tcl_UniChar ch;
#else
  int ch;
#endif
  char *p;
  int j, len, used=0, c;

  if (!pBlock->z) return;
  if (!pBlock->n) return;
  len=pBlock->n;
  p = pBlock->z;
  buf[used++]='(';
  for (j = 0; j < len;  j++) {
        /*
         * INTL: For now we just treat the characters as binary
         * data and display the lower byte.  Eventually this should
         * be revised to handle international postscript fonts.
         */

#ifdef NEWTK
        p += Tcl_UtfToUniChar(p, &ch);
#else
        ch=*p++;
#endif
        c = UCHAR(ch & 0xff);
        if ((c == '(') || (c == ')') || (c == '\\') || (c < 0x20)
                || (c >= UCHAR(0x7f))) {
            /*
             * Tricky point:  the "03" is necessary in the sprintf
             * below, so that a full three digits of octal are
             * always generated.  Without the "03", a number
             * following this sequence could be interpreted by
             * Postscript as part of this sequence.
             */

            sprintf(buf + used, "\\%03o", c);
            used += 4;
        } else {
            buf[used++] = c;
        }
        if (used >= MAXUSE) {
            buf[used] = '\0';
            Tcl_AppendResult(interp, buf, (char *) NULL);
            used = 0;
        }
  }
  buf[used++] = ')';
  buf[used++] = '\n';
  buf[used] = '\0';
  Tcl_AppendResult(interp, buf, (char *) NULL);
}

int HtmlPostscriptProc(Tcl_Interp *interp, HtmlWidget *htmlPtr, 
  HtmlBlock *pB, int prepass, TkPostscriptInfo* psInfo) {
  HtmlElement *pe;
  if( pB->base.style.flags & STY_Invisible ){ return TCL_OK; }
  pe=pB->base.pNext;
  switch (pe->base.type) {
     case Html_TEXTAREA:
       break;
     case Html_INPUT:
       /* Config(pe->input.tkwin); */
       break;
     case Html_SELECT:
       break;
     case Html_Text: 
       if (pB->n>0)return TextToPostscript(interp, htmlPtr, pB, prepass,psInfo);
       break;
     case Html_IMG: 
        if (psInfo->noimages) break;
	return HtmlTk_PostscriptImage(pe->image.pImage->image, interp,
	  htmlPtr->tkwin, psInfo,
	  pe->image.x, pe->image.y, pe->image.w, pe->image.h, prepass);
       break;
  }
  return TCL_OK;
}

static void
TkImageGetColor(
    TkColormapData *cdata,              /* Colormap data */
    unsigned long pixel,                /* Pixel value to look up */
    double *red, double *green, double *blue)         /* Color data to return */
{
    if (cdata->separated) {
        int r = (pixel & cdata->red_mask) >> cdata->red_shift;
        int g = (pixel & cdata->green_mask) >> cdata->green_shift;
        int b = (pixel & cdata->blue_mask) >> cdata->blue_shift;
        *red = cdata->colors[r].red / 65535.0;
        *green = cdata->colors[g].green / 65535.0;
        *blue = cdata->colors[b].blue / 65535.0;
    } else {
        *red = cdata->colors[pixel].red / 65535.0;
        *green = cdata->colors[pixel].green / 65535.0;
        *blue = cdata->colors[pixel].blue / 65535.0;
    }
}

static int
HtmlTkPostscriptImage(
    Tcl_Interp *interp,
    Tk_Window tkwin,
    TkPostscriptInfo *psInfo,	/* postscript info */
    XImage *ximage,		/* Image to draw */
    int x, int y,		/* First pixel to output */
    int width, int height)	/* Width and height of area */
{
    TkPostscriptInfo *psInfoPtr = (TkPostscriptInfo *) psInfo;
    char buffer[256];
    int xx, yy, band, maxRows;
    double red, green, blue;
    int bytesPerLine=0, maxWidth=0;
    int level = psInfoPtr->colorLevel;
    Colormap cmap;
    int i, depth, ncolors;
    Visual *visual;
    TkColormapData cdata;

    if (psInfoPtr->prepass) {
	return TCL_OK;
    }

    cmap = Tk_Colormap(tkwin);
    depth = Tk_Depth(tkwin);
    visual = Tk_Visual(tkwin);

    /*
     * Obtain information about the colormap, ie the mapping between
     * pixel values and RGB values.  The code below should work
     * for all Visual types.
     */

    ncolors = visual->map_entries;
    cdata.colors = (XColor *) HtmlAlloc(sizeof(XColor) * ncolors);
    cdata.ncolors = ncolors;

    if (visual->class == DirectColor || visual->class == TrueColor) {
	cdata.separated = 1;
	cdata.red_mask = visual->red_mask;
	cdata.green_mask = visual->green_mask;
	cdata.blue_mask = visual->blue_mask;
	cdata.red_shift = 0;
	cdata.green_shift = 0;
	cdata.blue_shift = 0;
	while ((0x0001 & (cdata.red_mask >> cdata.red_shift)) == 0)
	    cdata.red_shift ++;
	while ((0x0001 & (cdata.green_mask >> cdata.green_shift)) == 0)
	    cdata.green_shift ++;
	while ((0x0001 & (cdata.blue_mask >> cdata.blue_shift)) == 0)
	    cdata.blue_shift ++;
	for (i = 0; i < ncolors; i ++)
	    cdata.colors[i].pixel =
		((i << cdata.red_shift) & cdata.red_mask) |
		((i << cdata.green_shift) & cdata.green_mask) |
		((i << cdata.blue_shift) & cdata.blue_mask);
    } else {
	cdata.separated=0;
	for (i = 0; i < ncolors; i ++)
	    cdata.colors[i].pixel = i;
    }
    if (visual->class == StaticGray || visual->class == GrayScale)
	cdata.color = 0;
    else
	cdata.color = 1;


    XQueryColors(Tk_Display(tkwin), cmap, cdata.colors, ncolors);

    /*
     * Figure out which color level to use (possibly lower than the 
     * one specified by the user).  For example, if the user specifies
     * color with monochrome screen, use gray or monochrome mode instead. 
     */

    if (!cdata.color && level == 2) {
	level = 1;
    }

    if (!cdata.color && cdata.ncolors == 2) {
	level = 0;
    }

    /*
     * Check that at least one row of the image can be represented
     * with a string less than 64 KB long (this is a limit in the 
     * Postscript interpreter).
     */

    switch (level)
    {
	case 0: bytesPerLine = (width + 7) / 8;  maxWidth = 240000;  break;
	case 1: bytesPerLine = width;  maxWidth = 60000;  break;
	case 2: bytesPerLine = 3 * width;  maxWidth = 20000;  break;
    }

    if (bytesPerLine > 60000) {
	Tcl_ResetResult(interp);
	sprintf(buffer,
		"Can't generate Postscript for images more than %d pixels wide",
		maxWidth);
	Tcl_AppendResult(interp, buffer, (char *) NULL);
	HtmlFree((char *) cdata.colors);
	return TCL_ERROR;
    }

    maxRows = 60000 / bytesPerLine;

    for (band = height-1; band >= 0; band -= maxRows) {
	int rows = (band >= maxRows) ? maxRows : band + 1;
	int lineLen = 0;
	switch (level) {
	    case 0:
		sprintf(buffer, "%d %d 1 matrix {\n<", width, rows);
		Tcl_AppendResult(interp, buffer, (char *) NULL);
		break;
	    case 1:
		sprintf(buffer, "%d %d 8 matrix {\n<", width, rows);
		Tcl_AppendResult(interp, buffer, (char *) NULL);
		break;
	    case 2:
		sprintf(buffer, "%d %d 8 matrix {\n<",
			width, rows);
		Tcl_AppendResult(interp, buffer, (char *) NULL);
		break;
	}
	for (yy = band; yy > band - rows; yy--) {
	    switch (level) {
		case 0: {
		    /*
		     * Generate data for image in monochrome mode.
		     * No attempt at dithering is made--instead, just
		     * set a threshold.
		     */
		    unsigned char mask=0x80;
		    unsigned char data=0x00;
		    for (xx = x; xx< x+width; xx++) {
			TkImageGetColor(&cdata, XGetPixel(ximage, xx, yy),
					&red, &green, &blue);
			if (0.30 * red + 0.59 * green + 0.11 * blue > 0.5)
			    data |= mask;
			mask >>= 1;
			if (mask == 0) {
			    sprintf(buffer, "%02X", data);
			    Tcl_AppendResult(interp, buffer, (char *) NULL);
			    lineLen += 2;
			    if (lineLen > 60) {
			        lineLen = 0;
			        Tcl_AppendResult(interp, "\n", (char *) NULL);
			    }
			    mask=0x80;
			    data=0x00;
			}
		    }
		    if ((width % 8) != 0) {
		        sprintf(buffer, "%02X", data);
		        Tcl_AppendResult(interp, buffer, (char *) NULL);
		        mask=0x80;
		        data=0x00;
		    }
		    break;
		}
		case 1: {
		    /*
		     * Generate data in gray mode--in this case, take a 
		     * weighted sum of the red, green, and blue values.
		     */
		    for (xx = x; xx < x+width; xx ++) {
			TkImageGetColor(&cdata, XGetPixel(ximage, xx, yy),
					&red, &green, &blue);
			sprintf(buffer, "%02X", (int) floor(0.5 + 255.0 *
							    (0.30 * red +
							     0.59 * green +
							     0.11 * blue)));
			Tcl_AppendResult(interp, buffer, (char *) NULL);
			lineLen += 2;
			if (lineLen > 60) {
			    lineLen = 0;
			    Tcl_AppendResult(interp, "\n", (char *) NULL);
			}
		    }
		    break;
		}
		case 2: {
		    /*
		     * Finally, color mode.  Here, just output the red, green,
		     * and blue values directly.
		     */
			for (xx = x; xx < x+width; xx++) {
			TkImageGetColor(&cdata, XGetPixel(ximage, xx, yy),
				&red, &green, &blue);
			sprintf(buffer, "%02X%02X%02X",
				(int) floor(0.5 + 255.0 * red),
				(int) floor(0.5 + 255.0 * green),
				(int) floor(0.5 + 255.0 * blue));
			Tcl_AppendResult(interp, buffer, (char *) NULL);
			lineLen += 6;
			if (lineLen > 60) {
			    lineLen = 0;
			    Tcl_AppendResult(interp, "\n", (char *) NULL);
			}
		    }
		    break;
		}
	    }
	}
	switch (level) {
	    case 0: sprintf(buffer, ">\n} image\n"); break;
	    case 1: sprintf(buffer, ">\n} image\n"); break;
	    case 2: sprintf(buffer, ">\n} false 3 colorimage\n"); break;
	}
	Tcl_AppendResult(interp, buffer, (char *) NULL);
	sprintf(buffer, "0 %d translate\n", rows);
	Tcl_AppendResult(interp, buffer, (char *) NULL);
    }
    HtmlFree((char *) cdata.colors);
    return TCL_OK;
}
int
HtmlTk_PostscriptImage(
    Tk_Image image,		/* Token for image to redisplay. */
    Tcl_Interp *interp,
    Tk_Window tkwin,
    TkPostscriptInfo *psinfo,	/* postscript info */
    int x, int y,		/* Upper-left pixel of region in image that
				 * needs to be redisplayed. */
    int width, int height,	/* Dimensions of region to redraw. */
    int prepass)
{
    int result;
    XImage *ximage;
    Pixmap pmap;
    GC newGC;
    XGCValues gcValues;
    if (prepass) {
	return TCL_OK;
    }

    /*
     * Create a Pixmap, tell the image to redraw itself there, and then
     * generate an XImage from the Pixmap.  We can then read pixel 
     * values out of the XImage.
     */

    pmap = Tk_GetPixmap(Tk_Display(tkwin), Tk_WindowId(tkwin),
                        width, height, Tk_Depth(tkwin));

    gcValues.foreground = WhitePixelOfScreen(Tk_Screen(tkwin));
    newGC = Tk_GetGC(tkwin, GCForeground, &gcValues);
    if (0 && newGC != None) { /* ??? */
	XFillRectangle(Tk_Display(tkwin), pmap, newGC,
		0, 0, (unsigned int)width, (unsigned int)height);
	Tk_FreeGC(Tk_Display(tkwin), newGC);
    }

    Tk_RedrawImage(image, 0,0, /*x, y,*/ width, height, pmap, 0, 0);

#ifdef WIN32
    ximage = XGetImage(Tk_Display(tkwin), pmap, 0, 0,
	    (unsigned int)width, (unsigned int)height, 1, XYPixmap);
#else
    ximage = XGetImage(Tk_Display(tkwin), pmap, 0, 0,
	    (unsigned int)width, (unsigned int)height, AllPlanes, ZPixmap);
#endif

    Tk_FreePixmap(Tk_Display(tkwin), pmap);
    
    if (ximage == NULL) {
	/* The XGetImage() function is apparently not
	 * implemented on this system. Just ignore it.
	 */
	return TCL_OK;
    }
    if (!prepass) {
        char buffer[100 + TCL_DOUBLE_SPACE * 2 + TCL_INTEGER_SPACE * 4];
        sprintf(buffer, "%.15g %.15g translate\n", (double)x, 
	  Html_PostscriptY((double)(y),psinfo));
        if (psinfo->chan == NULL)
           Tcl_AppendResult(interp, buffer, (char *) NULL);
        else 
          Tcl_Write(psinfo->chan, buffer, -1);
    }

    result = HtmlTkPostscriptImage(interp, tkwin, psinfo, ximage, 0,0/*x, y*/,
	    width, height);

    XDestroyImage(ximage);
    return result;
}

extern int (*HtmlPostscriptPtr)(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
);

/*DLL_EXPORT*/ int Tkhtmlpr_Init(Tcl_Interp *interp) {
  Tcl_PkgProvide(interp, HTML_PKGNAME"pr", HTML_PKGVERSION);
  HtmlPostscriptPtr=HtmlPostscript;
  return TCL_OK;
}
#endif /* TKHTML_PS=1 */
