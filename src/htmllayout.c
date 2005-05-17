/*
 * htmllayout2.c --
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

struct FM {
   int x;
   int y;
   struct FM *pNext;
};
struct FloatMargin {
    struct FM *pLeft;
    struct FM *pRight;
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
 * Structure used whilst laying out inline context. See inlineLayout().
 */
struct InlineData {
    HtmlNode *pNode;         /* Current node */
    HtmlToken *pToken;       /* Current token if pN is Html_TEXT */
    HtmlCanvas *pCanvas;     /* Line canvas to draw into */
    int x;                   /* Current x coordinate */
    int ascent;              /* Pixels between top and baseline of line-box */
    int lineheight;          /* Height of this line-box */
    int linewidth;           /* Width of line box */
    int linefull;            /* True if line box is full */
    int rightFloat;          /* Value of right-margin (pixels) */
    int leftFloat;           /* Value of left-margin (pixels) */
    int noFloats;            /* True if there are no floating margins */
    int spaceOk;             /* True if a space character should be drawn */
};

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
S void floatListAdd(FloatMargin*, int, int, int);
S void floatListClear(FloatMargin*, int);
S void floatListMargins(FloatMargin*, int*, int*);
S int  floatListPlace(FloatMargin*, int, int, int);
S int  floatListIsEmpty(FloatMargin *);

S int  propertyToConstant(CssProperty *pProp, const char **, int *, int);
S CONST char *propertyToString(CssProperty *pProp, const char *);
S int propertyToPixels(LayoutContext*, HtmlNode*, CssProperty*, int, int);
S XColor *propertyToColor(LayoutContext *, CssProperty*);

S int attributeToPixels(HtmlNode*, const char*, int, int, int*);

S CssProperty *nodeGetProperty(HtmlNode *, int, int);

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

S HtmlNode * nextInlineNode(LayoutContext*, HtmlNode*, HtmlNode*, int*);
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
 * nodeGetProperty --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
CssProperty *nodeGetProperty(pNode, prop, inherit)
    HtmlNode *pNode;
    int prop;
    int inherit;
{
    CssProperty *p;
    p = HtmlCssPropertiesGet(pNode->pProperties, prop);

    /* Todo: We should deal with the special property value 'inherit' here.
     * As is property inheritance is only implemented for properties for
     * which it is implicit. Callers pass non-zero as the third argument
     * to this call if it is implicitly inherited, but it would be better
     * if this function just consulted a lookup table.
     */
    if (!p && inherit && pNode->pParent) {
        return nodeGetProperty(pNode->pParent, prop, 1);
    }
    return p;
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
 *     One of the DISPLAY_xxx constants defined above.
 *
 * Side effects:
 *     None.
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

    CssProperty *pProp;
    CssProperty prop;

    HtmlNodeGetProperty(pLayout->interp, pNode, CSS_PROPERTY_DISPLAY, &prop);
    pDisplayProperties->eDisplay = 
        propertyToConstant(&prop, zDisplay, eDisplay, DISPLAY_INLINE);

    pProp = nodeGetProperty(pNode, CSS_PROPERTY_FLOAT, 0);
    pDisplayProperties->eFloat = 
        propertyToConstant(pProp, zFloat, eFloat, FLOAT_NONE);

    /* Force all floating boxes to have display type 'block' or 'table' */
    if (pDisplayProperties->eFloat!=FLOAT_NONE &&
            pDisplayProperties->eDisplay!=DISPLAY_TABLE) {
        pDisplayProperties->eDisplay = DISPLAY_BLOCK;
    }
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
    CssProperty *pProp;

    pProp = nodeGetProperty(pNode, CSS_PROPERTY_LIST_STYLE_TYPE, 1);
    return propertyToConstant(
        pProp, zStyleOptions, eStyleOptions, LISTSTYLETYPE_DISC);
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
    CssProperty decor;                /* Property 'text-decoration' */
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
    pFontStyle = nodeGetProperty(pNode, CSS_PROPERTY_FONT_STYLE, 1);
    isItalic = propertyToConstant(pFontStyle, zStyleOptions, eStyleOptions, 0);

    /* If the 'font-weight' attribute is set to either "bold" or
     * "bolder", add the option "-weight bold" to the string version
     * of the Tk font.
     *
     * Todo: Handle numeric font-weight values. Tk restricts the weight
     * of the font to "bold" or "normal", but we should try to do something
     * sensible with other options.
     */
    pFontWeight = nodeGetProperty(pNode, CSS_PROPERTY_FONT_WEIGHT, 1);
    isBold = propertyToConstant(pFontWeight, zWeightOptions, eWeightOptions, 0);

    /* If 'font-family' is set, then use the value as the -family option
     * in the Tk font request. Otherwise use Helvetica, which is always
     * available.
     */
    pFontFamily = nodeGetProperty(pNode, CSS_PROPERTY_FONT_FAMILY, 1);
    zFamily = propertyToString(pFontFamily, "Helvetica");

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
    CssProperty *pBorderSpacing;
    int border_spacing = -1;
    int tagtype = HtmlNodeTagType(pNode);

    if (tagtype==Html_TD || tagtype==Html_TH || tagtype==Html_TABLE) {
        HtmlNode *p = pNode;
        while (p && Html_TABLE!=HtmlNodeTagType(p)) {
            p = HtmlNodeParent(p);
        }
        border_spacing = attributeToPixels(p, "cellspacing", 0, -1, 0);
    }
   
    if (border_spacing==-1) {
        pBorderSpacing = nodeGetProperty(pNode, CSS_PROPERTY_BORDER_SPACING, 1);
        border_spacing = propertyToPixels(pLayout, pNode, pBorderSpacing,0,-1);
    }

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

#if 0
    /* If the width was not specified explicitly with an attribute or
     * property, try and get an intrinsic width for a replaced element.
     */
    if (width.eType==CSS_TYPE_NONE) {
        Tcl_Obj *pReplace = HtmlNodeGetReplacement(pLayout->pHtml, pNode);
        if (pIsFixed) *pIsFixed = 0;

        if (pReplace) {
            CONST char *zReplace = Tcl_GetString(pReplace);
            Tk_Window tkwin = pLayout->tkwin;
            if (zReplace[0]=='.') {
                Tk_Window win = Tk_NameToWindow(pLayout->interp,zReplace,tkwin);
                if (win) {
                    val = Tk_ReqWidth(win);
                    if (pIsFixed) *pIsFixed = 1;
                }
            } else {
                int dummy;
                Tk_Image img = Tk_GetImage(pLayout->interp,tkwin,zReplace,0,0);
                if (img) {
                    Tk_SizeOfImage(img, &val, &dummy);
                    if (pIsFixed) *pIsFixed = 1;
                }
            }
        }
    } else {
#endif
        if (width.eType==CSS_TYPE_PX) {
            if (pIsFixed) *pIsFixed = 1;
        } else {
            if (pIsFixed) *pIsFixed = 0;
        }
#if 0
    }
#endif

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
    CssProperty *pProp;
    pProp = nodeGetProperty(pNode, CSS_PROPERTY_TEXT_ALIGN, 1);
    return propertyToConstant(pProp, zOptions, eOptions, TEXTALIGN_LEFT);
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
 * nodeIsContent --
 *    
 *     Return true if one of the following conditions is true for node
 *     pNode. Otherwise false.
 *
 *         * It is an Html_TEXT node.
 *         * The '-tkhtml-replace' property is set - i.e. the entire node
 *           is replaced by a Tk image or window.
 *         * It generates a floating box.
 *         * It generates a block box.
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
nodeIsContent(pLayout, pNode)
    LayoutContext *pLayout;
    HtmlNode *pNode;
{
    int tag;
    DisplayProperties display;

    tag = HtmlNodeTagType(pNode);
    if (tag==Html_BR || tag==Html_Space || tag==Html_Text) return 1;

    if (nodeGetTkhtmlReplace(pLayout, pNode)) return 1;

    nodeGetDisplay(pLayout, pNode, &display);
    if (display.eFloat!=FLOAT_NONE || display.eDisplay!=DISPLAY_INLINE) {
        return 1;
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * nextNode --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static HtmlNode *nextNode(pNode, pFin, noDescend)
    HtmlNode *pNode;
    HtmlNode *pFin;
    int noDescend;
{
    HtmlNode *pRet;
    if (!noDescend && HtmlNodeNumChildren(pNode)>0) {
        pRet = HtmlNodeChild(pNode, 0);
    } else {
        HtmlNode *pTmp = pNode;
        pRet = 0;
        while (!pRet && pTmp!=pFin) {
            pRet = HtmlNodeRightSibling(pTmp);
            pTmp = HtmlNodeParent(pTmp);
            assert(pTmp);
        }
        if (!pRet) {
            pRet = pFin;
        }
    }
    assert(pRet!=pNode);
    return pRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * firstInlineNode --
 *
 *     This is a helper function for inlineLayout() used to find the first
 *     node of content to layout in an inline context.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static HtmlNode *
firstInlineNode(pLayout, pNode, pFin)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    HtmlNode *pFin;
{
    HtmlNode *pN = pNode;
    while (0==nodeIsContent(pLayout, pN) && pN!=pFin) {
        pN = nextNode(pN, pFin, 0);
    }

    return (pN==pFin?0:pN);
}

/*
 *---------------------------------------------------------------------------
 *
 * nextInlineNode --
 * 
 *     This is a helper function for inlineLayout() used to find the next
 *     node of content to layout in an inline context.
 *
 *     Parameter pNode is the current node. The next node of content is
 *     returned. A node is a 'content' node if it meets one of the
 *     following criteria:
 *
 *         * It is an Html_TEXT node.
 *         * The '-tkhtml-replace' property is set - i.e. the entire node
 *           is replaced by a Tk image or window.
 *         * It generates a floating box.
 *         * It generates a block box.
 *
 *     If the node pFin is reached by the traversal algorithm, NULL is
 *     returned. The traversal algorithm is:
 *
 *         1. Traverse to right-sibling of pNode. If pNode does not have a
 *            right-sibling, then traverse to the right-sibling of the
 *            parent of pNode, and so on. If pFin is encountered (it is
 *            guaranteed to be an ancestor of pNode) NULL is returned.  
 *         2. If pNode meets the above criteria, return it.  
 *         3. If pNode has children, traverse to the left-most child and 
 *            goto step 2.
 *         4. Goto step 1.
 *
 *     If the pSkipFloats parameter points to an integer greater than 0,
 *     then skip this many floating point objects before returning a node.
 *     For each float skipped, *pSkipFloats is decremented.
 *
 * Results:
 *     Pointer to next node of content.
 *
 * Side effects:
 *     May change the value of *pSkipFloats.
 *
 *---------------------------------------------------------------------------
 */
static HtmlNode *
nextInlineNode(pLayout, pNode, pFin, pSkipFloats)
    LayoutContext *pLayout;       /* Layout context */
    HtmlNode *pNode;              /* Current node */
    HtmlNode *pFin;               /* Stop if we reach this node */
    int *pSkipFloats;             /* Pointer to number of floats to skip */
{
    HtmlNode *pRet = pNode;
    DisplayProperties display;

    assert (pNode!=pFin);
    assert((*pSkipFloats)>=0);

    do {
        nodeGetDisplay(pLayout, pRet, &display);
        if (pRet!=pNode && display.eFloat!=FLOAT_NONE) {
            (*pSkipFloats)--;
            assert((*pSkipFloats)>=0);
        }
        pRet = nextNode(pRet, pFin, 1);
        pRet = firstInlineNode(pLayout, pRet, pFin);
    } while (*pSkipFloats && pRet);

    assert (pRet!=pNode);
    return (pRet==pFin?0:pRet);

#if 0
    HtmlNode *pN = pNode;
    DisplayProperties display;
    CssProperty replace;

    assert(nodeIsContent(pNode));

    nodeGetDisplay(pLayout, pN, &display);

    if (display.eFloat!=FLOAT_NONE){
        if (*pSkipFloats) {
            (*pSkipFloats)--;
        } 
    }

    do {
        if (display.eDisplay==DISPLAY_INLINE && HtmlNodeNumChildren(pN)>0) {
            pN = HtmlNodeChild(pN, 0);
        } else {
            HtmlNode *pTmp = 0;
            while (!(pTmp = HtmlNodeRightSibling(pN))) {
                pN = HtmlNodeParent(pN);
                if (pN==pFin) { 
                    return 0;
                }
            }
            assert(pN==pFin || pTmp);
            if (pTmp) pN = pTmp;
        }
        nodeGetDisplay(pLayout, pN, &display);
        HtmlNodeGetProperty(
                pLayout->interp, pN, CSS_PROPERTY__TKHTML_REPLACE, &replace);
    } while (pN!=pFin && 
            replace.eType!=CSS_TYPE_STRING &&
            HtmlNodeNumChildren(pN)>0 &&
            display.eFloat==FLOAT_NONE &&
            display.eDisplay==DISPLAY_INLINE);
    
    if ((*pSkipFloats)>0 && display.eFloat!=FLOAT_NONE) {
        return nextInlineNode(pLayout, pN, pFin, pSkipFloats);
    }

    return pN;
#endif
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
static void inlineText(pLayout, pInline)
    LayoutContext *pLayout;
    InlineData *pInline;
{
    Tk_FontMetrics fm;
    Tk_Font font;
    int sw;
    int whitespace;

    int linefull = pInline->linefull;
    int linewidth = pInline->linewidth;
    int x = pInline->x;
    HtmlNode *pN = pInline->pNode;
    HtmlToken *pToken = pInline->pToken;
    int rightFloat = pInline->rightFloat;
    int spaceOk = pInline->spaceOk;

    Tcl_Obj *pText = 0;        /* Text to draw for this node */

    /* Find the first element in the node pN if pToken is NULL. */
    if (!pToken) {
        pToken = pN->pToken;
    }

    /* Query the stylesheet database for the font to use. */
    font = nodeGetFont(pLayout, pN);
    Tk_GetFontMetrics(font, &fm);

    /* Figure out how to deal with white-space. */
    whitespace = nodeGetWhitespace(pLayout, pN);

    sw = Tk_TextWidth(font, " ", 1);
    while (pToken) {

        char const *zText = 0;
        int nBytes;
        int len = 0;

        switch (pToken->type) {
            case Html_Space:
                if (spaceOk) {
                    zText = " ";
                    nBytes = 1;
                    len = sw;
                    spaceOk = 0;
                }
                break;
            case Html_Text:
                zText = pToken->x.zText;
                nBytes = strlen(zText);
                len = Tk_TextWidth(font, zText, nBytes);
                spaceOk = 1;
                break;
            default:
                pToken = 0;
        }

        if (zText) {
            if (whitespace==WHITESPACE_NORMAL && 
                    len+x>rightFloat && !(x==0 && pInline->noFloats)) {
                linefull = 1;
                break;
            } else {
                if (!pText) {
                    pText = Tcl_NewStringObj("", -1);
                    Tcl_IncrRefCount(pText);
                }
                Tcl_AppendToObj(pText, zText, nBytes);

                x += len;
                linewidth = x;
            }
        }
        if (pToken) pToken = pToken->pNext;
    }

    assert (!pToken||linefull);

    if (pText) {
        XColor *c = nodeGetColour(pLayout, pN);
        int decoration = nodeGetTextDecoration(pLayout, pN);
        int tw = linewidth - pInline->x;      /* Text Width */

        assert(tw>0);
        HtmlDrawText(pInline->pCanvas, pText, pInline->x, 0, tw, font, c);

        if (decoration!=TEXTDECORATION_NONE) {
            int ly;
            ly = 0; /* ly = fm.ascent */
            HtmlDrawQuad(pInline->pCanvas,
                            pInline->x, ly+1,  pInline->x, ly+2,
                            x,          ly+2,  x,          ly+1, c);
        }
        Tcl_DecrRefCount(pText);
        pInline->lineheight = MAX(pInline->lineheight, fm.linespace);
        pInline->ascent = MAX(pInline->ascent, fm.ascent);
    }

    pInline->pToken = pToken;
    pInline->x = x;
    pInline->linefull = linefull;
    pInline->linewidth = linewidth;
    pInline->spaceOk = spaceOk;
    pInline->x = x;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineReplace --
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
inlineReplace(pLayout, p, zReplace, parentWidth)
    LayoutContext *pLayout;     /* Layout context */
    InlineData *p;              /* Line box context */
    CONST char *zReplace;       /* Name of replacement image/window */
    int parentWidth;            /* Width of parent box (for % lengths) */
{
    BoxContext sBox;
    FloatMargin sFloat;
    int w, h;

    memset(&sBox, 0, sizeof(BoxContext));
    memset(&sFloat, 0, sizeof(FloatMargin));
    sBox.pFloats = &sFloat;
    sBox.parentWidth = parentWidth;

    layoutReplacement(pLayout, &sBox, p->pNode, zReplace);
                
    w = sBox.width;
    h = sBox.height;

    if ((w + p->x) > p->rightFloat && !(p->x==0 && p->noFloats)) {
        /* Line box is full. No drawing today. */
        p->linefull = 1;
    } else {
        /* Line box is not full, draw the replaced item into it. Currently,
         * the bottom of the image is always aligned with the baseline of
         * the text. Todo: Support the 'vertical-align' property.
         */
        HtmlDrawCanvas(p->pCanvas, &sBox.vc, p->x, -1*h);
        p->x += w;
        p->linewidth = p->x;
        p->spaceOk = 1;
        p->lineheight = MAX(h, p->lineheight);
        p->ascent = MAX(h, p->ascent);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineLayout --
 *
 *     Draw inline content. This procedure lays out sequential document
 *     nodes in an inline context.
 *
 *     pNode is the first inline node layed out. Following this, all
 *     right-hand siblings of pNode that also contain inline content
 *     are layed out in the same inline context. The number of siblings
 *     (not including pNode) is written into *pSiblings before returning.
 *
 *     If there are no floating margins, then pNode is drawn at coordinates
 *     (0, 0). The floating margins on pBox are expected to be normalized
 *     to this coordinate system by the caller. If there are floating
 *     margins, then inline content is wrapped around them. This function
 *     may call floatListClear().
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
inlineLayout(pLayout, pBox, pNode)
    LayoutContext *pLayout;       /* Layout context */
    BoxContext *pBox;             /* Box context to draw inline elements */
    HtmlNode *pNode;              /* Node to start drawing at */
{
    HtmlNode *pFin = HtmlNodeParent(pNode);
    HtmlToken *pSaveToken = 0;
    HtmlNode *pSaveNode = 0;
    Tcl_Interp *interp = pLayout->interp;
    int y = 0;
    int skipFloat = 0;
    int textalign;
    InlineData data;
    HtmlCanvas lineCanvas;

    memset(&lineCanvas, 0, sizeof(HtmlCanvas));
    memset(&data, 0, sizeof(InlineData));
    data.pCanvas = &lineCanvas;

    pSaveNode = firstInlineNode(pLayout, pNode, pFin);
    /* Each iteration of this top level loop will draw either one floating
     * box, via a call to floatLayout(), or one line of inline content. The 
     * strategy is: 
     *
     *     1) Draw inline elements (i.e. text) into virtual canvas 
     *        'lineCanvas' until a float is encountered or an inline
     *        element that won't fit on the current line. 
     *
     *     2) If we encounter a float, throw away lineCanvas and draw
     *        the float. Then return to step (1) and start drawing the
     *        line again. Set variable 'skipFloat' to the number of 
     *        floating objects to skip (because they are already drawn).
     *
     *     3) If we encounter an inline element that won't fit, draw 
     *        'lineCanvas' into the parent box canvas - 'pBox->vc'.
     */
    while (pSaveNode) {
        DisplayProperties display;

	/* 'sf' is the number of floats to skip (because they have already
         * been drawn) while filling in the line. 
         */
        int sf = skipFloat;

        /* Values 'pSaveNode' and 'pToken' are the point in the document 
         * tree to start drawing the next line of inline content from. If 
         * we encounter a float before we finish laying out this line, then 
         * we try again from the same starting point (but with skipFloat
         * incremented).
         */
        data.pNode = pSaveNode;
        data.pToken = pSaveToken;

        data.leftFloat = 0;
        data.rightFloat = pBox->parentWidth;
        data.lineheight = 0;
        data.ascent = 0;
        data.linewidth = 0;
        data.linefull = 0;
        data.spaceOk = 0;
        data.noFloats = floatListIsEmpty(pBox->pFloats);

        textalign = nodeGetTextAlign(pLayout, data.pNode);

        /* Figure out the margins for the line we are about to draw. */
        floatListMargins(pBox->pFloats, &data.leftFloat, &data.rightFloat);
        data.x = data.leftFloat;

        /* This loop runs once for each node that adds content in the
         * inline context. There are several possibilities:
         *
         *     * <br> tag (this is a hack to support HTML without
         *       supporting the :before pseudo-element).
         *     * Inline text.
         *     * Inline replaced node.
         *     * Floating box.
         *     * Block box.
         */
        do {
            assert(nodeIsContent(pLayout, data.pNode));

            /* Get the display type of the next node to draw, and figure
	     * out whether or not it is a replaced node (i.e.
	     * '-tkhtml-replace' is set). 
             */
	    nodeGetDisplay(pLayout, data.pNode, &display);
            /* A <BR> tag, jump to the next line. This would be better
             * implemented as part of the default stylesheet with a rule
             * like:
             *
             *     BR:before { content: "\A" }
             *
             * But :before is not supported right now.
             */
            if (HtmlNodeTagType(data.pNode)==Html_BR) {
                Tk_Font font; 
                Tk_FontMetrics fm;

                font = nodeGetFont(pLayout, data.pNode);
                Tk_GetFontMetrics(font, &fm);
                if (data.lineheight<fm.linespace) {
                    data.lineheight = fm.linespace;
                }

                data.linefull = 1;
                data.pNode = nextInlineNode(pLayout, data.pNode, pFin, &sf);
                assert(sf==0);
                assert(data.pToken==0);
            }

            else if (display.eDisplay==DISPLAY_INLINE) {
		/* If the display is 'inline', then this must be either a
		 * text node or a replaced inline element. Or possibly a
		 * bug.
                 */
                CONST char *zReplace = nodeGetTkhtmlReplace(pLayout,data.pNode);

                if (zReplace) {
                    assert(data.pToken==0);
                    assert(display.eFloat==FLOAT_NONE);
                    inlineReplace(pLayout, &data, zReplace, pBox->parentWidth);
                } else {
                    assert(HtmlNodeIsText(data.pNode));
                    inlineText(pLayout, &data);
                    assert(data.pToken || !data.linefull);
                }

                if (data.linefull && data.lineheight==0) {
                    /* If the line is full but it's height is zero.
                     *
                     * This can happen if the first word of text could not
                     * fit on the line because of floating margins. Set the
                     * lineheight to 10 so that the y-coordinate will have
                     * increased next loop. Otherwise we end up in an
                     * infinite loop.
                     */
                    data.lineheight = 10;
                }
    
                /* If data.linefull is false, then the inlineReplace() or
                 * inlineText() function drew the content of the node into
                 * lineCanvas. In this case advance to the next node.
                 * Otherwise, leave data.pNode pointing to this node so
                 * that it gets drawn into the next line box.
                 */
                if (!data.linefull) {
                    data.pNode = nextInlineNode(pLayout, data.pNode, pFin, &sf);
                } 
            }

            /* If the current node is a floating box, draw it into the
             * parent context. Then throw away whatever has been drawn
             * into the line-canvas.
             */
            else if (display.eFloat!=FLOAT_NONE){
                floatLayout(pLayout, pBox, data.pNode, &y);
                HtmlDrawCleanup(&lineCanvas);
                if (data.pNode==pSaveNode) {
                    assert(data.pToken==0);
                    assert(skipFloat==0);
                    assert(sf==0);
                    pSaveNode = nextInlineNode(pLayout,data.pNode,pFin,&sf);
                    assert(sf==0);
                } else {
                    skipFloat++;
                }
            }

            /* If the current node is a block node, then the document
             * is technically invalid. Nevertheless, for the good of
             * humanity we push on. Declare the current line "full".
             * We draw this block below, after the current line has been
             * copied to the box canvas.
             */
            else if (display.eDisplay!=DISPLAY_INLINE) {
                data.linefull = 1;
            }

        } while (data.pNode && !data.linefull && display.eFloat==FLOAT_NONE); 

        if (data.linefull || !data.pNode) {
            /* The linebox stored in lineCanvas has content between
             * x-coordinates 'leftFloat' and 'x' layed out assuming
	     * textalign is TEXTALIGN_LEFT. Set drawx to account for the
	     * actual value of the 'text-align' property.
             */
            int drawx;         /* X-coordinate to copy line canvas to */
            int boxwidth;
            switch (textalign) {
                case TEXTALIGN_RIGHT:
                    drawx = data.rightFloat-data.x;
                    break;
                case TEXTALIGN_CENTER:
                    drawx = (data.rightFloat-data.x)/2;
                    break;
                case TEXTALIGN_LEFT:
                default:
                    drawx = 0;
                    break;
            }

            /* If the line-canvas is not empty, draw it to the main canvas.
             * Before doing so, if the collapsing margin value stored in
	     * pLayout is valid, increase y-coordinate by it. After drawing
	     * the line-canvas, set the collapsing margin value to valid
	     * and 0.  This way, if a block box with a 'margin-top'
	     * specified is drawn next in the flow, the vertical margin
	     * between this line and the block box will be the value of the
	     * 'margin-top' property. If the next thing drawn is another
	     * line box, then no extra space will be added.
             *
	     * In lineCanvas, the y-coordinate is relative to the line-box
	     * baseline. But HtmlDrawCanvas interprets it's y-coordinate as
             * the top pixel of the box. To account for this, increment the
             * y-coodinate by lineCanvas.ascent when drawing the line-box.
             *
             * Todo: This might be the place to add the implementation of
             * the 'line-spacing' property. Is 'line-spacing' analogous to
             * a collapsing margin for a line-box?
             */
            if (!HtmlDrawIsEmpty(&lineCanvas)) {
                if (pLayout->marginValid) {
                    y += pLayout->marginValue;
                }
                HtmlDrawCanvas(&pBox->vc, &lineCanvas, drawx, y+data.ascent);
                HtmlDrawComment(&pBox->vc, "Line canvas");
                assert(skipFloat>=0);
                pLayout->marginValid = 1;
                pLayout->marginValue = 0;
            }
            y += data.lineheight;
            floatListClear(pBox->pFloats, y);

            /* Increase the actual box width if this is the longest line
             * drawn so far. Floating boxes drawn into pBox may update
             * pBox->width in floatLayout(), so we don't have to consider
             * them here.
             */
            boxwidth = data.linewidth+(pBox->parentWidth-data.rightFloat);
            pBox->width = MAX(boxwidth, pBox->width);

            pSaveNode = data.pNode;
            pSaveToken = data.pToken;
            skipFloat = sf;
        }

	/* If data.pNode points to a node that generates a non-floating
	 * block box, then call blockLayout2 to draw the block box and it's
	 * contents into the flow. The x-coordinate for the block box is
	 * always 0, blockLayout2 dodges around floating margins if
	 * required.
         */
        if (data.pNode && 
            display.eDisplay!=DISPLAY_INLINE && 
            display.eFloat==FLOAT_NONE) 
        {
            BoxContext sBox;
 
            memset(&sBox, 0, sizeof(BoxContext));
            sBox.parentWidth = pBox->parentWidth;
            sBox.pFloats = pBox->pFloats;

            floatListNormalize(sBox.pFloats, 0, -1*y);
            blockLayout2(pLayout, &sBox, data.pNode, 0);
            if (!HtmlDrawIsEmpty(&sBox.vc)) {
                HtmlDrawCanvas(&pBox->vc, &sBox.vc, 0, y);
                HtmlDrawComment(&pBox->vc, "Block canvas");
            }
            floatListNormalize(sBox.pFloats, 0, y);

            pBox->pFloats = sBox.pFloats;
            y += sBox.height;

            pBox->width = MAX(pBox->width, sBox.width);
            pSaveNode = nextInlineNode(pLayout, data.pNode, pFin, &sf);
        }
    }

    pBox->height = MAX(y, pBox->height);
    return TCL_OK;
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
            HtmlDrawCanvas(&pBox->vc, &sBox.vc, x + hoffset, y);
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

    /* Free up all the font references used to lay out the document. 
     * Todo: need something similar for colors.
     */
#if 0
    if (0) {
        Tcl_HashSearch s;
        Tcl_HashEntry *p = Tcl_FirstHashEntry(&sLayout.fontCache, &s);
        while (p) {
            Tk_FreeFont((Tk_Font)Tcl_GetHashValue(p));
            p = Tcl_NextHashEntry(&s);
        }

        p = Tcl_FirstHashEntry(&sLayout.widthCache, &s);
        while (p) {
            ckfree((char *)Tcl_GetHashValue(p));
            p = Tcl_NextHashEntry(&s);
        }
    }
#endif

    return rc;
}

