
#include "htmllayout.h"
#include <stdio.h>
#include <stdarg.h>

/*
 *
 * The InlineContext "object" encapsulates many of the details of
 * laying out an inline-context. The internals of the InlineContext 
 * and InlineBorder datatypes are both encapsulated within this file.
 *
 * ALLOCATION AND DEALLOCATION:
 *
 *     HtmlInlineContextNew()
 *     HtmlInlineContextCleanup()
 *
 * ADD INLINE BOXES:
 *
 *     HtmlInlineContextAddBox()
 *     HtmlInlineContextAddText()
 *
 * RETRIEVE LINE BOXES:
 *
 *     HtmlInlineContextGetLineBox()
 *
 * ADD INLINE BORDERS:
 *
 *     HtmlGetInlineBorder()
 *     HtmlInlineContextPushBorder()
 *     HtmlInlineContextPopBorder()
 *
 * QUERY:
 * 
 *     HtmlInlineContextIsEmpty
 */
static const char rcsid[] = "$Id: htmlinline.c,v 1.30 2006/08/11 12:24:05 danielk1977 Exp $";

typedef struct InlineBox InlineBox;

struct InlineBorder {
/*
  BorderProperties border;
*/
  MarginProperties margin;
  BoxProperties box;
  int iVerticalAlign;         /* Vertical offset for content (+ve = upwards) */
  int textdecoration;         /* Value of 'text-decoration' property */
  XColor *color;              /* Color for text-decoration */
  int iStartBox;              /* Leftmost inline-box */
  int iStartPixel;            /* Leftmost pixel of outer margin */
  HtmlNode *pNode;            /* Document node that generated this border */
  int parentblock;
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
  int nEmPixels;              /* em pixels of the font, if any */
};

struct InlineContext {
  HtmlTree *pTree;        /* Pointer to owner widget */
  HtmlNode *pNode;        /* Pointer to the node that generated the context */
  int isSizeOnly;         /* Do not draw, just estimate sizes of things */
 
  int textAlign;          /* One of TEXTALIGN_LEFT, TEXTALIGN_RIGHT etc. */
  int whiteSpace;         /* One of WHITESPACE_PRE, WHITESPACE_NORMAL etc. */
  int lineHeight;         /* Value of 'line-height' on inline parent */
  int iTextIndent;        /* Pixels of 'text-indent' for next line */
  int ignoreLineHeight;   /* Boolean - true to ignore lineHeight */

  int nInline;            /* Number of inline boxes in aInline */
  int nInlineAlloc;       /* Number of slots allocated in aInline */
  InlineBox *aInline;     /* Array of inline boxes. */

  int iVAlign;               /* Current vertical box offset */
  InlineBorder *pBorders;    /* Linked list of active inline-borders. */
  InlineBorder *pBoxBorders; /* Borders list for next box to be added */
};

#define START_LOG(x) \
if (pContext->pTree->options.logcmd && !pContext->isSizeOnly &&                \
    pContext->pNode->iNode >= 0) {                                             \
    const char *zFunction = x;                                                 \
    Tcl_Obj *pLog = Tcl_NewObj();                                              \
    Tcl_IncrRefCount(pLog);                                                    \
    {

#define END_LOG                                                                \
    }                                                                          \
    HtmlLog(pContext->pTree, "LAYOUTENGINE", "%s %s() -> %s",                  \
            Tcl_GetString(HtmlNodeCommand(pContext->pTree, pContext->pNode)),  \
            zFunction, Tcl_GetString(pLog)                                     \
    );                                                                         \
    Tcl_DecrRefCount(pLog);                                                    \
}

static void 
oprintf(Tcl_Obj *pObj, CONST char *zFormat, ...) {
    int nBuf = 0;
    char zBuf[1024];
    va_list ap;
    va_start(ap, zFormat);
    nBuf = vsnprintf(zBuf, 1023, zFormat, ap);
    Tcl_AppendToObj(pObj, zBuf, nBuf);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextPushBorder --
 *
 *     Configure the inline-context object with an inline-border that
 *     should start before the inline-box about to be added. The
 *     inline-border object should be obtained with a call to
 *     HtmlGetInlineBorder(). 
 *
 *     If this function is called twice for the same inline-box, then the
 *     second call creates the innermost border.
 *
 *     This function is used with HtmlInlineContextPopBorder() to define the
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
 *         HtmlInlineContextPushBorder( <Border-1> )
 *         inlineContextAddInlineCanvas( <Inline-box-1> )
 *         HtmlInlineContextPushBorder( <Border-2> )
 *         inlineContextAddInlineCanvas( <Inline-box-2> )
 *         HtmlInlineContextPopBorder( <Border 2> )
 *         HtmlInlineContextPopBorder( <Border 1> )
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlInlineContextPushBorder(p, pBorder)
    InlineContext *p;
    InlineBorder *pBorder;
{
    if (pBorder) {
        pBorder->pNext = p->pBoxBorders;
        p->pBoxBorders = pBorder;
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextPopBorder --
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
void 
HtmlInlineContextPopBorder(p, pBorder)
    InlineContext *p;
    InlineBorder *pBorder;
{
    if (!pBorder) return;

    if (p->pBoxBorders) {
        /* If there are any borders in the InlineContext.pBoxBorders list,
         * then we are popping a border for a node that has no content.
         * i.e. from the markup:
         *
         *     <a href="www.google.com"></a>
         *
	 * For this case just remove an entry from
	 * InlineContext.pBoxBorders. The border will never be drawn.
         */
        InlineBorder *pBorder = p->pBoxBorders;
        p->pBoxBorders = pBorder->pNext;
        HtmlFree(0, (char *)pBorder);
    } else {
        if (p->nInline > 0) {
            InlineBox *pBox = &p->aInline[p->nInline-1];
            pBox->nBorderEnd++;
            pBox->nRightPixels += pBorder->box.iRight;
            pBox->nRightPixels += pBorder->margin.margin_right;
        } else {
            pBorder = p->pBorders;
            assert(pBorder);
            p->pBorders = pBorder->pNext;
            HtmlFree(0, (char *)pBorder);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlGetInlineBorder --
 *
 *     This function retrieves the border, background, margin and padding
 *     properties for node pNode. If the properties still all have their
 *     default values, then NULL is returned. Otherwise an InlineBorder
 *     struct is allocated using HtmlAlloc(0, ), populated with the various
 *     property values and returned.
 *
 *     The returned struct is considered private to the inlineContextXXX()
 *     routines. The only legitimate use is to pass the pointer to
 *     HtmlInlineContextPushBorder().
 *
 * Results:
 *     NULL or allocated InlineBorder structure.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
InlineBorder *
HtmlGetInlineBorder(pLayout, pNode, parentblock)
    LayoutContext *pLayout; 
    HtmlNode *pNode;
    int parentblock;        /* True if pNode is the parent block-box */
{
    InlineBorder border;
    InlineBorder *pBorder = 0;
    HtmlComputedValues *pValues = pNode->pPropertyValues;

    HtmlNode *pParent;      /* Parent node */
    HtmlFont *pFont;        /* Font of the parent node */

    /* TODO: Pass a parent-width to this function to calculate
     * widths/heights specified as percentages.
     */

    if (!parentblock) {
        nodeGetBoxProperties(pLayout, pNode, 0,&border.box);
        nodeGetMargins(pLayout, pNode, 0, &border.margin);
    } else {
        memset(&border, 0, sizeof(InlineBorder));
    }
    border.textdecoration = pValues->eTextDecoration;
    border.pNext = 0;
    
    /* Find the font of the parent node. This is required to figure out
     * some values of the 'vertical-align' property.
     */
    pParent = HtmlNodeParent(pNode);
    if (!pParent) {
        pParent = pNode;
    }
    pFont = pParent->pPropertyValues->fFont;

    switch (pValues->eVerticalAlign) {
        case 0:
            border.iVerticalAlign = pValues->iVerticalAlign;
            break;

        case CSS_CONST_SUPER:
            border.iVerticalAlign = (pFont->ex_pixels * 4) / 5;
            break;
        case CSS_CONST_SUB:
            border.iVerticalAlign = (pFont->ex_pixels * -4) / 5;
            break;

        case CSS_CONST_BASELINE:
        default:
            border.iVerticalAlign = 0;
            break;
    }

    if (border.box.iLeft      || border.box.iRight     ||
        border.box.iBottom    || border.box.iTop       ||
        border.margin.margin_left || border.margin.margin_right   ||
        border.margin.margin_top  || border.margin.margin_bottom  ||
        pValues->cBackgroundColor->xcolor ||
        border.iVerticalAlign != CSS_CONST_BASELINE ||
        border.textdecoration != CSS_CONST_NONE ||
        pNode->pDynamic
    ) {
        border.color = pValues->cColor->xcolor;
        pBorder = (InlineBorder *)HtmlAlloc(0, sizeof(InlineBorder));
        memcpy(pBorder, &border, sizeof(InlineBorder));
        pBorder->parentblock = parentblock;
        pBorder->pNode = pNode;
    }

    return pBorder;
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
        p->aInline = (InlineBox *)HtmlRealloc(0, a, nAlloc*sizeof(InlineBox));
        p->nInlineAlloc = nAlloc;
    }

    pBox = &p->aInline[p->nInline - 1];
    memset(pBox, 0, sizeof(InlineBox));
    pBox->pBorderStart = p->pBoxBorders;
    for (pBorder = pBox->pBorderStart; pBorder; pBorder = pBorder->pNext) {
        pBox->nLeftPixels += pBorder->box.iLeft;
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
 *     The point (0, 0) on the canvas is assumed to correspond to the
 *     far left edge of the content, right on the baseline vertically.
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
inlineContextSetBoxDimensions(p, width, ascent, descent, em_pixels)
    InlineContext *p;
    int width;
    int ascent;
    int descent;
    int em_pixels;
{
    InlineBox *pBox;
    assert(p->nInline>0);
    pBox = &p->aInline[p->nInline-1];
    pBox->nContentPixels = width;
    pBox->nAscentPixels = ascent;
    pBox->nDescentPixels = descent;
    pBox->nEmPixels = em_pixels;
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
    if (p->nInline>0) {
        InlineBox *pBox = &p->aInline[p->nInline - 1];
        if (p->whiteSpace == CSS_CONST_PRE) {
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
    InlineBox *pBox;
    inlineContextAddInlineCanvas(p, 0, 0);
    pBox = &p->aInline[p->nInline - 1];
    pBox->eNewLine = nHeight;
    pBox->nEmPixels = nHeight;

    /* This inline-box is added only to account for space that may come
     * after the new line box.
     */
    inlineContextAddInlineCanvas(p, 0, 0);
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
    int textdecoration = pBorder->textdecoration;

    int dlb = (pBorder->iStartBox >= 0);        /* Draw Left Border */

    int flags = (dlb?0:CANVAS_BOX_OPEN_LEFT)|(drb?0:CANVAS_BOX_OPEN_RIGHT);
    int mmt = pLayout->minmaxTest;
    HtmlNode *pNode = pBorder->pNode;

    x1 += (dlb ? pBorder->margin.margin_left : 0);
    x2 -= (drb ? pBorder->margin.margin_right : 0);
    y1 += pBorder->margin.margin_top;
    y2 -= pBorder->margin.margin_bottom;

    if (!pBorder->parentblock) {
        HtmlDrawBox(pCanvas, x1, y1, x2-x1, y2-y1, pNode, flags, mmt);
    }

    if (textdecoration != CSS_CONST_NONE || pNode->pDynamic ) {
        int y_o;                  /* Y-coord for overline */
        int y_t;                  /* Y-coord for linethough */
        int y_u;                  /* Y-coord for underline */

#if 0
        x1 += (dlb ? pBorder->box.padding_left : 0);
        x2 -= (drb ? pBorder->box.padding_right : 0);
        y1 += pBorder->box.padding_top;
        y2 -= pBorder->box.padding_bottom;
#endif
        x1 += (dlb ? pBorder->box.iLeft : 0);
        x2 -= (drb ? pBorder->box.iRight : 0);
        y1 += pBorder->box.iTop;
        y2 -= pBorder->box.iBottom;

        y_o = y1;
        y_t = (y2+y1)/2;
        y_u = 1;

	/* At this point we draw a horizontal line for the underline,
	 * linethrough or overline decoration. The line is to be drawn
	 * between 'x1' and 'x2' x-coordinates, at y-coordinate 'y'.
         *
         * However, we don't want to draw this decoration on replaced
         * inline boxes. So use the aReplacedX[] array to avoid doing this.
         */
        if (nRepX > 0) {
            int xa = x1;
            int i;
            for (i = 0; i < nRepX; i++) {
                int xs = aRepX[i*2]; 
                int xe = aRepX[i*2+1]; 
                if (xe <= xs) continue;

                if (xs > xa) {
                    int xb = MIN(xs, x2);
                    HtmlDrawLine(pCanvas, xa, xb-xa, y_o, y_t, y_u, pNode, mmt);
                }
                xa = xe;
            }
            if (xa < x2) {
                HtmlDrawLine(pCanvas, xa, x2-xa, y_o, y_t, y_u, pNode, mmt);
            }
        } else {
            HtmlDrawLine(pCanvas, x1, x2 - x1, y_o, y_t, y_u, pNode, mmt);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextGetLineBox --
 *
 *     Parameter pWidth is a little complicated. When this function is
 *     called, *pWidth should point to the width available for the
 *     current-line box. If not even one inline-box can fit within this
 *     width, and the LINEBOX_FORCEBOX flag is not true, then zero is
 *     returned and *pWidth set to the minimum width required to draw
 *     content. If zero is returned and *pWidth is set to 0, then the
 *     InlineContext is completely empty of inline-boxes and no line-box
 *     can be generated.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlInlineContextGetLineBox(pLayout, p, pWidth, flags, pCanvas, pVSpace,pAscent)
    LayoutContext *pLayout;
    InlineContext *p;
    int *pWidth;              /* IN/OUT: See above */
    int flags;
    HtmlCanvas *pCanvas;      /* Canvas to render line box to */
    int *pVSpace;             /* OUT: Total height of generated linebox */
    int *pAscent;             /* OUT: Ascent of line box */
{
    InlineContext * const pContext = p;  /* For the benefit of the LOG macros */

    int bRet = 0;             /* Boolean return value. */
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
    int descent = 0;
    int em_pixels = 0;
    int line_height = 0;
    int *aReplacedX = 0;     /* List of x-coords - borders of replaced objs. */
    int nReplacedX = 0;      /* Size of aReplacedX divided by 2 */

    int forceline = (flags & LINEBOX_FORCELINE);
    int forcebox = (flags & LINEBOX_FORCEBOX);

    char const *zLogComment = 0;

    /* The amount of horizontal space available in which to stack boxes */
    const int width = *pWidth - p->iTextIndent;

    memset(&content, 0, sizeof(HtmlCanvas));
    memset(&borders, 0, sizeof(HtmlCanvas));

    /* This block sets the local variables nBox and lineboxwidth.
     *
     * If 'white-space' is not "nowrap", count how many of the inline boxes
     * fit within the requested line-box width. Store this in nBox. Also
     * remember the width of the line-box assuming normal word-spacing.
     * We'll need this to handle the 'text-align' attribute later on.
     * 
     * If 'white-space' is "nowrap", then this loop is used to determine
     * the width of the line-box only.
     */
    for(i = 0; i < p->nInline; i++) {
        InlineBox *pBox = &p->aInline[i];
        int boxwidth = pBox->nContentPixels;
        boxwidth += pBox->nRightPixels + pBox->nLeftPixels;
        if(i > 0) {
            boxwidth += p->aInline[i-1].nSpace;
        }
        if (lineboxwidth+boxwidth > width && 
            p->whiteSpace != CSS_CONST_NOWRAP
        ) {
            break;
        }
        lineboxwidth += boxwidth;
        if (pBox->eNewLine) {
            break;
        }
    }
    nBox = i;
    if (nBox < p->nInline && p->aInline[nBox].eNewLine) {
        nBox++;
    }

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
        goto exit_getlinebox;
    }

    if (0 == nBox) {
        if (p->aInline[0].eNewLine) {
            /* The line-box consists of a single new-line only.  */
            *pVSpace = p->aInline[0].eNewLine;
            p->iTextIndent = 0;
            bRet = 1;
            zLogComment = "a single newline box";
            goto exit_getlinebox;
        }
        if (forcebox && !p->aInline[0].eNewLine) {
	    /* The first inline-box is too wide for the supplied width, but
	     * the 'forcebox' flag is set so we have to lay out at least
	     * one box. A gotcha is that we don't want to lay out our last
	     * inline box unless the 'forceline' flag is set. We might need
	     * it to help close an inline-border.
             */
            if (p->nInline > 1 || forceline) {
                InlineBox *pBox = &p->aInline[i];
                assert(lineboxwidth == 0);
                lineboxwidth = pBox->nContentPixels;
                lineboxwidth += pBox->nRightPixels + pBox->nLeftPixels;
                nBox = 1;
            } else {
                *pWidth = 0;
                goto exit_getlinebox;
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
        goto exit_getlinebox;
    }

    if (p->whiteSpace == CSS_CONST_NOWRAP && 
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
        goto exit_getlinebox;
    }

    /* Adjust the initial left-margin offset and the nExtra variable to 
     * account for the 'text-align' property. nExtra is the number of extra
     * pixels added between each inline-box. This is how we implement
     * justification.
     */
    switch(p->textAlign) {
        case CSS_CONST_CENTER:
            iLeft = (width - lineboxwidth) / 2;
            break;
        case CSS_CONST_RIGHT:
            iLeft = (width - lineboxwidth);
            break;
        case CSS_CONST_JUSTIFY:
            if (nBox > 1 && width > lineboxwidth && nBox < p->nInline) {
                nExtra = (double)(width - lineboxwidth) / (double)(nBox-1);
            }
            break;
    }
    iLeft += p->iTextIndent;
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
        int nBorderDraw = 0;

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
            x1 -= pBorder->box.iLeft;
            pBorder->iStartBox = i;
            pBorder->iStartPixel = x1;
            p->iVAlign += pBorder->iVerticalAlign;
            if (!pBorder->pNext) {
                pBorder->pNext = p->pBorders;
                p->pBorders = pBox->pBorderStart;
                break;
            }
        }

        *pAscent = MAX(pBox->nAscentPixels + p->iVAlign, *pAscent);
        descent = MAX(pBox->nDescentPixels - p->iVAlign, descent);
        em_pixels = MAX(pBox->nEmPixels, em_pixels);

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
            aReplacedX = (int *)HtmlRealloc(0, (char *)aReplacedX, nBytes);
            aReplacedX[(nReplacedX-1)*2] = x1;
            aReplacedX[(nReplacedX-1)*2+1] = x1 + boxwidth;
        }
        DRAW_CANVAS(&content, &pBox->canvas, x1, -1 * p->iVAlign, pBox->pNode);
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
                nBorderDraw++;
            }
        } else {
            nBorderDraw = pBox->nBorderEnd;
        }
        for(j = 0; j < nBorderDraw; j++) {
            int k;
            int nTopPixel = 0;
            int nBottomPixel = 0;
            int rb;
            HtmlCanvas tmpcanvas;

            pBorder = p->pBorders;
            for (k=0; k<j+1; k++) {
                nTopPixel += pBorder->box.iTop;
                nTopPixel += pBorder->margin.margin_top;
                nBottomPixel += pBorder->box.iBottom;
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
                x2 += pBorder->box.iRight;
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
                p->iVAlign -= pBorder->iVerticalAlign;
                p->pBorders = pBorder->pNext;
            }
            HtmlFree(0, (char *)pBorder);
        }

        x += pBox->nSpace;
    }

    /* If any borders are still in the InlineContext.pBorders list, then
     * they flow over onto the next line. Draw the portion that falls on
     * this line now. Set InlineBorder.iStartBox to -1 so that the next
     * call to HtmlInlineContextGetLineBox() knows that this border does not
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
    line_height = p->lineHeight;
    if (line_height < 0) {
        line_height = (line_height * em_pixels) / -100;
    }
    if (*pVSpace < line_height && !p->ignoreLineHeight) {
        *pAscent += (line_height - *pVSpace) / 2;
        *pVSpace = line_height;
    }

    /* Draw the borders and content canvas into the target canvas. Draw the
     * borders canvas first so that it is under the content.
     */
    DRAW_CANVAS(pCanvas, &borders, 0, 0, 0);
    DRAW_CANVAS(pCanvas, &content, 0, 0, 0);

    p->nInline -= nBox;
    memmove(p->aInline, &p->aInline[nBox], p->nInline * sizeof(InlineBox));

    bRet = 1;
exit_getlinebox:
    if (aReplacedX) {
        HtmlFree(0, (char *)aReplacedX);
    }
    if (bRet) {
        p->iTextIndent = 0;

        START_LOG("HtmlInlineContextGetLineBox");
            oprintf(pLog, "<ul>");
            oprintf(pLog, "<li>Requested line box width: %d", width);
            oprintf(pLog, "<li>Generated a line box containing %d boxes", nBox);
            oprintf(pLog, " (%s)", zLogComment ? zLogComment : "unspecified");
            oprintf(pLog, "<li>line box height: %dpx", *pVSpace);
            oprintf(pLog, "<li>line box ascent: %dpx", *pAscent);
        END_LOG;

    }
    return bRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextIsEmpty --
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
int 
HtmlInlineContextIsEmpty(pContext)
    InlineContext *pContext;
{
    return (pContext->nInline==0);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextCleanup --
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
void 
HtmlInlineContextCleanup(pContext)
    InlineContext *pContext;
{
    InlineBorder *pBorder;

    if (pContext->aInline) {
        HtmlFree(0, (char *)pContext->aInline);
    }
    
    pBorder = pContext->pBoxBorders;
    while (pBorder) {
        InlineBorder *pTmp = pBorder->pNext;
        HtmlFree(0, (char *)pBorder);
        pBorder = pTmp;
    }

    pBorder = pContext->pBorders;
    while (pBorder) {
        InlineBorder *pTmp = pBorder->pNext;
        HtmlFree(0, (char *)pBorder);
        pBorder = pTmp;
    }

    HtmlFree(0, (char *)pContext);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextNew --
 *
 *     Allocate and return a new InlineContext object. pNode is a pointer to
 *     the block box that generates the inline context. 
 * 
 *     If argument isSizeOnly is non-zero, then the context uses a value of
 *     "left" for the 'text-align' property, regardless of the value of
 *     pNode->eTextAlign.
 *
 *     The third argument is the used value, in pixels, of the 'text-indent'
 *     property. This value is passed in seperately (instead of being extracted
 *     from pNode->pPropertyValues) because it may be specified as a percentage
 *     of the containing block. This module does not have access to that data,
 *     hence the used property value must be calculated by the caller.
 *
 * Results:
 *     Pointer to new InlineContext structure.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
InlineContext *
HtmlInlineContextNew(pTree, pNode, isSizeOnly, iTextIndent)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int isSizeOnly;
    int iTextIndent;    /* Pixel balue of 'text-indent' for parent block box */
{
    HtmlComputedValues *pValues = pNode->pPropertyValues;
    InlineContext *pContext;

    pContext = (InlineContext *)HtmlClearAlloc(0, sizeof(InlineContext));
    pContext->pTree = pTree;
    pContext->pNode = pNode;

    /* Set the value of the 'text-align' property to use when formatting an
     * inline-context. An entire inline context always has the same value
     * for 'text-align', the value assigned to the block that generates the
     * inline context. For example, in the following code:
     *
     *     <p style="text-align:center">
     *         .... text ....
     *         <span style="text-align:left">
     *         .... more text ....
     *     </p>
     *
     * all lines are centered. The style attribute of the <span> tag has no
     * effect on the layout.
     *
     * If the 'white-space' property is set to other than 'normal', then
     * any specified value of 'text-align' is ignored and inline blocks
     * are aligned against the left margin.
     */
    pContext->whiteSpace = pValues->eWhitespace;
    pContext->textAlign = pValues->eTextAlign;
    if (isSizeOnly) { 
        pContext->textAlign = CSS_CONST_LEFT;
    } else if (
        pValues->eWhitespace != CSS_CONST_NORMAL && 
        pContext->textAlign == CSS_CONST_JUSTIFY
    ) {
        pContext->textAlign = CSS_CONST_LEFT;
    }

    /* The 'line-height' property for the block-box that generates this 
     * inline context is used as the minimum line height for all generated 
     * line-boxes. At least, that's the story in "standards" mode.
     */
    if (pValues->iLineHeight >= 0) {
        pContext->lineHeight = pValues->iLineHeight;
    } else {
        int il = -120;
        if (pValues->iLineHeight != PIXELVAL_NORMAL) {
            il = pValues->iLineHeight;
        } 
        pContext->lineHeight = (pValues->fFont->em_pixels * il) / -100;
    }

    if (
        pTree->options.mode != HTML_MODE_STANDARDS && 
        pValues->eDisplay == CSS_CONST_TABLE_CELL
    ) {
        pContext->ignoreLineHeight = 1;
    }

    /* 'text-indent' property affects the geometry of the first line box
     * generated by this inline context. The value of isSizeOnly is passed
     * to all of the HtmlDrawXXX() calls (so that they don't allocate a screen
     * graph if we are just testing for the min/max size of a block).
     */
    pContext->iTextIndent = iTextIndent;
    pContext->isSizeOnly = isSizeOnly;

    START_LOG("HtmlInlineContextNew")
        const char *zWhiteSpace = HtmlCssConstantToString(pContext->whiteSpace);
        const char *zTextAlign = HtmlCssConstantToString(pContext->textAlign);

        oprintf(pLog, "<p>Created a new inline context initialised with:</p>");
        oprintf(pLog, "<ul><li>'white-space': %s", zWhiteSpace);
        oprintf(pLog, "    <li>'text-align': %s", zTextAlign);
        if (pValues->iLineHeight != PIXELVAL_NORMAL) {
            oprintf(pLog, "    <li>'line-height': %dpx", pContext->lineHeight);
        } else {
            oprintf(pLog, "    <li>'line-height': normal");
        }
        oprintf(pLog, "    <li>'text-indent': %dpx", pContext->iTextIndent);
    END_LOG;

    return pContext;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextAddText --
 *
 *     Argument pNode must be a pointer to a text node. All tokens that 
 *     make up the text node are added to the InlineContext object pContext.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlInlineContextAddText(pContext, pNode)
    InlineContext *pContext;
    HtmlNode *pNode;
{
    HtmlToken *pToken;
    XColor *color;                 /* Color to render in */
    HtmlFont *pFont;               /* Font to render in */
    Tk_Font tkfont;                /* Copy of pFont->tkfont */

    int sw;                        /* Space-Width in pFont. */
    int nh;                        /* Newline-height in pFont */
    int szonly = pContext->isSizeOnly;

    HtmlNode *pParent;             /* Parent of text node */
    HtmlComputedValues *pValues;   /* Computed values of parent node */
    int isFirst = 1;               /* Set to zero after first token */
    int iIndex = 0;

    assert(pNode && HtmlNodeIsText(pNode) && HtmlNodeParent(pNode));
    assert(HtmlNodeParent(pNode)->pPropertyValues);

    pParent = HtmlNodeParent(pNode);
    pValues = pParent->pPropertyValues;
    pFont = pValues->fFont;

    tkfont = pFont->tkfont;
    color = pValues->cColor->xcolor;

    sw = pFont->space_pixels;
    nh = pFont->metrics.ascent + pFont->metrics.descent;

    for (pToken=pNode->pToken; pToken; pToken=pToken->pNextToken) {
        switch(pToken->type) {
            case Html_Text: {
                Tcl_Obj *pText;
                HtmlCanvas *p; 
                int tw;            /* Text width */
                int ta;            /* Text ascent */
                int td;            /* Text descent */
                int tem;            /* Text descent */

                p = inlineContextAddInlineCanvas(pContext, 0, pNode);
                pText = Tcl_NewStringObj(pToken->x.zText, pToken->count);
                Tcl_IncrRefCount(pText);
                tw = Tk_TextWidth(tkfont, pToken->x.zText, pToken->count);
                ta = pFont->metrics.ascent;
                td = pFont->metrics.descent;
                tem = pFont->em_pixels;
                inlineContextSetBoxDimensions(pContext, tw, ta, td, tem);
                HtmlDrawText(p, pText, 0, 0, tw, szonly, pNode, iIndex);
                Tcl_DecrRefCount(pText);
                iIndex += pToken->count;
                pContext->ignoreLineHeight = 0;
                break;
            }
            case Html_Space: {
                int i;
                if (
                    pContext->whiteSpace == CSS_CONST_PRE && 
                    pToken->x.newline /* && !isFirst */
                ) {
                    inlineContextAddNewLine(pContext, nh);
                } else {
                    if (
                        pContext->whiteSpace == CSS_CONST_PRE &&
                        HtmlInlineContextIsEmpty(pContext) &&
                        !pToken->x.newline
                    ) {
                        inlineContextAddInlineCanvas(pContext, 0, pNode);
                        inlineContextSetBoxDimensions(pContext, 0, 0, 0, 0);
                    }
                    for (i = 0; i < pToken->count; i++) {
                        inlineContextAddSpace(pContext, sw);
                    }
                }
                iIndex++;
                break;
            }
            default:
                return;
        }
        isFirst = 0;
    }

    return;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContextAddBox --
 *
 *     Add a pre-rendered box to the InlineContext object *pContext.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlInlineContextAddBox(pContext, pNode, pCanvas, iWidth, iHeight, iOffset)
    InlineContext *pContext;
    HtmlNode * pNode;
    HtmlCanvas *pCanvas;
    int iWidth;
    int iHeight;
    int iOffset;
{
    HtmlCanvas *pInlineCanvas;
    int ascent = -1 * iOffset;           /* Ascent of added box */
    int descent = iHeight + iOffset;     /* Descent of added box */

    CHECK_INTEGER_PLAUSIBILITY(ascent);
    CHECK_INTEGER_PLAUSIBILITY(descent);

    pInlineCanvas = inlineContextAddInlineCanvas(pContext, 1, pNode);
    DRAW_CANVAS(pInlineCanvas, pCanvas, 0, iOffset, pNode);
    inlineContextSetBoxDimensions(pContext, iWidth, ascent, descent, 0);
}

void 
HtmlInlineContextSetTextIndent(pContext, iTextIndent)
    InlineContext *pContext;
    int iTextIndent;
{
    pContext->iTextIndent = iTextIndent;
}

HtmlNode *HtmlInlineContextCreator(pContext)
    InlineContext *pContext;
{
    return pContext->pNode;
}
