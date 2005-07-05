/*
 * htmllayout.c --
 *
 *     This file contains code to layout a document for display using
 *     the CSS box model.
 *
 *     This is a rewrite of the layout engine from Tkhtml 2.0. Many 
 *     ideas from the original are reused. The main differences are
 *     that:
 *     
 *     * This version renders in terms of CSS properties, which are
 *       assigned to document nodes using stylesheets based on HTML
 *       attributes and tag types.
 *     * This version is written to the CSS specification with the
 *       goal of being "pixel-perfect". The HTML spec did not contain
 *       sufficient detail to do this when the original module was
 *       created.
 *     * This version supports two targets, the internal widget and
 *       an external Tk canvas widget.
 *
 *--------------------------------------------------------------------------
 * COPYRIGHT:
 */

#include "html.h"
#include <assert.h>
#include <string.h>

/*
 * At the moment the widget supports two rendering targets:
 *     * Tk canvas, and
 *     * The widget window.
 *
 * The Tk canvas target is included chiefly for testing. It may also
 * be handy for printing (although there are no plans to support the CSS
 * paged output mode) or for a lightweight deployment of the widget.
 */
#define LAYOUT_CANVAS 1
#define LAYOUT_INTERNAL 2

typedef struct BoxProperties BoxProperties;
typedef struct BorderProperties BorderProperties;
typedef struct MarginProperties MarginProperties;
typedef struct DisplayProperties DisplayProperties;

typedef struct LayoutContext LayoutContext;
typedef struct BoxContext BoxContext;

typedef struct TableData TableData;
typedef struct InlineData InlineData;

/*
 * A single Layout context object is allocated for use throughout
 * the entire layout process. It contains global resources required
 * by the drawing routines.
 *
 * The 'marginValid' and 'marginValue' variables are used to implement
 * collapsing margins.
 */
struct LayoutContext {
    HtmlTree *pTree;       /* The Html widget. */
    HtmlNode *pTop;          /* Top level node rendered (<body>). */
    Tk_Window tkwin;
    Tcl_Interp *interp;      /* The interpreter */
    int eTarget;             /* One of LAYOUT_CANVAS or LAYOUT_INTERNAL. */
    Tcl_HashTable widthCache;
    int minmaxTest;          /* Currently figuring out min/max widths */

    int marginValid;         /* True to include the top margin in next block */
    int marginValue;         /* Bottom margin of previous block box */
    int marginParent;
};

/*
 * A seperate BoxContext struct is used for each block box layed out.
 */
struct BoxContext {
    int parentWidth;           /* DOWN: Width of parent block box. */
    int contentWidth;          /* DOWN: Width of content for this box. */
    int height;                /* UP: Generated box height. */
    int width;                 /* UP: Generated box width. */
    HtmlFloatList *pFloat;     /* Margins. */
    HtmlCanvas vc;             /* Canvas to draw the block on. */
};

struct BoxProperties {
    int padding_top;
    int padding_left;
    int padding_bottom;
    int padding_right;

    int border_top;
    int border_left;
    int border_bottom;
    int border_right;
};

struct BorderProperties {
    XColor *color_top;
    XColor *color_left;
    XColor *color_bottom;
    XColor *color_right;

    XColor *color_bg;
};

struct MarginProperties {
    int margin_top;
    int margin_left;
    int margin_bottom;
    int margin_right;
    int leftAuto;        /* True if ('margin-left' == "auto") */
    int rightAuto;       /* True if ('margin-right' == "auto") */
};

/*
 * A DisplayProperties struct wraps up all the properties required to
 * decide how to layout a box:
 *     * display
 *     * float
 *     * position
 *     * clear
 */
struct DisplayProperties {
    int eDisplay;            /* DISPLAY_xxx constant */
    int eFloat;              /* FLOAT_xxx constant */
    int eClear;              /* CLEAR_xxx constant */
};

struct TableCell {
    BoxContext box;
    int startrow;
    int finrow;
    int colspan;
    HtmlNode *pNode;
};
typedef struct TableCell TableCell;

/*
 * Structure used whilst laying out tables. See tableLayout().
 */
struct TableData {
    LayoutContext *pLayout;
    int nCol;                /* Total number of columns in table */
    int nRow;                /* Total number of rows in table */
    int *aMaxWidth;          /* Maximum width of each column */
    int *aMinWidth;          /* Minimum width of each column */
    int border_spacing;      /* Pixel value of 'border-spacing' property */
    int availablewidth;      /* Width available between margins for table */

    int *aWidth;             /* Actual width of each column  */
    int *aY;                 /* Top y-coord for each row+1, wrt table box */
    TableCell *aCell;

    int row;                 /* Current row */
    int y;                   /* y-coord to draw at */
    int x;                   /* x-coord to draw at */
    BoxContext *pBox;        /* Box to draw into */
};

/*
 * Inline Context Requirements Notes:
 * ---------------------
 *
 *     Laying out elements in an inline context is superficially simple.
 *     Inline boxes are added to a line box until the line box is full, and
 *     it is then drawn into the normal flow in the same way as a block
 *     box. But, as I have discovered, the following complications exist,
 *     which make things anything but simple :)
 *
 *     + Floating boxes.
 *     + Block boxes that occur in inline contexts.
 *     + 'text-align' (i.e. left, right, center or justified).
 *     + 'word-spacing' property.
 *     + 'letter-spacing' property.
 *     + 'text-decoration' (i.e. underlining, striking etc.)
 *     + Borders and backgrounds of inline boxes.
 *     + Padding and margins of inline boxes.
 *
 * FLOATS:
 *
 *     Floating boxes are tricky because if a floating box occurs midway
 *     through a line, the top of the floating box is supposed to be level
 *     with the top of the current line box. i.e. The following code:
 *
 *         <p>The quick brown fox 
 *             <img src="fox.jpg" align="left"> 
 *             jumped over...
 *         </p>
 *
 *     should render as follows:
 *
 *         |                                             |
 *         |+---------------+ The quick brown fox jumped |
 *         ||    fox.jpg    | over the...                |
 *         |+---------------+                            |
 *         |                                             |
 *
 *     Specifically, the "fox.jpg" image should never be floated against
 *     the line below the text "fox". Instead, it displaces the line
 *     containing "fox", even if this means "fox" moves onto the next line.
 *
 * BLOCK BOXES:
 *
 *     Sometimes a block box can occur inside an inline context:
 *
 *         <div>First part of text 
 *             <b>Second part<div>Another block box</div>of text</b>
 *         </div>
 *
 *     This should render as:
 *
 *         |                                             |
 *         |First part of text Second part               |
 *         |Another block box                            |
 *         |of text                                      |
 *         |                                             |
 *
 *     All text from "Second" onwards should be in bold font. Both KHTML
 *     and Gecko render this way, but the specification is ambiguous.
 *     Tkhtml handles this by considering the current inline context
 *     finished, drawing the block box into the normal flow, then starting
 *     a new inline context. 
 *
 * TEXT-ALIGN, WORD-SPACING and LETTER-SPACING:
 *
 *     The 'text-align' property may take the values "left", "right",
 *     "center" or "justify". The first three values just affect the
 *     alignment of each line-box within the parent context - easy.
 *
 *     A value of "justify" for 'text-align' is more complicated. In this
 *     case Tkhtml may adjust the spaces between inline-boxes to justify
 *     the line-box. Tkhtml considers a word of text to be an atomic inline
 *     box, it never adjusts letter-spacing to achieve justification. IMO
 *     this looks terrible anyway.
 *
 *     If the 'word-spacing' property takes a value other than "normal",
 *     then the space between words is not adjusted, even if this means the
 *     line-box cannot be justified.
 *
 *     Todo: Support the <string> option for 'text-align'. This only
 *     applies to table-cells and doesn't seem to be used much anyway.
 *
 * BORDERS, BACKGROUNDS, PADDING and MARGINS.
 *
 *     The tricky bit. Well, not quite true, backgrounds are easy enough
 *     anyway. The background color or image covers all the content area of
 *     the inline box. If the inline box spills over two lines, then the
 *     background ends with the last word on the line, it does not extend
 *     the whole width of the parent context.
 *
 *     When a border is drawn around an inline box that spills over two
 *     line boxes, then three sides of the border are draw in each line
 *     box. For example:
 *
 *          <p>There was 
 *          <span style="border:1px solid">
 *              movement at the station for the word had passed around,
 *          </span>
 *              , that the colt from Old Regret had got away.
 *          </p>
 * 
 *         |                                             |
 *         |                   +----------------------   |
 *         |There was movement |at the station for the   |
 *         |                   +----------------------   |
 *         |-----------------------+                     |
 *         |word had passed around,| that the colt from  |
 *         |-----------------------+                     |
 *         |Old Regret had got away.                     |
 *         |                                             |
 *
 *     The wierd part is that Gecko and KHTML never allocate space for
 *     vertical margins, padding or borders. They both draw borders
 *     correctly, and backgrounds are extended to allow for padding, but 
 *     no space is ever allocated. The border of an inline box may flow
 *     over the content of above line-box. For now, Tkhtml works this way
 *     too, not because I think they're right, but because it's easier and
 *     everyone else is getting away with it.
 *
 *     Both Gecko and KHTML allocate space for horizontal padding, margins
 *     and borders correctly.
 *
 * Inline Context Implementation Notes:
 * ------------------------------------
 *
 *     The InlineContext "object" encapsulates many of the details of
 *     laying out an inline-context. The details of struct InlineContext
 *     are considered private to functions with names that start with
 *     "inlineContext", although this convention is not enforced by any
 *     programming constructs.
 *
 *     inlineContextSetTextAlign():
 *         Used to set the value of the 'text-align' property to be used
 *         for this inline context. The 'text-align' property applies to
 *         document elements that generate inline contexts, not ones that
 *         participate in them, so all line-boxes in a single inline
 *         context are horizontally aligned in the same way.
 *
 *     inlineContextSetWhiteSpace():
 *         Used to set the value of the 'white-space' property to be used.
 *         Like 'text-align', the 'white-space' property only applies to
 *         block-level elements.
 * 
 *     inlineContextAddInlineCanvas():
 *         Add a rendered inline box to the context.
 *
 *     inlineContextSetBoxDimensions():
 *         Set the effective dimensions of the inline box most recently added.
 *
 *     inlineContextAddSpace():
 *         Add a "space" box (i.e. one generated by white-space text) to
 *         the context. Width of the space is specified in pixels.
 *
 *     inlineContextGetLineBox():
 *         Retrieve the next rendered line-box from the inline context. The
 *         line-box is created based on the inline-boxes that have already
 *         been passed in using the inlineContextAddInlineCanvas() call.
 *
 *     inlineContextCleanup():
 *         This is called to deallocate all resources associated with the
 *         InlineContext object.
 */
typedef struct InlineContext InlineContext;
typedef struct InlineBorder InlineBorder;
typedef struct InlineBox InlineBox;
struct InlineBorder {
  BorderProperties border;
  MarginProperties margin;
  BoxProperties box;
  int textdecoration;         /* Value of 'text-decoration' property */
  XColor *color;              /* Color for text-decoration */
  int iStartBox;              /* Leftmost inline-box */
  int iStartPixel;            /* Leftmost pixel of outer margin */
  InlineBorder *pNext;
};

/*
 * This structure is used internally by the InlineContext object functions.
 * A single instance represents a single inline-box, for example a word of
 * text, a widget or an inline image.
 */
struct InlineBox {
  HtmlCanvas canvas;          /* Canvas containing box content. */
  int nSpace;                 /* Pixels of space between this and next box. */
  int eNewLine;               /* True if a new-line, not an inline-box */
  InlineBorder *pBorderStart; /* List of borders that start with this box */
  int eReplaced;              /* True if a replaced inline box */
  HtmlNode *pNode;            /* Associated tree node */
  int nBorderEnd;             /* Number of borders that end here */
  int nLeftPixels;            /* Total left width of borders that start here */
  int nRightPixels;           /* Total right width of borders that start here */
  int nContentPixels;         /* Width of content. */
  int nAscentPixels;          /* Distance between baseline and content top */
  int nDescentPixels;         /* Distance between baseline and content bottom */
};
struct InlineContext {
  int textAlign;          /* One of TEXTALIGN_LEFT, TEXTALIGN_RIGHT etc. */
  int whiteSpace;         /* One of WHITESPACE_PRE, WHITESPACE_NORMAL etc. */
  int lineHeight;         /* Value of 'line-height' on inline parent */

  int nInline;            /* Number of inline boxes in aInline */
  int nInlineAlloc;       /* Number of slots allocated in aInline */
  InlineBox *aInline;     /* Array of inline boxes. */

  InlineBorder *pBorders;    /* Linked list of active inline-borders. */
  InlineBorder *pBoxBorders; /* Borders list for next box to be added */
};
static void inlineContextSetTextAlign(InlineContext *, int);
static void inlineContextSetWhiteSpace(InlineContext *, int);
static HtmlCanvas *inlineContextAddInlineCanvas(InlineContext*, int, HtmlNode*);
static void inlineContextAddSpace(InlineContext *, int);
static int 
inlineContextGetLineBox(
LayoutContext *pLayout, InlineContext*,int*,int,int,HtmlCanvas*,int*, int*);
static int inlineContextIsEmpty(InlineContext *);
static InlineBorder *inlineContextGetBorder(LayoutContext *, HtmlNode *, int);
static int inlineContextPushBorder(InlineContext *, InlineBorder *);
static int inlineContextPopBorder(InlineContext *, InlineBorder *);
static void inlineContextSetBoxDimensions(InlineContext *, int, int, int);
static void inlineContextCleanup(InlineContext *);

/*
 * Potential values for the 'display' property. Not supported yet are
 * 'run-in' and 'compact'. And some table types...
 */
#define DISPLAY_BLOCK        CSS_CONST_BLOCK
#define DISPLAY_INLINE       CSS_CONST_INLINE
#define DISPLAY_TABLE        CSS_CONST_TABLE
#define DISPLAY_LISTITEM     CSS_CONST_LIST_ITEM
#define DISPLAY_NONE         CSS_CONST_NONE
#define DISPLAY_TABLECELL    CSS_CONST_TABLE_CELL

#define LISTSTYLETYPE_SQUARE CSS_CONST_SQUARE 
#define LISTSTYLETYPE_DISC   CSS_CONST_DISC 
#define LISTSTYLETYPE_CIRCLE CSS_CONST_CIRCLE
#define LISTSTYLETYPE_NONE   CSS_CONST_NONE

#define VALIGN_MIDDLE        CSS_CONST_MIDDLE
#define VALIGN_TOP           CSS_CONST_TOP
#define VALIGN_BOTTOM        CSS_CONST_BOTTOM
#define VALIGN_BASELINE      CSS_CONST_BASELINE
#define VALIGN_SUB           CSS_CONST_SUB
#define VALIGN_SUPER         CSS_CONST_SUPER
#define VALIGN_TEXT_TOP      CSS_CONST_TEXT_TOP
#define VALIGN_TEXT_BOTTOM   CSS_CONST_TEXT_BOTTOM

#define TEXTALIGN_LEFT       CSS_CONST_LEFT
#define TEXTALIGN_RIGHT      CSS_CONST_RIGHT
#define TEXTALIGN_CENTER     CSS_CONST_CENTER
#define TEXTALIGN_JUSTIFY    CSS_CONST_JUSTIFY

#define TEXTDECORATION_NONE           CSS_CONST_NONE
#define TEXTDECORATION_UNDERLINE      CSS_CONST_UNDERLINE
#define TEXTDECORATION_OVERLINE       CSS_CONST_OVERLINE
#define TEXTDECORATION_LINETHROUGH    CSS_CONST_LINE_THROUGH

#define WHITESPACE_PRE       CSS_CONST_PRE
#define WHITESPACE_NOWRAP    CSS_CONST_NOWRAP
#define WHITESPACE_NORMAL    CSS_CONST_NORMAL

/*
 * These are prototypes for all the static functions in this file. We
 * don't need most of them, but the help with error checking that normally
 * wouldn't happen because of the old-style function declarations. Also
 * they function as a table of contents for this file.
 */
#define S static

S int  propertyToConstant(CssProperty *pProp, const char **, int *, int);
S CONST char *propertyToString(CssProperty *pProp, const char *);
S int propertyToPixels(LayoutContext*, HtmlNode*, CssProperty*, int, int);
S XColor *propertyToColor(LayoutContext *, CssProperty*);
S int physicalToPixels(LayoutContext *, double, char);
S int propertyIsAuto(CssProperty *);

S int nodeGetFontSize(LayoutContext *pLayout, HtmlNode *pNode);
S int  nodeGetEmPixels(LayoutContext*, HtmlNode*);
S int nodeGetExPixels(LayoutContext*, HtmlNode*);
S void nodeGetDisplay(LayoutContext*, HtmlNode*, DisplayProperties*);
S int  nodeGetListStyleType(LayoutContext *, HtmlNode *);
S XColor *nodeGetColour(LayoutContext *, HtmlNode*);
S int nodeGetBorderSpacing(LayoutContext *, HtmlNode*);
S int nodeGetVAlign(LayoutContext *, HtmlNode*, int);
S void nodeGetBoxProperties(LayoutContext *, HtmlNode *, int, BoxProperties *);
S void nodeGetBorderProperties(LayoutContext *, HtmlNode *, BorderProperties *);
S int nodeGetWidth(LayoutContext *, HtmlNode *, int, int, int*, int*);
S int nodeGetHeight(LayoutContext *, HtmlNode *, int, int);
S int nodeGetTextAlign(LayoutContext *, HtmlNode *);
S int nodeGetTextDecoration(LayoutContext *, HtmlNode *);
S CONST char *nodeGetTkhtmlReplace(LayoutContext *, HtmlNode *);

S void nodeComment(LayoutContext *, HtmlCanvas *, HtmlNode *);
S void endNodeComment(LayoutContext *, HtmlCanvas *, HtmlNode *);

S void borderLayout(LayoutContext*, HtmlNode*, BoxContext*, int, int, int, int);
S int floatLayout(LayoutContext*, BoxContext*, HtmlNode*, int*);
S int markerLayout(LayoutContext*, BoxContext*, HtmlNode*);
S int inlineLayout(LayoutContext*, BoxContext*, HtmlNode*);
S int tableLayout(LayoutContext*, BoxContext*, HtmlNode*);
S int blockLayout(LayoutContext*, BoxContext*, HtmlNode*, int, int);
S void layoutReplacement(LayoutContext*, BoxContext*, HtmlNode*, CONST char*);

S int tableIterate(
    HtmlNode*, 
    int(*)(HtmlNode *, int, int, int, int, void *),
    int(*)(HtmlNode *, int, void *),
    void*
);
S int blockMinMaxWidth(LayoutContext *, HtmlNode *, int *, int *);

#undef S

#define DRAW_TEXT(a, b, c, d, e, f, g, h) \
HtmlDrawText(a, b, c, d, e, f, g, h, pLayout->minmaxTest)
#define DRAW_CANVAS(a, b, c, d, e) \
HtmlDrawCanvas(a, b, c, d, e)
#define DRAW_IMAGE(a, b, c, d, e, f) \
HtmlDrawImage(a, b, c, d, e, f, pLayout->minmaxTest)
#define DRAW_WINDOW(a, b, c, d, e, f) \
HtmlDrawWindow(a, b, c, d, e, f, pLayout->minmaxTest)
#define DRAW_BACKGROUND(a, b) \
HtmlDrawBackground(a, b, pLayout->minmaxTest)
#define DRAW_QUAD(a, b, c, d, e, f, g, h, i, j) \
HtmlDrawQuad(a, b, c, d, e, f, g, h, i, j, pLayout->minmaxTest)
#define DRAW_COMMENT(a, b) \
HtmlDrawComment(a, b, pLayout->minmaxTest)

/*
 *---------------------------------------------------------------------------
 *
 * propertyToConstant --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyToConstant(pProp, zOptions, eOptions, eDefault)
    CssProperty *pProp;
    const char **zOptions;
    int *eOptions;
    int eDefault;
{
    if (pProp && pProp->eType==CSS_TYPE_STRING) {
        CONST char *z = HtmlCssPropertyGetString(pProp);
        while (*zOptions) {
            if( 0==stricmp(z, *zOptions) ) return *eOptions;
            eOptions++;
            zOptions++;
        }
    }
    return eDefault;
}

static CONST char *
propertyToString(pProp, zDefault)
    CssProperty *pProp; 
    const char *zDefault;
{
    CONST char *z = HtmlCssPropertyGetString(pProp);
    if (!z) {
        z = zDefault;
    }
    return z;
}

/*
 *---------------------------------------------------------------------------
 *
 * pixelsToPoints --
 *
 *     Convert a pixel length to points (1/72 of an inch). 
 *
 *     Note: An "inch" is an anachronism still in use in some of the more
 *           stubborn countries :). It is equivalent to approximately 25.4
 *           millimeters. 
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
pixelsToPoints(pLayout, pixels)
    LayoutContext *pLayout;
    int pixels;
{
    double mm;
    Tcl_Obj *pObj = Tcl_NewIntObj(pixels);
    Tcl_IncrRefCount(pObj);
    Tk_GetMMFromObj(pLayout->interp, pLayout->tkwin, pObj, &mm);
    Tcl_DecrRefCount(pObj);
    return (int) ((mm * 72.0 / 25.4) + 0.5);
}

/*
 *---------------------------------------------------------------------------
 *
 * physicalToPixels --
 *
 *     This function is a wrapper around Tk_GetPixels(), used to convert
 *     physical units to pixels. The first argument is the layout-context.
 *     The second argument is the distance in terms of the physical unit
 *     being converted from. The third argument determines the unit type,
 *     as follows:
 *
 *         Character          Unit
 *         ------------------------------
 *         'c'                Centimeters
 *         'i'                Inches
 *         'm'                Millimeters
 *         'p'                Points (1 point = 1/72 inches)
 *
 *     The value returned is the distance in pixels.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
physicalToPixels(pLayout, rVal, type)
    LayoutContext *pLayout;
    double rVal;
    char type;
{
    char zBuf[64];
    int pixels;
    sprintf(zBuf, "%f%c", rVal, type);
    Tk_GetPixels(pLayout->interp, pLayout->tkwin, zBuf, &pixels);
    return pixels;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyToPixels --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyToPixels(pLayout, pNode, pProp, parentwidth, default_val)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    CssProperty *pProp;
    int parentwidth;
    int default_val;
{
    if (pProp) {
        switch (pProp->eType) {
            case CSS_TYPE_FLOAT:
                return (int)(pProp->v.rVal);
            case CSS_TYPE_PX:
                return pProp->v.iVal;
            case CSS_TYPE_EM: {
                return pProp->v.rVal * nodeGetEmPixels(pLayout, pNode);
            }
            case CSS_TYPE_EX: {
                return pProp->v.rVal * nodeGetExPixels(pLayout, pNode);
            }
            case CSS_TYPE_PERCENT: {
                return (pProp->v.iVal * parentwidth) / 100;
            }
            case CSS_TYPE_CENTIMETER: {
                return physicalToPixels(pLayout, pProp->v.rVal, 'c');
            }
            case CSS_TYPE_MILLIMETER: {
                return physicalToPixels(pLayout, pProp->v.rVal, 'm');
            }
            case CSS_TYPE_INCH: {
                return physicalToPixels(pLayout, pProp->v.rVal, 'i');
            }
            case CSS_TYPE_PC: {
                return physicalToPixels(pLayout, pProp->v.rVal * 12.0, 'p');
            }
            case CSS_TYPE_PT: {
                return physicalToPixels(pLayout, (double)pProp->v.iVal, 'p');
            }
        }
    }

    return default_val;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyToColor --
 *
 *     Convert a CSS property to an XColor*.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static XColor *
propertyToColor(pLayout, pProp)
    LayoutContext *pLayout;
    CssProperty *pProp;
{
    XColor *color = 0;
    CONST char *zColor = 0;

    /* The following constants are the web standard colors. */
    switch (pProp->eType) {
        case CSS_TYPE_XCOLOR:   
            return (XColor *)pProp->v.p;
        case CSS_CONST_BLACK:   zColor = "#000000"; break;
        case CSS_CONST_SILVER:  zColor = "#C0C0C0"; break;
        case CSS_CONST_GRAY:    zColor = "#808080"; break;
        case CSS_CONST_WHITE:   zColor = "#FFFFFF"; break;
        case CSS_CONST_MAROON:  zColor = "#800000"; break;
        case CSS_CONST_RED:     zColor = "#FF0000"; break;
        case CSS_CONST_PURPLE:  zColor = "#800080"; break;
        case CSS_CONST_FUCHSIA: zColor = "#FF00FF"; break;
        case CSS_CONST_GREEN:   zColor = "#008000"; break;
        case CSS_CONST_LIME:    zColor = "#00FF00"; break;
        case CSS_CONST_OLIVE:   zColor = "#808000"; break;
        case CSS_CONST_YELLOW:  zColor = "#FFFF00"; break;
        case CSS_CONST_NAVY:    zColor = "#000080"; break;
        case CSS_CONST_BLUE:    zColor = "#0000FF"; break;
        case CSS_CONST_TEAL:    zColor = "#008080"; break;
        case CSS_CONST_AQUA:    zColor = "#00FFFF"; break;
        case CSS_TYPE_STRING:
            zColor = HtmlCssPropertyGetString(pProp);
    }

    if (zColor) {
        int newentry = 1;
        Tcl_HashTable *pHash = &pLayout->pTree->aColor;
        Tcl_HashEntry *pEntry;

        pEntry = Tcl_CreateHashEntry(pHash, zColor, &newentry);
        if (!newentry) {
            color = (XColor *)Tcl_GetHashValue(pEntry);
        } else {
            Tk_Window tkwin = pLayout->tkwin;
            Tcl_Interp *interp = pLayout->interp;

            color = Tk_GetColor(interp, tkwin, zColor);
            if (!color && strlen(zColor) <= 12) {
		/* Old versions of netscape used to support hex colors
		 * without the '#' character (i.e. "FFF" is the same as
		 * "#FFF"). So naturally this has become a defacto
		 * standard, even though it is obviously wrong.
                 */
                char zBuf[14];
                sprintf(zBuf, "#%s", zColor);
                color = Tk_GetColor(interp, tkwin, zBuf);
            }
 
            if (!color) {
                Tcl_DeleteHashEntry(pEntry);
            } else {
                Tcl_SetHashValue(pEntry, color);
#if 0
                pProp->eType = CSS_TYPE_XCOLOR;
                pProp->v.p = (void *)color;
#endif
            }
        }
    }

    return color;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyIsAuto --
 *
 *     Return non-zero if the CSS property passed as the first argument
 *     takes the string value "auto".
 *
 * Results:
 *     See above.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int propertyIsAuto(pProp)
    CssProperty *pProp;
{
    assert(pProp);
    return (pProp->eType == CSS_CONST_AUTO);
}

/*
 *---------------------------------------------------------------------------
 *
 * bwToPixels --
 *
 *     This function interprets a CSS property as a border-width and
 *     returns the corresponding number of pixels to use. A border length
 *     property may be either a <length> value or one of the constant
 *     strings "thin", "medium" or "thick".
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
bwToPixels(pLayout, pNode, pProp, parentwidth, default_val)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    CssProperty *pProp;
    int parentwidth;
    int default_val;
{
    int ret;
    switch (pProp->eType) {
        case CSS_CONST_THIN:
            ret = 1;
            break;
        case CSS_CONST_MEDIUM:
            ret = 2;
            break;
        case CSS_CONST_THICK:
            ret = 4;
            break;
        default:
            ret = propertyToPixels(
                    pLayout, pNode, pProp, parentwidth, default_val);
    }
    return ret;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetDisplay --
 *
 *     Query the 'display', 'position' and 'float' properties of a node.
 *     This function fixes any inconsistencies between these values 
 *     according to CSS2 section 9.7 "Relationships between 'display', 
 *     'position' and 'float'".
 * 
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Fills in values of structure pointed to by pDisplayProperties.
 *
 *---------------------------------------------------------------------------
 */
static void 
nodeGetDisplay(pLayout, pNode, pDisplayProperties)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    DisplayProperties *pDisplayProperties;
{
    CssProperty *pProp;
    int f;        /* 'float' */
    int d;        /* 'display' */
    int c;        /* 'clear' */

    pProp = HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_DISPLAY);
    d = pProp->eType;
    if (d != DISPLAY_INLINE    && d != DISPLAY_BLOCK && 
        d != DISPLAY_NONE      && d != DISPLAY_LISTITEM && 
        d != DISPLAY_TABLE     && d != DISPLAY_NONE &&
        d != DISPLAY_TABLECELL
    ) {
        d = DISPLAY_INLINE;
    }

    pProp = HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_FLOAT);
    f = pProp->eType;
    if (f != FLOAT_NONE && f != FLOAT_LEFT && f != FLOAT_RIGHT) {
        f = FLOAT_NONE;
    }

    pProp = HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_CLEAR);
    c = pProp->eType;
    if (c != CLEAR_NONE        && c != CLEAR_LEFT && 
        c != CLEAR_RIGHT       && c != CLEAR_BOTH
    ) {
        c = CLEAR_NONE;
    }

    /* Force all floating boxes to have display type 'block' or 'table' */
    if (f!=FLOAT_NONE && d!=DISPLAY_TABLE) {
        d = DISPLAY_BLOCK;
    }

    pDisplayProperties->eDisplay = d;
    pDisplayProperties->eFloat = f;
    pDisplayProperties->eClear = c;
}

static int nodeGetListStyleType(pLayout, pNode)
    LayoutContext *pLayout; 
    HtmlNode *pNode;
{
    CssProperty *pProp;
    int l;
    Tcl_Interp *interp = pLayout->pTree->interp;

    pProp = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_LIST_STYLE_TYPE);
    l = pProp->eType;
    if (l != LISTSTYLETYPE_SQUARE &&        l != LISTSTYLETYPE_DISC &&
        l != LISTSTYLETYPE_CIRCLE &&        l != LISTSTYLETYPE_NONE
    ) {
        l = LISTSTYLETYPE_DISC;
    }
    return l;
}

/*
 *---------------------------------------------------------------------------
 *
 * getScaledFontSize --
 *
 *     This is used to calculate the sizes of fonts in points when they are
 *     specified relative to other font-sizes (i.e. "xx-small" or
 *     "larger").
 *
 *     If paramter 'body' is true, then the font-size should be calculated
 *     relative to the font-size of the <body> tag, or the root-node if no
 *     <body> tag exists. Otherwise it is calculated relative to the parent
 *     of pNode.
 *
 *     Pameter rVal is the scale by which the <body> or <parent> font-size
 *     should be multiplied.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
getScaledFontSize(pLayout, pNode, rVal, body)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    double rVal;
    int body;
{
    HtmlNode *pRelative;
    pRelative = HtmlNodeParent(pNode);
    if (body) {
        while (pRelative && HtmlNodeTagType(pRelative) != Html_BODY) {
            pRelative = HtmlNodeParent(pRelative);
        }
    }
    return (int)(0.5 + ((double)nodeGetFontSize(pLayout, pRelative) * rVal));
}
    
/*
 *---------------------------------------------------------------------------
 *
 * nodeGetFontSize --
 * 
 *     Return the value of the 'font-size' property, in points to use for 
 *     the node.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
nodeGetFontSize(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    int val = 0;
    CssProperty *pProp;

    /* Default of 'font-size' should be "medium". */
    if (!pNode) return 10;
    pProp = HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_FONT_SIZE);

    switch (pProp->eType) {
        case CSS_TYPE_EM:
            val = nodeGetFontSize(pLayout, HtmlNodeParent(pNode));
            val = val * pProp->v.rVal;
            break;
        case CSS_TYPE_EX:
            val = nodeGetExPixels(pLayout,  HtmlNodeParent(pNode));
            val = val * pProp->v.rVal;
            break;
        case CSS_TYPE_PT:
            val = pProp->v.iVal;
            break;
        case CSS_TYPE_PERCENT:
            val = nodeGetFontSize(pLayout, pNode->pParent);
            val = (val * pProp->v.iVal) / 100;
            break;
        case CSS_CONST_XX_SMALL: 
            val = getScaledFontSize(pLayout, pNode, 0.6944, 0);
            break;
        case CSS_CONST_X_SMALL: 
            val = getScaledFontSize(pLayout, pNode, 0.8333, 0);
            break;
        case CSS_CONST_SMALL: 
            val = getScaledFontSize(pLayout, pNode, 1.0, 0);
            break;
        case CSS_CONST_MEDIUM: 
            val = getScaledFontSize(pLayout, pNode, 1.2, 0);
            break;
        case CSS_CONST_LARGE: 
            val = getScaledFontSize(pLayout, pNode, 1.44, 0);
            break;
        case CSS_CONST_X_LARGE: 
            val = getScaledFontSize(pLayout, pNode, 1.728, 0);
            break;
        case CSS_CONST_XX_LARGE: 
            val = getScaledFontSize(pLayout, pNode, 2.074, 0);
            break;
        case CSS_CONST_SMALLER: 
            val = getScaledFontSize(pLayout, pNode, 0.8333, 1);
            break;
        case CSS_CONST_LARGER: 
            val = getScaledFontSize(pLayout, pNode, 1.2, 1);
            break;
        default: {
            int pixels = propertyToPixels(pLayout, pNode, pProp, 0, 0);
            val = pixelsToPoints(pLayout, pixels);
        }
    }

    if (val<=0) {
        val = nodeGetFontSize(pLayout, pNode->pParent);
    }
    return val;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetFontFamily --
 *
 *     Return a Tcl list object with reference count 1 containing the names
 *     of potential font-families to use for this node. The first entry in
 *     the list has the highest priority.
 *
 *     CSS specifies that the font-families "serif", "sans-serif",
 *     "monospace", "fantasy" and "cursive" are always available. Tk on the
 *     other hand guarantees only families "Helvetica", "Courier" and
 *     "Times". The first three CSS families we can map directly to these.
 *
 *     What to do about "cursive" and "fantasy" is a bit of a problem. I
 *     think we will end up adding an option for the Tcl script to specify
 *     a font for these two. Or maybe not. The user shouldn't need to write
 *     100 scripts to get decent rendering from the widget.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Tcl_Obj *nodeGetFontFamily(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    struct FamilyMap {
        CONST char *cssFont;
        CONST char *tkFont;
        int isItalic;
    } familyMap [] = {
        {"serif",      "Times", 0},
        {"sans-serif", "Helvetica", 0},
        {"monospace",  "Courier", 0},
    };
    CssProperty *pFamily;
    CONST char *zFamily;
    CONST char *zFamilyEnd;
    Tcl_Obj *pObj;
    Tcl_Obj *pFallback = 0;
    Tcl_Interp *interp = pLayout->interp;

    pFamily = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_FONT_FAMILY);
    zFamily = propertyToString(pFamily, "Helvetica");

    /* Split the zFamily attribute on the "," character. If the list
     * contains any of the families contained in the "familyMap" array
     * above, then add the name of the Tk font to the end of the list. We
     * use this as a last resort. For example, if the value of the
     * font-family property is:
     *
     *     "Arial Mono, monospace"
     *
     * then return the Tcl list:
     *
     *     {{Arial Mono} monospace Courier}
     *
     * This means that if the user specifies "monospace", we give the
     * system a chance to map the font before we default to Courier (which
     * Tk always supports).
     */
    pObj = Tcl_NewObj();
    Tcl_IncrRefCount(pObj);
    for ( ; zFamily; zFamily = zFamilyEnd) {
        int n;
        Tcl_Obj *p;
        CONST char *z;
        int i;

        zFamilyEnd = strchr(zFamily, (int)',');
        if (!zFamilyEnd) {
            n = strlen(zFamily);
        } else {
            n = zFamilyEnd - zFamily;
        }
        while (zFamilyEnd && (*zFamilyEnd==',' || *zFamilyEnd==' ')) {
            zFamilyEnd++;
        }

        p = Tcl_NewStringObj(zFamily, n);
        Tcl_IncrRefCount(p);
        z = Tcl_GetString(p);
        Tcl_ListObjAppendElement(interp, pObj, p);

        for (i = 0; i < sizeof(familyMap)/sizeof(struct FamilyMap); i++) {
            if (!pFallback && 0 == strcmp(familyMap[i].cssFont, z)) {
                pFallback = Tcl_NewStringObj(familyMap[i].tkFont, -1); 
                Tcl_IncrRefCount(pFallback);
            }
        }

        Tcl_DecrRefCount(p);
    }

    if (pFallback) {
        Tcl_ListObjAppendElement(interp, pObj, pFallback);
        Tcl_DecrRefCount(pFallback);
    }

    return pObj;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetFont --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Tk_Font nodeGetFont(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    int sz = nodeGetFontSize(pLayout, pNode);
    int isItalic = 0;
    int isBold = 0;
    int i;
    int nFamily;
    int eType;
    Tk_Font font = 0;
    CssProperty *pFontStyle;            /* Property 'font-style' */
    CssProperty *pFontWeight;           /* Property 'font-weight' */
    CssProperty *pFontFamily;           /* Property 'font-family' */
    Tcl_Obj *pFamily;                   /* List of potential font-families */
    Tcl_Interp *interp = pLayout->pTree->interp;
    Tcl_HashTable *pFontCache = &pLayout->pTree->aFontCache;

    /* If the 'font-style' attribute is set to either "italic" or
     * "oblique", add the option "-slant italic" to the string version
     * of the Tk font.
     */
    pFontStyle = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_FONT_STYLE);
    eType = pFontStyle->eType;
    if (eType == CSS_CONST_ITALIC || eType == CSS_CONST_OBLIQUE) {
        isItalic = 1;
    }

    /* If the 'font-weight' attribute is set to either "bold" or
     * "bolder", add the option "-weight bold" to the string version
     * of the Tk font.
     *
     * Todo: Handle numeric font-weight values. Tk restricts the weight
     * of the font to "bold" or "normal", but we should try to do something
     * sensible with other options.
     */
    pFontWeight = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_FONT_WEIGHT);
    eType = pFontWeight->eType;
    if (eType == CSS_CONST_BOLD || eType == CSS_CONST_BOLDER) {
        isBold = 1;
    }

    pFamily = nodeGetFontFamily(pLayout, pNode);
    Tcl_ListObjLength(interp, pFamily, &nFamily);
    for (i = 0; font == 0 && i < nFamily; i++) {
        Tcl_Obj *pFont;
        Tcl_HashEntry *pEntry;
        int newentry;

        Tcl_ListObjIndex(interp, pFamily, i, &pFont);
        pFont = Tcl_DuplicateObj(pFont);
        Tcl_IncrRefCount(pFont);
        Tcl_ListObjAppendElement(interp, pFont, Tcl_NewIntObj(sz));

        if (isItalic) {
            Tcl_Obj *p = Tcl_NewStringObj("italic", -1);
            Tcl_ListObjAppendElement(interp, pFont, p);
        }
        if (isBold) {
            Tcl_Obj *p = Tcl_NewStringObj("bold", -1);
            Tcl_ListObjAppendElement(interp, pFont, p);
        }

        pEntry = Tcl_CreateHashEntry(pFontCache,Tcl_GetString(pFont),&newentry);
        if (newentry) {
            font = Tk_AllocFontFromObj(interp, pLayout->tkwin, pFont); 
            if (font) {
                Tcl_SetHashValue(pEntry, font);
            } else {
                Tcl_DeleteHashEntry(pEntry);
            }
        } else {
            font = Tcl_GetHashValue(pEntry);
        }
        Tcl_DecrRefCount(pFont);
    }
    assert(font);

    Tcl_DecrRefCount(pFamily);
    return font;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetColour --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static XColor *nodeGetColour(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    XColor *color = 0;
    CssProperty *pColor;
    CssProperty sColor;

    pColor = HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_COLOR);
    color = propertyToColor(pLayout, pColor);

    if (color) {
        return color;
    }

return_default:
    HtmlNodeGetDefault(pNode, CSS_PROPERTY_COLOR, &sColor);
    return propertyToColor(pLayout, &sColor);
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetBorderSpacing --
 *
 *     Return the value of the "cellspacing" attribute in pixels. If no
 *     value is assigned to "cellspacing", return the value of the
 *     'border-spacing' property, if it exists.
 *
 *     This should only be called on elements with display property 'table'
 *     or 'inline-table'. But no checking is done here. 
 *
 *     Possibly this should only be called when using the seperated borders
 *     model (property 'border-collapse' set to 'separate'). The other
 *     option is the collapsing borders model (property 'border-collapse'
 *     set to 'collapse' - the default).
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
nodeGetBorderSpacing(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    CssProperty *pProp;
    int border_spacing;
    Tcl_Interp *interp = pLayout->pTree->interp;

    pProp = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_SPACING);
    border_spacing = propertyToPixels(pLayout, pNode, pProp, 0, 0);

    return border_spacing;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetVAlign --
 * 
 *     Return the value of the 'vertical-align' property for pNode. 
 * 
 *     This property is a little strange (unique?) because the default
 *     value depends on the type of the node. If the node is a table-cell,
 *     then the default is VALIGN_MIDDLE. If it is an inline element, then
 *     the default value is VALIGN_BASELINE. To handle this, the caller
 *     passes the default value for the context as the third parameter.
 *
 * Results:
 *     One of the VALIGN_xxx constants.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
nodeGetVAlign(pLayout, pNode, defval)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    int defval;
{
#if 1
    CssProperty *pValign;
    int ret;
    Tcl_Interp *interp = pLayout->interp;

    pValign = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_VERTICAL_ALIGN);
    ret = pValign->eType;
    if (ret != VALIGN_TOP      && ret != VALIGN_MIDDLE && 
        ret != VALIGN_BOTTOM   && ret != VALIGN_BASELINE && 
        ret != VALIGN_SUB      && ret != VALIGN_SUPER && 
        ret != VALIGN_TEXT_TOP && ret != VALIGN_TEXT_BOTTOM
    ) {
        ret = defval;
    }
#else
    CssProperty *pValign;
    int ret;
    Tcl_Interp *interp = pLayout->interp;

    const char *zOptions[] = {
	"top",      "middle",      "bottom",        "baseline", 
        "sub",      "super",       "text-top",      "text-bottom", 
        0
    };
    int eOptions[] = {
        VALIGN_TOP, VALIGN_MIDDLE, VALIGN_BOTTOM,   VALIGN_BASELINE,
        VALIGN_SUB, VALIGN_SUPER,  VALIGN_TEXT_TOP, VALIGN_TEXT_BOTTOM
    };

    pValign = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_VERTICAL_ALIGN);
    ret = propertyToConstant(pValign, zOptions, eOptions, defval);
#endif

    return ret;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetEmPixels --
 *
 *     Return the number of pixels for a single "em" unit of pNode.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
nodeGetEmPixels(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    int ret;
    int points;
/*
    Tk_FontMetrics fontMetrics;
    Tk_Font font = nodeGetFont(pLayout, pNode);
    Tk_GetFontMetrics(font, &fontMetrics);
    ret = fontMetrics.ascent;
*/
    points = nodeGetFontSize(pLayout, pNode);
    ret = physicalToPixels(pLayout, (double)(points), 'p');
    
    return ret;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetExPixels --
 *
 *     Return the number of pixels for a single "ex" unit of pNode.
 *
 *     1ex is the same as the height of the letter 'x' in the nodes font.
 *
 *     Todo: I don't know how to get the height of a single character from
 *           Tk, so for now use the formula (2ex == 1em), which is
 *           reasonably close for most fonts.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int nodeGetExPixels(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    return (nodeGetEmPixels(pLayout, pNode) / 2);
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetBoxProperties --
 *    
 *     Get the border and padding properties for a node.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
nodeGetBoxProperties(pLayout, pNode, parentwidth, pBoxProperties)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    int parentwidth;
    BoxProperties *pBoxProperties;
{
    CssProperty *b;
    CssProperty *p;
    int w = parentwidth;
    Tcl_Interp *interp = pLayout->interp;

    p = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_PADDING_TOP);
    pBoxProperties->padding_top = propertyToPixels(pLayout, pNode, p, w, 0);
    p = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_PADDING_LEFT);
    pBoxProperties->padding_left = propertyToPixels(pLayout, pNode, p, w, 0);
    p = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_PADDING_RIGHT);
    pBoxProperties->padding_right = propertyToPixels(pLayout, pNode, p, w, 0);
    p = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_PADDING_BOTTOM);
    pBoxProperties->padding_bottom = propertyToPixels(pLayout, pNode, p, w, 0);

    /* A negative value is not allowed for padding. If one has been
     * specified, treat it as 0.  
     */
    pBoxProperties->padding_bottom = MAX(0, pBoxProperties->padding_bottom);
    pBoxProperties->padding_top = MAX(0, pBoxProperties->padding_top);
    pBoxProperties->padding_left = MAX(0, pBoxProperties->padding_left);
    pBoxProperties->padding_right = MAX(0, pBoxProperties->padding_right);

    b = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_TOP_STYLE);
    if (b->eType == CSS_CONST_NONE) {
        pBoxProperties->border_top = 0;
    }else{
        b = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_TOP_WIDTH);
        pBoxProperties->border_top = bwToPixels(pLayout, pNode, b, w, 0);
    }

    b = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_BOTTOM_STYLE);
    if (b->eType == CSS_CONST_NONE) {
        pBoxProperties->border_bottom = 0;
    }else{
        b = HtmlNodeGetProperty(interp, pNode,CSS_PROPERTY_BORDER_BOTTOM_WIDTH);
        pBoxProperties->border_bottom = bwToPixels(pLayout,pNode,b,w,0);
    }

    b = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_LEFT_STYLE);
    if (b->eType == CSS_CONST_NONE) {
        pBoxProperties->border_left = 0;
    }else{
        b = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_LEFT_WIDTH);
        pBoxProperties->border_left = bwToPixels(pLayout,pNode,b,w,0);
    }

    b = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_RIGHT_STYLE);
    if (b->eType == CSS_CONST_NONE) {
        pBoxProperties->border_right = 0;
    }else{
        b = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_RIGHT_WIDTH);
        pBoxProperties->border_right = bwToPixels(pLayout,pNode,b,w,0);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetColourProperty --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static XColor *
nodeGetColourProperty(pLayout, pNode, prop)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    int prop;
{
    XColor *color;
    CssProperty *pProp;
    CONST char *zColour;

    pProp = HtmlNodeGetProperty(pLayout->interp, pNode, prop);
    color = propertyToColor(pLayout, pProp);
    if (!color) {
        color = nodeGetColour(pLayout, pNode);
    }
    return color;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetBorderProperties --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
nodeGetBorderProperties(pLayout, pNode, pBorderProperties)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    BorderProperties *pBorderProperties;
{
    CONST char *zBg;
    CssProperty *pBg;
    Tcl_Interp *interp = pLayout->interp;

    pBorderProperties->color_top = 
        nodeGetColourProperty(pLayout, pNode, CSS_PROPERTY_BORDER_TOP_COLOR);
    pBorderProperties->color_bottom = 
        nodeGetColourProperty(pLayout, pNode, CSS_PROPERTY_BORDER_BOTTOM_COLOR);
    pBorderProperties->color_right = 
        nodeGetColourProperty(pLayout, pNode, CSS_PROPERTY_BORDER_RIGHT_COLOR);
    pBorderProperties->color_left = 
        nodeGetColourProperty(pLayout, pNode, CSS_PROPERTY_BORDER_LEFT_COLOR);

    /* Now figure out the background color for this block. This is done
     * here because the background is drawn at the same time as the border.
     */
    pBg = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BACKGROUND_COLOR);
    zBg = propertyToString(pBg, 0);
    if (zBg && strcmp(zBg, "transparent")) {
        pBorderProperties->color_bg = propertyToColor(pLayout, pBg);
    } else {
        pBorderProperties->color_bg = 0;
   }
}

static int nodeGetHeight(pLayout, pNode, pwidth, def)
    LayoutContext *pLayout; 
    HtmlNode *pNode; 
    int pwidth; 
    int def;
{
    int val;
    CssProperty *pHeight;
    pHeight = HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_HEIGHT);
    val = propertyToPixels(pLayout, pNode, pHeight, pwidth, def);
    return val;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetWidth --
 * 
 *     Return the value of the 'width' property for a given node.
 *
 *     This function also handles the 'max-width' and 'min-width'
 *     properties. If there is no 'width' attribute and the default value
 *     supplied as the fourth argument is greater than zero, then the
 *     'min-width' and 'max-width' properties are taken into account when
 *     figuring out the return value.
 * 
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int nodeGetWidth(pLayout, pNode, pwidth, def, pIsFixed, pIsAuto)
    LayoutContext *pLayout;   /* Layout context */
    HtmlNode *pNode;          /* Node */
    int pwidth;               /* Value to calculate percentage widths of */
    int def;                  /* Default value */
    int *pIsFixed;            /* OUT: True if a pixel width */
    int *pIsAuto;             /* OUT: True if value is "auto" */
{
    int val;

    CssProperty *pWidth;
    int min;
    int max;
    CssProperty *pMin;
    CssProperty *pMax;

    pWidth = HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_WIDTH);
    val = propertyToPixels(pLayout, pNode, pWidth, pwidth, def);

    switch (pWidth->eType) {
        case CSS_TYPE_PX:
            if (pIsFixed) *pIsFixed = 1;
            break;
        case CSS_TYPE_STRING: {
            /* The only string value the 'width' property can take is
             * "auto". So we assume that this is the case if the property
             * is a string.
             *
             * Note: In CSS2, 'width' can also be specified as "inherit".
             * But HtmlNodeGetProperty() has already dealt with this case.
             */
            int min;
            int max;
            pMin = HtmlNodeGetProperty(
                    pLayout->interp, pNode, CSS_PROPERTY_MIN_WIDTH);
            pMax = HtmlNodeGetProperty(
                    pLayout->interp, pNode, CSS_PROPERTY_MAX_WIDTH);
            min = propertyToPixels(pLayout, pNode, pMin, pwidth, 0);
            max = propertyToPixels(pLayout, pNode, pMax, pwidth, val);
            val = MAX(val, min);
            val = MIN(val, max);
        }
        /* Fall through */
        default:
            if (pIsFixed) *pIsFixed = 0;
           
    }

    if (pIsAuto) *pIsAuto = propertyIsAuto(pWidth);

    return val;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetTextAlign --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int nodeGetTextAlign(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    char const *zOptions[] = {
        "left", "right", "center", "justify", 0
    };
    int eOptions[] = {
        TEXTALIGN_LEFT, TEXTALIGN_RIGHT, TEXTALIGN_CENTER, TEXTALIGN_JUSTIFY
    };
    CssProperty *p;
    int v;
    p = HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_TEXT_ALIGN);

    v = p->eType;
    if (v != TEXTALIGN_LEFT &&      v != TEXTALIGN_RIGHT &&
        v != TEXTALIGN_CENTER &&    v != TEXTALIGN_JUSTIFY
    ) {
        v = TEXTALIGN_LEFT;
    }
    
    return v;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetTextDecoration --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
nodeGetTextDecoration(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    CssProperty *p;
    int ret;
    Tcl_Interp *interp = pLayout->interp;
    p = HtmlNodeGetProperty(interp ,pNode, CSS_PROPERTY_TEXT_DECORATION);
    ret = p->eType;
    if (ret != TEXTDECORATION_UNDERLINE &&   ret != TEXTDECORATION_OVERLINE &&
        ret != TEXTDECORATION_LINETHROUGH && ret != TEXTDECORATION_NONE
    ) {
        ret = TEXTDECORATION_NONE;
    }
    return ret;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetTkhtmlReplace --
 *
 *     Return a pointer to the string value of the '-tkhtml-replace'
 *     property of pNode, if defined. Otherwise, return NULL.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static CONST char *
nodeGetTkhtmlReplace(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    CssProperty *pR;
    Tcl_Interp *interp = pLayout->pTree->interp;
    pR = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY__TKHTML_REPLACE);
    return HtmlCssPropertyGetString(pR);
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetMargins --
 *
 *     Get the margin properties for a node.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void nodeGetMargins(pLayout, pNode, parentWidth, pMargins)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    int parentWidth;
    MarginProperties *pMargins;
{
    CssProperty *pM;
    Tcl_Interp *interp = pLayout->pTree->interp;
    int widthisauto = 0;   /* True if the 'width' property is set to "auto" */
    int pw = parentWidth;

    pM = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_WIDTH);
    if (!nodeGetTkhtmlReplace(pLayout,pNode)) {
        widthisauto = propertyIsAuto(pM);
    }

    /* Todo: It is also legal to specify an integer between 1 and 4 for
     * margin width. propertyToPixels() can't deal with this because it
     * doesn't know when it is converting is a margin, so it will have to
     * be done here.
     */
    pM = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_MARGIN_TOP);
    pMargins->margin_top = propertyToPixels(pLayout, pNode, pM, pw, 0);

    pM = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_MARGIN_BOTTOM);
    pMargins->margin_bottom = propertyToPixels(pLayout, pNode, pM, pw, 0);

    pM = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_MARGIN_LEFT);
    pMargins->margin_left = propertyToPixels(pLayout, pNode, pM, pw, 0);
    pMargins->leftAuto = (!widthisauto?propertyIsAuto(pM):0);

    pM = HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_MARGIN_RIGHT);
    pMargins->margin_right = propertyToPixels(pLayout, pNode, pM, pw, 0);
    pMargins->rightAuto = (!widthisauto?propertyIsAuto(pM):0);
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetWhitespace --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int nodeGetWhitespace(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    CssProperty *p;
    int ret;
    p = HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_WHITE_SPACE);
    ret = p->eType;
    if (ret != WHITESPACE_PRE && ret != WHITESPACE_NORMAL && 
        ret != WHITESPACE_NOWRAP
    ) {
        ret = WHITESPACE_NORMAL;
    }
    return ret;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetLineHeight --
 *
 *     Return the value of the 'line-height' property, in pixels.
 *
 *     Percentage line-heights are calculated with respect to the font-size
 *     of the node.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int nodeGetLineHeight(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    CssProperty *p;
    int font_pixels = nodeGetEmPixels(pLayout, pNode);
    p = HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_LINE_HEIGHT);
    return propertyToPixels(pLayout, pNode, p, font_pixels, 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeComment --
 *
 *     Add a comment to the canvas using HtmlDrawComment() that describes
 *     the node pNode.  This call is a no-op when HTML_DEBUG is not defined.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
nodeComment(pLayout, pCanvas, pNode)
    LayoutContext *pLayout;
    HtmlCanvas *pCanvas;
    HtmlNode *pNode;
{
#ifdef HTML_DEBUG
    char *zComment;
    zComment = HtmlNodeToString(pNode);
    DRAW_COMMENT(pCanvas, zComment);
    ckfree(zComment);
#endif
}

/*
 *---------------------------------------------------------------------------
 *
 * endNodeComment --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
endNodeComment(pLayout, pCanvas, pNode)
    LayoutContext *pLayout;
    HtmlCanvas *pCanvas;
    HtmlNode *pNode;
{
#ifdef HTML_DEBUG
    char zComment[64];
    sprintf(zComment, "</%s>", HtmlMarkupName(HtmlNodeTagType(pNode)));
    DRAW_COMMENT(pCanvas, zComment);
#endif
}

/*
 *---------------------------------------------------------------------------
 *
 * floatLayout --
 * 
 *     Draw the floating box generated by document node pNode. The box
 *     is drawn into the containing box pBox.
 *
 *     Usually, the floating box is drawn into pBox at y-coordinate *pY.
 *     If there is not enough room horizontally at *pY, the floating box
 *     is moved down. *pY is set to the y-coordinate where the box is
 *     actually drawn before returning.
 *
 *     pBox->pFloats is expected to use the coordinate system of pBox.
 *     The floating margins generated by this box are added to
 *     pBox->pFloats before returning. If box was moved down (i.e if *pY
 *     is modified), then some margins are removed from pBox->pFloats.
 *
 *     The generated box, drawn directly into pBox, is associated with node
 *     pNode at the virtual canvas level.
 *
 * Results:
 *     A Tcl return value - TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int floatLayout(pLayout, pBox, pNode, pY)
    LayoutContext *pLayout;  /* Layout context */
    BoxContext *pBox;        /* Containing box context */
    HtmlNode *pNode;         /* Node that generates floating box */
    int *pY;                 /* IN/OUT: y-coord to draw float at */
{
    int y = *pY;
    int y2;                          /* y-coord of bottom of box */
    BoxContext sBox;                 /* Generated box. */
    MarginProperties margins;        /* Generated box margins. */
    int width;                       /* Width of generated box content. */
    int marginwidth;                 /* Width of box including margins */
    int leftFloat = 0;                   /* left floating margin */
    int rightFloat = pBox->parentWidth;  /* right floating margin */
    DisplayProperties display;       /* Display proprerties */
    int x;
    int marginValid = pLayout->marginValid;
    int marginValue = pLayout->marginValue;
    int marginParent = pLayout->marginParent;

    /* A floating box generates it's own floating margin stack. It is
     * the responsiblity of the caller to make sure the float does not
     * overlap with anything it shouldn't.
     *
     * Todo: The CSS spec implies that if a floating box generates internal 
     * floats that overlap the generated block box the containing boxes
     * content may overlap them. i.e if we have:
     *
     *      YYYYYYYYYYYYYYY ZZZZZZZZZZZZZZZZZZZZZZZZ
     *      YYYYYYYYYYYYYYY ZZZZZZZZZZZZZZZZZZZZZZZZ
     *      XXXXXXX YYYYYYY ZZZZZZZZZZZZZZZZZZZZZZZZ
     *      XXXXXXX YYYYYYY ZZZZZZZZZZZZZZZZZZZZZZZZ
     *      XXXXXXX ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ    *
     *      XXXXXXX ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ    *
     *
     * where Y is this float box, X is a sub-float and Z is content of the
     * containing box then Z would overlap with Z in the two rows marked
     * '*'. This doesn't seem optimal. Maybe we should deviate from the
     * spec here (or maybe I'm reading it wrong).
     */
    memset(&sBox, 0, sizeof(BoxContext));
    sBox.pFloat = HtmlFloatListNew();

    /* Get the display properties. The caller should have already made
     * sure that the node generates a floating block box. But we need
     * to do this too in order to figure out if this is a FLOAT_LEFT or
     * FLOAT_RIGHT box.
     */
    nodeGetDisplay(pLayout, pNode, &display);
    assert(display.eFloat!=FLOAT_NONE);
    assert(display.eDisplay==DISPLAY_BLOCK || display.eDisplay==DISPLAY_TABLE);

    /* According to CSS, a floating box must have an explicit width or
     * replaced content. But if it doesn't, we just assign the minimum
     * width of the floating box. The 'width' value calculated here is the
     * content width of the floating box, it doesn't include margins,
     * padding, or borders.
     */
    width = nodeGetWidth(pLayout, pNode, pBox->parentWidth, -1, 0, 0);
    if (width<0) {
        int min, max;
        blockMinMaxWidth(pLayout, pNode, &min, &max);
        width = min;
    }

    /* Draw the floating box. Set marginValid to 1 and marginValue to 0 to
     * ensure that the top margin of the floating box is allocated. Margins
     * of floating boxes never collapse.
     */
    pLayout->marginValid = 1;
    pLayout->marginValue = 0;
    pLayout->marginParent = 0;
    sBox.contentWidth = width;
    sBox.parentWidth = pBox->parentWidth;
    blockLayout(pLayout, &sBox, pNode, 0, 0);
    sBox.height += pLayout->marginValue;
    pLayout->marginValid = marginValid;
    pLayout->marginValue = marginValue;
    pLayout->marginParent = marginParent;

    if (marginValid) {
        y += marginValue;
    }

    /* Figure out the y-coordinate to draw the floating box at. This is
     * usually the current y coordinate. However if other floats mean that
     * the parent box is not wide enough for this float, we may shift this
     * float downward until there is room.
     */
    y = HtmlFloatListClear(pBox->pFloat, display.eClear, y);
    y = HtmlFloatListPlace(
            pBox->pFloat, pBox->parentWidth, sBox.width, sBox.height, y);
    y2 = y + sBox.height;
    HtmlFloatListMargins(pBox->pFloat, y, y2, &leftFloat, &rightFloat);

    /* Get the exact x coordinate to draw the box at. If it won't fit, 
     * even after shifting down past other floats in code above, then
     * align with the left margin, even if the box is right-floated.
     * Once we have the x coordinate, we can copy the generated box into
     * it's parent box pBox.
     */ 
    if (display.eFloat==FLOAT_LEFT) {
        x = leftFloat;
    } else {
        x = rightFloat - sBox.width;
        if (x<leftFloat) {
            x = leftFloat;
        }
    }
    DRAW_CANVAS(&pBox->vc, &sBox.vc, x, y, pNode);

    /* If the right-edge of this floating box exceeds the current actual
     * width of the box it is drawn in, set the actual width to the 
     * right edge. Floating boxes do not affect the height of the parent
     * box.
     */
    pBox->width = MAX(x+sBox.width, pBox->width);

    /* Fix the float list in the parent block so that nothing overlaps
     * this floating box.
     */
    if (display.eFloat==FLOAT_LEFT) {
        int m = x + sBox.width;
        HtmlFloatListAdd(pBox->pFloat, FLOAT_LEFT, m, *pY, y + sBox.height);
    } else {
        int m = x;
        HtmlFloatListAdd(pBox->pFloat, FLOAT_RIGHT, m, *pY, y + sBox.height);
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * markerLayout --
 *
 *     This is called just before the block part of a list item is 
 *     drawn into *pBox at coordinates (0, 0). This function draws the
 *     list marker. 
 *     
 *     If the 'list-style-position' property is 'inside', then the marker
 *     is drawn with a zero x-coordinate value and a floating
 *     margin added so that the content is wrapped around it. If the
 *     'list-style-position' property is 'outside', then the marker is
 *     drawn with a negative x-coordinate value and no margins are added.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
markerLayout(pLayout, pBox, pNode)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
{
    int style; 
    CONST char *zMarker;     /* Text to draw in the marker box. */
    Tcl_Obj *pMarker;        /* Tcl_Obj copy of zMarker */
    int width;               /* Width of string zMarker in current font */
    Tk_Font font;
    Tk_FontMetrics fontMetrics;
    XColor *color;
    int offset;
    int yoffset;

    /* Todo: This code has to co-operate with nodeGetListStyleType()
     * to generate markers for ordered lists ("1.", "2." etc.).
     */
    style = nodeGetListStyleType(pLayout, pNode);
    switch (style) {
        case LISTSTYLETYPE_SQUARE:
             zMarker = "\xe2\x96\xa1";      /* Unicode 0x25A1 */ 
             break;
        case LISTSTYLETYPE_CIRCLE:
             zMarker = "\xe2\x97\x8b";      /* Unicode 0x25CB */ 
             break;
        case LISTSTYLETYPE_DISC:
             zMarker = "\xe2\x80\xa2";      /* Unicode 0x25CF */ 
             break;
        case LISTSTYLETYPE_NONE:
             zMarker = "";                  /* Nothin' */
             break;
    }
    font = nodeGetFont(pLayout, pNode);
    color = nodeGetColour(pLayout, pNode);
    pMarker = Tcl_NewStringObj(zMarker, -1);
    Tcl_IncrRefCount(pMarker);
    width = Tk_TextWidth(font, zMarker, strlen(zMarker));

    /* Todo: The code below assumes a value of 'outside' for property
     * 'list-marker-position'. Should handle 'inside' as well.
     */

    /* It's not clear to me exactly where the list marker should be
     * drawn when the 'list-style-position' property is 'outside'.
     * The algorithm used is to draw it the width of 1 'x' character
     * in the current font to the left of the content box.
     */
    offset = Tk_TextWidth(font, "x", 1) + width;
    Tk_GetFontMetrics(font, &fontMetrics);
    yoffset = -1 * fontMetrics.ascent;
    DRAW_TEXT(&pBox->vc, pMarker, -1*offset, -1*yoffset, width, 0, font, color);
    Tcl_DecrRefCount(pMarker);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextPushBorder --
 *
 *     Configure the inline-context object with an inline-border that
 *     should start before the inline-box about to be added. The
 *     inline-border object should be obtained with a call to
 *     inlineContextGetBorder(). 
 *
 *     If this function is called twice for the same inline-box, then the
 *     second call creates the innermost border.
 *
 *     This function is used with inlineContextPopBorder() to define the
 *     start and end of inline borders. For example, to create the
 *     following inline layout:
 *
 *             +------------ Border-1 --------+
 *             |              +-- Border-2 --+|
 *             | Inline-Box-1 | Inline-Box-2 ||
 *             |              +--------------+|
 *             +------------------------------+
 *
 *     The sequence of calls should be:
 *
 *         inlineContextPushBorder( <Border-1> )
 *         inlineContextAddInlineCanvas( <Inline-box-1> )
 *         inlineContextPushBorder( <Border-2> )
 *         inlineContextAddInlineCanvas( <Inline-box-2> )
 *         inlineContextPopBorder( <Border 2> )
 *         inlineContextPopBorder( <Border 1> )
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int inlineContextPushBorder(p, pBorder)
    InlineContext *p;
    InlineBorder *pBorder;
{
    pBorder->pNext = p->pBoxBorders;
    p->pBoxBorders = pBorder;
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextPopBorder --
 *
 *     Configure the inline-context such that the innermost active border
 *     is closed after the inline-box most recently added is drawn.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int inlineContextPopBorder(p, pBorder)
    InlineContext *p;
    InlineBorder *pBorder;
{
    InlineBox *pBox;
    if (p->nInline > 0) {
        pBox = &p->aInline[p->nInline-1];
        pBox->nBorderEnd++;
        pBox->nRightPixels += pBorder->box.padding_right;
        pBox->nRightPixels += pBorder->box.border_right;
        pBox->nRightPixels += pBorder->margin.margin_right;
    } else {
        InlineBorder *pBorder = p->pBoxBorders;
        assert(pBorder);
        p->pBoxBorders = pBorder->pNext;
        ckfree((char *)pBorder);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextGetBorder --
 *
 *     This function retrieves the border, background, margin and padding
 *     properties for node pNode. If the properties still all have their
 *     default values, then NULL is returned. Otherwise an InlineBorder
 *     struct is allocated using ckalloc(), populated with the various
 *     property values and returned.
 *
 *     The returned struct is considered private to the inlineContextXXX()
 *     routines. The only legitimate use is to pass the pointer to
 *     inlineContextPushBorders().
 *
 * Results:
 *     NULL or allocated InlineBorder structure.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static InlineBorder *inlineContextGetBorder(pLayout, pNode, parentblock)
    LayoutContext *pLayout; 
    HtmlNode *pNode;
    int parentblock;        /* True if pNode is the parent block-box */
{
    InlineBorder border;
    InlineBorder *pBorder = 0;

    /* TODO: Pass a parent-width to this function to calculate
     * widths/heights specified as percentages.
     */

    if (!parentblock) {
        nodeGetBoxProperties(pLayout, pNode, 0,&border.box);
        nodeGetMargins(pLayout, pNode, 0, &border.margin);
        nodeGetBorderProperties(pLayout, pNode, &border.border);
        nodeGetBorderProperties(pLayout, pNode, &border.border);
    } else {
        memset(&border, 0, sizeof(InlineBorder));
    }
    border.textdecoration = nodeGetTextDecoration(pLayout, pNode);
    border.pNext = 0;

    if (border.box.padding_left   || border.box.padding_right     ||
        border.box.padding_top    || border.box.padding_bottom    ||
        border.box.border_left    || border.box.border_right      ||
        border.box.border_top     || border.box.border_bottom     ||
        border.margin.margin_left || border.margin.margin_right   ||
        border.margin.margin_top  || border.margin.margin_bottom  ||
        border.border.color_bg    || 
        border.textdecoration != TEXTDECORATION_NONE
    ) {
        border.color = nodeGetColour(pLayout, pNode);
        pBorder = (InlineBorder *)ckalloc(sizeof(InlineBorder));
        memcpy(pBorder, &border, sizeof(InlineBorder));
    }

    return pBorder;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextSetTextAlign --
 *
 *     Set the value of the 'text-align' property to use when formatting an
 *     inline-context. An entire inline context always has the same value
 *     for 'text-align', the value assigned to the block that generates the
 *     inline context. For example, in the following code:
 *
 *         <p style="text-align:center">
 *             .... text ....
 *             <span style="text-align:left">
 *             .... more text ....
 *         </p>
 *
 *     all lines are centered. The style attribute of the <span> tag has no
 *     effect on the layout.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
inlineContextSetTextAlign(pInline, textAlign)
    InlineContext *pInline;
    int textAlign;
{
    assert(textAlign == TEXTALIGN_LEFT ||
            textAlign == TEXTALIGN_RIGHT ||
            textAlign == TEXTALIGN_CENTER ||
            textAlign == TEXTALIGN_JUSTIFY
    );
    pInline->textAlign = textAlign;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextSetWhiteSpace --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
inlineContextSetWhiteSpace(pInline, whiteSpace)
    InlineContext *pInline;
    int whiteSpace;
{
    assert(whiteSpace == WHITESPACE_PRE ||
            whiteSpace == WHITESPACE_NOWRAP ||
            whiteSpace == WHITESPACE_NORMAL
    );
    pInline->whiteSpace = whiteSpace;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextSetLineHeight --
 *
 *     If the 'line-height' property is applied to a block-box that
 *     generates an inline context, then this is the minimum height of the
 *     line. 
 *
 *     This function sets the minimum line-height in pixels for an
 *     inline-context.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
inlineContextSetLineHeight(pInline, lineHeight)
    InlineContext *pInline;
    int lineHeight;
{
    pInline->lineHeight = lineHeight;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextAddInlineCanvas --
 *
 *     This function is used to add inline box content to an inline
 *     context. The content is drawn by the caller into the canvas object
 *     returned by this function.
 *
 * Results:
 *     Returns a pointer to an empty html canvas to draw the content of the
 *     new inline-box to.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static HtmlCanvas * 
inlineContextAddInlineCanvas(p, eReplaced, pNode)
    InlineContext *p;
    int eReplaced;          /* True if 'text-decoration' border applies */
    HtmlNode *pNode;
{
    InlineBox *pBox;
    InlineBorder *pBorder;

    p->nInline++;
    if(p->nInline > p->nInlineAlloc) {
        /* We need to grow the InlineContext.aInline array. Note that we
         * don't bother to zero the newly allocated memory. The InlineBox
         * for which the canvas is returned is zeroed below.
         */
        char *a = (char *)p->aInline;
        int nAlloc = p->nInlineAlloc + 25;
        p->aInline = (InlineBox *)ckrealloc(a, nAlloc*sizeof(InlineBox));
        p->nInlineAlloc = nAlloc;
    }

    pBox = &p->aInline[p->nInline - 1];
    memset(pBox, 0, sizeof(InlineBox));
    pBox->pBorderStart = p->pBoxBorders;
    for (pBorder = pBox->pBorderStart; pBorder; pBorder = pBorder->pNext) {
        pBox->nLeftPixels += pBorder->box.padding_left;
        pBox->nLeftPixels += pBorder->box.border_left;
        pBox->nLeftPixels += pBorder->margin.margin_left;
    }
    p->pBoxBorders = 0;
    pBox->eReplaced = eReplaced;
    pBox->pNode = pNode;
    return &pBox->canvas;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextSetBoxDimensions --
 *
 *     This is used to set the effective size of the inline-box most
 *     recently added to this inline-context via AddInlineCanvas().
 *
 *     Inline-box dimensions are specified as three quantities, all in
 *     pixel units:
 *
 *         width:   Width of content.
 *         ascent:  Distance between top of content and the baseline.
 *         descent: Distance between bottom of content and the baseline.
 *
 *     The total height of the content is calculated as (ascent+descent).
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
inlineContextSetBoxDimensions(p, width, ascent, descent)
    InlineContext *p;
    int width;
    int ascent;
    int descent;
{
    InlineBox *pBox;
    assert(p->nInline>0);
    pBox = &p->aInline[p->nInline-1];
    pBox->nContentPixels = width;
    pBox->nAscentPixels = ascent;
    pBox->nDescentPixels = descent;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextAddSpace --
 * 
 *     This function is used to add space generated by white-space
 *     characters to an inline context.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
inlineContextAddSpace(p, nPixels)
    InlineContext *p; 
    int nPixels;
{
    if( p->nInline>0 ){
        InlineBox *pBox = &p->aInline[p->nInline - 1];
        if (p->whiteSpace == WHITESPACE_PRE) {
            pBox->nSpace += nPixels;
        } else {
            pBox->nSpace = MAX(nPixels, pBox->nSpace);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextAddNewLine --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
inlineContextAddNewLine(p, nHeight)
    InlineContext *p; 
    int nHeight;
{
    if (p->nInline > 0 && p->whiteSpace == WHITESPACE_PRE){
        InlineBox *pBox;
        inlineContextAddInlineCanvas(p, 0, 0);
        pBox = &p->aInline[p->nInline - 1];
        pBox->eNewLine = nHeight;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextDrawBorder --
 *
 *     The integer array aRepX[], size (2 * nRepX), stores the
 *     x-coordinates of any replaced inline boxes that have been added to
 *     the line. This is required so that we don't draw the
 *     'text-decoration' on replaced objects (i.e. we don't want to
 *     underline images). Every second entry in aRepX is the start of a
 *     replaced inline box. Each subsequent entry is the end of the
 *     replaced inline box. All values are in the same coordinate system as
 *     the x1 and x2 parameters.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void inlineContextDrawBorder(
pLayout, pCanvas, pBorder, x1, y1, x2, y2, drb, aRepX, nRepX)
    LayoutContext *pLayout;
    HtmlCanvas *pCanvas;
    InlineBorder *pBorder;
    int x1, y1;
    int x2, y2;
    int drb;                  /* Draw Right Border */
    int *aRepX;
    int nRepX;
{
    XColor *c = pBorder->border.color_bg;
    int textdecoration = pBorder->textdecoration;

    int tw, rw, bw, lw;
    XColor *tc, *rc, *bc, *lc;
    int dlb = (pBorder->iStartBox >= 0);        /* Draw Left Border */

    tw = pBorder->box.border_top;
    rw = pBorder->box.border_right;
    bw = pBorder->box.border_bottom;
    lw = pBorder->box.border_left;

    tc = pBorder->border.color_top;
    rc = pBorder->border.color_right;
    bc = pBorder->border.color_bottom;
    lc = pBorder->border.color_left;

    x1 += (dlb ? pBorder->margin.margin_left : 0);
    x2 -= (drb ? pBorder->margin.margin_right : 0);
    y1 += pBorder->margin.margin_top;
    y2 -= pBorder->margin.margin_bottom;
    if (tw>0) {
        DRAW_QUAD(pCanvas, 
            x1, y1, x1+(dlb?lw:0), y1+tw, 
            x2-(drb?rw:0), y1+tw, x2, y1, 
            tc
        );
    }
    if (rw > 0 && drb) {
        DRAW_QUAD(pCanvas, x2, y1, x2-rw, y1+tw, x2-rw, y2-bw, x2, y2, rc);
    }
    if (bw>0) {
        DRAW_QUAD(pCanvas, 
            x2, y2, x2-(drb?rw:0), y2-bw,
            x1+(dlb?lw:0), y2-bw, x1, y2, 
            bc
        );
    }
    if (lw > 0 && dlb) {
        DRAW_QUAD(pCanvas, x1, y2, x1+lw, y2-bw, x1+lw, y1+tw, x1, y1, lc);
    }

    if (c) {
        x1 += (dlb ? pBorder->box.border_left : 0);
        x2 -= (drb ? pBorder->box.border_right : 0);
        y1 += pBorder->box.border_top;
        y2 -= pBorder->box.border_bottom;

        DRAW_QUAD(pCanvas, x1, y1, x2, y1, x2, y2, x1, y2, c);
    }

    if (textdecoration != TEXTDECORATION_NONE) {
        int y;                /* y-coordinate for horizontal line */
        int i;
        XColor *color = pBorder->color;

        x1 += (dlb ? pBorder->box.padding_left : 0);
        x2 -= (drb ? pBorder->box.padding_right : 0);
        y1 += pBorder->box.padding_top;
        y2 -= pBorder->box.padding_bottom;

        switch (textdecoration) {
            case TEXTDECORATION_OVERLINE:
                y = y1;
                break;
            case TEXTDECORATION_LINETHROUGH:
                y = (y2+y1)/2;
                break;
            case TEXTDECORATION_UNDERLINE:
                y = 1;
                break;
            default:
                assert(0);
        }

	/* At this point we draw a horizontal line for the underline,
	 * linethrough or overline decoration. The line is to be drawn
	 * between 'x1' and 'x2' x-coordinates, at y-coordinate 'y'.
         *
         * However, we don't want to draw this decoration on replaced
         * inline boxes. So use the aReplacedX[] array to avoid doing this.
         */
        if (nRepX > 0) {
            int xa = x1;
            for (i = 0; i < nRepX; i++) {
                int xs = aRepX[i*2]; 
                int xe = aRepX[i*2+1]; 
                if (xe <= xs) continue;

                if (xs > xa) {
                    int xb = MIN(xs, x2);
	            DRAW_QUAD(pCanvas,xa,y,xb,y,xb,y+1,xa,y+1,color);
                }
                xa = xe;
            }
            if (xa < x2) {
	        DRAW_QUAD(pCanvas, xa, y, x2, y, x2, y+1, xa, y+1, color);
            }
        } else {
	    DRAW_QUAD(pCanvas, x1, y, x2, y, x2, y+1, x1, y+1, color);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextGetLineBox --
 *
 *     Parameter pWidth is a little complicated. When this function is
 *     called, *pWidth should point to the width available for the
 *     current-line box. If not even one inline-box can fit within this
 *     width, and the 'forcebox' flag is not true, then zero is returned
 *     and *pWidth set to the minimum width required to draw content. If
 *     zero is returned and *pWidth is set to 0, then the InlineContext is
 *     completely empty of inline-boxes and no line-box can be generated.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
inlineContextGetLineBox(
pLayout, p, pWidth, forceline, forcebox, pCanvas, pVSpace, pAscent)
    LayoutContext *pLayout;
    InlineContext *p;
    int *pWidth;              /* IN/OUT: See above */
    int forceline;            /* Draw line even if line is not full */
    int forcebox;             /* Draw at least one inline box */
    HtmlCanvas *pCanvas;      /* Canvas to render line box to */
    int *pVSpace;             /* OUT: Total height of generated linebox */
    int *pAscent;             /* OUT: Ascent of line box */
{
    int i;                   /* Iterator variable for aInline */
    int j;
    int lineboxwidth = 0;    /* Width of line-box */
    int nBox = 0;            /* Number of inline boxes to draw */
    int x = 0;               /* Current x-coordinate */
    double nExtra = -10.0;   /* Extra justification pixels between each box */
    HtmlCanvas content;      /* Canvas for content (as opposed to borders */
    HtmlCanvas borders;      /* Canvas for borders */
    InlineBorder *pBorder;
    int iLeft = 0;           /* Leftmost pixel of line box */
    int width = *pWidth;
    int descent = 0;
    int *aReplacedX = 0;     /* List of x-coords - borders of replaced objs. */
    int nReplacedX = 0;      /* Size of aReplacedX divided by 2 */

    memset(&content, 0, sizeof(HtmlCanvas));
    memset(&borders, 0, sizeof(HtmlCanvas));

    /* If 'white-space' is not "nowrap", count how many of the inline boxes
     * fit within the requested line-box width. Store this in nBox. Also
     * remember the width of the line-box assuming normal word-spacing.
     * We'll need this to handle the 'text-align' attribute later on.
     * 
     * If 'white-space' is "nowrap", then this loop is used to determine
     * the width of the line-box.
     */
    for(i = 0; i < p->nInline; i++) {
        int j;
        InlineBorder *pBorder;
        InlineBox *pBox = &p->aInline[i];
        int boxwidth = pBox->nContentPixels;
        boxwidth += pBox->nRightPixels + pBox->nLeftPixels;
        if(i > 0) {
            boxwidth += p->aInline[i-1].nSpace;
        }
        if (lineboxwidth+boxwidth > width && 
            p->whiteSpace != WHITESPACE_NOWRAP
        ) {
            break;
        }
        lineboxwidth += boxwidth;
        if (pBox->eNewLine) {
            break;
        }
    }
    nBox = i;

    if ((p->nInline == 0) || (!forceline && (nBox == p->nInline))) {
        /* Either the inline context contains no inline-boxes or there are
         * not enough to fill the line-box and the 'force-line' flag is not
         * set. In this case return 0 and set *pWidth to 0 too.
         *
         * This also catches the case where 'white-space' is "nowrap". In
         * that case, we only want to draw the line-box if the 'force-line'
         * flag is set.
         */
        *pWidth = 0;
        return 0;
    }

    if (0 == nBox) {
        if (p->aInline[0].eNewLine) {
            /* The line-box consists of a single new-line only.  */
            *pVSpace = p->aInline[0].eNewLine;
            return 1;
        }
        if (forcebox && !p->aInline[0].eNewLine) {
	    /* The first inline-box is too wide for the supplied width, but
	     * the 'forcebox' flag is set so we have to lay out at least
	     * one box. A gotcha is that we don't want to lay out our last
	     * inline box unless the 'forceline' flag is set. We might need
	     * it to help close an inline-border.
             */
            if (p->nInline > 1 || forceline) {
                nBox = 1;
            } else {
                *pWidth = 0;
                return 0;
            }
        }
    }
    if (nBox == 0) {
	/* If we get here, then their are inline-boxes, but the first
         * of them is too wide for the width we've been offered and the
         * 'forcebox' flag is not true. Return zero, but set *pWidth to the
         * minimum width required before doing so.
         */
        InlineBox *pBox = &p->aInline[i];
        *pWidth = pBox->nContentPixels;
        *pWidth += pBox->nRightPixels + pBox->nLeftPixels;
        return 0;
    }

    if (p->whiteSpace == WHITESPACE_NOWRAP && 
        lineboxwidth > width && 
        !forcebox
    ) {
        /* If the 'white-space' property is set to "nowrap" and the linebox
         * is wider than the allocated width, then only draw it if the
         * 'forcebox' flag is true. Otherwise, give the caller the
         * opportunity to shift the line-box vertically downwards to clear
         * some floating margins.
         */
        *pWidth = lineboxwidth;
        return 0;
    }

    /* Adjust the initial left-margin offset and the nExtra variable to 
     * account for the 'text-align' property. nExtra is the number of extra
     * pixels added between each inline-box. This is how we implement
     * justification.
     */
    switch(p->textAlign) {
        case TEXTALIGN_CENTER:
            iLeft = (width - lineboxwidth) / 2;
            break;
        case TEXTALIGN_RIGHT:
            iLeft = (width - lineboxwidth);
            break;
        case TEXTALIGN_JUSTIFY:
            if (nBox > 1 && width > lineboxwidth && nBox < p->nInline) {
                nExtra = (double)(width - lineboxwidth) / (double)(nBox-1);
            }
            break;
    }
    x += iLeft;

    /* Draw nBox boxes side by side in pCanvas to create the line-box. */
    assert(nBox <= p->nInline);
    *pAscent = 0;
    for(i = 0; i < nBox; i++) {
        int extra_pixels = 0;   /* Number of extra pixels for justification */
        InlineBox *pBox = &p->aInline[i];
        int boxwidth = pBox->nContentPixels;
        int x1, y1;
        int x2, y2;
        int nBorderEnd = 0;

        *pAscent = MAX(pBox->nAscentPixels, *pAscent);
        descent = MAX(pBox->nDescentPixels, descent);
  
        /* If the 'text-align' property is set to "justify", then we add a
         * few extra pixels between each inline box to justify the line.
	 * The calculation of exactly how many is slightly complicated
	 * because we need to avoid rounding bugs. If the right margins
         * vertically adjacent lines of text don't align by 1 or 2 pixels,
         * it spoils the whole effect.
         */
        if (nExtra > 0.0) {
            if (i < nBox-1) {
                extra_pixels = (nExtra * i);
            } else {
                extra_pixels = width - lineboxwidth;
            }
        }

        /* If any inline-borders start with this box, then add them to the
         * active borders list now. Remember the current x-coordinate and
         * inline-box for when we have to go back and draw the border.
         */
        pBorder = pBox->pBorderStart;
        x1 = x + extra_pixels + pBox->nLeftPixels;
        for (pBorder=pBox->pBorderStart; pBorder; pBorder=pBorder->pNext) {
            x1 -= pBorder->margin.margin_left;
            x1 -= pBorder->box.padding_left + pBorder->box.border_left;
            pBorder->iStartBox = i;
            pBorder->iStartPixel = x1;
            if (!pBorder->pNext) {
                pBorder->pNext = p->pBorders;
                p->pBorders = pBox->pBorderStart;
                break;
            }
        }

        /* Copy the inline box canvas into the line-content canvas. If this
         * is a replaced inline box, then add the right and left
         * coordinates for the box to the aReplacedX[] array. This is used
         * to make sure we don't underline replaced objects when drawing
         * inline borders. 
         */
        x1 = x + extra_pixels + pBox->nLeftPixels;
        if (pBox->eReplaced) {
            int nBytes;
            nReplacedX++;
            nBytes = nReplacedX * 2 * sizeof(int);
            aReplacedX = (int *)ckrealloc((char *)aReplacedX, nBytes);
            aReplacedX[(nReplacedX-1)*2] = x1;
            aReplacedX[(nReplacedX-1)*2+1] = x1 + boxwidth;
        }
        DRAW_CANVAS(&content, &pBox->canvas, x1, 0, pBox->pNode);
        x += (boxwidth + pBox->nLeftPixels + pBox->nRightPixels);

        /* If any inline-borders end with this box, then draw them to the
         * border canvas and remove them from the active borders list now. 
         * When drawing borders, we have to traverse the list backwards, so
         * that inner borders (and backgrounds) are drawn on top of outer
         * borders. This is a little clumsy with the singly linked list,
         * but we don't expect the list to ever have more than a couple of
         * elements, so it should be Ok.
         */
        x2 = x + extra_pixels - pBox->nRightPixels;
        y1 = pBox->canvas.top;
        y2 = pBox->canvas.bottom;
        if (i == nBox-1) {
            for (pBorder = p->pBorders; pBorder; pBorder = pBorder->pNext) {
                nBorderEnd++;
            }
        } else {
            nBorderEnd = pBox->nBorderEnd;
        }
        for(j = 0; j < nBorderEnd; j++) {
            int k;
            int nTopPixel = 0;
            int nBottomPixel = 0;
            int rb;
            HtmlCanvas tmpcanvas;

            pBorder = p->pBorders;
            for (k=0; k<j+1; k++) {
                nTopPixel += pBorder->box.padding_top + pBorder->box.border_top;
                nTopPixel += pBorder->margin.margin_top;
                nBottomPixel += pBorder->box.padding_bottom;
                nBottomPixel += pBorder->box.border_bottom;
                nBottomPixel += pBorder->margin.margin_bottom;
                if (k < j) {
                    pBorder = pBorder->pNext;
                }
            }
            assert(pBorder);

            y1 = 0;
            y2 = 0;
            for (k=pBorder->iStartBox; k<=i; k++) {
                if (k >= 0) {
                    y1 = MIN(y1, p->aInline[k].canvas.top);
                    y2 = MAX(y2, p->aInline[k].canvas.bottom);
                }
            }
            y1 -= nTopPixel;
            y2 += nBottomPixel;

            if (pBorder->iStartBox >= 0) {
                x1 = pBorder->iStartPixel;
            } else {
                x1 = iLeft;
            }
            rb = (j < pBox->nBorderEnd);
            if (rb) {
                x2 += pBorder->margin.margin_right;
                x2 += pBorder->box.padding_right + pBorder->box.border_right;
            }

            memset(&tmpcanvas, 0, sizeof(HtmlCanvas));
            DRAW_CANVAS(&tmpcanvas, &borders, 0, 0, 0);
            memset(&borders, 0, sizeof(HtmlCanvas));
            inlineContextDrawBorder(pLayout, &borders, 
                    pBorder, x1, y1, x2, y2, rb, aReplacedX, nReplacedX);
            DRAW_CANVAS(&borders, &tmpcanvas, 0, 0, 0);
        }

        for(j = 0; j < pBox->nBorderEnd; j++) {
            pBorder = p->pBorders;
            if (!pBorder) {
                pBorder = p->pBoxBorders;
                assert(pBorder);
                p->pBoxBorders = pBorder->pNext;;
            } else {
                p->pBorders = pBorder->pNext;
            }
            ckfree((char *)pBorder);
        }

        x += pBox->nSpace;
    }

    /* If any borders are still in the InlineContext.pBorders list, then
     * they flow over onto the next line. Draw the portion that falls on
     * this line now. Set InlineBorder.iStartBox to -1 so that the next
     * call to inlineContextGetLineBox() knows that this border does not
     * require a left-margin.
     */
    for(pBorder = p->pBorders; pBorder; pBorder = pBorder->pNext) {
        pBorder->iStartBox = -1;
    }

    /* If the line-box is vertically smaller than the value of the
     * 'line-height' property applied to the parent of the inline-context,
     * then use this value for the vertical size instead of the content
     * height.
     */
    *pVSpace = *pAscent + descent;
    if (*pVSpace < p->lineHeight) {
        *pVSpace = p->lineHeight;
        *pAscent += (p->lineHeight - *pVSpace) / 2;
    }

    /* Draw the borders and content canvas into the target canvas. Draw the
     * borders canvas first so that it is under the content.
     */
    DRAW_CANVAS(pCanvas, &borders, 0, 0, 0);
    DRAW_CANVAS(pCanvas, &content, 0, 0, 0);

    p->nInline -= nBox;
    memmove(p->aInline, &p->aInline[nBox], p->nInline * sizeof(InlineBox));
    if (p->nInline > 0 && p->aInline[0].eNewLine) {
        int diff = p->aInline[0].eNewLine - (pCanvas->bottom - pCanvas->top);
        if (diff > 0) {
            pCanvas->bottom += diff;
        }
        p->aInline[0].eNewLine = 0;
    }

    if (aReplacedX) {
        ckfree((char *)aReplacedX);
    }
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextIsEmpty --
 *
 *     Return true if there are no inline-boxes currently accumulated in
 *     the inline-context.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
inlineContextIsEmpty(pContext)
    InlineContext *pContext;
{
    return (pContext->nInline==0);
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextCleanup --
 *
 *     Clean-up all the dynamic allocations made during the life-time of
 *     this InlineContext object. The InlineContext structure itself is not
 *     deleted - as this is usually allocated on the stack, not the heap.
 *
 *     The InlineContext object should be considered unusable (as it's
 *     internal state is inconsistent) after this function is called.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
inlineContextCleanup(pContext)
    InlineContext *pContext;
{
    InlineBorder *pBorder;

    if (pContext->aInline) {
        ckfree((char *)pContext->aInline);
    }
    
    pBorder = pContext->pBoxBorders;
    while (pBorder) {
        InlineBorder *pTmp = pBorder->pNext;
        ckfree((char *)pBorder);
        pBorder = pTmp;
    }

    pBorder = pContext->pBorders;
    while (pBorder) {
        InlineBorder *pTmp = pBorder->pNext;
        ckfree((char *)pBorder);
        pBorder = pTmp;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineText --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
inlineText(pLayout, pNode, pContext)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    InlineContext *pContext;
{
    HtmlToken *pToken;
    Tk_Font font;
    Tk_FontMetrics fontmetrics;
    XColor *color;
    int sw;                    /* Space-Width in current font. */
    int nh;                    /* Newline-height in current font */

    assert(pNode && HtmlNodeIsText(pNode));

    font = nodeGetFont(pLayout, pNode);
    color = nodeGetColour(pLayout, pNode);

    sw = Tk_TextWidth(font, " ", 1);
    Tk_GetFontMetrics(font, &fontmetrics);
    nh = fontmetrics.ascent + fontmetrics.descent;

    for(pToken=pNode->pToken; pToken; pToken=pToken->pNext) {
        switch(pToken->type) {
            case Html_Text: {
                Tcl_Obj *pText;
                HtmlCanvas *pCanvas; 
                int tw;            /* Text width */
                int ta;            /* Text ascent */
                int td;            /* Text descent */

                pCanvas = inlineContextAddInlineCanvas(pContext, 0, pNode);
                pText = Tcl_NewStringObj(pToken->x.zText, pToken->count);
                Tcl_IncrRefCount(pText);
                tw = Tk_TextWidth(font, pToken->x.zText, pToken->count);
                ta = fontmetrics.ascent;
                td = fontmetrics.descent;
                inlineContextSetBoxDimensions(pContext, tw, ta, td);
                DRAW_TEXT(pCanvas, pText, 0, 0, tw, sw, font, color);
                Tcl_DecrRefCount(pText);
                break;
            }
            case Html_Space: {
                int i;
                for (i = 0; i < pToken->count; i++) {
                    inlineContextAddSpace(pContext, sw);
                }
                if (pToken->x.newline) {
                    inlineContextAddNewLine(pContext, nh);
                }
                break;
            }
            default:
                return 0;
        }
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineLayoutDrawLines --
 *
 *     This function extracts zero or more line-boxes from an InlineContext
 *     object and draws them to a BoxContext. Variable *pY holds the
 *     Y-coordinate to draw line-boxes at in pBox. It is incremented by the
 *     height of each line box drawn before this function returns.
 *
 *     If parameter 'forceflag' is true, then all inline-boxes currently
 *     held by pContext are layed out, even if this means laying out an
 *     incomplete line. This is used (for example) at the end of a
 *     paragraph.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
inlineLayoutDrawLines(pLayout, pBox, pContext, forceflag, pY)
    LayoutContext *pLayout;
    BoxContext *pBox;
    InlineContext *pContext;
    int forceflag;               /* True to lay out final, incomplete line. */
    int *pY;                     /* IN/OUT: Y-coordinate in sBox.vc. */
{
    int have;
    do {
        HtmlCanvas lc;             /* Line-Canvas */
        int w;
        int f;                     /* Force at least one inline-box per line */
        int y = *pY;               /* Y coord for line-box baseline. */
        int leftFloat = 0;
        int rightFloat = pBox->parentWidth;
        int nV = 0;                /* Vertical height of line. */
        int nA = 0;                /* Ascent of line box. */

        /* Todo: We need a real line-height here, not a hard-coded '10' */
        HtmlFloatListMargins(pBox->pFloat, y, y+10, &leftFloat, &rightFloat);
        f = (rightFloat==pBox->parentWidth && leftFloat==0);

        memset(&lc, 0, sizeof(HtmlCanvas));
        w = rightFloat - leftFloat;
        have = inlineContextGetLineBox(
                pLayout, pContext, &w, forceflag, f, &lc, &nV, &nA);

        if (have) {
            if (pLayout->marginValid) {
                y += pLayout->marginValue;
                pLayout->marginValid = 0;
            }
            DRAW_CANVAS(&pBox->vc, &lc, leftFloat, y+nA, 0);
            y += nV;
            pBox->width = MAX(pBox->width, lc.right + leftFloat);
            pBox->height = MAX(pBox->height, y);
            pLayout->marginParent = 0;
        } else if( w ) {
            /* If have==0 but w has been set to some non-zero value, then
             * there are inline-boxes in the inline-context, but there is
             * not enough space for the first inline-box in the width
             * provided. Increase the Y-coordinate and try the loop again.
             *
             * TODO: Pass the minimum height of the line-box to
             * HtmlFloatListPlace().
             */
            assert(!f);
            y = HtmlFloatListPlace(pBox->pFloat, pBox->parentWidth, w, 1, y);
            have = 1;
           
            /* If we shifted down to avoid a floating margin, then do not
	     * worry about any vertical margin. 
             *
	     * Todo: Possibly we should collapse the existing margin with
	     * the delta-y? Not sure about that right now though. It's a
	     * pretty minor issue really.
             */
            pLayout->marginValid = 0;
        } 

	/* floatListClear(pBox->pFloats, y); */
        *pY = y;
    } while (have);

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineLayoutNode --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
inlineLayoutNode(pLayout, pBox, pNode, pY, pContext)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
{
    DisplayProperties display;   /* Display properties of pNode */
    const char *zReplace = 0;
    int rc = 0;

    nodeGetDisplay(pLayout, pNode, &display);

    /* A floating box. Draw it immediately and update the floating margins 
     * to account for it.
     */
    if (display.eFloat != FLOAT_NONE) {
        int y = *pY;
        floatLayout(pLayout, pBox, pNode, &y);
    }

    /* A block box. 
     */
    else if (display.eDisplay != DISPLAY_INLINE) {
        BoxContext sBox;
        int y = *pY;
        int br_fix = 0;

        /* Flush any inline-boxes accumulated in the inline-context.
         *
         * Handling the <br> tag is a special hack. In the default CSS
         * file for HTML, we have:
         *
         *     BR {
         *         display: block;
         *         height: 1em;
         *     }
         *
         * So if this is a <br> tag, and there are one or more inline-boxes
	 * accumulated in the inline-context, then the <br> is the newline
	 * at the end of the last line-box. But blockLayout() will also add
	 * on 1em of vertical space. So we have the hack below to get
	 * around this.
         *
         * If somebody messes with the <br> tag in another stylesheet, for
         * example to draw a horizontal line or something, this will all go
         * horribly wrong. But no-one would do that, right? Right.
         */
        if (!inlineContextIsEmpty(pContext) &&
            HtmlNodeTagType(pNode)==Html_BR) 
        {
            br_fix = nodeGetHeight(pLayout, pNode, 0, 0);
        }
        rc = inlineLayoutDrawLines(pLayout, pBox, pContext, 1, &y);
        y -= br_fix;

        memset(&sBox, 0, sizeof(BoxContext));
        sBox.parentWidth = pBox->parentWidth;
        sBox.pFloat = pBox->pFloat;

        HtmlFloatListNormalize(sBox.pFloat, 0, -1*y);
        blockLayout(pLayout, &sBox, pNode, 0, 0);
        DRAW_CANVAS(&pBox->vc, &sBox.vc, 0, y, pNode);
        HtmlFloatListNormalize(sBox.pFloat, 0, y);

        y += sBox.height;

        pBox->width = MAX(pBox->width, sBox.width);
        pBox->height = MAX(pBox->height, y);

        *pY = y;
    }

    /* If the node is a text node, then add each word of text to the
     * inline-context as a seperate inline-box.
     */
    else if (HtmlNodeIsText(pNode)) {
        rc = inlineText(pLayout, pNode, pContext);
    }

    /* If none of the above conditions is true, then we have either a 
     * replaced inline node, or an inline node that does not generate
     * any content itself, for example <b> or <span>. What these two cases 
     * have in common is that they may generate inline borders, margins
     * padding and backgrounds.
     */
    else {

        if (0 != (zReplace=nodeGetTkhtmlReplace(pLayout, pNode))) {
            BoxContext sBox;

            LayoutContext sLayout;
            HtmlCanvas *pCanvas;
            int yoffset;
            HtmlNode *pParent = HtmlNodeParent(pNode);
            int marginValid = pLayout->marginValid;
            int marginValue = pLayout->marginValue;
            int marginParent = pLayout->marginParent;
             
            assert(pParent);
            memset(&sBox, 0, sizeof(BoxContext));
            sBox.pFloat = HtmlFloatListNew();
            sBox.parentWidth = pBox->parentWidth;
            memcpy(&sLayout, pLayout, sizeof(LayoutContext));

            pLayout->marginValid = 1;
            pLayout->marginValue = 0;
            pLayout->marginParent = 0;

            blockLayout(pLayout, &sBox, pNode, 0, 1);
            sBox.height += pLayout->marginValue;
            HtmlFloatListDelete(sBox.pFloat);

            pLayout->marginValid = marginValid;
            pLayout->marginValue = marginValue;
            pLayout->marginParent = marginParent;

            switch (nodeGetVAlign(pLayout, pNode, VALIGN_BASELINE)) {
                case VALIGN_TEXT_BOTTOM: {
                    Tk_FontMetrics fm;
                    Tk_Font font = nodeGetFont(pLayout, pParent);
                    Tk_GetFontMetrics(font, &fm);
                    yoffset = -1 * (sBox.height - fm.descent);
                    break;
                }
                case VALIGN_TEXT_TOP: {
                    Tk_FontMetrics fm;
                    Tk_Font font = nodeGetFont(pLayout, pParent);
                    Tk_GetFontMetrics(font, &fm);
                    yoffset = -1 * fm.ascent;
                    break;
                }
                case VALIGN_MIDDLE: {
                    int ex = nodeGetExPixels(pLayout, pNode);
                    yoffset = -1 * (sBox.height+ex) / 2;
                    break;
                }
                case VALIGN_BASELINE:
                default:
                    yoffset = -1 * sBox.height;
                    break;
            }

            pCanvas = inlineContextAddInlineCanvas(pContext, 1, pNode);
            DRAW_CANVAS(pCanvas, &sBox.vc, 0, yoffset, pNode);
            inlineContextSetBoxDimensions(
                pContext, sBox.width, -1 * yoffset, sBox.height + yoffset);
        }

        /* If there was no replacement image or widget, recurse through the
         * child nodes.
         */
        if (!zReplace) {
            int i;
            InlineBorder *pBorder;
            pBorder = inlineContextGetBorder(pLayout, pNode, 0);
            if (pBorder) {
                inlineContextPushBorder(pContext, pBorder);
            }
            for(i=0; i<HtmlNodeNumChildren(pNode) && 0==rc; i++) {
                HtmlNode *pChild = HtmlNodeChild(pNode, i);
                rc = inlineLayoutNode(pLayout, pBox, pChild, pY, pContext);
            }
            if (pBorder) {
                inlineContextPopBorder(pContext, pBorder);
            }
        }
    }

    /* See if there are any complete line-boxes to copy to the main canvas. 
     */
    if(rc == 0) {
        rc = inlineLayoutDrawLines(pLayout, pBox, pContext, 0, pY);
    }

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineLayout --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int inlineLayout(pLayout, pBox, pNode)
    LayoutContext *pLayout;       /* Layout context */
    BoxContext *pBox;             /* Box context to draw inline elements */
    HtmlNode *pNode;              /* Node to start drawing at */
{
    InlineContext context;
    int y = 0;
    HtmlNode *pN;
    HtmlCanvas lastline;
    int width;
    int rc;                       /* Return Code */
    HtmlNode *pParent;
    InlineBorder *pBorder;
   
    memset(&context, 0, sizeof(InlineContext));
    memset(&lastline, 0, sizeof(HtmlCanvas));

    pParent = HtmlNodeParent(pNode);
    assert(pParent);

    /* If we are currently running a min-max width test, then set the
     * effective value of the 'text-align' property to "left". Any of the
     * other values ("justify", "center", "right") will screw up the 
     * required width calculation.
     */
    if (pLayout->minmaxTest) {
        inlineContextSetTextAlign(&context, TEXTALIGN_LEFT);
    } else {
        inlineContextSetTextAlign(&context, nodeGetTextAlign(pLayout, pParent));
    }

    inlineContextSetWhiteSpace(&context, nodeGetWhitespace(pLayout, pParent));
    inlineContextSetLineHeight(&context, nodeGetLineHeight(pLayout, pParent));

    pBorder = inlineContextGetBorder(pLayout, pParent, 1);
    if (pBorder) {
        inlineContextPushBorder(&context, pBorder);
    }
    for(pN=pNode; pN ; pN = HtmlNodeRightSibling(pN)) {
        inlineLayoutNode(pLayout, pBox, pN, &y, &context);
    }

    rc = inlineLayoutDrawLines(pLayout, pBox, &context, 1, &y);
    inlineContextCleanup(&context);
    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * blockMinMaxWidth --
 *
 *     Figure out the minimum and maximum widths that this block may use.
 *     This is used during table and floating box layout.
 *
 *     The returned widths include the content, borders, padding and
 *     margins.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
blockMinMaxWidth(pLayout, pNode, pMin, pMax)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    int *pMin;
    int *pMax;
{
    BoxContext sBox;
    int min;        /* Minimum width of this block */
    int max;        /* Maximum width of this block */
    int *pCache;

    Tcl_HashEntry *pEntry;
    int newentry = 1;

    pEntry = Tcl_CreateHashEntry(
        &pLayout->widthCache, (char*)pNode, &newentry);
    if (newentry) {
        int minmaxTestOrig = pLayout->minmaxTest;
        pLayout->minmaxTest = 1;

        /* Figure out the minimum width of the box by
         * pretending to lay it out with a parent-width of 0.
         * Todo: We should make the virtual canvas sBox.vc a
         * black-hole here, so we don't waste energy building
         * up a canvas we will never use.
         */
        memset(&sBox, 0, sizeof(BoxContext));
        sBox.pFloat = HtmlFloatListNew();
        blockLayout(pLayout, &sBox, pNode, 0, 1);
        HtmlDrawCleanup(&sBox.vc);
        min = sBox.width;
        HtmlFloatListDelete(sBox.pFloat);
    
        /* Figure out the maximum width of the box by pretending to lay it
         * out with a very large parent width. It is not expected to
         * be a problem that tables may be layed out incorrectly on
         * displays wider than 10000 pixels.
         */
        memset(&sBox, 0, sizeof(BoxContext));
        sBox.pFloat = HtmlFloatListNew();
        sBox.parentWidth = 10000;
        blockLayout(pLayout, &sBox, pNode, 0, 1);
        HtmlDrawCleanup(&sBox.vc);
        max = sBox.width;
        HtmlFloatListDelete(sBox.pFloat);

        assert(max>=min);

        pCache = (int *)ckalloc(sizeof(int)*2);
        pCache[0] = min;
        pCache[1] = max;
        Tcl_SetHashValue(pEntry, pCache);

        pLayout->minmaxTest = minmaxTestOrig;
    } else {
        pCache = Tcl_GetHashValue(pEntry);
        min = pCache[0];
        max = pCache[1];
    }

    *pMin = min;
    *pMax = max;
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * tableColWidthSingleSpan --
 *
 *     A tableIterate() callback to calculate the widths of all single
 *     span columns in the table.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
tableColWidthSingleSpan(pNode, col, colspan, row, rowspan, pContext)
    HtmlNode *pNode;
    int col;
    int colspan;
    int row;
    int rowspan;
    void *pContext;
{
    TableData *pData = (TableData *)pContext;

    if (colspan==1) {
        int max;
        int min;
        int w;
        int f = 0;

        /* See if the cell has an explicitly requested width. */
        w = nodeGetWidth(pData->pLayout, pNode, pData->availablewidth, 0, &f,0);

        /* And figure out the minimum and maximum widths of the content */
        blockMinMaxWidth(pData->pLayout, pNode, &min, &max);

        if (w && f && w>min && w>pData->aMinWidth[col]) {
            pData->aMinWidth[col] = w;
            pData->aMaxWidth[col] = w;
            pData->aWidth[col] = w;
        } else {
            pData->aMinWidth[col] = MAX(pData->aMinWidth[col], min);
            pData->aMaxWidth[col] = MAX(pData->aMaxWidth[col], max);
            if (w && w>pData->aMinWidth[col]) {
                pData->aWidth[col] = w;
                pData->aMaxWidth[col] = w;
            }
        }
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * tableColWidthMultiSpan --
 *
 *     A tableIterate() callback to calculate the minimum and maximum
 *     widths of multi-span cells, and adjust the minimum and maximum
 *     column widths if required.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int tableColWidthMultiSpan(pNode, col, colspan, row, rowspan, pContext)
    HtmlNode *pNode;
    int col;
    int colspan;
    int row;
    int rowspan;
    void *pContext;
{
    TableData *pData = (TableData *)pContext;
    if (colspan>1) {
        int max;
        int min;
        int currentmin = 0;
        int currentmax = 0;
        int i;
        int numstretchable = 0;

        for (i=col; i<(col+colspan); i++) {
            currentmin += pData->aMinWidth[i];
            currentmax += pData->aMaxWidth[i];
            if (!pData->aWidth[i]) {
                numstretchable++;
            }
        }
        currentmin += (pData->border_spacing * (colspan-1));
        currentmax += (pData->border_spacing * (colspan-1));

        blockMinMaxWidth(pData->pLayout, pNode, &min, &max);
        if (min>currentmin) {
            int incr = (min-currentmin)/(numstretchable?numstretchable:colspan);
            for (i=col; i<(col+colspan); i++) {
                if (numstretchable==0 || pData->aWidth[i]==0) {
                    pData->aMinWidth[i] += incr;
                    if (pData->aMinWidth[i]>pData->aMaxWidth[i]) {
                        currentmax += (pData->aMinWidth[i]-pData->aMaxWidth[i]);
                        pData->aMaxWidth[i] = pData->aMinWidth[i];
                    }
                }
            }
        }
        if (max>currentmax) {
            int incr = (max-currentmax)/(numstretchable?numstretchable:colspan);
            for (i=col; i<(col+colspan); i++) {
                if (numstretchable==0 || pData->aWidth[i]==0) {
                    pData->aMaxWidth[i] += incr;
                }
            }
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * tableCountCells --
 *
 *     A callback invoked by the tableIterate() function to figure out
 *     how many columns are in the table.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
tableCountCells(pNode, col, colspan, row, rowspan, pContext)
    HtmlNode *pNode;
    int col;
    int colspan;
    int row;
    int rowspan;
    void *pContext;
{
    TableData *pData = (TableData *)pContext;
 
    /* For the purporses of figuring out the dimensions of the table, cells
     * with rowspan or colspan of 0 count as 1.
     */
    if (rowspan==0) {
        rowspan = 1;
    }
    if (colspan==0) {
        colspan = 1;
    }

    if (pData->nCol<(col+colspan)) {
        pData->nCol = col+colspan;
    }
    if (pData->nRow<(row+rowspan)) {
        pData->nRow = row+rowspan;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * borderLayout --
 *
 *     Draw a border into context pBox around the rectangle with top-left
 *     and bottom right coordinates (x1,y1) and (x2,y2). The line style,
 *     color and thickness of the border are dealt with by this function.
 *     The caller is responsible for making sure space has been allocated
 *     to draw the border area.
 * 
 *     Each coordinate refers to the 'outside' pixel that is included in
 *     the border. i.e if the coordinates are (3, 3) and (8,10) and the
 *     border width is 2, then points (3, 3), (4, 4), (7, 9) and
 *     (8, 10) are all part of the border area. The box left to draw in
 *     inside the border will be 4 pixels wide and 2 pixels high.
 *
 *         34567890
 *        3########
 *        4########
 *        5##    ##                '#' = border area.
 *        6##    ##                ' ' = interal area.
 *        7########
 *        8########
 *       
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void borderLayout(pLayout, pNode, pBox, x1, y1, x2, y2)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    BoxContext *pBox;
    int x1;
    int y1;
    int x2;
    int y2;
{
    BoxProperties boxproperties;
    BorderProperties borderproperties;
    int tw, rw, bw, lw;
    XColor *tc, *rc, *bc, *lc;

    /* TODO: Parent width */
    nodeGetBoxProperties(pLayout, pNode, 0, &boxproperties);
    nodeGetBorderProperties(pLayout, pNode, &borderproperties);

    tw = boxproperties.border_top;
    rw = boxproperties.border_right;
    bw = boxproperties.border_bottom;
    lw = boxproperties.border_left;

    tc = borderproperties.color_top;
    rc = borderproperties.color_right;
    bc = borderproperties.color_bottom;
    lc = borderproperties.color_left;

    /* Top border polygon */
    if (tw>0) {
        DRAW_QUAD(&pBox->vc, x1, y1, x1+lw, y1+tw, x2-rw, y1+tw, x2, y1, tc);
    }
    if (rw>0) {
        DRAW_QUAD(&pBox->vc, x2, y1, x2-rw, y1+tw, x2-rw, y2-bw, x2, y2, rc);
    }
    if (bw>0) {
        DRAW_QUAD(&pBox->vc, x2, y2, x2-rw, y2-bw, x1+lw, y2-bw, x1, y2, bc);
    }
    if (lw>0) {
        DRAW_QUAD(&pBox->vc, x1, y2, x1+lw, y2-bw, x1+lw, y1+tw, x1, y1, lc);
    }

    if (borderproperties.color_bg) {
        if (pNode!=pLayout->pTop) {
            DRAW_QUAD(&pBox->vc, 
                    x1+lw, y1+tw, 
                    x2-rw, y1+tw, 
                    x2-rw, y2-bw, 
                    x1+lw, y2-bw, borderproperties.color_bg);
        } else {
            DRAW_BACKGROUND(&pBox->vc, borderproperties.color_bg);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * tableDrawRow --
 *
 *     This is a tableIterate() 'row callback' used while actually drawing
 *     table data to canvas. See comments above tableDrawCells() for a
 *     description.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
tableDrawRow(pNode, row, pContext)
    HtmlNode *pNode;
    int row;
    void *pContext;
{
    TableData *pData = (TableData *)pContext;
    LayoutContext *pLayout = pData->pLayout;
    int nextrow = row+1;
    int x = pData->border_spacing;
    int i;

    for (i=0; i<pData->nCol; i++) {
        TableCell *pCell = &pData->aCell[i];
        if (pCell->finrow==nextrow) {
            int x1, y1, x2, y2;
            int y;
            int valign;
            int k;

            x1 = x;
            x2 = x1;
            for (k=i; k<(i+pCell->colspan); k++) {
                x2 += pData->aWidth[k];
            }
            x2 += ((pCell->colspan-1) * pData->border_spacing);
            y1 = pData->aY[pCell->startrow];
            y2 = pData->aY[pCell->finrow] - pData->border_spacing;

            borderLayout(pLayout, pCell->pNode, pData->pBox, x1, y1, x2, y2);

            /* Todo: The formulas for the various vertical alignments below
             *       only work if the top and bottom borders of the cell
             *       are of the same thickness. Same goes for the padding.
             */
            valign = nodeGetVAlign(pData->pLayout, pCell->pNode, VALIGN_MIDDLE);
            switch (valign) {
                case VALIGN_TOP:
                case VALIGN_BASELINE:
                    y = pData->aY[pCell->startrow];
                    break;
                case VALIGN_BOTTOM:
                    y = pData->aY[pCell->finrow] - 
                        pCell->box.height -
                        pData->border_spacing;
                    break;
                default:
                    y = pData->aY[pCell->startrow] + 
                            (y2-y1-pCell->box.height) / 2;
                    break;
            }
            DRAW_CANVAS(&pData->pBox->vc, &pCell->box.vc,x,y,pCell->pNode);
            memset(pCell, 0, sizeof(TableCell));
        }
        x += pData->aWidth[i] + pData->border_spacing;
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * tableDrawCells --
 *
 *     tableIterate() callback to actually draw cells. Drawing uses two
 *     callbacks. This function is called for each cell in the table
 *     and the tableDrawRow() function above is called after each row has
 *     been completed.
 *
 *     This function draws the cell into the BoxContext at location
 *     aCell[col-number].box  in the TableData struct. The border and
 *     background are not drawn at this stage.
 *
 *     When the tableDrawRow() function is called, it is possible to
 *     determine the height of the row. This is needed before cell contents
 *     can be copied into the table canvas, so that the cell can be
 *     vertically aligned correctly, and so that the cell border and
 *     background match the height of the row they are in.
 * 
 *     Plus a few complications for cells that span multiple rows.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
tableDrawCells(pNode, col, colspan, row, rowspan, pContext)
    HtmlNode *pNode;
    int col;
    int colspan;
    int row;
    int rowspan;
    void *pContext;
{
    TableData *pData = (TableData *)pContext;
    BoxContext *pBox;
    int i;
    int x = 0;
    int y = 0;
    int belowY;

    /* A rowspan of 0 means the cell spans the remainder of the table
     * vertically.  Similarly, a colspan of 0 means the cell spans the
     * remainder of the table horizontally. 
     */
    if (rowspan<=0) {
        rowspan = (pData->nRow-row);
    }
    if (colspan<=0) {
        colspan = (pData->nCol-col);
    }

    y = pData->aY[row];
    if (y==0) {
        y = pData->border_spacing * (row+1);
        pData->aY[row] = y;
    }

    for (i=0; i<col; i++) {
        x += pData->aWidth[i];
    }
    x += ((col+1) * pData->border_spacing);

    pBox = &pData->aCell[col].box;
    assert (pData->aCell[col].finrow==0);
    pData->aCell[col].finrow = row+rowspan;
    pData->aCell[col].startrow = row;
    pData->aCell[col].pNode = pNode;
    pData->aCell[col].colspan = colspan;

    pBox->pFloat = HtmlFloatListNew();

    pBox->parentWidth = pData->aWidth[col];
    for (i=col+1; i<col+colspan; i++) {
        pBox->parentWidth += (pData->aWidth[i] + pData->border_spacing);
    }

    pData->pLayout->marginValid = 0;
    blockLayout(pData->pLayout, pBox, pNode, 1, 0);
    belowY = y + pBox->height + pData->border_spacing;

    assert(row+rowspan < pData->nRow+1);
    pData->aY[row+rowspan] = MAX(pData->aY[row+rowspan], belowY);
    for (i=row+rowspan+1; i<=pData->nRow; i++) {
        pData->aY[i] = MAX(pData->aY[row+rowspan], pData->aY[i]);
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * tableIterate --
 *
 *     Helper function for tableLayout, used to iterate through cells
 *     of the table. For the table below, the iteration order is W, X,
 *     Y, Z.
 *
 *     /-------\
 *     | W | X |       row number = 0
 *     |-------|
 *     | Y | Z |       row number = 1
 *     \-------/
 *
 *     For each cell, the function passed as the second argument is 
 *     invoked. The arguments are a pointer to the <td> or <th> node
 *     that identifies the cell, the column number, the colspan, the row
 *     number, the rowspan, and a copy of the pContext argument passed to
 *     iterateTable().
 *
 * Results:
 *     TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *     Whatever xCallback does.
 *
 *---------------------------------------------------------------------------
 */
static int 
tableIterate(pNode, xCallback, xRowCallback, pContext)
    HtmlNode *pNode;                               /* The <table> node */
    int (*xCallback)(HtmlNode *, int, int, int, int, void *);  /* Callback */
    int (*xRowCallback)(HtmlNode *, int, void *);  /* Row Callback */
    void *pContext;                                /* pContext of callbacks */
{
    int row = 0;
    int i;
    int maxrow = 0;

    /* The following two variables are used to keep track of cells that
     * span multiple rows. The array aRowSpan is dynamically allocated as
     * needed and freed before this function returns. The allocated size
     * of aRowSpan is stored in nRowSpan.
     * 
     * When iterating through the columns in a row (i.e. <th> or <td> tags
     * that are children of a <tr>) if a table cell with a rowspan greater
     * than 1 is encountered, then aRowSpan[<col-number>] is set to
     * rowspan.
     */
    int nRowSpan = 0;        /* Current allocated size of aRowSpans */
    int *aRowSpan = 0;       /* Space to hold row-span data */

    for (i=0; i<HtmlNodeNumChildren(pNode); i++) {
        HtmlNode *pChild = HtmlNodeChild(pNode, i);
        int tagtype = HtmlNodeTagType(pChild);
        if (tagtype==Html_TR) {
            int col = 0;
            int j;
            int k;
            for (j=0; j<HtmlNodeNumChildren(pChild); j++) {
                HtmlNode *p = HtmlNodeChild(pChild, j);
                int tt = HtmlNodeTagType(p);
                if (tt==Html_TD || tt==Html_TH) {
                    CONST char *zSpan;
                    int nSpan;
                    int nRSpan;
                    int rc; 
                    int col_ok = 0;

                    /* Set nSpan to the number of columns this cell spans */
                    zSpan = HtmlNodeAttr(p, "colspan");
                    nSpan = zSpan?atoi(zSpan):1;
                    if (nSpan<0) {
                        nSpan = 1;
                    }

                    /* Set nRowSpan to the number of rows this cell spans */
                    zSpan = HtmlNodeAttr(p, "rowspan");
                    nRSpan = zSpan?atoi(zSpan):1;
                    if (nRSpan<0) {
                        nRSpan = 1;
                    }

                    /* Now figure out what column this cell falls in. The
                     * value of the 'col' variable is where we would like
                     * to place this cell (i.e. just to the right of the
                     * previous cell), but that might change based on cells
                     * from a previous row with a rowspan greater than 1.
                     * If this is true, we shift the cell one column to the
                     * right until the above condition is false.
                     */
                    do {
                        for (k=col; k<(col+nSpan); k++) {
                            if (k<nRowSpan && aRowSpan[k]) break;
                        }
                        if (k==(col+nSpan)) {
                            col_ok = 1;
                        } else {
                            col++;
                        }
                    } while (!col_ok);

                    /* Update the aRowSpan array. It grows here if required. */
                    if (nRSpan!=1) {
                        if (nRowSpan<(col+nSpan)) {
                            int n = col+nSpan;
                            aRowSpan = (int *)ckrealloc((char *)aRowSpan, 
                                    sizeof(int)*n);
                            for (k=nRowSpan; k<n; k++) {
                                aRowSpan[k] = 0;
                            }
                            nRowSpan = n;
                        }
                        for (k=col; k<col+nSpan; k++) {
                            aRowSpan[k] = (nRSpan>1?nRSpan:-1);
                        }
                    }

                    maxrow = MAX(maxrow, row+nRSpan-1);
                    rc = xCallback(p, col, nSpan, row, nRSpan, pContext);
                    if (rc!=TCL_OK) {
                        ckfree((char *)aRowSpan);
                        return rc;
                    }
                    col += nSpan;
                }
            }
            if (xRowCallback) {
                xRowCallback(pChild, row, pContext);
            }
            row++;
            for (k=0; k<nRowSpan; k++) {
                if (aRowSpan[k]) aRowSpan[k]--;
            }
        }
    }

    while (row<=maxrow && xRowCallback) {
        xRowCallback(0, row, pContext);
        row++;
    }

    ckfree((char *)aRowSpan);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * tableCalculateCellWidths  --
 *
 *     Decide on some actual widths for the cells, based on the maximum and
 *     minimum widths, the total width of the table and the floating
 *     margins. As far as I can tell, neither CSS nor HTML specify exactly
 *     how to do this. So we use the following approach:
 *
 *     1. Each cell is assigned it's minimum width.  
 *     2. If there are any columns with an explicit width specified as a
 *        percentage, we allocate extra space to them to try to meet these
 *        requests. Explicit widths may mean that the table is not
 *        completely filled.
 *     3. Remaining space is divided up between the cells without explicit
 *        percentage widths. 
 *     
 *     Data structure notes:
 *        * If a column had an explicit width specified in pixels, then the
 *          aWidth[], aMinWidth[] and aMaxWidth[] entries are all set to
 *          this value.
 *        * If a column had an explicit width as a percentage, then the
 *          aMaxWidth[] and aWidth[] entries are set to this value
 *          (converted to pixels, not as a percentage). The aMinWidth entry
 *          is still set to the minimum width required to render the
 *          column.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
tableCalculateCellWidths(pData, width)
    TableData *pData;
    int width;                       /* Total width available for cells */
{
    int extraspace;
    int extraspace_req;
    int i;                           /* Counter variable for small loops */
    int space;                       /* Remaining pixels to allocate */
    int requested;                   /* How much extra space requested */

    int *aTmpWidth;
    int *aWidth = pData->aWidth;
    int *aMinWidth = pData->aMinWidth;
    int *aMaxWidth = pData->aMaxWidth;
    int nCol = pData->nCol;

    aTmpWidth = (int *)ckalloc(sizeof(int)*nCol);
    space = width;

    requested = 0;
    for (i=0; i<nCol; i++) {
        aTmpWidth[i] = aMinWidth[i];
        space -= aMinWidth[i];
        if (aWidth[i]) {
            requested += (aWidth[i] - aTmpWidth[i]);
        }
    }
    assert(space>=0);

    if (space<requested) {
        /* This algorithm runs if more space has been requested than is
         * available. i.e. if a table contains two cells with widths of
         * 60%. In this case we only asign extra space to cells that have
         * explicitly requested it.
         */
        for (i=0; i<nCol; i++) {
            if (aWidth[i]) {
                int colreq = (aWidth[i] - aTmpWidth[i]);
                int extra;
 
                if (colreq==requested) {
                    extra = space;
                } else {
                    extra = ((double)space/(double)requested)*(double)colreq;
                }

                space -= extra;
                aTmpWidth[i] += extra;
                requested -= colreq;
            }
        }
        assert(space==0);
    } else {
        /* There is more space available than has been requested. The width
         * of each column is increased as follows:
         *
         * 1. Give every column the extra width it has requested.
         */

        int increase_all_cells = 0;
        requested = 0;
        for (i=0; i<nCol; i++) {
            if (aWidth[i]) {
                space -= (aWidth[i] - aTmpWidth[i]);
                aTmpWidth[i] = aWidth[i];
            }
            assert(aMaxWidth[i]>=aTmpWidth[i]);
            requested += (aMaxWidth[i]-aTmpWidth[i]);
        }
        assert(space>=0);

        if (requested == 0) {
            increase_all_cells = 1;
            requested = nCol;
        }

        for (i=0; i<nCol; i++) {
            int colreq = (aMaxWidth[i] - aTmpWidth[i]) + increase_all_cells;
            int extra;

            if (colreq==requested) {
                extra = space;
            } else {
                extra = ((double)space/(double)requested)*(double)colreq;
            }

            space -= extra;
            aTmpWidth[i] += extra;
            requested -= colreq;
        }

    }

    memcpy(aWidth, aTmpWidth, sizeof(int)*nCol);
    ckfree((char *)aTmpWidth);

#ifndef NDEBUG
    for (i=0; i<nCol; i++) {
        assert(aWidth[i] >= aMinWidth[i]);
    }
#endif
}

/*
 *---------------------------------------------------------------------------
 *
 * tableLayout --
 *
 *     Lay out a table node.
 *
 *     Laying out tables largely uses HTML tags directly, instead of
 *     mapping them to CSS using a stylesheet first. This is not ideal, but
 *     the CSS table model is not complete by itself, it relies on the
 *     document language to specify some elements of table structure.
 *     Todo: In the long term, figure out if this can be fixed - either
 *     with CSS3, custom style-sheet syntax, or something I'm not currently
 *     aware of in CSS2.
 *
 *     Note that HTML tags are only used to determine table structure, 
 *     stylesheet rules that apply to table cells are still applied, and
 *     CSS properties assigned to table elements are still respected.
 *     i.e. stuff like "TH {font-weight: bold}" still works.
 *
 *     Todo: List of Html tags/attributes used directly.
 *
 *     This is an incomplete implementation of HTML tables - it does not
 *     support the <col>, <colspan>, <thead>, <tfoot> or <tbody> elements.
 *     Since the parser just ignores tags that we don't know about, this
 *     means that all children of the <table> node should have tag-type
 *     <tr>. Omitting <thead>, <tfoot> and <tbody> is not such a big deal
 *     since it is optional to format these elements differently anyway,
 *     but <col> and <colspan> are fairly important.
 * 
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int tableLayout(pLayout, pBox, pNode)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;         /* The node to layout */
{
    int nCol = 0;            /* Number of columns in this table */
    int i;
    int minwidth;            /* Minimum width of entire table */
    int maxwidth;            /* Maximum width of entire table */
    int width;               /* Actual width of entire table */
    int availwidth;          /* Total width available for cells */
    BoxProperties boxproperties; /* Box properties of the <table> */

    int *aMinWidth = 0;      /* Minimum width for each column */
    int *aMaxWidth = 0;      /* Minimum width for each column */
    int *aWidth = 0;         /* Actual width for each column */
    int *aY = 0;             /* Top y-coord for each row */
    TableCell *aCell = 0;    /* Array of nCol cells used during drawing */
    int marginwidth = 0;
    TableData data;

    int marginValid;         /* Saved pLayout->marginValid value */
    int marginValue;         /* Saved pLayout->marginValue value */
    marginValid = pLayout->marginValid;
    marginValue = pLayout->marginValue;

    memset(&data, 0, sizeof(struct TableData));
    data.pLayout = pLayout;

    assert(pBox->parentWidth>=0);

    #ifndef NDEBUG
    if (1) {
        DisplayProperties display;
        nodeGetDisplay(pLayout, pNode, &display);
        assert(display.eDisplay==DISPLAY_TABLE);
    }
    #endif

    /* Read the value of the 'border-spacing' property (or 'cellspacing'
     * attribute if 'border-spacing' is not defined).
     */
    data.border_spacing = nodeGetBorderSpacing(pLayout, pNode);

    /* First step is to figure out how many columns this table has.
     * There are two ways to do this - by looking at COL or COLGROUP
     * children of the table, or by counting the cells in each rows.
     * Technically, we should use the first method if one or more COL or
     * COLGROUP elements exist. For now though, always use the second 
     * method.
     */
    tableIterate(pNode, tableCountCells, 0, &data);
    nCol = data.nCol;

    /* Allocate arrays for the minimum and maximum widths of each column */

    aMinWidth = (int *)ckalloc(nCol*sizeof(int));
    memset(aMinWidth, 0, nCol*sizeof(int));
    aMaxWidth = (int *)ckalloc(nCol*sizeof(int));
    memset(aMaxWidth, 0, nCol*sizeof(int));
    aWidth = (int *)ckalloc(nCol*sizeof(int));
    memset(aWidth, 0, nCol*sizeof(int));
    aY = (int *)ckalloc((data.nRow+1)*sizeof(int));
    memset(aY, 0, (data.nRow+1)*sizeof(int));
    aCell = (TableCell *)ckalloc(data.nCol*sizeof(TableCell));
    memset(aCell, 0, data.nCol*sizeof(TableCell));

    data.aMaxWidth = aMaxWidth;
    data.aMinWidth = aMinWidth;
    data.aWidth = aWidth;

    /* Figure out the width available for the table between the margins.
     *
     * Todo: Need to take into account padding properties and floats to
     *       figure out the margins.
     */
    data.availablewidth = pBox->parentWidth;

    /* Now calculate the minimum and maximum widths of each column. 
     * The first pass only considers cells that span a single column. In
     * this case the min/max width of each column is the maximum of the 
     * min/max widths for all cells in the column.
     * 
     * If the table contains one or more cells that span more than one
     * column, we make a second pass. The min/max widths are increased,
     * if necessary, to account for the multi-column cell. In this case,
     * the width of each column that the cell spans is increased by 
     * the same amount (plus or minus a pixel to account for integer
     * rounding).
     *
     * The minimum and maximum widths for cells take into account the
     * widths of borders (if applicable).
     */
    tableIterate(pNode, tableColWidthSingleSpan, 0, &data);
    tableIterate(pNode, tableColWidthMultiSpan, 0, &data);

    /* Set variable 'width' to the actual width for the entire table. This
     * is the sum of the widths of the cells plus the border-spacing, plus
     * (Todo) the border of the table itself. Variables minwidth and
     * maxwidth are the minimum and maximum allowable widths for the table
     * based on the min and max widths of the columns.
     *
     * The actual width of the table is based on the following rules, in
     * order of precedence:
     *     * It is never less than minwidth,
     *     * If a width has been specifically requested, via the width
     *       property (or HTML attribute), and it is greater than minwidth,
     *       use the specifically requested width.
     *     * Otherwise use the smaller of maxwidth and the width of the
     *       parent box.
     */
    nodeGetBoxProperties(pLayout, pNode, pBox->parentWidth, &boxproperties);
    minwidth = (nCol+1) * data.border_spacing;
    maxwidth = (nCol+1) * data.border_spacing;
    for (i=0; i<nCol; i++) {
        minwidth += aMinWidth[i];
        maxwidth += aMaxWidth[i];
    }
    assert(maxwidth>=minwidth);

    /* When this function is called, the parentWidth has already been set
     * by blockLayout() if there is an explicit 'width'. So we just need to
     * worry about the implicit minimum and maximum width as determined by
     * the table content here.
     */
    if (pBox->contentWidth) {
        width = pBox->contentWidth;
    } else {
        width = MIN(pBox->parentWidth, maxwidth);
    }
    width = MAX(minwidth, width);

    /* Decide on some actual widths for the cells */
    availwidth = width - (nCol+1)*data.border_spacing;
    tableCalculateCellWidths(&data, availwidth);
    
    /* Now actually draw the cells. */
    data.aY = aY;
    data.aCell = aCell;
    data.pBox = pBox;

    tableIterate(pNode, tableDrawCells, tableDrawRow, &data);

    pBox->height = MAX(pBox->height, data.aY[data.nRow]);
    pBox->width = MAX(pBox->width, width);

    ckfree((char *)aMinWidth);
    ckfree((char *)aMaxWidth);
    ckfree((char *)aWidth);
    ckfree((char *)aY);
    ckfree((char *)aCell);

    pLayout->marginValue = marginValue;
    pLayout->marginValid = marginValid;
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * layoutReplacement --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
layoutReplacement(pLayout, pBox, pNode, zReplace)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    CONST char *zReplace;
{
    int width;
    int height;

    Tk_Window tkwin = pLayout->tkwin;
    Tcl_Interp *interp = pLayout->interp;

    /* Read any explicit 'width' or 'height' property values assigned to
     * the node.
     */
    width = nodeGetWidth(pLayout, pNode, pBox->parentWidth, -1, 0, 0);
    height = nodeGetHeight(pLayout, pNode, pBox->parentWidth, -1);

    if (zReplace[0]=='.') {
        Tk_Window win = Tk_NameToWindow(interp, zReplace, tkwin);
        if (win) {
            Tcl_Obj *pWin = 0;
            if (!pLayout->minmaxTest) {
                pWin = Tcl_NewStringObj(zReplace, -1);
            }
            width = Tk_ReqWidth(win);
            height = Tk_ReqHeight(win);
            DRAW_WINDOW(&pBox->vc, pWin, 0, 0, width, height);
        }
    } else {
	/* Must be an image. Or garbage data returned by an bad Tcl proc.
         * If the later, then resizeImage will return 0.
         */
        Tcl_Obj *pImg;
        int t = pLayout->minmaxTest;
        pImg = HtmlResizeImage(pLayout->pTree, zReplace, &width, &height, t);
        assert(!pLayout->minmaxTest || !pImg);
        DRAW_IMAGE(&pBox->vc, pImg, 0, 0, width, height);
    }

    pBox->width = MAX(pBox->width, width);
    pBox->height = MAX(pBox->height, height);
}

/*
 *---------------------------------------------------------------------------
 *
 * collapseMargins --
 * 
 *     Collapse two margin values according to the following table:
 *
 *         Condition                 Result
 *         -------------------------------------------------------
 *         Both margins -ve          Minimum (most negative) of 
 *                                   top and bottom margins
 *         Both margins +ve          Maximum (most positive) of 
 *                                   top and bottom margins
 *         One margin -ve, one +ve   Sum of top and bottom margins
 *                                   margins.
 *
 * Results:
 *     Collapsed margin value.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
collapseMargins(margin_one, margin_two)
    int margin_one;
    int margin_two;
{
    if (margin_one < 0 && margin_two < 0) {
        return MIN(margin_one, margin_two);
    }
    if (margin_one < 0 || margin_two < 0) {
        return margin_one + margin_two;
    }
    return MAX(margin_one, margin_two);
}

/*
 *---------------------------------------------------------------------------
 *
 * blockLayout --
 *
 *     This function lays out the block box generated by pNode according
 *     to the information in pBox. pNode must generate a block box, not
 *     inline content.
 * 
 *     When this function returns, the generated box has been drawn to
 *     canvas BoxContext.vc with the top-left corner at coordinates (0, 0).
 *     The "top-left corner" is defined as the pixel just inside the top
 *     and left *margins* of the box.
 *
 *     Box width calculation:
 *
 *         If BoxContext.contentWidth is greater than zero, then this is
 *         the width used for the content of the box. Otherwise, the value
 *         of BoxContext.parentWidth, the width of the parent content area
 *         is used along with the 'width' property to calculate a content
 *         width.  If no 'width' property is specified, then the content
 *         width is calculated as follows:
 *
 *         <content-width> =
 *                 BoxContext.parentWidth -
 *                 Width of left and right padding -
 *                 Width of left and right borders -
 *                 Width of left and right margins 
 *
 *         If a 'width' property is specified, then this is the width of
 *         the content. Percentage values are calculated relative to
 *         BoxContext.parentWidth.
 *
 *     Floating boxes:
 *
 *         Normally, boxes layed out by this function do not wrap around
 *         any floating boxes. Instead, the content (i.e. the text) inside
 *         the box wraps around them. The exceptions are:
 * 
 *             * Boxes that are replaced by images or widgets, and
 *             * Boxes that are tables (i.e. <table>).
 *
 *         In this case, the box is positioned either to the side or below
 *         the current floating margins.
 * 
 *     Collapsing vertical margins:
 *
 *         Collapsing margins are implemented using the
 *         LayoutContext.marginValid and LayoutContext.marginValue
 *         variables. 'marginValid' is a flag to say whether the value of
 *         'marginValue' is valid and should be respected.
 *
 *         Assuming it is valid, when this function is called marginValue
 *         is set to the size in pixels of the bottom margin of the block
 *         above this one in the normal flow. Before this function returns,
 *         it is set to the bottom margin of this block. The actual space
 *         allocated for the margin between two vertically adjacent blocks
 *         is given by the following table:
 *
 *         Condition                 Result
 *         -------------------------------------------------------
 *         Both margins -ve          Minimum (most negative) of 
 *                                   top and bottom margins
 *         Both margins +ve          Maximum (most positive) of 
 *                                   top and bottom margins
 *         One margin -ve, one +ve   Sum of top and bottom margins
 *                                   margins.
 *
 *         Situations like this:
 *
 *             <p id="one">First paragraph</p>
 *             <div>
 *                 <p id="two">First paragraph
 *
 *         are a little tricky. In this instance, if the <div> block has no
 *         padding or border, then it's effective margin is calculated by
 *         collapsing the top margin of the <div> and the top margin of
 *         paragraph "two". This calculated margin then collapses with the
 *         bottom-margin of paragraph "one".
 *
 *         i.e. if we have:
 *
 *             #one {margin-bottom-width: 50px}
 *             div  {margin-top-width: 20px}
 *             #two {margin-top-width: 20px}
 *
 *         then there must be 50 pixels, not 70, between the two
 *         paragraphs. Alternatively, if we were to have:
 *
 *             #two {margin-top-width: -20px}
 * 
 *         On the other hand, if we add either of the following rules:
 *
 *             div  {border:  solid 1px}
 *             div  {padding: 1px      }
 *
 *         Then there will be 71 pixels between the two paragraphs.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int blockLayout(pLayout, pBox, pNode, omitborder, noalign)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;         /* The node to layout */
    int omitborder;          /* True to allocate but not draw the border */
    int noalign;             /* True to ignore horizontal alignment props */
{
    DisplayProperties display;     /* Display properties of pNode */
    MarginProperties margin;       /* Margin properties of pNode */
    BoxProperties boxproperties;   /* Padding and border properties */
    BoxContext sBox;               /* Box that tableLayout() etc. use */
    CONST char *zReplace;          /* Value of -tkhtml-replace property */
    int width;                     /* Explicit width of node */
    int availablewidth;            /* Maximum width available */
    int top_margin;                /* Actual top margin for box */
    int y = 0;
    int x = 0;
    int leftFloat = 0;             /* Floating margins. Used for tables */
    int rightFloat = 0;            /* and replaced blocks only. */
    Tcl_Interp *interp = pLayout->interp;
    int isBoxObject;               /* True if the node cannot wrap. */
    int isReplaced;                /* True if the node is an image or window */
    int marginValid = pLayout->marginValid; /* Value of marginValid on entry */

    memset(&sBox, 0, sizeof(BoxContext));

    /* Retrieve the required CSS property values. Return early if the
     * display is set to 'none'. There is nothing to draw for this box.
     */
    nodeGetDisplay(pLayout, pNode, &display);
    if (display.eDisplay==DISPLAY_NONE) {
        return TCL_OK;
    }
    nodeGetMargins(pLayout, pNode, pBox->parentWidth, &margin);
    nodeGetBoxProperties(pLayout, pNode, pBox->parentWidth, &boxproperties);
    zReplace = nodeGetTkhtmlReplace(pLayout, pNode);

    /* If the node is a table, or a replaced node (image or Tk window),
     * then it's content cannot wrap around any floating boxes. Also it
     * needs to be horizontally aligned after it's drawn. Set the
     * isBoxObject flag so we know to do these things.
     */
    isReplaced = (zReplace?1:0);
    isBoxObject = (display.eDisplay==DISPLAY_TABLE || isReplaced);

    /* Adjust the Y-coordinate to account for the 'clear' property. */
    y = HtmlFloatListClear(pBox->pFloat, display.eClear, y);

    leftFloat = 0;
    rightFloat = pBox->parentWidth;
    if (pLayout->minmaxTest==0 && isBoxObject) {
        /* If this is a replaced node or a table, then see if we need to
         * dodge around any floating margins. With other blocks, the
         * content wraps around the margin, not the block itself.
         */
        int min; 
        int dummymax;
    
        blockMinMaxWidth(pLayout, pNode, &min, &dummymax);
        y = HtmlFloatListPlace(pBox->pFloat, pBox->parentWidth, min, 1, y);
        /* Todo: Need the actual height of the box here, not '10' */
        HtmlFloatListMargins(pBox->pFloat, y, y+10, &leftFloat, &rightFloat);
    }

    /* Figure out how much horizontal space the node content will
     * have available to it. This is used to set the parentWidth when the
     * of the box that the node is drawn into, and to justify the box
     * withing it's parent if required.
     */
    availablewidth = rightFloat - leftFloat -
            margin.margin_left - margin.margin_right -
            boxproperties.border_left - boxproperties.border_right -
            boxproperties.padding_left - boxproperties.padding_right;

    if (display.eDisplay == DISPLAY_TABLECELL) {
        sBox.parentWidth = availablewidth;
    } else if (display.eDisplay == DISPLAY_TABLE) {
        int isauto;
        int w = rightFloat - leftFloat;
        w = nodeGetWidth(pLayout, pNode, w, availablewidth, 0, &isauto);
        if (isauto) {
            sBox.parentWidth = w;
        } else {
            sBox.parentWidth = w;
            sBox.contentWidth = w;
            sBox.width = w;
        }
        sBox.parentWidth = MAX(sBox.parentWidth, 0);
    } else {
        if (pBox->contentWidth > 0) {
            sBox.parentWidth = pBox->contentWidth;
            availablewidth = pBox->contentWidth;
        } else if (!isReplaced) {
            int w = rightFloat - leftFloat;
            int isauto;
            w = nodeGetWidth(pLayout, pNode, w, availablewidth, 0, &isauto);
            sBox.parentWidth = w;
            if (!isauto) {
                sBox.contentWidth = w;
                sBox.width = w;
            }
        } else {
            /* If this is a replaced node, then set the parent-width to the 
             * total available width. The code to layout the replacement
             * will determin the width based on the intrinsic width, or
             * relative to the entire parent width.
             */
            sBox.parentWidth = rightFloat - leftFloat;
        }
        sBox.parentWidth = MAX(sBox.parentWidth, 0);
        if (!pLayout->minmaxTest && !isBoxObject) {
            sBox.width = sBox.parentWidth;
        }
    }

    /* Allocate space for the top margin. See the header comment of this
     * function for an explanation.
     */
    if (pLayout->marginValid) {
        top_margin = collapseMargins(pLayout->marginValue, margin.margin_top);
        top_margin = collapseMargins(top_margin, pLayout->marginParent);
        y -= pLayout->marginParent;
    } else {
        top_margin = 0;
    }
    y += top_margin;

    /* Also leave a pixel or two for the top-border. */
    y += boxproperties.border_top;
    y += boxproperties.padding_top;

    /* Allocate space for the left margin of the box. Horizontal margins
     * never collapse, so there is nothing complex to do here.
     */
    x = leftFloat;
    x += margin.margin_left;
    x += boxproperties.border_left;
    x += boxproperties.padding_left;

    /* Normalize the floating margins for the box to be drawn. */
    sBox.pFloat = pBox->pFloat;
    HtmlFloatListNormalize(sBox.pFloat, -1*x, -1*y);

    /* The minimum height of the box is set by the 'height' property. 
     * So sBox.height to this now. Layout routines only ever overwrite
     * sBox.height with larger values.
     */
    sBox.height = nodeGetHeight(pLayout, pNode, 0, 0);

    pLayout->marginValid = 1;
    pLayout->marginValue = 0;
    if (boxproperties.border_top > 0 || boxproperties.padding_top > 0) {
        pLayout->marginParent = 0;
    } else {
        pLayout->marginParent = top_margin;
    }
    if (isReplaced) {
        layoutReplacement(pLayout, &sBox, pNode, zReplace);
    } else {
        /* Draw the box using the function specific to it's display type,
         * then copy the drawing into the parent canvas.
         */
        switch (display.eDisplay) {
            case DISPLAY_LISTITEM:
                markerLayout(pLayout, &sBox, pNode);
            case DISPLAY_BLOCK:
            case DISPLAY_INLINE:
            case DISPLAY_TABLECELL:
                if (HtmlNodeNumChildren(pNode)>0) {
                    inlineLayout(pLayout, &sBox, HtmlNodeChild(pNode, 0));
                }
                break;
            case DISPLAY_TABLE:
                tableLayout(pLayout, &sBox, pNode);
                break;
            default:
                assert(!"Impossible!");
        };
    };

    /* Special case: If the block had no contents or explicit height, do
     * not draw the border, or allocate any padding. This is the observed
     * behaviour of modern browsers.
     * 
     * Update: I'm not so sure about this anymore....
     */
    if (!HtmlDrawIsEmpty(&sBox.vc) || 
        sBox.height > 0 || 
        display.eDisplay == DISPLAY_TABLECELL ||
        display.eClear != CLEAR_NONE
    ) {
        int hoffset = 0;

        if (!noalign) {
            int textalign = 0;
    
            /* A table or replaced object may be aligned horizontally using
             * the 'text-align' property. This does not apply if the object
             * is floated, as horizontal alignment is done by the caller in
             * that case.
             */
            if (display.eFloat==FLOAT_NONE && isBoxObject) {
                textalign = nodeGetTextAlign(pLayout, pNode);
            }
    
            /* There are two ways to specify the horizontal align a block. If
             * the block is a table or a replaced element, then it respects the
             * 'text-align' property. (For other blocks, 'text-align' 
             */
            if ((textalign == TEXTALIGN_CENTER || 
                 textalign == TEXTALIGN_JUSTIFY ||
                (margin.leftAuto && margin.rightAuto)) &&
                availablewidth > sBox.width 
            ) {
                hoffset = (availablewidth - sBox.width)/2;
            }
            else if ((textalign == TEXTALIGN_RIGHT || (margin.leftAuto)) &&
                availablewidth > sBox.width 
            ) {
                hoffset = (availablewidth-sBox.width);
            }
        }

        /* Draw the border directly into the parent canvas. */
        if (!omitborder) {
            int x1 = margin.margin_left + leftFloat + hoffset;
            int y1 = y - boxproperties.border_top - boxproperties.padding_top;
            int x2 = x1 + sBox.width + 
                    boxproperties.padding_left + 
                    boxproperties.padding_right + 
                    boxproperties.border_left + 
                    boxproperties.border_right;
            int y2 = y1 + sBox.height + 
                    boxproperties.padding_bottom +
                    boxproperties.padding_top + 
                    boxproperties.border_top +
                    boxproperties.border_bottom;
            borderLayout(pLayout, pNode, pBox, x1, y1, x2, y2);
        }

        nodeComment(pLayout, &pBox->vc, pNode);
        DRAW_CANVAS(&pBox->vc, &sBox.vc, x + hoffset, y, pNode);
        endNodeComment(pLayout, &pBox->vc, pNode);
    
        pBox->height = sBox.height + y + 
            boxproperties.border_bottom + boxproperties.padding_bottom;
        pBox->width = MAX(pBox->width, sBox.width + 
                + margin.margin_left + margin.margin_right +
                boxproperties.border_left + boxproperties.border_right +
                boxproperties.padding_left + boxproperties.padding_right);
 
        /* We set the value of pLayout->marginValid to 0 before drawing
         * anything above, so if it is non-zero now then
         * pLayout->marginValue is the size of the collapsing bottom-margin
         * of the last thing drawn in this block context. The bottom margin
         * for this block (to be collapsed into the top margin of the next)
         * becomes the maximum of 'margin-bottom' and pLayout->marginValue.
         *
         * Whether or not this is what the spec says is supposed to happen
         * is a difficult question to answer. It seems to be what Gecko
         * does.
         * 
         * Todo: Maybe we should only do this if this box has no bottom
         * border or padding?
         */
        if (!pLayout->marginValid) {
            pLayout->marginValue = 0;
        }
        pLayout->marginValue = 
                collapseMargins(margin.margin_bottom, pLayout->marginValue);
        pLayout->marginValid = 1;
    } else if (marginValid) {
        /* If there is a box above this one in the flow (i.e. the
	 * top-margin is valid), then set the saved collapsing margin value
	 * to the maximum of it's current value, the top margin and the
	 * bottom margin. Again, the observed behaviour of modern browsers.
	 * The CSS2 spec doesn't really specify this case.
         *
         * i.e. If we have:
         *
         *            <p style="margin:10cm">Some text
         *            <p style="margin-top:20cm ; margin-bottom:15cm>
         *            <p style="margin:10>Some more text
         *
         * and this function was responsible for laying out the middle
         * paragraph, then we set the effective value of the bottom margin
         * of the first paragraph to 20cm.
         */
        pLayout->marginValue = collapseMargins(top_margin,margin.margin_bottom);
        pLayout->marginValid = marginValid;
        HtmlDrawCleanup(&sBox.vc);
    }

    /* Restore the floating margins to the parent boxes coordinate system */
    HtmlFloatListNormalize(sBox.pFloat, x, y);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayout2 --
 * 
 *     Build the internal representation of the formatted document. 
 *     The internal representation is either the text of a Tcl proc
 *     for rendering the document to canvas, or a linked-list of 
 *     HtmlCanvasItem structs for rendering to the widget itself.
 *
 * Results:
 *
 * Side effects:
 *     Destroys the existing document layout, if one exists.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlLayoutForce(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlNode *pBody = 0;
    int rc;
    int i;
    int width = 600;               /* Default width if no -width option */
    BoxContext sBox;               /* The imaginary box <body> is inside */
    LayoutContext sLayout;
    Tcl_HashSearch s;
    Tcl_HashEntry *p;

    HtmlDrawDeleteControls(pTree, &pTree->canvas);
    HtmlDrawCleanup(&pTree->canvas);
    memset(&pTree->canvas, 0, sizeof(HtmlCanvas));

    memset(&sLayout, 0, sizeof(LayoutContext));

    /* Look for the -width and -win options */
    for (i=3; i<objc; i++) {
        const char *zArg = Tcl_GetString(objv[i]);
        if (i!=objc-1 && 0==strcmp(zArg, "-width")) {
            i++;
            rc = Tcl_GetIntFromObj(interp, objv[i], &width);
            if (rc!=TCL_OK) return rc;
        }
        else if (i!=objc-1 && 0==strcmp(zArg, "-win")) {
            const char *zWin;
            i++;
            zWin = Tcl_GetString(objv[i]);
            sLayout.tkwin = Tk_NameToWindow(interp,zWin,Tk_MainWindow(interp));
            if (!sLayout.tkwin) {
                return TCL_ERROR;
            }
        }
        else {
            return TCL_ERROR;
        }
    }

    if (!sLayout.tkwin) {
        sLayout.tkwin = pTree->win;
    }

    /* Set up the layout context object. */
    sLayout.pTree = pTree;
    sLayout.interp = interp;
    sLayout.eTarget = LAYOUT_CANVAS;
    Tcl_InitHashTable(&sLayout.widthCache, TCL_ONE_WORD_KEYS);

    memset(&sBox, 0, sizeof(BoxContext));
    sBox.parentWidth = width;
    sBox.pFloat = HtmlFloatListNew();

    /* Assume we already have a styled tree. Todo: This isn't always going
     * to be true, as this function is called as a Tcl proc and may be
     * called incorrectly by a script.
     */
    assert(pTree->pRoot);
    pBody = pTree->pRoot;

    /* Call blockLayout() to layout the top level box, generated by the
     * <body> tag 
     */
    sLayout.pTop = pBody;
    rc = blockLayout(&sLayout, &sBox, pBody, 0, 0);

    memcpy(&pTree->canvas, &sBox.vc, sizeof(HtmlCanvas));

    /* Clear the width cache and delete the float-list. */
    HtmlFloatListDelete(sBox.pFloat);
    for (
        p = Tcl_FirstHashEntry(&sLayout.widthCache, &s); 
        p; 
        p = Tcl_NextHashEntry(&s)) 
    {
        ckfree((char *)Tcl_GetHashValue(p));
    }
    Tcl_DeleteHashTable(&sLayout.widthCache);

    return rc;
}
