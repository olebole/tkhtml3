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
typedef struct FloatMargin FloatMargin;

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

    int marginValid;       /* True to include the top margin in next block */
    int marginValue;       /* Bottom margin of previous block box */
};

/*
 * A seperate BoxContext struct is used for each block box layed out.
 */
struct BoxContext {
    int parentWidth;           /* DOWN: Width of parent block box. */
    int height;                /* UP: Generated box height. */
    int width;                 /* UP: Generated box width. */
    FloatMargin *pFloats;      /* Margins. */
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
 *     inlineContextAddInlineCanvas():
 *         Add a rendered inline box to the context.
 *
 *     inlineContextAddSpace():
 *         Add a "space" box (i.e. one generated by white-space text) to
 *         the context. Width of the space is specified in pixels.
 *
 *     inlineContextGetLineBox():
 *         Retrieve the next rendered line-box from the inline context. The
 *         line-box is created based on the inline-boxes that have already
 *         been passed in using the inlineContextAddInlineCanvas() call.
 */
typedef struct InlineContext InlineContext;
typedef struct InlineBorder InlineBorder;
typedef struct InlineBox InlineBox;
struct InlineBorder {
  BorderProperties border;
  MarginProperties margin;
  BoxProperties box;
  int iStartBox;
  int iStartPixel;            /* Left most pixel of outer margin */
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
  InlineBorder *pBorderStart; /* List of borders that start with this box */
  int nBorderEnd;             /* Number of borders that end here */
  int nLeftPixels;            /* Total left width of borders that start here */
  int nRightPixels;           /* Total right width of borders that start here */
};
struct InlineContext {
  int textAlign;          /* One of TEXTALIGN_LEFT, TEXTALIGN_RIGHT etc. */

  int nInline;            /* Number of inline boxes in aInline */
  int nInlineAlloc;       /* Number of slots allocated in aInline */
  InlineBox *aInline;     /* Array of inline boxes. */

  InlineBorder *pBorders;    /* Linked list of active inline-borders. */
  InlineBorder *pBoxBorders; /* Borders list for next box to be added */
};
static void inlineContextSetTextAlign(InlineContext *, int);
static HtmlCanvas *inlineContextAddInlineCanvas(InlineContext *);
static void inlineContextAddSpace(InlineContext *, int);
static int inlineContextGetLineBox(InlineContext *,int,int,int,HtmlCanvas *);

static InlineBorder *inlineContextGetBorder(LayoutContext *, HtmlNode *);
static int inlineContextPushBorder(InlineContext *, InlineBorder *);
static int inlineContextPopBorder(InlineContext *, InlineBorder *);

/*
 * Floating Margins Notes
 * ----------------------
 *
 *     When a floating box is added to the layout, it creates a floating
 *     margin which other content flows around. A set of floating margins
 *     for a flow context is stored as a FloatMargin struct.
 * 
 * Implementation Notes
 * --------------------
 */
struct FM {
   int x;
   int y;
   struct FM *pNext;
};
struct FloatMargin {
    struct FM *pLeft;
    struct FM *pRight;
};
static void floatListAdd(FloatMargin*, int, int, int);
static void floatListClear(FloatMargin*, int);
static void floatListMargins(FloatMargin*, int*, int*);
static int  floatListPlace(FloatMargin*, int, int, int);
static int  floatListIsEmpty(FloatMargin *);

/*
 * Potential values for the 'display' property. Not supported yet are
 * 'run-in' and 'compact'. And some table types...
 */
#define DISPLAY_BLOCK    1
#define DISPLAY_INLINE   2
#define DISPLAY_TABLE    3
#define DISPLAY_LISTITEM 4
#define DISPLAY_NONE     5

#define FLOAT_LEFT       1
#define FLOAT_RIGHT      2
#define FLOAT_NONE       3

#define LISTSTYLETYPE_SQUARE 1 
#define LISTSTYLETYPE_DISC   2 
#define LISTSTYLETYPE_CIRCLE 3

#define VALIGN_MIDDLE 1
#define VALIGN_TOP 2
#define VALIGN_BOTTOM 3
#define VALIGN_BASELINE 4

#define TEXTALIGN_LEFT 1
#define TEXTALIGN_RIGHT 2
#define TEXTALIGN_CENTER 3
#define TEXTALIGN_JUSTIFY 4

#define TEXTDECORATION_NONE 1
#define TEXTDECORATION_UNDERLINE 2
#define TEXTDECORATION_OVERLINE 3
#define TEXTDECORATION_LINETHROUGH 4

#define WHITESPACE_PRE 1
#define WHITESPACE_NOWRAP 2
#define WHITESPACE_NORMAL 3

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

S int attributeToPixels(HtmlNode*, const char*, int, int, int*);

S int  nodeGetEmPixels(LayoutContext*, HtmlNode*);
S void nodeGetDisplay(LayoutContext*, HtmlNode*, DisplayProperties*);
S int  nodeGetListStyleType(LayoutContext *, HtmlNode *);
S XColor *nodeGetColour(LayoutContext *, HtmlNode*);
S int nodeGetBorderSpacing(LayoutContext *, HtmlNode*);
S int nodeGetVAlign(LayoutContext *, HtmlNode*, int);
S void nodeGetBoxProperties(LayoutContext *, HtmlNode *, BoxProperties *);
S void nodeGetBorderProperties(LayoutContext *, HtmlNode *, BorderProperties *);
S int nodeGetWidth(LayoutContext *, HtmlNode *, int, int, int*);
S int nodeGetHeight(LayoutContext *, HtmlNode *, int, int);
S int nodeGetTextAlign(LayoutContext *, HtmlNode *);
S int nodeGetTextDecoration(LayoutContext *, HtmlNode *);
S CONST char *nodeGetTkhtmlReplace(LayoutContext *, HtmlNode *);

S void nodeComment(HtmlCanvas *, HtmlNode *);
S void endNodeComment(HtmlCanvas *, HtmlNode *);

S void borderLayout(LayoutContext*, HtmlNode*, BoxContext*, int, int, int, int);
S int floatLayout(LayoutContext*, BoxContext*, HtmlNode*, int*);
S int markerLayout(LayoutContext*, BoxContext*, HtmlNode*);
S int inlineLayout(LayoutContext*, BoxContext*, HtmlNode*);
S int tableLayout(LayoutContext*, BoxContext*, HtmlNode*);
S int blockLayout(LayoutContext*, BoxContext*, HtmlNode*);
S int blockLayout2(LayoutContext*, BoxContext*, HtmlNode*, int);
S void layoutReplacement(LayoutContext*, BoxContext*, HtmlNode*, CONST char*);

S int tableIterate(
    HtmlNode*, 
    int(*)(HtmlNode *, int, int, int, int, void *),
    int(*)(HtmlNode *, int, void *),
    void*
);
S int blockMinMaxWidth(LayoutContext *, HtmlNode *, int *, int *);

#undef S

static void floatListAdd(pList, side, x, y)
    FloatMargin *pList;
    int side;                /* x-coord for left margin. */
    int x;                   /* x-coord for right margin. */
    int y;                   /* Margin expires at y-coord. */
{
    struct FM *pNew = (struct FM *)ckalloc(sizeof(struct FM));
    assert(side==FLOAT_LEFT || side==FLOAT_RIGHT);
    pNew->x = x;
    pNew->y = y;
    if (side==FLOAT_LEFT) {
        pNew->pNext = pList->pLeft;
        pList->pLeft = pNew;
    } else {
        pNew->pNext = pList->pRight;
        pList->pRight = pNew;
    }
}

static void floatListClear(pList, y)
    FloatMargin *pList;
    int y;
{
    struct FM *pIter = pList->pLeft;
    while (pIter && pIter->y<y) {
        struct FM *pIter2 = pIter->pNext;
        pIter = pIter2;
    }
    pList->pLeft = pIter;

    pIter = pList->pRight;
    while (pIter && pIter->y<y) {
        struct FM *pIter2 = pIter->pNext;
        pIter = pIter2;
    }
    pList->pRight = pIter;
}

static void floatListMargins(pList, pLeft, pRight)
    FloatMargin *pList;
    int *pLeft;
    int *pRight;
{
    if (pList->pLeft) *pLeft = pList->pLeft->x;
    if (pList->pRight) *pRight = pList->pRight->x;
}

static void floatListNormalize(pList, x, y)
    FloatMargin *pList;
    int x;
    int y;
{
    struct FM *pIter = pList->pLeft;
    for (pIter=pList->pLeft; pIter; pIter=pIter->pNext) {
        pIter->x += x;
        pIter->y += y;
    }

    for (pIter=pList->pRight; pIter; pIter=pIter->pNext) {
        pIter->x += x;
        pIter->y += y;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * floatListPlace --
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
floatListPlace(pList, parentWidth, width, def_val)
    FloatMargin *pList;           /* Floating box margin list */
    int parentWidth;              /* Width of containing box */
    int width;                    /* Width of block being placed */
    int def_val;                  /* Current y coordinate */
{
    struct FM *pLeft = pList->pLeft;
    struct FM *pRight = pList->pRight;
    while (1) {
        int lx = pLeft?pLeft->x:0;
        int rx = pRight?pRight->x:parentWidth;

        if ( (rx-lx)>=width || (!pLeft && !pRight) ) return def_val;

        if (pLeft && (!pRight || pLeft->y<pRight->y)) {
            def_val = pLeft->y+1;
            pLeft = pLeft->pNext;
        } else {
            def_val = pRight->y+1;
            pRight = pRight->pNext;
        }
    }
}

static int floatListIsEmpty(pList)
    FloatMargin *pList;
{
    return (pList->pLeft==0 && pList->pRight==0);
}

/*
 *---------------------------------------------------------------------------
 *
 * attributeToPixels --
 *
 *     Retrieve the value of HTML attribute zAttr and return it as an
 *     integer number of pixels. If zAttr is not defined, return a copy of
 *     iDefault instead.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int attributeToPixels(pNode, zAttr, pwidth, iDefault, pAbsolute)
    HtmlNode *pNode; 
    const char *zAttr; 
    int pwidth;         /* Width of parent box, for % values */
    int iDefault;
    int *pAbsolute;
{
    int val = iDefault;
    if (pNode) {
        const char *zVal = HtmlNodeAttr(pNode, zAttr);
        if (zVal) {
            val = atoi(zVal);
            if (zVal[0] && zVal[strlen(zVal)-1]=='%') {
                val = (pwidth*val)/100;
                if (pAbsolute) *pAbsolute = 0;
            } else {
                if (pAbsolute) *pAbsolute = 1;
            }
        }
    }
    return val;
}

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
        while (*zOptions) {
            if( 0==strcmp(pProp->v.zVal, *zOptions) ) return *eOptions;
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
    if (pProp && pProp->eType==CSS_TYPE_STRING) {
        return pProp->v.zVal;
    }
    return zDefault;
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
            case CSS_TYPE_PX:
                return pProp->v.iVal;
            case CSS_TYPE_EM: {
                return pProp->v.rVal * nodeGetEmPixels(pLayout, pNode);
            }
            case CSS_TYPE_PERCENT: {
                return (pProp->v.iVal * parentwidth) / 100;
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
static XColor *propertyToColor(pLayout, pProp)
    LayoutContext *pLayout;
    CssProperty *pProp;
{
    XColor *color = 0;
    CONST char *zColor;

    zColor = propertyToString(pProp, 0);
    if (zColor) {
        char zBuf[14];
        int n = strlen(zColor);
        if (n == 6 || n == 3 || n == 9 || n == 12) {
            int i;
            for (i = 0; i < n; i++) {
                if (!isxdigit(zColor[i]))
                    break;
            }
            if (i == n) {
                sprintf(zBuf, "#%s", zColor);
                zColor = zBuf;
            }
        }

        color = Tk_GetColor(pLayout->interp, pLayout->tkwin, zColor);
    }

    return color;
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
static void nodeGetDisplay(pLayout, pNode, pDisplayProperties)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    DisplayProperties *pDisplayProperties;
{
    char const *zDisplay[] = {
        "inline",         "block",           "none", 
        "list-item",      "table", 0
    };
    int eDisplay[] = {
        DISPLAY_INLINE,   DISPLAY_BLOCK,     DISPLAY_NONE, 
        DISPLAY_LISTITEM, DISPLAY_TABLE
    };

    char const *zFloat[] = {
        "left",         "right",           "none", 
        0
    };
    int eFloat[] = {
        FLOAT_LEFT,   FLOAT_RIGHT,     FLOAT_NONE
    };

    CssProperty prop;
    int f;
    int d;

    HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_DISPLAY, &prop);
    d = propertyToConstant(&prop, zDisplay, eDisplay, DISPLAY_INLINE);

    HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_FLOAT, &prop);
    f = propertyToConstant(&prop, zFloat, eFloat, FLOAT_NONE);

    /* Force all floating boxes to have display type 'block' or 'table' */
    if (f!=FLOAT_NONE && d!=DISPLAY_TABLE) {
        d = DISPLAY_BLOCK;
    }

    pDisplayProperties->eDisplay = d;
    pDisplayProperties->eFloat = f;
}

static int nodeGetListStyleType(pLayout, pNode)
    LayoutContext *pLayout; 
    HtmlNode *pNode;
{
    char const *zStyleOptions[] = {
        "square",             "disc",             "circle", 
        0
    };
    int eStyleOptions[] = {
        LISTSTYLETYPE_SQUARE, LISTSTYLETYPE_DISC, LISTSTYLETYPE_CIRCLE
    };
    CssProperty prop;
    Tcl_Interp *interp = pLayout->pTree->interp;

    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_LIST_STYLE_TYPE, &prop);
    return propertyToConstant(
        &prop, zStyleOptions, eStyleOptions, LISTSTYLETYPE_DISC);
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
static int nodeGetFontSize(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    int val = 0;
    CssProperty prop;

    /* Default of 'font-size' should be "medium". */
    if (!pNode) return 8;

    HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_FONT_SIZE, &prop);

    switch (prop.eType) {
        case CSS_TYPE_EM:
            val = nodeGetFontSize(pLayout, pNode->pParent);
            val = val * prop.v.rVal;
            break;
        case CSS_TYPE_PX:
            val = prop.v.iVal * 0.8;
            break;
        case CSS_TYPE_PT:
            val = prop.v.iVal;
            break;
        case CSS_TYPE_STRING: {
            CONST char *zOptions[] = {"xx-small", "x-small", "small", 
                               "medium", "large", "x-large", "xx-large", 0};
            int eOptions[] =  {0, 1, 2, 3, 4, 5, 6};
            double rOptions[] = {0.6944, 0.8333, 1.0, 1.2, 1.44, 1.728, 2.074};
            int i = propertyToConstant(&prop, zOptions, eOptions, -1);
            if (i>=0) {
                HtmlNode *pParent;
                double r = rOptions[i];

                pParent = HtmlNodeParent(pNode);
                while (pParent && HtmlNodeTagType(pParent)!=Html_BODY) {
                    pParent = HtmlNodeParent(pParent);
                }

                val = nodeGetFontSize(pLayout, pParent) * r;
            }
        }
    }

    if (val==0) {
        val = nodeGetFontSize(pLayout, pNode->pParent);
    }
    return val;
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
    CssProperty *pFontStyle;
    CssProperty *pFontWeight;
    int sz = nodeGetFontSize(pLayout, pNode);
    int isItalic;
    int isBold;
    Tk_Font font = 0;
    const char *zFamily;
    int nFamily;
    const char *zFamilyEnd;
    CssProperty *pFontFamily;
    CssProperty fontStyle;            /* Property 'font-style' */
    CssProperty fontWeight;           /* Property 'font-weight' */
    CssProperty fontFamily;           /* Property 'font-family' */
    Tcl_Interp *interp = pLayout->pTree->interp;
    Tcl_HashTable *pFontCache = &pLayout->pTree->aFontCache;

    CONST char *zStyleOptions [] = {"italic", "oblique", 0};
    int eStyleOptions [] = {1, 1};

    CONST char *zWeightOptions [] = {"bold", "bolder", 0};
    int eWeightOptions [] = {1, 1};

    /* Todo: Other options for 'text-decoration' are "overline",
     * "line-through" and "blink".
     */
    CONST char *zDecOptions [] = {"underline", 0};
    int eDecOptions [] = {1};

    /* If the 'font-style' attribute is set to either "italic" or
     * "oblique", add the option "-slant italic" to the string version
     * of the Tk font.
     */
    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_FONT_STYLE, &fontStyle);
    isItalic = propertyToConstant(&fontStyle, zStyleOptions, eStyleOptions, 0);

    /* If the 'font-weight' attribute is set to either "bold" or
     * "bolder", add the option "-weight bold" to the string version
     * of the Tk font.
     *
     * Todo: Handle numeric font-weight values. Tk restricts the weight
     * of the font to "bold" or "normal", but we should try to do something
     * sensible with other options.
     */
    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_FONT_WEIGHT, &fontWeight);
    isBold = propertyToConstant(&fontWeight, zWeightOptions, eWeightOptions, 0);

    /* If 'font-family' is set, then use the value as the -family option
     * in the Tk font request. Otherwise use Helvetica, which is always
     * available.
     */
    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_FONT_FAMILY, &fontFamily);
    zFamily = propertyToString(&fontFamily, "Helvetica");

    zFamilyEnd = strchr(zFamily, (int)',');
    if (!zFamilyEnd) {
        nFamily = strlen(zFamily);
    } else {
        nFamily = zFamilyEnd - zFamily;
    }

    while (font==0) {
        char zBuf[100];
        Tcl_HashEntry *pEntry;
        int newentry;
        Tk_Window tkwin = pLayout->tkwin;

        sprintf(zBuf, "-family %.*s -size %d%s%s", nFamily, zFamily, sz, 
            isItalic?" -slant italic":"",
            isBold?" -weight bold":""
        );

        pEntry = Tcl_CreateHashEntry(pFontCache, zBuf, &newentry);
        if (newentry) {
            font = Tk_GetFont(pLayout->interp, tkwin, zBuf); 
            if (!font) {
                if (isItalic) {
                    isItalic = 0;
                } else if (isBold) {
                    isBold = 0;
                } else {
                    zFamily = "Helvetica";
                }
            } else {
                Tcl_SetHashValue(pEntry, font);
            }
        } else {
            font = Tcl_GetHashValue(pEntry);
        }
    }
 
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
    CssProperty sColor;

    HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_COLOR, &sColor);
    color = propertyToColor(pLayout, &sColor);

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
    CssProperty prop;
    int border_spacing;
    Tcl_Interp *interp = pLayout->pTree->interp;

    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_SPACING, &prop);
    border_spacing = propertyToPixels(pLayout, pNode, &prop, 0, 0);

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
 *     VALIGN_MIDDLE, VALIGN_TOP, VALIGN_BOTTOM or VALIGN_BASELINE.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int nodeGetVAlign(pLayout, pNode, defval)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    int defval;
{
    CssProperty valign;
    int ret;
    Tcl_Interp *interp = pLayout->interp;

    const char *zOptions[] = {"top", "middle", "bottom", "baseline", 0};
    int eOptions[] = {VALIGN_TOP,VALIGN_MIDDLE,VALIGN_BOTTOM,VALIGN_BASELINE};

    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_VERTICAL_ALIGN, &valign);
    ret = propertyToConstant(&valign, zOptions, eOptions, defval);

    return ret;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetEmPixels --
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
    Tk_FontMetrics fontMetrics;
    Tk_Font font = nodeGetFont(pLayout, pNode);
    Tk_GetFontMetrics(font, &fontMetrics);
    ret = fontMetrics.ascent;
    return ret;
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
nodeGetBoxProperties(pLayout, pNode, pBoxProperties)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    BoxProperties *pBoxProperties;
{
    CssProperty b;
    CssProperty p;
    Tcl_Interp *interp = pLayout->interp;

    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_PADDING_TOP, &p);
    pBoxProperties->padding_top = propertyToPixels(pLayout, pNode, &p, 0, 0);
    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_PADDING_LEFT, &p);
    pBoxProperties->padding_left = propertyToPixels(pLayout, pNode, &p, 0, 0);
    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_PADDING_RIGHT, &p);
    pBoxProperties->padding_right = propertyToPixels(pLayout, pNode, &p, 0, 0);
    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_PADDING_BOTTOM, &p);
    pBoxProperties->padding_bottom = propertyToPixels(pLayout, pNode, &p, 0, 0);

    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_TOP_STYLE, &b);
    if (b.eType==CSS_TYPE_STRING && 0==strcmp(b.v.zVal, "none")) {
        pBoxProperties->border_top = 0;
    }else{
        HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_TOP_WIDTH, &b);
        pBoxProperties->border_top = propertyToPixels(pLayout, pNode, &b, 0, 0);
    }

    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_BOTTOM_STYLE, &b);
    if (b.eType==CSS_TYPE_STRING && 0==strcmp(b.v.zVal, "none")) {
        pBoxProperties->border_bottom = 0;
    }else{
        HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_BOTTOM_WIDTH, &b);
        pBoxProperties->border_bottom = propertyToPixels(pLayout,pNode,&b,0,0);
    }

    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_LEFT_STYLE, &b);
    if (b.eType==CSS_TYPE_STRING && 0==strcmp(b.v.zVal, "none")) {
        pBoxProperties->border_left = 0;
    }else{
        HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_LEFT_WIDTH, &b);
        pBoxProperties->border_left = propertyToPixels(pLayout,pNode,&b,0,0);
    }

    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_RIGHT_STYLE, &b);
    if (b.eType==CSS_TYPE_STRING && 0==strcmp(b.v.zVal, "none")) {
        pBoxProperties->border_right = 0;
    }else{
        HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BORDER_RIGHT_WIDTH, &b);
        pBoxProperties->border_right = propertyToPixels(pLayout,pNode,&b,0,0);
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
    CssProperty sProp;
    CONST char *zColour;

    HtmlNodeGetProperty(pLayout->interp, pNode, prop, &sProp);
    color = propertyToColor(pLayout, &sProp);
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
    CssProperty bgcolor;
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
    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_BACKGROUND_COLOR, &bgcolor);
    zBg = propertyToString(&bgcolor, 0);
    if (zBg && strcmp(zBg, "transparent")) {
        pBorderProperties->color_bg = propertyToColor(pLayout, &bgcolor);
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
    CssProperty height;
    HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_HEIGHT, &height);
    val = propertyToPixels(pLayout, pNode, &height, pwidth, def);
    return val;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetWidth --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int nodeGetWidth(pLayout, pNode, pwidth, def, pIsFixed)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    int pwidth; 
    int def;
    int *pIsFixed;
{
    int val;
    CssProperty width;

    HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_WIDTH, &width);
    val = propertyToPixels(pLayout, pNode, &width, pwidth, def);

    if (width.eType==CSS_TYPE_PX) {
        if (pIsFixed) *pIsFixed = 1;
    } else {
        if (pIsFixed) *pIsFixed = 0;
    }
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
    CssProperty prop;
    HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_TEXT_ALIGN, &prop);
    return propertyToConstant(&prop, zOptions, eOptions, TEXTALIGN_LEFT);
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
    char const *zOptions[] = {
        "underline", "overline", "line-through", 0
    };
    int eOptions[] = {
        TEXTDECORATION_UNDERLINE, TEXTDECORATION_OVERLINE,
        TEXTDECORATION_LINETHROUGH
    };
    CssProperty prop;
    HtmlNodeGetProperty(pLayout->interp,pNode, CSS_PROPERTY_TEXT_DECORATION, &prop);
    return propertyToConstant(&prop, zOptions, eOptions, TEXTDECORATION_NONE);
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
    CssProperty r;
    Tcl_Interp *interp = pLayout->pTree->interp;
    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY__TKHTML_REPLACE, &r);
    if (r.eType==CSS_TYPE_STRING) {
        return r.v.zVal;
    }
    return 0;
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
static void nodeGetMargins(pLayout, pNode, pMargins)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    MarginProperties *pMargins;
{
    CssProperty m;
    Tcl_Interp *interp = pLayout->pTree->interp;

    /* Todo: It is also legal to specify an integer between 1 and 4 for
     * margin width. propertyToPixels() can't deal with this because it
     * doesn't know what it is converting is a margin, so it will have to
     * be done here.
     */
    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_MARGIN_TOP, &m);
    pMargins->margin_top = propertyToPixels(pLayout, pNode, &m, 0, 0);

    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_MARGIN_RIGHT, &m);
    pMargins->margin_right = propertyToPixels(pLayout, pNode, &m, 0, 0);

    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_MARGIN_BOTTOM, &m);
    pMargins->margin_bottom = propertyToPixels(pLayout, pNode, &m, 0, 0);

    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_MARGIN_LEFT, &m);
    pMargins->margin_left = propertyToPixels(pLayout, pNode, &m, 0, 0);
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
    char const *zOptions[] = {
        "pre", "nowrap", "normal", 0
    };
    int eOptions[] = {
        WHITESPACE_PRE, WHITESPACE_NOWRAP, WHITESPACE_NORMAL
    };
    CssProperty prop;
    HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_WHITE_SPACE, &prop);
    return propertyToConstant(&prop, zOptions, eOptions, WHITESPACE_NORMAL);
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
nodeComment(pCanvas, pNode)
    HtmlCanvas *pCanvas;
    HtmlNode *pNode;
{
#ifdef HTML_DEBUG
    char *zComment;
    zComment = HtmlNodeToString(pNode);
    HtmlDrawComment(pCanvas, zComment);
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
static void endNodeComment(pCanvas, pNode)
    HtmlCanvas *pCanvas;
    HtmlNode *pNode;
{
#ifdef HTML_DEBUG
    char zComment[64];
    sprintf(zComment, "</%s>", HtmlMarkupName(HtmlNodeTagType(pNode)));
    HtmlDrawComment(pCanvas, zComment);
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
    BoxContext sBox;                 /* Generated box. */
    FloatMargin sFloat;              /* Internal floating margin list. */
    MarginProperties margins;        /* Generated box margins. */
    int width;                       /* Width of generated box. */
    int marginwidth;                 /* Width of box including margins */
    int leftFloat = 0;                   /* left floating margin */
    int rightFloat = pBox->parentWidth;  /* right floating margin */
    DisplayProperties display;       /* Display proprerties */
    int x;
    int marginValid = pLayout->marginValid;
    int marginValue = pLayout->marginValue;

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
    memset(&sFloat, 0, sizeof(FloatMargin));
    sBox.pFloats = &sFloat;

    /* Get the display properties. The caller should have already made
     * sure that the node generates a floating block box. But we need
     * to do this too in order to figure out if this is a FLOAT_LEFT or
     * FLOAT_RIGHT box.
     */
    nodeGetDisplay(pLayout, pNode, &display);
    assert(display.eFloat!=FLOAT_NONE);
    assert(display.eDisplay==DISPLAY_BLOCK || display.eDisplay==DISPLAY_TABLE);

    /* According to CSS, a floating box must have an explicit width or
     * replaced content (in which case nodeGetWidth() returns the width of
     * the replaced content). But if it doesn't, we just assign the maximum
     * width of the floating box, or 1/3 the width of the parent box,
     * whichever is smaller.
     */
    width = nodeGetWidth(pLayout, pNode, pBox->parentWidth, -1, 0);
    if (width<0) {
        int min, max;
        blockMinMaxWidth(pLayout, pNode, &min, &max);
        width = min;
    }

    /* Get the margins for the floating box. Floating box margins never
     * collapse with any other margins.
     */
    nodeGetMargins(pLayout, pNode, &margins);
    marginwidth = width + margins.margin_left + margins.margin_right;

    /* Figure out the y-coordinate to draw the floating box at. This is
     * usually the current y coordinate. However if other floats mean that
     * the parent box is not wide enough for this float, we may shift this
     * float downward until there is room.
     */
    y = floatListPlace(pBox->pFloats, pBox->parentWidth, marginwidth, y);
    floatListClear(pBox->pFloats, y);
    *pY = y;
    floatListMargins(pBox->pFloats, &leftFloat, &rightFloat);

    /* Get the exact x coordinate to draw the box at. If it won't fit, 
     * even after shifting down past other floats in code above, then
     * align with the left margin, even if the box is right-floated.
     */ 
    if (display.eFloat==FLOAT_LEFT) {
        x = leftFloat;
    } else {
        x = rightFloat - width;
        if (x<leftFloat) {
            x = leftFloat;
        }
    }

    /* Draw the floating box. Set marginValid to 1 and marginValue to 0 to
     * ensure that the top margin of the floating box is allocated. Margins
     * of floating boxes never collapse.
     */
    pLayout->marginValid = 1;
    pLayout->marginValue = 0;
    sBox.parentWidth = width;
    blockLayout2(pLayout, &sBox, pNode, 0);
    HtmlDrawCanvas(&pBox->vc, &sBox.vc, x, y);

    /* If the right-edge of this floating box exceeds the current actual
     * width of the box it is drawn in, set the actual width to the 
     * right edge.
     */
    pBox->width = MAX(marginwidth+(pBox->parentWidth-rightFloat), pBox->width);
    pBox->height = MAX(y+sBox.height, pBox->height);

    /* Fix the float list in the parent block so that nothing overlaps
     * this floating box.
     */
    if (display.eFloat==FLOAT_LEFT) {
        int m = x + width;
        floatListAdd(pBox->pFloats, FLOAT_LEFT, m, y + sBox.height);
    } else {
        int m = x;
        floatListAdd(pBox->pFloats, FLOAT_RIGHT, m, y + sBox.height);
    }

    pLayout->marginValid = marginValid;
    pLayout->marginValue = marginValue;
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
    XColor *color;
    int offset;

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
    }
    font = nodeGetFont(pLayout, pNode);
    color = nodeGetColour(pLayout, pNode);
    pMarker = Tcl_NewStringObj(zMarker, -1);
    width = Tk_TextWidth(font, zMarker, strlen(zMarker));

    /* Todo: The code below assumes a value of 'outside' for property
     * 'list-marker-position'. Should handle 'inside' as well.
     */

    /* It's not clear to me exactly where the list marker should be
     * drawn when the 'list-style-position' property is 'outside'.
     * The algorithm used is to draw it the width of 3 'x' characters
     * in the current font to the left of the content box.
     */
    offset = Tk_TextWidth(font, "xxx", 3);
    HtmlDrawText(&pBox->vc, pMarker, -1*offset, 0, width, font, color);
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
        pBox->nRightPixels += pBorder->box.padding_left;
        pBox->nRightPixels += pBorder->box.border_left;
        pBox->nRightPixels += pBorder->margin.margin_left;
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
static InlineBorder *inlineContextGetBorder(pLayout, pNode)
    LayoutContext *pLayout; 
    HtmlNode *pNode;
{
    InlineBorder border;
    InlineBorder *pBorder = 0;

    nodeGetBoxProperties(pLayout, pNode, &border.box);
    nodeGetMargins(pLayout, pNode, &border.margin);
    nodeGetBorderProperties(pLayout, pNode, &border.border);
    border.pNext = 0;

    if (border.box.padding_left   || border.box.padding_right     ||
        border.box.padding_top    || border.box.padding_bottom    ||
        border.box.border_left    || border.box.border_right      ||
        border.box.border_top     || border.box.border_bottom     ||
        border.margin.margin_left || border.margin.margin_right   ||
        border.margin.margin_top  || border.margin.margin_bottom  ||
        border.border.color_bg  
    ) {
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
inlineContextAddInlineCanvas(p)
    InlineContext *p;
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
    return &pBox->canvas;
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
        pBox->nSpace = MAX(nPixels, pBox->nSpace);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextDrawBorder --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void inlineContextDrawBorder(pCanvas, pBorder, x1, y1, x2, y2, drb)
    HtmlCanvas *pCanvas;
    InlineBorder *pBorder;
    int x1, y1;
    int x2, y2;
    int drb;                  /* Draw Right Border */
{
    XColor *c = pBorder->border.color_bg;

    int tw, rw, bw, lw;
    XColor *tc, *rc, *bc, *lc;

    tw = pBorder->box.border_top;
    rw = pBorder->box.border_right;
    bw = pBorder->box.border_bottom;
    lw = pBorder->box.border_left;

    tc = pBorder->border.color_top;
    rc = pBorder->border.color_right;
    bc = pBorder->border.color_bottom;
    lc = pBorder->border.color_left;

    x1 += pBorder->margin.margin_left;
    x2 -= pBorder->margin.margin_right;
    y1 += pBorder->margin.margin_top;
    y2 -= pBorder->margin.margin_bottom;
    if (tw>0) {
        HtmlDrawQuad(pCanvas, x1, y1, x1+lw, y1+tw, x2-rw, y1+tw, x2, y1, tc);
    }
    if (rw > 0 && drb) {
        HtmlDrawQuad(pCanvas, x2, y1, x2-rw, y1+tw, x2-rw, y2-bw, x2, y2, rc);
    }
    if (bw>0) {
        HtmlDrawQuad(pCanvas, x2, y2, x2-rw, y2-bw, x1+lw, y2-bw, x1, y2, bc);
    }
    if (lw > 0 && pBorder->iStartBox >= 0) {
        HtmlDrawQuad(pCanvas, x1, y2, x1+lw, y2-bw, x1+lw, y1+tw, x1, y1, lc);
    }

    if (c) {
        x1 += pBorder->box.border_left;
        x2 -= pBorder->box.border_right;
        y1 += pBorder->box.border_top;
        y2 -= pBorder->box.border_bottom;

        HtmlDrawQuad(pCanvas, x1, y1, x2, y1, x2, y2, x1, y2, c);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineContextGetLineBox --
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
inlineContextGetLineBox(p, width, forceline, forcebox, pCanvas)
    InlineContext *p;
    int width;                /* Maximum width available for line-box */
    int forceline;            /* Draw line even if line is not full */
    int forcebox;             /* Draw at least one inline box */
    HtmlCanvas *pCanvas;      /* Canvas to render line box to */
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

    memset(&content, 0, sizeof(HtmlCanvas));
    memset(&borders, 0, sizeof(HtmlCanvas));

    /* Count how many of the inline boxes fit within the requested line-box
     * width. Store this in nBox. Also remember the width of the line-box
     * assuming normal word-spacing. We'll need this to handle the
     * 'text-align' attribute later on.
     */
    for(i = 0; i < p->nInline; i++) {
        int j;
        InlineBorder *pBorder;
        InlineBox *pBox = &p->aInline[i];
        int boxwidth = (pBox->canvas.right - pBox->canvas.left); 
        boxwidth += pBox->nRightPixels + pBox->nLeftPixels;
        if(i > 0) {
            boxwidth += p->aInline[i-1].nSpace;
        }
        if(lineboxwidth+boxwidth > width) {
            break;
        }
        lineboxwidth += boxwidth;
    }
    nBox = i;

    /* If the forceline flag is not true, and the line-box is not full,
     * return 0 and do not draw anything.
     *
     * If the forcebox flag is true and no boxes fit within the requested
     * width, then draw one box anyway.
     */
    if(forcebox && !nBox) {
        nBox = 1;
    }
    if((p->nInline == 0) || (!forceline && (nBox == p->nInline)) || nBox == 0) {
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
    for(i = 0; i < nBox; i++) {
        int extra_pixels = 0;   /* Number of extra pixels for justification */
        InlineBox *pBox = &p->aInline[i];
        int boxwidth = (pBox->canvas.right - pBox->canvas.left);
        int x1, y1;
        int x2, y2;
        int nBorderEnd = 0;

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

        x1 = x + extra_pixels + pBox->nLeftPixels;
        HtmlDrawCanvas(&content, &pBox->canvas, x1, 0);
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
            HtmlDrawCanvas(&tmpcanvas, &borders, 0, 0);
            memset(&borders, 0, sizeof(HtmlCanvas));
            inlineContextDrawBorder(&borders, pBorder, x1, y1, x2, y2, rb);
            HtmlDrawCanvas(&borders, &tmpcanvas, 0, 0);

        }

        for(j = 0; j < pBox->nBorderEnd; j++) {
            pBorder = p->pBorders;
            assert(pBorder);
            p->pBorders = pBorder->pNext;
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

    /* Draw the borders and content canvas into the target canvas. Draw the
     * borders canvas first so that it is under the content.
     */
    HtmlDrawCanvas(pCanvas, &borders, 0, 0);
    HtmlDrawCanvas(pCanvas, &content, 0, 0);

    p->nInline -= nBox;
    memmove(p->aInline, &p->aInline[nBox], p->nInline * sizeof(InlineBox));

    return 1;
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
    XColor *color;
    int sw;                    /* Space-Width in current font. */

    assert(pNode && HtmlNodeIsText(pNode));

    font = nodeGetFont(pLayout, pNode);
    color = nodeGetColour(pLayout, pNode);
    sw = Tk_TextWidth(font, " ", 1);

    for(pToken=pNode->pToken; pToken; pToken=pToken->pNext) {
        switch(pToken->type) {
            case Html_Text: {
                Tcl_Obj *pText;
                HtmlCanvas *pCanvas; 
                int tw;

                pCanvas = inlineContextAddInlineCanvas(pContext);
                pText = Tcl_NewStringObj(pToken->x.zText, pToken->count);
                tw = Tk_TextWidth(font, pToken->x.zText, pToken->count);

                HtmlDrawText(pCanvas, pText, 0, 0, tw, font, color);
                break;
            }
            case Html_Space:
                inlineContextAddSpace(pContext, sw);
                break;
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
    int forceflag;
    int *pY;
{
    int have;
    do {
        HtmlCanvas linecanvas;
        int w;
        int y = *pY;               /* Y coord for line-box baseline. */
        int leftFloat = 0;
        int rightFloat = pBox->parentWidth;
        floatListMargins(pBox->pFloats, &leftFloat, &rightFloat);

        memset(&linecanvas, 0, sizeof(HtmlCanvas));
        w = rightFloat - leftFloat;
        have = inlineContextGetLineBox(pContext, w, forceflag, 1, &linecanvas);
        if (!have) {
            return 0;
        }

        if (have) {
            HtmlDrawCanvas(&pBox->vc, &linecanvas, leftFloat, y-linecanvas.top);
            y += (linecanvas.bottom - linecanvas.top);
            pBox->width = MAX(pBox->width, linecanvas.right + leftFloat);
            pBox->height = MAX(pBox->height, y);
        }

	floatListClear(pBox->pFloats, y);
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

        rc = inlineLayoutDrawLines(pLayout, pBox, pContext, 1, &y);

        memset(&sBox, 0, sizeof(BoxContext));
        sBox.parentWidth = pBox->parentWidth;
        sBox.pFloats = pBox->pFloats;

        floatListNormalize(sBox.pFloats, 0, -1*y);
        blockLayout2(pLayout, &sBox, pNode, 0);
        if (!HtmlDrawIsEmpty(&sBox.vc)) {
            HtmlDrawCanvas(&pBox->vc, &sBox.vc, 0, y);
        }
        floatListNormalize(sBox.pFloats, 0, y);

        pBox->pFloats = sBox.pFloats;
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

    /* If we have a <br> tag, then add a line-break. This is a hack to
     * support HTML without actually supporting the CSS :before
     * pseudo-element. A more elegant way to handle BR is with a stylesheet
     * rule like:
     *
     *      br:before {content:"\A"} 
     *
     */
    else if(HtmlNodeTagType(pNode) == Html_BR) {
        rc = inlineLayoutDrawLines(pLayout, pBox, pContext, 1, pY);
    }

    /* If none of the above conditions is true, then we have either a 
     * replaced inline node, or an inline node that does not generate
     * any content itself, for example <b> or <span>. What these two cases 
     * have in common is that they may generate inline borders, margins
     * padding and backgrounds.
     */
    else {
        InlineBorder *pBorder;

        pBorder = inlineContextGetBorder(pLayout, pNode);
        if (pBorder) {
            inlineContextPushBorder(pContext, pBorder);
        }

        if(0 != (zReplace=nodeGetTkhtmlReplace(pLayout,pNode))) {
            BoxContext sBox;
            HtmlCanvas *pCanvas;
            memset(&sBox, 0, sizeof(BoxContext));
            layoutReplacement(pLayout, &sBox, pNode, zReplace);
            pCanvas = inlineContextAddInlineCanvas(pContext);
            HtmlDrawCanvas(pCanvas, &sBox.vc, -1*sBox.vc.left, -1*sBox.vc.top);
        }

        /* If there was no replacement image or widget, recurse through the
         * child nodes.
         */
        if (!zReplace) {
            int i;
            for(i=0; i<HtmlNodeNumChildren(pNode) && 0==rc; i++) {
                HtmlNode *pChild = HtmlNodeChild(pNode, i);
                rc = inlineLayoutNode(pLayout, pBox, pChild, pY, pContext);
            }
        }

        if (pBorder) {
            inlineContextPopBorder(pContext, pBorder);
        }
    }

    /* See if there are any complete line-boxes to copy to the main canvas. */
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
    HtmlNode *pParent;
   
    memset(&context, 0, sizeof(InlineContext));
    memset(&lastline, 0, sizeof(HtmlCanvas));

    pParent = HtmlNodeParent(pNode);

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

    for(pN=pNode; pN ; pN = HtmlNodeRightSibling(pN)) {
        inlineLayoutNode(pLayout, pBox, pN, &y, &context);
    }

    return inlineLayoutDrawLines(pLayout, pBox, &context, 1, &y);
}

/*
 *---------------------------------------------------------------------------
 *
 * blockMinMaxWidth --
 *
 *     Figure out the minimum and maximum widths that this block may use.
 *     This is used during table layout.
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
    FloatMargin sFloat;
    int min;        /* Minimum width of this block */
    int max;        /* Maximum width of this block */
    int *pCache;

    Tcl_HashEntry *pEntry;
    int newentry;

    pEntry = Tcl_CreateHashEntry(&pLayout->widthCache, (char*)pNode, &newentry);
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
        memset(&sFloat, 0, sizeof(FloatMargin));
        sBox.pFloats = &sFloat;
        blockLayout2(pLayout, &sBox, pNode, 0);
        HtmlDrawCleanup(&sBox.vc);
        min = sBox.width;
    
        /* Figure out the maximum width of the box by pretending to lay it
         * out with a very large parent width. It is not expected to
         * be a problem that tables may be layed out incorrectly on
         * displays wider than 10000 pixels.
         */
        memset(&sBox, 0, sizeof(BoxContext));
        memset(&sFloat, 0, sizeof(FloatMargin));
        sBox.pFloats = &sFloat;
        sBox.parentWidth = 10000;
        blockLayout2(pLayout, &sBox, pNode, 0);
        HtmlDrawCleanup(&sBox.vc);
        max = sBox.width;
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
        w = nodeGetWidth(pData->pLayout, pNode, pData->availablewidth, 0, &f);

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

    nodeGetBoxProperties(pLayout, pNode, &boxproperties);
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
        HtmlDrawQuad(&pBox->vc, x1, y1, x1+lw, y1+tw, x2-rw, y1+tw, x2, y1, tc);
    }
    if (rw>0) {
        HtmlDrawQuad(&pBox->vc, x2, y1, x2-rw, y1+tw, x2-rw, y2-bw, x2, y2, rc);
    }
    if (bw>0) {
        HtmlDrawQuad(&pBox->vc, x2, y2, x2-rw, y2-bw, x1+lw, y2-bw, x1, y2, bc);
    }
    if (lw>0) {
        HtmlDrawQuad(&pBox->vc, x1, y2, x1+lw, y2-bw, x1+lw, y1+tw, x1, y1, lc);
    }

    if (borderproperties.color_bg) {
        if (pNode!=pLayout->pTop) {
            HtmlDrawQuad(&pBox->vc, 
                    x1+lw, y1+tw, 
                    x2-rw, y1+tw, 
                    x2-rw, y2-bw, 
                    x1+lw, y2-bw, borderproperties.color_bg);
        } else {
            HtmlDrawBackground(&pBox->vc, borderproperties.color_bg);
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
            HtmlDrawCanvas(&pData->pBox->vc, &pCell->box.vc, x, y);
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
    FloatMargin sFloat;
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

    memset(&sFloat, 0, sizeof(FloatMargin));
    pBox->pFloats = &sFloat;

    pBox->parentWidth = pData->aWidth[col];
    for (i=col+1; i<col+colspan; i++) {
        pBox->parentWidth += (pData->aWidth[i] + pData->border_spacing);
    }

    pData->pLayout->marginValid = 0;
    blockLayout2(pData->pLayout, pBox, pNode, 1);
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

    /* Decide on some actual widths for the cells, based on the maximum and
     * minimum widths, the total width of the table and the floating
     * margins. As far as I can tell, neither CSS nor HTML specify exactly
     * how to do this. So we use the following approach:
     *
     * 1. Each cell is assigned it's minimum width.
     * 2. If there are any columns with an explicit width specified as a
     *    percentage, we allocate extra space to them to try to meet these
     *    requests. Explicit widths may mean that the table is not
     *    completely filled.
     * 3. Remaining space is divided up between the cells without explicit
     *    percentage widths. 
     * 
     * Data structure notes:
     *    * If a column had an explicit width specified in pixels, then the
     *      aWidth[], aMinWidth[] and aMaxWidth[] entries are all set to
     *      this value.
     *    * If a column had an explicit width as a percentage, then the
     *      aMaxWidth[] and aWidth[] entries are set to this value
     *      (converted to pixels, not as a percentage). The aMinWidth entry
     *      is still set to the minimum width required to render the
     *      column.
     */
static void tableCalculateCellWidths(pData, width)
    TableData *pData;
    int width;                       /* Total width available for cells */
{
    int extraspace;
    int extraspace_req;
    int i;                           /* Counter variable for small loops */
    int space;                       /* Remaining pixels to allocate */
    int requested;

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

        for (i=0; i<nCol; i++) {
            int colreq = (aMaxWidth[i] - aTmpWidth[i]);
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
     * Todo: This is used by cells that have a percentage value for a
     *       width. So we probably also need to subtract table borders and
     *       gaps between the cells.
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
    nodeGetBoxProperties(pLayout, pNode, &boxproperties);
    minwidth = (nCol+1) * data.border_spacing;
    maxwidth = (nCol+1) * data.border_spacing;
    for (i=0; i<nCol; i++) {
        minwidth += aMinWidth[i];
        maxwidth += aMaxWidth[i];
    }
    assert(maxwidth>=minwidth);

    width = nodeGetWidth(pLayout, pNode, pBox->parentWidth, -1, 0);
    if (width<0) {
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
    int dummy;
    int width;
    int height;

    Tk_Window tkwin = pLayout->tkwin;
    Tcl_Interp *interp = pLayout->interp;

    /* Read any explicit 'width' or 'height' property values assigned to
     * the node.
     */
    width = nodeGetWidth(pLayout, pNode, pBox->parentWidth, -1, &dummy);
    height = nodeGetHeight(pLayout, pNode, pBox->parentWidth, -1);

    if (zReplace[0]=='.') {
        Tk_Window win = Tk_NameToWindow(interp, zReplace, tkwin);
        if (win) {
            Tcl_Obj *pWin = Tcl_NewStringObj(zReplace, -1);
            width = Tk_ReqWidth(win);
            height = Tk_ReqHeight(win);
            HtmlDrawWindow(&pBox->vc, pWin, 0, 0, width, height);
        }
    } else {
	/* Must be an image. Or garbage data returned by an bad Tcl proc.
         * If the later, then resizeImage will return 0.
         */
        Tcl_Obj *pImg;
        pImg = HtmlResizeImage(pLayout->pTree, zReplace, &width, &height);
        if (pImg) {
            HtmlDrawImage(&pBox->vc, pImg, 0, 0, width, height);
        }
    }

    pBox->width = width;
    pBox->height = height;
}

/*
 *---------------------------------------------------------------------------
 *
 * blockLayout2 --
 *
 *     This function is directly responsible for the borders, margins,
 *     padding and background of pNode. Drawing the actual content of pNode
 *     is done by calling a display type specific function - tableLayout(),
 *     blockLayout() or markerLayout(). It also deals with horizontal
 *     alignment of the block within the parent.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int blockLayout2(pLayout, pBox, pNode, omitborder)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;         /* The node to layout */
    int omitborder;          /* True to allocate byt not draw the border */
{
    DisplayProperties display;     /* Display properties of pNode */
    MarginProperties margin;       /* Margin properties of pNode */
    BoxProperties boxproperties;   /* Padding and border properties */
    BoxContext sBox;               /* Box that tableLayout() etc. use */
    CssProperty replace;           /* Value of -tkhtml-replace property */
    int width;                     /* Explicit width of node */
    int top_margin;                /* Actual top margin for box */

    int y = 0;
    int x = 0;

    int leftFloat = 0;             /* Floating margins. Used for tables */
    int rightFloat = 0;            /* and replaced blocks only. */
    Tcl_Interp *interp = pLayout->interp;

    int isBoxObject;               /* True if the node cannot wrap. */
    int isReplaced;                /* True if the node is an image or window */
    int marginValid;

    memset(&sBox, 0, sizeof(BoxContext));

    /* Retrieve the required CSS property values. Return early if the
     * display is set to 'none'. There is nothing to draw for this box.
     */
    nodeGetDisplay(pLayout, pNode, &display);
    if (display.eDisplay==DISPLAY_NONE) {
        return TCL_OK;
    }
    nodeGetMargins(pLayout, pNode, &margin);
    nodeGetBoxProperties(pLayout, pNode, &boxproperties);
    HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY__TKHTML_REPLACE, &replace);

    /* If the node is a table, or a replaced node (image or Tk window),
     * then it's content cannot wrap around any floating boxes. Also it
     * needs to be horizontally aligned after it's drawn. Set the
     * isBoxObject flag so we know to do these things.
     */
    isReplaced = replace.eType==CSS_TYPE_STRING;
    isBoxObject = (display.eDisplay==DISPLAY_TABLE || isReplaced);

    if (pLayout->minmaxTest==0 && isBoxObject) { 
        /* If this is a replaced node or a table, then see if we need to
         * dodge around any floating margins. With other blocks, the
         * content wraps around the margin, not the block itself.
         */
        int min; 
        int dummymax;
    
        blockMinMaxWidth(pLayout, pNode, &min, &dummymax);
        y = floatListPlace(pBox->pFloats, pBox->parentWidth, min, 0);
        floatListClear(pBox->pFloats, y);
        floatListMargins(pBox->pFloats, &leftFloat, &rightFloat);
    }

    /* Figure out how much horizontal space the node content will
     * have available to it. This is the width of the parent box, 
     * less the horizontal margins, border and padding of this box.
     * Floating margins are taken into account for tables and replaced
     * objects only.
     */
    sBox.parentWidth = 
            pBox->parentWidth - margin.margin_left - margin.margin_right -
            boxproperties.border_left - boxproperties.border_right -
            boxproperties.padding_left - boxproperties.padding_right -
            leftFloat - rightFloat;
    if (sBox.parentWidth<0) {
        sBox.parentWidth = 0;
    }

    /* Allocate space for the top margin. The top margin is the greater of
     * margin.margin_top and pLayout->marginValue. This is how collapsing
     * margins are implemented.
     */
    if (pLayout->marginValid) {
        top_margin = MAX(margin.margin_top, pLayout->marginValue);
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
    sBox.pFloats = pBox->pFloats;
    floatListNormalize(sBox.pFloats, -1*x, -1*y);

    /* The minimum height of the box is set by the 'height' property. 
     * So sBox.height to this now. Layout routines only ever overwrite
     * sBox.height with larger values.
     */
    sBox.height = nodeGetHeight(pLayout, pNode, 0, 0);

    marginValid = pLayout->marginValid;
    pLayout->marginValid = 0;
    if (isReplaced) {
        layoutReplacement(pLayout, &sBox, pNode, replace.v.zVal);
    } else {
        /* Draw the box using the function specific to it's display type,
         * then copy the drawing into the parent canvas.
         */
        switch (display.eDisplay) {
            case DISPLAY_LISTITEM:
                markerLayout(pLayout, &sBox, pNode);
            case DISPLAY_BLOCK:
            case DISPLAY_INLINE:
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
     */
    if (sBox.height>0) {
        int hoffset = 0;

        if (display.eDisplay==DISPLAY_TABLE || isReplaced) {
            /* For a table or replaced object, deal with horizontal
             * alignment here.
             */
            int textalign = nodeGetTextAlign(pLayout, pNode);
            switch (textalign) {
                case TEXTALIGN_LEFT:
                    break;
                case TEXTALIGN_RIGHT:
                    if (sBox.parentWidth>sBox.width) {
                        hoffset = (sBox.parentWidth-sBox.width);
                    }
                    break;
                case TEXTALIGN_CENTER:
                case TEXTALIGN_JUSTIFY:
                    if (sBox.parentWidth>sBox.width) {
                        hoffset = (sBox.parentWidth-sBox.width)/2;
                    }
                    break;
            }
        } else {
            if (pLayout->minmaxTest==0) {
                sBox.width = sBox.parentWidth;
            }
        }

        /* Draw the border directly into the parent canvas. */
        if (!omitborder) {
            int x1 = margin.margin_left + leftFloat + hoffset;
            int y1 = top_margin;
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

        if (!pLayout->minmaxTest) {
            nodeComment(&pBox->vc, pNode);
            HtmlDrawCanvas(&pBox->vc, &sBox.vc, x + hoffset, y);
            endNodeComment(&pBox->vc, pNode);
        } else {
            HtmlDrawCleanup(&pBox->vc);
        }
    
        pBox->height = sBox.height + y + 
            boxproperties.border_bottom + boxproperties.padding_bottom;
        pBox->width = MAX(pBox->width, sBox.width + 
                + margin.margin_left + margin.margin_right +
                boxproperties.border_left + boxproperties.border_right +
                boxproperties.padding_left + boxproperties.padding_right);
 
        /* We set the value of pLayout->marginValid to 0 before drawing
         * anything above, so if it is non-zero now then
         * pLayout-marginValue is the size of the collapsing bottom-margin
         * of the last thing drawn in this block context. The bottom margin
         * for this block (to be collapsed into the top margin of the next)
         * becomes the maximum of 'margin-bottom' and pLayout->marginValue.
         *
         * Whether or not this is what the spec says is supposed to happen
         * is a difficult question to answer. It seems to be what Gecko
         * does.
         */
        if (!pLayout->marginValid) {
            pLayout->marginValue = 0;
        }
        pLayout->marginValue = MAX(margin.margin_bottom, pLayout->marginValue);
        pLayout->marginValid = 1;
    } else if (marginValid) {
        /* If there is a box above this one in the flow (i.e. the
	 * top-margin is valid), then set the saved collapsing margin value
	 * to the maximum of it's current value, the top margin and the
	 * bottom margin. Again, the observed behaviour of modern browsers.
	 * The CSS2 spec doesn't really specify this case.
         */
        pLayout->marginValue = MAX(top_margin, margin.margin_bottom);
        pLayout->marginValid = marginValid;
    }
  

    /* Restore the floating margins to the parent boxes coordinate system */
    floatListNormalize(sBox.pFloats, x, y);
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
int HtmlLayoutForce(clientData, interp, objc, objv)
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
    FloatMargin sFloat;            /* Float margins for normal flow */
    LayoutContext sLayout;

    memset(&sLayout, 0, sizeof(LayoutContext));
    memset(&sFloat, 0, sizeof(FloatMargin));

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
    sBox.pFloats = &sFloat;

    /* Assume we already have a styled tree. Todo: This isn't always going
     * to be true, as this function is called as a Tcl proc (and therefore
     * may be called incorrectly by a script.
     */
    assert(pTree->pRoot);

    /* Find the <body> tag in the tree. This tag should always be present.
     * Even if this tag was not specified as part of the document, it
     * should have been inserted by the tree-building code in htmltree.c.
     * We assume that the htmltree.c code forced the document to follow
     * the basic pattern required for HTML. i.e. 
     *
     *     <html>
     *         <head>...</head>
     *         <body>...</body>
     *     </html>
     *
     * The <head> section is handled entirely by stylesheets (possibly 
     * containing custom attributes to run callbacks). The <body> section
     * is the only one of interest to this module.
     */
    for (i=0; !pBody && i<HtmlNodeNumChildren(pTree->pRoot); i++) {
        pBody = HtmlNodeChild(pTree->pRoot, i);
        if (Html_BODY!=HtmlNodeTagType(pBody)) {
            pBody = 0;
        }
    }
    assert(pBody);

    /* Call blockLayout() to layout the top level box, generated by the
     * <body> tag 
     */
    sLayout.pTop = pBody;
    rc = blockLayout2(&sLayout, &sBox, pBody, 0);

    HtmlDrawCleanup(&pTree->canvas);
    memcpy(&pTree->canvas, &sBox.vc, sizeof(HtmlCanvas));

    return rc;
}

