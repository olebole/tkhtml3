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
 *
 *--------------------------------------------------------------------------
 * Copyright (c) 2005 Eolas Technologies Inc.
 * All rights reserved.
 *
 * This Open Source project was made possible through the financial support
 * of Eolas Technologies Inc.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <ORGANIZATION> nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
static const char rcsid[] = "$Id: htmllayout.c,v 1.136 2006/03/21 16:47:16 danielk1977 Exp $";

#include "htmllayout.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/*
 * Notes on layout-engine logging:
 */
#define LOG if (pLayout->pTree->options.logcmd && 0 == pLayout->minmaxTest)

/*
 * The code to lay out a "normal-flow" is located in this file:
 *
 *     + A new normal flow is established by:
 *         - the viewport (in the initial containing block)
 *         - a float
 *         - a table cell
 *
 *     + A flow contains a single block box.
 *
 *     + A flow has an associated float list (type HtmlFloatList*).
 *
 * Block box:
 *
 *     + A block box may contain either:
 *         - zero or more block boxes.
 *         - inline context.
 *         - replaced content
 *
 *     + A replaced content box is one of:
 *         - replaced node - i.e. a widget or image
 *         - a table
 *
 *     + A replaced content box is different from other block boxes in
 *       that it is placed around floats (other block boxes are placed under
 *       floats, the box content wraps around them instead).
 *
 * Table:
 *     + A table contains zero or more flows, arranged in a grid.  
 *     + Generation of tables requires knowing the "minimum" and "maximum"
 *       widths of the flow within each cell - important consideration for
 *       design of other layout code. 
 */


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
 */



typedef struct NormalFlow NormalFlow;
typedef struct NormalFlowCallback NormalFlowCallback;

/*
 * The iMaxMargin, iMinMargin and isValid variables are used to manage
 * collapsing vertical margins. Each margin that collapses at a given
 * point in the vertical flow is added to the structure using
 * normalFlowMarginAdd(). iMaxMargin stores the largest positive value seen 
 * (or zero) and iMinMargin stores the largest negative value seen (or zero).
 */
struct NormalFlow {
    int iMaxMargin;          /* Most positive margin value in pixels */
    int iMinMargin;          /* Most negative margin value in pixels */
    int isValid;             /* True if iMaxMargin and iMinMargin are valid */
    int nonegative;          /* Ignore any negative margins */
    NormalFlowCallback *pCallbackList;
    HtmlFloatList *pFloat;   /* Floating margins */
};

struct NormalFlowCallback {
    void (*xCallback)(NormalFlow *, NormalFlowCallback *, int);
    ClientData clientData;
    NormalFlowCallback *pNext;
};

#define CACHE_MINMAX_VALID 0x01
#define CACHE_LAYOUT_VALID 0x02
struct HtmlLayoutCache {
    unsigned char flags;     /* Combination of CACHE_XXX_VALID values */

    /* Cached return values for blockMinMaxWidth() */
    int iMinWidth;
    int iMaxWidth;

    /* Cached input values for normalFlowLayout() */
    NormalFlow normalFlowIn;
    int iContaining;
    int iFloatLeft;
    int iFloatRight;

    /* Cached output values for normalFlowLayout() */
    NormalFlow normalFlowOut;
    int iWidth;
    int iHeight;
    HtmlCanvas canvas;
};


/*
 * These are prototypes for all the static functions in this file. We
 * don't need most of them, but the help with error checking that normally
 * wouldn't happen because of the old-style function declarations. Also
 * they function as a table of contents for this file.
 */

static int markerLayout(LayoutContext*, BoxContext*, NormalFlow *, HtmlNode*, int);
static void layoutReplacement(LayoutContext*, BoxContext*, HtmlNode*);

static int inlineLayoutDrawLines
    (LayoutContext*,BoxContext*,InlineContext*,int,int*, NormalFlow*);

/* Values returned by normalFlowType() */
#define FLOWTYPE_NONE              1
#define FLOWTYPE_TEXT              2
#define FLOWTYPE_INLINE            3
#define FLOWTYPE_INLINE_REPLACED   4
#define FLOWTYPE_FLOAT             5
#define FLOWTYPE_BLOCK             6
#define FLOWTYPE_TABLE             7
#define FLOWTYPE_BLOCK_REPLACED    8
#define FLOWTYPE_LIST_ITEM         9
#define FLOWTYPE_BR                10

static int normalFlowType(HtmlNode *);
static int normalFlowLayout(LayoutContext*, BoxContext*, HtmlNode*,NormalFlow*);

static int normalFlowLayoutNode
(LayoutContext*, BoxContext*, HtmlNode*, int*, InlineContext*,NormalFlow*);

static int normalFlowLayoutFloat(
LayoutContext*, BoxContext*, HtmlNode*, int*, InlineContext*, NormalFlow*);
static int normalFlowLayoutBlock(
LayoutContext*, BoxContext*, HtmlNode*, int*, InlineContext*, NormalFlow*);
static int normalFlowLayoutReplaced(
LayoutContext*, BoxContext*, HtmlNode*, int*, InlineContext*, NormalFlow*);
static int normalFlowLayoutTable(
LayoutContext*, BoxContext*, HtmlNode*, int*, InlineContext*, NormalFlow*);

static void normalFlowCbAdd(NormalFlow *, NormalFlowCallback *);
static void normalFlowCbDelete(NormalFlow *, NormalFlowCallback *);
static void normalFlowMarginCollapse(NormalFlow *, int *);
static void normalFlowMarginAdd(NormalFlow *, int);

static void drawReplacement(LayoutContext*, BoxContext*, HtmlNode*);
static void drawBlock(LayoutContext*, BoxContext*, HtmlNode*, int,unsigned int);

/* 
 * Flags that can be passed to drawBlock()
 */
#define DRAWBLOCK_OMITBORDER    0x00000001
#define DRAWBLOCK_CONTENTWIDTH  0x00000002
#define DRAWBLOCK_ENFORCEWIDTH  0x00000004
#define DRAWBLOCK_ENFORCEHEIGHT 0x00000008

/*
 *---------------------------------------------------------------------------
 *
 * unsigned char DISPLAY(HtmlComputedValues *); --
 *
 *     Return the value of the computed 'display' property from the
 *     HtmlComputedValues structure passed as an argument. Or, if NULL is
 *     passed, return CSS_CONST_INLINE. This is so that the 'display' property
 *     of a text node can be requested, even though a text node does not have
 *     computed properties.
 *
 * Results:
 *     Value of 'display' property.
 *
 * Side effects:
 *     None.
 *---------------------------------------------------------------------------
 */
#define DISPLAY(pV) ((pV) ? (pV)->eDisplay : CSS_CONST_INLINE)

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetBoxProperties --
 *    
 *     Calculate and return the border and padding widths of a node as exact
 *     pixel values based on the width of the containing block (parameter
 *     iContaining) and the computed values of the following properties:
 *
 *         'padding'
 *         'border-width'
 *         'border-style'
 *
 *     Eight non-negative integer pixel values are returned, the top, right,
 *     bottom and left paddings and the top, right, bottom and left border
 *     widths (borders are always rendered as solid lines).
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     The eight calculated values are written into *pBoxProperties before
 *     returning.
 *
 *---------------------------------------------------------------------------
 */
void 
nodeGetBoxProperties(pLayout, pNode, iContaining, pBoxProperties)
    LayoutContext *pLayout;            /* Unused */
    HtmlNode *pNode;                   /* Node to calculate values for */
    int iContaining;                   /* Width of pNode's containing block */
    BoxProperties *pBoxProperties;     /* OUT: Write pixel values here */
{
    HtmlComputedValues *pV = pNode->pPropertyValues;


    /* Under some circumstance, a negative value may be passed for iContaining.
     * If this happens, use 0 as the containing width when calculating padding
     * widths with computed percentage values. Otherwise we will return a
     * negative padding width, which is illegal.
     *
     * Also, if we are running a min-max text, percentage widths are zero.
     */
    int c = (iContaining >= 0 ? iContaining : 0);
    if (pLayout->minmaxTest) {
        c = 0;
    }
    pBoxProperties->padding_top =    PIXELVAL(pV, PADDING_TOP, c);
    pBoxProperties->padding_right =  PIXELVAL(pV, PADDING_RIGHT, c);
    pBoxProperties->padding_bottom = PIXELVAL(pV, PADDING_BOTTOM, c);
    pBoxProperties->padding_left =   PIXELVAL(pV, PADDING_LEFT, c);

    /* For each border width, use the computed value if border-style is
     * something other than 'none', otherwise use 0. The PIXELVAL macro is not
     * used because 'border-width' properties may not be set to % values.
     */
    pBoxProperties->border_top = (
        (pV->eBorderTopStyle != CSS_CONST_NONE) ? pV->border.iTop : 0);
    pBoxProperties->border_right = (
        (pV->eBorderRightStyle != CSS_CONST_NONE) ? pV->border.iRight : 0);
    pBoxProperties->border_bottom = (
        (pV->eBorderBottomStyle != CSS_CONST_NONE) ? pV->border.iBottom : 0);
    pBoxProperties->border_left = (
        (pV->eBorderLeftStyle != CSS_CONST_NONE) ?  pV->border.iLeft : 0);

    assert(
        pBoxProperties->border_top >= 0 &&
        pBoxProperties->border_right >= 0 &&
        pBoxProperties->border_bottom >= 0 &&
        pBoxProperties->border_left >= 0 &&
        pBoxProperties->padding_top >= 0 &&
        pBoxProperties->padding_right >= 0 &&
        pBoxProperties->padding_bottom >= 0 &&
        pBoxProperties->padding_left >= 0
    );
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
void 
nodeGetMargins(pLayout, pNode, iContaining, pMargins)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    int iContaining;
    MarginProperties *pMargins;
{
    int iMarginTop;
    int iMarginRight;
    int iMarginBottom;
    int iMarginLeft;

    HtmlComputedValues *pV = pNode->pPropertyValues;
    assert(pV);

    /* Margin properties do not apply to table cells or rows. So return zero
     * for all. We have to do this sort of thing here in the layout engine, not
     * in the styler code, because a table-cell can still have a computed value
     * of a margin property (which may be inherited from), just not an actual
     * one.
     */
    if (
        pV->eDisplay == CSS_CONST_TABLE_CELL ||
        pV->eDisplay == CSS_CONST_TABLE_ROW
    ) {
       memset(pMargins, 0, sizeof(MarginProperties));
       return;
    }

    /* If we are running a min-max text, percentage widths are zero. */
    if (pLayout->minmaxTest) {
        iContaining = 0;
    }

    iMarginTop =    PIXELVAL(pV, MARGIN_TOP, iContaining);
    iMarginRight =  PIXELVAL(pV, MARGIN_RIGHT, iContaining);
    iMarginBottom = PIXELVAL(pV, MARGIN_BOTTOM, iContaining);
    iMarginLeft =   PIXELVAL(pV, MARGIN_LEFT, iContaining);

    pMargins->margin_top = ((iMarginTop > MAX_PIXELVAL)?iMarginTop:0);
    pMargins->margin_bottom = ((iMarginBottom > MAX_PIXELVAL)?iMarginBottom:0);
    pMargins->margin_left = ((iMarginLeft > MAX_PIXELVAL)?iMarginLeft:0);
    pMargins->margin_right = ((iMarginRight > MAX_PIXELVAL)?iMarginRight:0);

    pMargins->leftAuto = ((iMarginLeft == PIXELVAL_AUTO) ? 1 : 0);
    pMargins->rightAuto = ((iMarginRight == PIXELVAL_AUTO) ? 1 : 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowMarginCollapse --
 * normalFlowMarginQuery --
 * normalFlowMarginAdd --
 *
 *     The following two functions are used to manage collapsing vertical
 *     margins within a normal flow.
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
normalFlowMarginQuery(pNormal) 
    NormalFlow *pNormal;
{
    return pNormal->iMinMargin + pNormal->iMaxMargin;
}
static void 
normalFlowMarginCollapse(pNormal, pY) 
    NormalFlow *pNormal;
    int *pY;
{
    NormalFlowCallback *pCallback = pNormal->pCallbackList;
    int iMargin = pNormal->iMinMargin + pNormal->iMaxMargin;
    while (pCallback) {
        pCallback->xCallback(pNormal, pCallback, iMargin);
        pCallback = pCallback->pNext;
    }
    *pY += iMargin;

    assert(pNormal->isValid || (!pNormal->iMaxMargin && !pNormal->iMaxMargin));
    pNormal->isValid = 1;
    pNormal->iMaxMargin = 0;
    pNormal->iMinMargin = 0;
    pNormal->nonegative = 0;
}
static void 
normalFlowMarginAdd(pNormal, y) 
    NormalFlow *pNormal;
    int y;
{
    if (pNormal->isValid && (y >= 0 || !pNormal->nonegative)) {
        assert(pNormal->iMaxMargin >= 0);
        assert(pNormal->iMinMargin <= 0);
        pNormal->iMaxMargin = MAX(pNormal->iMaxMargin, y);
        pNormal->iMinMargin = MIN(pNormal->iMinMargin, y);
    }
}

static void 
normalFlowCbAdd(pNormal, pCallback)
    NormalFlow *pNormal;
    NormalFlowCallback *pCallback;
{
    pCallback->pNext = pNormal->pCallbackList;
    pNormal->pCallbackList = pCallback;
}
static void 
normalFlowCbDelete(pNormal, pCallback)
    NormalFlow *pNormal;
    NormalFlowCallback *pCallback;
{
    NormalFlowCallback *pCallbackList = pNormal->pCallbackList;
    if (pCallback == pCallbackList) {
        pNormal->pCallbackList = pCallback->pNext;
    } else {
        NormalFlowCallback *p;
        for (p = pCallbackList; p && p->pNext != pCallback; p = p->pNext);
        if (p) {
            assert(p->pNext && p->pNext == pCallback);
            p->pNext = p->pNext->pNext;
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeIsReplaced --
 *
 *     Return true if the node should be handled as a replaced node, 
 *     false otherwise. A node is handled as a replaced node if a 
 *     replacement widget has been supplied via [node replace], or
 *     if the custom -tkhtml-replacement-image property is defined.
 *
 * Results:
 *     1 or 0.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
nodeIsReplaced(pNode)
    HtmlNode *pNode;
{
    HtmlComputedValues *pComputed = pNode->pPropertyValues;
    return (
        pNode->pReplacement || 
        (pComputed && pComputed->imReplacementImage)
    ) ? 1 : 0;
}


/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutFloat --
 * 
 * Results:
 *     A Tcl return value - TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
normalFlowLayoutFloat(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;  /* Layout context */
    BoxContext *pBox;        /* Containing box context */
    HtmlNode *pNode;         /* Node that generates floating box */
    int *pY;                 /* IN/OUT: y-coord to draw float at */
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    HtmlComputedValues *pV = pNode->pPropertyValues;
    int isReplaced = nodeIsReplaced(pNode);
    int eFloat = pV->eFloat;
    int iContaining = pBox->iContaining;
    HtmlFloatList *pFloat = pNormal->pFloat;

    int iWidth;              /* Content width of floating box in pixels */
    int iTotalHeight;        /* Height of floating box (incl. margins) */
    int iTotalWidth;         /* Width of floating box (incl. margins) */
    int x, y;                /* Coords for content to be drawn */
    int iLeft;               /* Left floating margin where box is drawn */
    int iRight;              /* Right floating margin where box is drawn */
    int iTop;                /* Top of top margin of box */

    MarginProperties margin;          /* Margin properties of pNode */
    BoxContext sBox;   /* Box context for content to be drawn into */

    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = iContaining;

    y = (*pY) + (pNormal->iMaxMargin + pNormal->iMinMargin);

    nodeGetMargins(pLayout, pNode, iContaining, &margin);

    assert(DISPLAY(pV) == CSS_CONST_BLOCK || DISPLAY(pV) == CSS_CONST_TABLE);
    assert(DISPLAY(pV) == CSS_CONST_BLOCK || !isReplaced);
    assert(eFloat == CSS_CONST_LEFT || eFloat == CSS_CONST_RIGHT);

    /* Draw the floating element to sBox and set the value of iWidth to the
     * width to use for the element.  The procedure for determining the width
     * to use for the element is described in sections 10.3.5 (non-replaced)
     * and 10.3.6 (replaced) of the CSS 2.1 spec.
     */
    if (isReplaced) {
        drawReplacement(pLayout, &sBox, pNode);
    } else {
        unsigned int flags = DRAWBLOCK_ENFORCEWIDTH | DRAWBLOCK_ENFORCEHEIGHT;
        int c = pLayout->minmaxTest ? PIXELVAL_AUTO : iContaining;
        iWidth = PIXELVAL(pV, WIDTH, c);
        if (iWidth == PIXELVAL_AUTO) {
            int iMin; 
            int iMax;

            blockMinMaxWidth(pLayout, pNode, &iMin, &iMax);
            iWidth = MIN(MAX(iMin, iContaining), iMax);
        } else {
            flags |= DRAWBLOCK_CONTENTWIDTH;
        }
        drawBlock(pLayout, &sBox, pNode, iWidth, flags);
    }

    iTotalWidth = sBox.width;
    iTotalHeight = sBox.height + margin.margin_top + margin.margin_bottom;

    iLeft = 0;
    iRight = iContaining;

    iTop = y;
    iTop = HtmlFloatListPlace(
            pFloat, iContaining, iTotalWidth, iTotalHeight, iTop);
    HtmlFloatListMargins(pFloat, iTop, iTop+iTotalHeight, &iLeft, &iRight);

    y = iTop + margin.margin_top;
    if (eFloat == CSS_CONST_LEFT) {
        x = iLeft;
    } else {
        x = iRight - iTotalWidth;
    }
    DRAW_CANVAS(&pBox->vc, &sBox.vc, x, y, pNode);

    /* If the right-edge of this floating box exceeds the current actual
     * width of the box it is drawn in, set the actual width to the 
     * right edge. Floating boxes do not affect the height of the parent
     * box.
     */
    pBox->width = MAX(x + iTotalWidth, pBox->width);

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        HtmlFloatListLog(pTree, zNode, pNormal->pFloat);
        HtmlLog(pTree, "LAYOUTENGINE", "%s !!!!!!!!!!!!!!!", zNode);
    }

    /* Fix the float list in the parent block so that nothing overlaps
     * this floating box.
     */
    HtmlFloatListAdd(pNormal->pFloat, eFloat, 
        ((eFloat == CSS_CONST_LEFT) ? x + iTotalWidth : x),
        iTop, iTop + iTotalHeight);

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        HtmlLog(pTree, "LAYOUTENGINE", "%s (Float) %dx%d (%d,%d)", 
            zNode, iTotalWidth, iTotalHeight, x, iTop);
        HtmlLog(pTree, "LAYOUTENGINE", "%s !!!!!!!!!!!!!!!", zNode);
        HtmlFloatListLog(pTree, zNode, pNormal->pFloat);
    }


    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * getRomanIndex --
 *
 *     Print an ordered list index into the given buffer.  Use roman
 *     numerals.  For indices greater than a few thousand, revert to
 *     decimal.
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
getRomanIndex(zBuf, index, isUpper)
    char *zBuf;
    int index;
    int isUpper;
{
    int i = 0;
    int j;
    static struct {
        int value;
        char *name;
    } values[] = {
        { 1000, "m" },
        { 999, "im" },
        { 990, "xm" },
        { 900, "cm" },
        { 500, "d"  },
        { 499, "id" },
        { 490, "xd" },
        { 400, "cd" },
        { 100, "c"  },
        { 99, "ic"  },
        { 90, "xc"  },
        { 50, "l"   },
        { 49, "il"  },
        { 40, "xl"  },
        { 10, "x"   },
        { 9, "ix"   },
        { 5, "v"    },
        { 4, "iv"   },
        { 1, "i"    },
    };
    if (index<1 || index>=5000) {
        sprintf(zBuf, "%d", index);
        return;
    }
    for (j = 0; index > 0 && j < sizeof(values)/sizeof(values[0]); j++) {
        int k;
        while (index >= values[j].value) {
            for (k = 0; values[j].name[k]; k++) {
                zBuf[i++] = values[j].name[k];
            }
            index -= values[j].value;
        }
    }
    zBuf[i] = 0;
    if (isUpper) {
        for(i=0; zBuf[i]; i++){
            zBuf[i] += 'A' - 'a';
        }
    }
    strcat(zBuf,".");
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
markerLayout(pLayout, pBox, pNormal, pNode, y)
    LayoutContext *pLayout;
    BoxContext *pBox;
    NormalFlow *pNormal;
    HtmlNode *pNode;
    int y;
{
    CONST char *zMarker = 0; /* Text to draw in the marker box. */
    Tcl_Obj *pMarker;        /* Tcl_Obj copy of zMarker */
    HtmlComputedValues *pComputed = pNode->pPropertyValues;
    int yoffset = pComputed->fFont->metrics.ascent;
    int offset;
    HtmlCanvas sCanvas;
    int width;               /* Width of string zMarker in current font */

    memset(&sCanvas, 0, sizeof(HtmlCanvas));

    if (pComputed->imListStyleImage) {
        int iWidth = PIXELVAL_AUTO;
        int iHeight = PIXELVAL_AUTO;

        HtmlImageScale(pComputed->imListStyleImage, &iWidth, &iHeight, 0);
        offset = MAX(0, PIXELVAL(pComputed, MARGIN_LEFT, 0));
        offset -= (pComputed->fFont->ex_pixels + iWidth);

        if (pComputed->eListStylePosition == CSS_CONST_OUTSIDE) {
            HtmlDrawImage(&sCanvas, pComputed->imListStyleImage,
                offset, y,
                iWidth, iHeight, pNode, pLayout->minmaxTest
            );
        } else {
            HtmlDrawImage(&sCanvas, pComputed->imListStyleImage,
                0, y, 
                iWidth, iHeight, pNode, pLayout->minmaxTest
            );
            HtmlFloatListAdd(pNormal->pFloat, FLOAT_LEFT, iWidth, y, y+iHeight);
        }
    } else {
        Tk_Font font;
        int style;
        char zBuf[128];
        int iList = 1;

        HtmlNode *pParent = HtmlNodeParent(pNode);
        if (pParent) {
            int ii;
            for (ii = 0; ii < HtmlNodeNumChildren(pParent); ii++) {
                HtmlNode *pSibling = HtmlNodeChild(pParent, ii);
                if (pSibling == pNode) {
                    break;
                }
                if (DISPLAY(pSibling->pPropertyValues) == CSS_CONST_LIST_ITEM) {
                    iList++;
                }
            }
        }

        style = pComputed->eListStyleType;
        if (style == CSS_CONST_LOWER_ALPHA || style == CSS_CONST_UPPER_ALPHA) {
            if (iList > 26) {
                style = CSS_CONST_DECIMAL;
            }
        }
        switch (style) {
            case CSS_CONST_SQUARE:
                 zMarker = "\xe2\x96\xa1";      /* Unicode 0x25A1 */ 
                 break;
            case CSS_CONST_CIRCLE:
                 zMarker = "\xe2\x97\x8b";      /* Unicode 0x25CB */ 
                 break;
            case CSS_CONST_DISC:
                 zMarker = "\xe2\x80\xa2";      /* Unicode 0x25CF */ 
                 break;
    
            case CSS_CONST_LOWER_ALPHA:
                 sprintf(zBuf, "%c.", iList + 96);
                 zMarker = zBuf;
                 break;
            case CSS_CONST_UPPER_ALPHA:
                 sprintf(zBuf, "%c.", iList + 64);
                 zMarker = zBuf;
                 break;
    
            case CSS_CONST_LOWER_ROMAN:
                 getRomanIndex(zBuf, iList, 0);
                 zMarker = zBuf;
                 break;
            case CSS_CONST_UPPER_ROMAN:
                 getRomanIndex(zBuf, iList, 1);
                 zMarker = zBuf;
                 break;
            case CSS_CONST_DECIMAL:
                 sprintf(zBuf, "%d.", iList);
                 zMarker = zBuf;
                 break;
            case CSS_CONST_NONE:
                 zMarker = "";                  /* Nothin' */
                 break;
        }
        font = pComputed->fFont->tkfont;
        pMarker = Tcl_NewStringObj(zMarker, -1);
        Tcl_IncrRefCount(pMarker);
        width = Tk_TextWidth(font, zMarker, strlen(zMarker));
    
        if (pComputed->eListStylePosition == CSS_CONST_OUTSIDE) {
	    /* It's not specified in CSS 2.1 exactly where the list marker
	     * should be drawn when the 'list-style-position' property is
	     * 'outside'.  The algorithm used is to draw it the width of 1 'x'
	     * character in the current font to the left of the content box.
             */
            /* offset = pComputed->fFont->ex_pixels + width; */
            offset = MAX(0, PIXELVAL(pComputed, MARGIN_LEFT, 0));
            offset -= (pComputed->fFont->ex_pixels + width);
        } else {
            assert(pComputed->eListStylePosition == CSS_CONST_INSIDE);
            offset = MAX(0, PIXELVAL(pComputed, MARGIN_LEFT, 0));
            HtmlFloatListAdd(pNormal->pFloat, FLOAT_LEFT, 
                  pComputed->fFont->ex_pixels + width, y, y + yoffset);
        }

        HtmlDrawText(&sCanvas, pMarker, offset, y + yoffset, width,
                pLayout->minmaxTest, pNode, -1);
    
        Tcl_DecrRefCount(pMarker);
    }

    DRAW_CANVAS(&pBox->vc, &sCanvas, 0, 0, pNode);
    return TCL_OK;
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
inlineLayoutDrawLines(pLayout, pBox, pContext, forceflag, pY, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    InlineContext *pContext;
    int forceflag;               /* True to lay out final, incomplete line. */
    int *pY;                     /* IN/OUT: Y-coordinate in sBox.vc. */
    NormalFlow *pNormal;
{
    int have;
    do {
        HtmlCanvas lc;             /* Line-Canvas */
        int w;
        int forcebox;              /* Force at least one inline-box per line */
        int closeborders = 0;
        int f;
        int y = *pY;               /* Y coord for line-box baseline. */
        int leftFloat = 0;
        int rightFloat = pBox->iContaining;
        int nV = 0;                /* Vertical height of line. */
        int nA = 0;                /* Ascent of line box. */

	/* If the inline-context is not completely empty, we collapse any
	 * vertical margin here. Even though a line box may not be drawn by
	 * this routine, at least one will be drawn by this InlineContext
	 * eventually. Therefore it is safe to collapse the vertical margin.
         */
        if (!HtmlInlineContextIsEmpty(pContext)) {
            normalFlowMarginCollapse(pNormal, &y);
        }

        /* Todo: We need a real line-height here, not a hard-coded '10' */
        HtmlFloatListMargins(pNormal->pFloat, y, y+10, &leftFloat, &rightFloat);
        forcebox = (rightFloat==pBox->iContaining && leftFloat==0);

        memset(&lc, 0, sizeof(HtmlCanvas));
        w = rightFloat - leftFloat;
        f = (forcebox ? LINEBOX_FORCEBOX : 0) | 
            (forceflag ? LINEBOX_FORCELINE : 0) |
            (closeborders ? LINEBOX_CLOSEBORDERS : 0);
        have = HtmlInlineContextGetLineBox(pLayout, pContext, &w, f, &lc, &nV, &nA);

	if (have) {
            DRAW_CANVAS(&pBox->vc, &lc, leftFloat, y+nA, 0);
            y += nV;
            pBox->width = MAX(pBox->width, lc.right + leftFloat);
            pBox->height = MAX(pBox->height, y);
        } else if( w ) {
            /* If have==0 but w has been set to some non-zero value, then
             * there are inline-boxes in the inline-context, but there is
             * not enough space for the first inline-box in the width
             * provided. Increase the Y-coordinate and try the loop again.
             *
             * TODO: Pass the minimum height of the line-box to
             * HtmlFloatListPlace().
             */
            assert(!(f & LINEBOX_FORCEBOX));
            y = HtmlFloatListPlace(pNormal->pFloat, pBox->iContaining,w,10,y);
            have = 1;
        } 

	/* floatListClear(pBox->pFloats, y); */
        *pY = y;
    } while (have);

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowType --
 *
 *     Helper function for normalFlowLayoutNode().
 *
 *     Depending on various characteristics, a node to be laid out in a normal
 *     flow context is handled differently. Tkhtml treats the following cases
 *     distinctly:
 *
 *         1. Replaced inline                   -> FLOWTYPE_INLINE_REPLACED
 *         2. Text element                      -> FLOWTYPE_TEXT
 *         3. Non-replaced inline               -> FLOWTYPE_INLINE
 *         4. A floating box                    -> FLOWTYPE_FLOAT
 *         5. Replaced block element            -> FLOWTYPE_BLOCK_REPLACED
 *         6. Non-replaced block element        -> FLOWTYPE_BLOCK
 *         7. List item                         -> FLOWTYPE_LISTITEM
 *         8. Table                             -> FLOWTYPE_TABLE
 *         9. <br> element with "display:block" -> FLOWTYPE_BR
 *         10. Element with display "none"      -> FLOWTYPE_NONE
 *
 *     This function examines the node passed as an argument, determines the
 *     category into which the node falls and returns the corresponding
 *     hash-define constant.
 *
 * Results:
 *     One of the FLOWTYPE_XXX values.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
normalFlowType(pNode)
    HtmlNode *pNode;
{
    int eDisplay   = DISPLAY(pNode->pPropertyValues);
    int isReplaced = nodeIsReplaced(pNode);
    int isText     = HtmlNodeIsText(pNode);
    int eFloat     = isText ? CSS_CONST_NONE : pNode->pPropertyValues->eFloat;
    int eRet = FLOWTYPE_NONE;

    if (eDisplay == CSS_CONST_INLINE) {
        assert(eFloat == CSS_CONST_NONE);
        if (isReplaced) {
            eRet = FLOWTYPE_INLINE_REPLACED;
        }
        else if (isText) {
            eRet = FLOWTYPE_TEXT;
        }
        else {
            eRet = FLOWTYPE_INLINE;
        }
    }

    else if (eFloat != CSS_CONST_NONE) {
        eRet = FLOWTYPE_FLOAT;
    }

    else if (isReplaced) {
        eRet = FLOWTYPE_BLOCK_REPLACED;
    }

    else if (eDisplay == CSS_CONST_BLOCK) {
        if (HtmlNodeTagType(pNode) == Html_BR) {
            eRet = FLOWTYPE_BR;
        } else {
            eRet = FLOWTYPE_BLOCK;
        }
    }

    else if (eDisplay == CSS_CONST_LIST_ITEM) {
        eRet = FLOWTYPE_LIST_ITEM;
    }

    else if (eDisplay == CSS_CONST_TABLE) {
        eRet = FLOWTYPE_TABLE;
    }

    return eRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * drawBlock --
 *
 *     pNode must be a non-replaced element with display 'block' or 'table'.
 *     This function is used to render such an element that will be laid out
 *     independant of a normal flow context (creating it's own context). For
 *     example if pNode:
 *
 *         + is a floating box
 *         + is the root node of a document
 *         + is a table cell
 *         + is a table
 *
 *     The node content, background and borders are drawn into argument pBox.
 *     The pBox coordinates (0, 0) correspond to the outside edge of the left
 *     margin and the upper edge of the top border. That is, horizontal margins
 *     are included but vertical are not.
 *
 *     When this function returns, pBox->width is set to the width between
 *     outer edges of the right and left margins. pBox->height is set to the
 *     distance between the outside edges of the top and bottom borders. Again,
 *     horizontal margins are included but vertical are not.
 *
 *     Box width:
 *
 *         * The "containing-width", used to calculate percentage lengths etc.,
 *           is passed in pBox->iContaining
 *         * iAvailable is the desired width between the outside and inside
 *           margins. The actual width of the returned rendering might be
 *           different from this.
 *
 *     Flags:
 *
 *         Argument flags may be any combination of the following bitmasks:
 *
 *             DRAWBLOCK_OMITBORDER    
 *                 Do not actually draw the border & background (but still
 *                 allocate space for them).
 *             DRAWBLOCK_CONTENTWIDTH 
 *                 Parameter iAvailable refers to the content-width of the
 *                 block, not the width between the inside and outside margins. 
 *             DRAWBLOCK_ENFORCEWIDTH
 *             DRAWBLOCK_ENFORCEHEIGHT
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
drawBlock(pLayout, pBox, pNode, iAvailable, flags)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int iAvailable;
    unsigned int flags;        /* Combination of DRAW_XXX flags */
{
    HtmlComputedValues *pV = pNode->pPropertyValues;
    BoxContext sBox;
    BoxProperties box;
    MarginProperties margin;

    NormalFlow sNormal;
    memset(&sNormal, 0, sizeof(NormalFlow));

    int yBorderBottom; /* Y-coordinate for bottom of block border */
    int xBorderRight;  /* X-coordinate for right of block border */
    int xContent;      /* X-coordinate for content */
    int yContent;      /* Y-coordinate for content */

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        HtmlLog(pTree, "LAYOUTENGINE", "%s drawBlock()", zNode);
    }

    nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);
    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);

    memset(&sBox, 0, sizeof(BoxContext));   

    sBox.iContaining = iAvailable;
    if (!(flags & DRAWBLOCK_CONTENTWIDTH)) {
        sBox.iContaining -=
            margin.margin_left + box.border_left + box.padding_left +
            margin.margin_right + box.border_right + box.padding_right;
    } 
    if (flags & DRAWBLOCK_ENFORCEWIDTH) {
        sBox.width = sBox.iContaining;
    }
    sNormal.pFloat = HtmlFloatListNew();

    switch (DISPLAY(pV)) {
        case CSS_CONST_INLINE:
        case CSS_CONST_BLOCK:
        case CSS_CONST_TABLE_CELL: {
            if (pNode == pLayout->pTop) {
                int tmp = 0;
                normalFlowMarginCollapse(&sNormal, &tmp);
            }
            normalFlowLayout(pLayout, &sBox, pNode, &sNormal);
            break;
        }
        case CSS_CONST_TABLE:
            HtmlTableLayout(pLayout, &sBox, pNode);
            break;
        default:
            assert(!"Bad display value in drawBlock()");
    }

    if (flags & DRAWBLOCK_ENFORCEHEIGHT) {
	/* Note: This is a bug (calculating percentage height relative to the
         * *width* of the containing block. But it could be worse...
         */
        int iHeight = PIXELVAL(pV, HEIGHT, pBox->iContaining);
        sBox.height = MAX(sBox.height, iHeight);
    }

    xContent = margin.margin_left + box.border_left + box.padding_left;
    yContent = box.border_top + box.padding_top; 
    yBorderBottom = yContent+sBox.height+box.border_bottom+box.padding_bottom;
    xBorderRight = xContent + sBox.width + box.border_right + box.padding_right;

    if (!(flags & DRAWBLOCK_OMITBORDER)) {
        int l = margin.margin_left;
        borderLayout(pLayout, pNode, pBox, l, 0, xBorderRight, yBorderBottom);
    }
    DRAW_CANVAS(&pBox->vc, &sBox.vc, xContent, yContent, pNode);

    pBox->height = MAX(pBox->height, yBorderBottom);
    pBox->height = HtmlFloatListClear(sNormal.pFloat,CSS_CONST_BOTH,pBox->height);
    pBox->width = MAX(pBox->width, xBorderRight + margin.margin_right);

    if (pNode == pLayout->pTop) {
        normalFlowMarginCollapse(&sNormal, &pBox->height);
    }

    HtmlFloatListDelete(sNormal.pFloat);
}

int 
HtmlLayoutTableCell(pLayout, pBox, pNode, iAvailable)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int iAvailable;
{
    unsigned int flags = (DRAWBLOCK_OMITBORDER|DRAWBLOCK_ENFORCEHEIGHT);
    drawBlock(pLayout, pBox, pNode, iAvailable, flags);
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * drawReplacement --
 *
 *     pNode must be a replaced element. 
 *
 *     The node content, background and borders are drawn into argument pBox.
 *     The pBox coordinates (0, 0) correspond to the outside edge of the left
 *     margin and the upper edge of the top border. That is, horizontal margins
 *     are included but vertical are not.
 *
 *     When this function returns, pBox->width is set to the width between
 *     outer edges of the right and left margins. pBox->height is set to the
 *     distance between the outside edges of the top and bottom borders. Again,
 *     horizontal margins are included but vertical are not.
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
drawReplacement(pLayout, pBox, pNode)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
{
    BoxProperties box;                /* Box properties of pNode */
    MarginProperties margin;          /* Margin properties of pNode */
    BoxContext sBox;

    int yBorderBottom; /* Y-coordinate for bottom of block border */
    int xBorderLeft;   /* X-coordinate for left of block border */
    int xBorderRight;  /* X-coordinate for right of block border */
    int x, y;          /* Coords for content to be drawn */

    nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);
    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);

    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = pBox->iContaining;
    layoutReplacement(pLayout, &sBox, pNode);

    xBorderLeft = margin.margin_left;
    x = xBorderLeft + box.border_left + box.padding_left;
    xBorderRight = x + sBox.width + box.padding_right + box.border_right;
    y = box.border_top + box.padding_top;
    yBorderBottom = y + sBox.height + box.padding_bottom + box.border_bottom;

    borderLayout(pLayout, pNode, pBox, 
        xBorderLeft, 0, xBorderRight, yBorderBottom);
    DRAW_CANVAS(&pBox->vc, &sBox.vc, x, y, pNode);
    pBox->height = yBorderBottom;
    pBox->width = xBorderRight + margin.margin_right;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutTable --
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
normalFlowLayoutTable(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    int iMinWidth;                  /* Minimum from blockMinMaxWidth */
    int iMaxWidth;
    int iLeftFloat = 0;
    int iRightFloat = pBox->iContaining;
    int iAvailable;

    int iWidth;
    unsigned int flags;
    int eAlign = CSS_CONST_LEFT;

    int x, y;                     /* Coords for content to be drawn */
    BoxContext sBox;              /* Box context for content to be drawn into */
    MarginProperties margin;      /* Margin properties of pNode */

    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);

    /* Account for the 'margin-top' property of this node. The margin always
     * collapses for a table element.
     */
    normalFlowMarginAdd(pNormal, margin.margin_top);
    normalFlowMarginCollapse(pNormal, pY);

    /* Note: Passing 10000 as the required height means in some (fairly
     * unlikely circumstances the table will be placed lower in the flow than
     * would have been necessary. But it's not that big of a deal.
     */
    blockMinMaxWidth(pLayout, pNode, &iMinWidth, &iMaxWidth);
    *pY = HtmlFloatListPlace(
        pNormal->pFloat, pBox->iContaining, iMinWidth, 10000, *pY);
    HtmlFloatListMargins(
            pNormal->pFloat, *pY, *pY + 10000, &iLeftFloat, &iRightFloat);
    iAvailable = iRightFloat - iLeftFloat;

    iWidth = PIXELVAL(
        pNode->pPropertyValues, WIDTH, 
        pLayout->minmaxTest ? PIXELVAL_AUTO : iAvailable
    );
    if (iWidth == PIXELVAL_AUTO) {
        iWidth = iAvailable;
        flags = 0;
    } else {
        flags = DRAWBLOCK_ENFORCEWIDTH;
    }

    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = pBox->iContaining;
    drawBlock(pLayout, &sBox, pNode, iWidth, flags);

    y = HtmlFloatListPlace(
            pNormal->pFloat, pBox->iContaining, sBox.width, sBox.height, *pY);
    *pY = y + sBox.height;
 
    if (pLayout->minmaxTest) {
        /* If this is a min-max size test, leave the table left-aligned. */
        assert(eAlign == CSS_CONST_LEFT);
    } else if (margin.leftAuto && margin.rightAuto) {
        eAlign = CSS_CONST_CENTER;
    } else if (margin.leftAuto) {
        eAlign = CSS_CONST_RIGHT;
    } else {
	/* In this case the box is over-constrained. So we'll respect the
	 * text-align option of the parent node to select an alignment. 
         *
	 * I can't find anything to justify this in the specification, but
	 * http://www.google.com has code like this:
         *
         *       <body>
         *         <center>
         *           <form>
         *             <table>
         *
         * where the table is supposed to be centered within the body block.
         * Todo: Find out more about this. Later: Also http://www.yahoo.com. 
         * This use of the <center> tag seems pretty common.
         */
        int eTextAlign = CSS_CONST_LEFT;
        HtmlNode *pParent = HtmlNodeParent(pNode);
        if (pParent) {
            eTextAlign = pParent->pPropertyValues->eTextAlign;
        }
        switch (eTextAlign) {
            case CSS_CONST_RIGHT:
                eAlign = CSS_CONST_RIGHT;
                break;
            case CSS_CONST_CENTER:
            case CSS_CONST_JUSTIFY:
                eAlign = CSS_CONST_CENTER;
                break;
            case CSS_CONST_LEFT:
            default:
                eAlign = CSS_CONST_LEFT;
                break;
        }
    }
    switch (eAlign) {
        case CSS_CONST_RIGHT:
            x = iRightFloat - sBox.width;
            break;
        case CSS_CONST_CENTER: {
            int iSpare = (iRightFloat - iLeftFloat - sBox.width);
            x = iLeftFloat + (iSpare / 2);
            break;
        }
        case CSS_CONST_LEFT:
            x = iLeftFloat;
            break;
        default: assert(!"Impossible");
    }

    DRAW_CANVAS(&pBox->vc, &sBox.vc, x, y, pNode);
    pBox->height = MAX(pBox->height, *pY);
    pBox->width = MAX(pBox->width, x + sBox.width);

    /* Account for the 'margin-bottom' property of this node. */
    normalFlowMarginAdd(pNormal, margin.margin_bottom);

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutReplaced --
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
normalFlowLayoutReplaced(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    MarginProperties margin;          /* Margin properties of pNode */

    int x;             /* X-coord for content to be drawn */
    BoxContext sBox;   /* Box context for replacement to be drawn into */

    int iLeftFloat = 0;
    int iRightFloat = pBox->iContaining;

    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);

    /* First lay out the content of the element into sBox. Then figure out
     * where to put it in the parent box. 
     */
    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = pBox->iContaining;
    drawReplacement(pLayout, &sBox, pNode);

    /* Account for the 'margin-top' property of this node. The margin always
     * collapses for a replaced block node.
     */
    normalFlowMarginAdd(pNormal, margin.margin_top);
    normalFlowMarginCollapse(pNormal, pY);

    *pY = HtmlFloatListPlace(
	pNormal->pFloat, pBox->iContaining, sBox.width, sBox.height, *pY);
    HtmlFloatListMargins(
        pNormal->pFloat, *pY, *pY + sBox.height, &iLeftFloat, &iRightFloat);

    if (margin.leftAuto && margin.rightAuto) {
        x = (iRightFloat - iLeftFloat - sBox.width) / 2;
    } else if (margin.leftAuto) {
        x = (iRightFloat - sBox.width);
    } else {
        x = iLeftFloat;
    }

    DRAW_CANVAS(&pBox->vc, &sBox.vc, x, *pY, pNode);
    *pY += sBox.height;
    pBox->height = MAX(pBox->height, *pY);
    pBox->width = MAX(pBox->width, sBox.width);

    /* Account for the 'margin-bottom' property of this node. */
    normalFlowMarginAdd(pNormal, margin.margin_bottom);

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutBlock --
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
normalFlowLayoutBlock(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    HtmlComputedValues *pV = pNode->pPropertyValues;

    BoxProperties box;                /* Box properties of pNode */
    MarginProperties margin;          /* Margin properties of pNode */
    int iWidth;                       /* Content width of pNode in pixels */

    int yBorderTop;    /* Y-coordinate for top of block border */
    int yBorderBottom; /* Y-coordinate for bottom of block border */
    int xBorderLeft;   /* X-coordinate for left of block border */
    int xBorderRight;  /* X-coordinate for right of block border */
    int x, y;          /* Coords for content to be drawn */
    BoxContext sBox;   /* Box context for content to be drawn into */

    memset(&sBox, 0, sizeof(BoxContext));

    nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);
    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);

    /* Calculate iWidth, xBorderLeft, xBorderRight and yBorderTop. All are
     * interpreted as pixel values. For a non-replaced block element, the width
     * is always as calculated here, even if the content is not as wide.
     */
    iWidth = PIXELVAL(
        pV, WIDTH, pLayout->minmaxTest ? PIXELVAL_AUTO : pBox->iContaining
    );
    if (iWidth == PIXELVAL_AUTO) {
        xBorderLeft = margin.margin_left;
        iWidth = pBox->iContaining -
            margin.margin_left - box.border_left - box.padding_left -
            margin.margin_right - box.border_right - box.padding_right;
    } else {
        int iSpareWidth = pBox->iContaining - iWidth - 
            margin.margin_left - box.border_left - box.padding_left -
            margin.margin_right - box.border_right - box.padding_right;
        if (margin.leftAuto & margin.rightAuto) {
            xBorderLeft = iSpareWidth / 2;
        }
        else if (margin.leftAuto) {
            xBorderLeft = iSpareWidth;
        } else {
            xBorderLeft = margin.margin_left;
        }
        sBox.width = iWidth;
    }
    xBorderRight = xBorderLeft + iWidth + 
        box.border_left + box.padding_left +
        box.border_right + box.padding_right;

    /* Account for the 'margin-top' property of this node. */
    normalFlowMarginAdd(pNormal, margin.margin_top);

    /* If this box has either top-padding or a top border, then collapse the
     * vertical margin between this block and the one above now.
     */
    if (box.padding_top > 0 || box.border_top > 0) {
        normalFlowMarginCollapse(pNormal, pY);
    } 

    yBorderTop = *pY + normalFlowMarginQuery(pNormal);

    /* Calculate x and y as pixel values. */
    *pY += box.border_top + box.padding_top;
    y = *pY;
    x = xBorderLeft + box.border_left + box.padding_left;

    /* Set up the box-context used to draw the content. */
    /* sBox.pFloat = pBox->pFloat; */
    HtmlFloatListNormalize(pNormal->pFloat, -1 * x, -1 * y);
    sBox.iContaining = iWidth;

    /* Layout the box content and copy it into the parent box-context. */
    normalFlowLayout(pLayout, &sBox, pNode, pNormal);
    *pY += sBox.height;

    /* Re-normalize the float-list. */
    HtmlFloatListNormalize(pNormal->pFloat, x, y);

    if (box.padding_bottom > 0 || box.border_bottom > 0) {
        normalFlowMarginCollapse(pNormal, pY);
    } 
    *pY += box.padding_bottom + box.border_bottom;
    yBorderBottom = *pY;

    borderLayout(pLayout, pNode, pBox, 
        xBorderLeft, yBorderTop, xBorderRight, yBorderBottom);

    DRAW_CANVAS(&pBox->vc, &sBox.vc, x, y, pNode);
    pBox->width = MAX(pBox->width, sBox.width + 
       margin.margin_left + box.border_left + box.padding_left +
       margin.margin_right + box.border_right + box.padding_right);
    pBox->height = MAX(pBox->height, *pY);

    /* Account for the 'margin-bottom' property of this node. */
    normalFlowMarginAdd(pNormal, margin.margin_bottom);

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowClearFloat --
 *
 *     This is called when a node with the 'clear' property set is 
 *     encountered in the normal flow.
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
normalFlowClearFloat(pBox, pNode, pNormal, y)
    BoxContext *pBox;
    HtmlNode *pNode;
    NormalFlow *pNormal;
    int y;
{
    int eClear = pNode->pPropertyValues->eClear;
    int ynew = y;
    if (eClear != CSS_CONST_NONE) {
        ynew = HtmlFloatListClear(pNormal->pFloat, eClear, ynew);
        int ydiff = ynew - y;
        assert(ydiff >= 0);
        pNormal->iMaxMargin = MAX(pNormal->iMaxMargin, ydiff);
        pNormal->iMinMargin = -1 * ydiff;
        pNormal->nonegative = 1;
        pBox->height = MAX(ynew, pBox->height);
    }
    return ynew;
}

typedef struct MarkerLayoutArgs MarkerLayoutArgs;
struct MarkerLayoutArgs {
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int y;
};
static void
markerLayoutCallback(pNormal, pCallback, iMargin)
    NormalFlow *pNormal;
    NormalFlowCallback *pCallback;
    int iMargin;
{
    MarkerLayoutArgs *pArgs = (MarkerLayoutArgs *)pCallback->clientData;
    markerLayout(pArgs->pLayout, pArgs->pBox, pNormal, pArgs->pNode, pArgs->y + iMargin);
    normalFlowCbDelete(pNormal, pCallback);
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutNode --
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
normalFlowLayoutNode(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    HtmlComputedValues *pV = pNode->pPropertyValues;
    int rc = 0;

    int eFlowType = normalFlowType(pNode);

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        const char *zType;
        switch (eFlowType) {
            case FLOWTYPE_NONE: zType = "NONE"; break;
            case FLOWTYPE_TEXT: zType = "TEXT"; break;
            case FLOWTYPE_INLINE: zType = "INLINE"; break;
            case FLOWTYPE_INLINE_REPLACED: zType = "INLINE_REPLACED"; break;
            case FLOWTYPE_FLOAT: zType = "FLOAT"; break;
            case FLOWTYPE_BLOCK: zType = "BLOCK"; break;
            case FLOWTYPE_TABLE: zType = "TABLE"; break;
            case FLOWTYPE_BLOCK_REPLACED: zType = "BLOCK_REPLACED"; break;
            case FLOWTYPE_LIST_ITEM: zType = "ITEM"; break;
            case FLOWTYPE_BR: zType = "BR"; break;
            default: zType = "N/A (this is a problem)";
        }
        HtmlLog(pTree, "LAYOUTENGINE", "%s normalFlowType() -> %s",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)), zType
        );
    }


    switch (eFlowType) {

        /* A text node */
        case FLOWTYPE_TEXT:
            rc = HtmlInlineContextAddText(pContext, pNode);
            break;

        /* A non-replaced block box.  */
	case FLOWTYPE_BLOCK: {
            inlineLayoutDrawLines(pLayout, pBox, pContext, 1, pY, pNormal);
            *pY = normalFlowClearFloat(pBox, pNode, pNormal, *pY);
            normalFlowLayoutBlock(pLayout, pBox, pNode, pY, pContext, pNormal);
            break;
        }

        /* A list item.  */
	case FLOWTYPE_LIST_ITEM: {
            MarkerLayoutArgs sArgs;
            NormalFlowCallback sCallback;

            inlineLayoutDrawLines(pLayout, pBox, pContext, 1, pY, pNormal);

            sCallback.xCallback = markerLayoutCallback;
            sCallback.clientData = (ClientData) &sArgs;
            sCallback.pNext = 0;
            sArgs.pLayout = pLayout;
            sArgs.pBox = pBox;
            sArgs.pNode = pNode;
            sArgs.y = *pY;
           
            *pY = normalFlowClearFloat(pBox, pNode, pNormal, *pY);
            normalFlowCbAdd(pNormal, &sCallback);
            normalFlowLayoutBlock(pLayout, pBox, pNode, pY, pContext, pNormal);
            normalFlowCbDelete(pNormal, &sCallback);
            break;
        }

        /* A <br> tag. */
	case FLOWTYPE_BR: {
	    /* Handling the <br> tag is currently a hack. In the default CSS
             * file for HTML, we have:
             *
             *     BR {
             *         display: block;
             *     }
             */
            if (HtmlInlineContextIsEmpty(pContext)) {
                *pY += pV->fFont->em_pixels;
            }
            inlineLayoutDrawLines(pLayout, pBox, pContext, 1, pY, pNormal);
            *pY = normalFlowClearFloat(pBox, pNode, pNormal, *pY);
            normalFlowLayoutBlock(pLayout, pBox, pNode, pY, pContext, pNormal);
            break;
        }

        /* A floating box. */
        case FLOWTYPE_FLOAT: {
            int y = *pY;
            *pY = normalFlowClearFloat(pBox, pNode, pNormal, *pY);
            normalFlowLayoutFloat(pLayout, pBox, pNode, &y, pContext, pNormal);
            break;
        }

        /* A replaced block element. */
        case FLOWTYPE_BLOCK_REPLACED: {
            inlineLayoutDrawLines(pLayout, pBox, pContext, 1, pY, pNormal);
            *pY = normalFlowClearFloat(pBox, pNode, pNormal, *pY);
            normalFlowLayoutReplaced(pLayout,pBox,pNode,pY,pContext,pNormal);
            break;
        }

        /* A replaced inline element. */
        case FLOWTYPE_INLINE_REPLACED: {
            BoxContext sBox;
            BoxContext sBox2;
            int yoffset;
            int iHeight;
            MarginProperties margin;

            memset(&sBox, 0, sizeof(BoxContext));
            sBox.iContaining = pBox->iContaining;
            drawReplacement(pLayout, &sBox, pNode);

            nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);
            iHeight = sBox.height + margin.margin_top + margin.margin_bottom;

#if 0
            switch (pV->eVerticalAlign) {
                case CSS_CONST_TEXT_BOTTOM: {
                    yoffset = -1 * (iHeight - pV->fFont->metrics.descent);
                    break;
                }
                case CSS_CONST_TEXT_TOP: {
                    yoffset = -1 * pV->fFont->metrics.ascent;
                    break;
                }
                case CSS_CONST_MIDDLE: {
                    yoffset = -1 * (iHeight+pV->fFont->em_pixels ) / 2;
                    break;
                }
                case CSS_CONST_BASELINE:
                default:
                    yoffset = -1 * (iHeight - margin.margin_top);
                    break;
            }
#endif

            yoffset = -1 * (iHeight - margin.margin_bottom);
            if (pNode->pReplacement) {
              yoffset += pNode->pReplacement->iOffset;
            }

            memset(&sBox2, 0, sizeof(BoxContext));
            DRAW_CANVAS(&sBox2.vc, &sBox.vc, 0, margin.margin_top, pNode);
	    HtmlInlineContextAddBox(pContext, pNode, &sBox2.vc,
                sBox.width, iHeight, yoffset);
            break;
        }

        /* An inline element. Recurse through the child nodes.  */
        case FLOWTYPE_INLINE: {
            int i;
            InlineBorder *pBorder;
            pBorder = HtmlGetInlineBorder(pLayout, pNode, 0);
            HtmlInlineContextPushBorder(pContext, pBorder);
            for(i=0; i<HtmlNodeNumChildren(pNode) && 0==rc; i++) {
                HtmlNode *pChild = HtmlNodeChild(pNode, i);
                rc = normalFlowLayoutNode(pLayout, pBox, pChild, pY, pContext,
                    pNormal);
            }
            HtmlInlineContextPopBorder(pContext, pBorder);
            break;
        }

	/* An element with "display: none". 
         *
	 * TODO: The CSS 2.1 spec says, in section 9.2.4, that an element with
	 * display 'none' has no effect on layout at all. But rendering of
	 * http://slashdot.org depends on honouring the 'clear' property on an
	 * element with display 'none'. And Mozilla, KHTML and Opera do so.
	 * Find out about this and if there are any other properties that need
	 * handling here.
         */
	case FLOWTYPE_NONE:
            *pY = normalFlowClearFloat(pBox, pNode, pNormal, *pY);
            break;

        case FLOWTYPE_TABLE: {
            inlineLayoutDrawLines(pLayout, pBox, pContext, 1, pY, pNormal);
            *pY = normalFlowClearFloat(pBox, pNode, pNormal, *pY);
            normalFlowLayoutTable(pLayout, pBox, pNode, pY, pContext, pNormal);
            break;
        }

        default:
            assert(!"Illegal return from normalFlowType()");
    }

    /* See if there are any complete line-boxes to copy to the main canvas. 
     */
    if(rc == 0) {
        rc = inlineLayoutDrawLines(pLayout, pBox, pContext, 0, pY, pNormal);
    }

    return rc;
}

static int 
nodeGetHeight(pNode)
    HtmlNode *pNode;
{
    int iRet = 0;
    if (pNode->pPropertyValues) {
        iRet = PIXELVAL(pNode->pPropertyValues, HEIGHT, 0);
    }
    return iRet;
}


/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayout --
 *
 *     This function is called in two circumstances:
 *
 *         1. When pNode creates a new normal flow context, and 
 *         2. When pNode is a non-replaced, non-floating block box in a
 *            normal flow (this function is called recursively in that case).
 *
 *     In either case, the content of pNode is drawn to pBox->vc with the
 *     top-left hand corner at canvas coordinates (0, 0). Therefore, for pNode,
 *     it is always the callers responsibility to deal with margins, border
 *     padding and background. The exception to this rule is collapsing
 *     vertical margins (see below).
 *
 *     Collapsing Margins:
 *
 *         Within a normal flow context, adjoining vertical margins collapse.
 *         This sounds simple, but has a major influence on the code design.
 *
 *         As stated above, it is the callers responsibility to deal with
 *         'margin-top' and 'margin-bottom' for pNode. But the top margin of
 *         the first child of pNode may collapse with other margins. The
 *         following NormalFlow variables are used to manage collapsing
 *         margins:
 *
 *             int iMaxMargin;
 *             int iMinMargin;
 *             int isValid;
 *
 *         When this function is called to create a new normal flow context,
 *         all these variables are set to 0. 
 *
 *         An element collapses the margins above it if:
 *
 *             * It has top-padding, or
 *             * It has a top border, or
 *             * It's a text, table or replaced node
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
normalFlowLayout(pLayout, pBox, pNode, pNormal)
    LayoutContext *pLayout;       /* Layout context */
    BoxContext *pBox;             /* Box context to draw to */
    HtmlNode *pNode;              /* Node to start drawing at */
    NormalFlow *pNormal;
{
    InlineContext *pContext;
    int y = 0;
    int rc = 0;                       /* Return Code */
    InlineBorder *pBorder;
    int ii;
    HtmlLayoutCache *pCache = pNode->pLayoutCache;
    HtmlFloatList *pFloat = pNormal->pFloat;

    int left = 0; 
    int right = pBox->iContaining;
    int overhang;

    assert( 
        DISPLAY(pNode->pPropertyValues) == CSS_CONST_BLOCK ||
        DISPLAY(pNode->pPropertyValues) == CSS_CONST_TABLE_CELL ||
        DISPLAY(pNode->pPropertyValues) == CSS_CONST_LIST_ITEM ||
 
        /* TODO: Should this case really be here? */
        DISPLAY(pNode->pPropertyValues) == CSS_CONST_INLINE
    );
    assert(!nodeIsReplaced(pNode));

    pNormal->isValid = (pNormal->isValid ? 1 : 0);

    /* Figure out if a cached layout can be used. The more often a cached 
     * layout can be used, the more efficient reflow (and incremental layout
     * will be).
     */
    HtmlFloatListMargins(pFloat, 0, 1, &left, &right);
    if (
        pLayout->pTree->options.layoutcache && 
        !pLayout->minmaxTest && 
        pCache && (pCache->flags & CACHE_LAYOUT_VALID) &&
        pNormal->isValid    == pCache->normalFlowIn.isValid &&
        pNormal->iMinMargin == pCache->normalFlowIn.iMinMargin &&
        pNormal->iMaxMargin == pCache->normalFlowIn.iMaxMargin &&
        pBox->iContaining == pCache->iContaining &&
        left == pCache->iFloatLeft && right == pCache->iFloatRight &&
        HtmlFloatListIsConstant(pFloat, 0, pCache->iHeight)
    ) {
        /* In this case we can use the cached layout. */
        assert(!pBox->vc.pFirst);
        assert(!pLayout->minmaxTest);
        HtmlDrawCopyCanvas(&pBox->vc, &pCache->canvas);
        pBox->width = pCache->iWidth;
        pBox->height = pCache->iHeight;
        pNormal->iMaxMargin = pCache->normalFlowOut.iMaxMargin;
        pNormal->iMinMargin = pCache->normalFlowOut.iMinMargin;
        pNormal->isValid = pCache->normalFlowOut.isValid;
        return rc;
    }
    if (!pCache) {
        pCache = (HtmlLayoutCache *)HtmlClearAlloc(sizeof(HtmlLayoutCache));
        pNode->pLayoutCache = pCache;
    }
    HtmlDrawCleanup(&pCache->canvas);
    pCache->flags &= ~(CACHE_LAYOUT_VALID);
    pCache->normalFlowIn.iMaxMargin = pNormal->iMaxMargin;
    pCache->normalFlowIn.iMinMargin = pNormal->iMinMargin;
    pCache->normalFlowIn.isValid = pNormal->isValid;
    pCache->iContaining = pBox->iContaining;
    pCache->iFloatLeft = left;
    pCache->iFloatRight = right;

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        const char *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        HtmlFloatListLog(pTree, zNode, pFloat);
    }

    /* Check for 'height' and 'width' attributes. */
    pBox->height = MAX(pBox->height, nodeGetHeight(pNode));

    /* Create the InlineContext object for this containing box */
    pContext = HtmlInlineContextNew(pNode, pLayout->minmaxTest);

    /* Add any inline-border created by the node that generated this
     * normal-flow to the InlineContext. Actual border attributes do not apply
     * in this case, but the 'text-decoration' attribute may.
     */
    pBorder = HtmlGetInlineBorder(pLayout, pNode, 1);
    HtmlInlineContextPushBorder(pContext, pBorder);

    /* Layout each of the child nodes into BoxContext. */
    for(ii = 0; ii < HtmlNodeNumChildren(pNode) ; ii++) {
        HtmlNode *p = HtmlNodeChild(pNode, ii);
        normalFlowLayoutNode(pLayout, pBox, p, &y, pContext, pNormal);
    }

    /* Finish the inline-border started by the parent, if any. */
    HtmlInlineContextPopBorder(pContext, pBorder);

    rc = inlineLayoutDrawLines(pLayout, pBox, pContext, 1, &y, pNormal);
    HtmlInlineContextCleanup(pContext);

    left = 0;
    right = pBox->iContaining;
    assert(pNode->pLayoutCache == pCache);
    assert(pBox->iContaining == pCache->iContaining);
    HtmlFloatListMargins(pFloat, pBox->height-1, pBox->height, &left, &right);
    overhang = MAX(0, pBox->vc.bottom - pBox->height);
    if (
        pLayout->pTree->options.layoutcache && 
        !pLayout->minmaxTest && 
        pCache->iFloatLeft == left &&
        pCache->iFloatRight == right &&
        HtmlFloatListIsConstant(pFloat, pBox->height, overhang)
    ) {
        HtmlDrawOrigin(&pBox->vc);
        HtmlDrawCopyCanvas(&pCache->canvas, &pBox->vc);
        pCache->iWidth = pBox->width;
        pCache->iHeight = pBox->height;
        pCache->normalFlowOut.iMaxMargin = pNormal->iMaxMargin;
        pCache->normalFlowOut.iMinMargin = pNormal->iMinMargin;
        pCache->normalFlowOut.isValid = pNormal->isValid;
        pCache->flags |= CACHE_LAYOUT_VALID;
    }

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * blockMinMaxWidth --
 *
 *     Figure out the minimum and maximum widths that the box generated by
 *     pNode may use. This is used during table floating box layout.
 *
 *     The returned widths include the content, borders, padding and
 *     margins.
 *
 *     TODO: There's a huge bug in this function to do with % widths.
 *     Observation indicate that similar bugs exist in KHTML and Gecko. Opera
 *     handles the situation differently, but still not correctly.
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
blockMinMaxWidth(pLayout, pNode, pMin, pMax)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    int *pMin;
    int *pMax;
{
    int min;        /* Minimum width of this block */
    int max;        /* Maximum width of this block */

    HtmlLayoutCache *pCache = pNode->pLayoutCache;

    if (!pCache) {
        pCache = (HtmlLayoutCache *)HtmlClearAlloc(sizeof(HtmlLayoutCache));
        pNode->pLayoutCache = pCache;
    }

    if (
        pLayout->pTree->options.layoutcache && 
        (pCache->flags & CACHE_MINMAX_VALID)
    ) {
        min = pCache->iMinWidth;
        max = pCache->iMaxWidth;
    } else {
        BoxContext sBox;
        int minmaxTestOrig = pLayout->minmaxTest;
        pLayout->minmaxTest = 1;

        /* Figure out the minimum width of the box by
         * pretending to lay it out with a parent-width of 0.
         */
        memset(&sBox, 0, sizeof(BoxContext));
        drawBlock(pLayout, &sBox, pNode, 0, DRAWBLOCK_OMITBORDER);
        HtmlDrawCleanup(&sBox.vc);
        min = sBox.width;
    
        /* Figure out the maximum width of the box by pretending to lay it
         * out with a very large parent width. It is not expected to
         * be a problem that tables may be layed out incorrectly on
         * displays wider than 10000 pixels.
         */
        memset(&sBox, 0, sizeof(BoxContext));
        sBox.iContaining = 10000;
        drawBlock(pLayout, &sBox, pNode, 10000, DRAWBLOCK_OMITBORDER);
        HtmlDrawCleanup(&sBox.vc);
        max = sBox.width;

        assert(max>=min);
        pCache->iMinWidth = min;
        pCache->iMaxWidth = max;
        pCache->flags |= CACHE_MINMAX_VALID;

        pLayout->minmaxTest = minmaxTestOrig;

        /* Log the fact that we just calculated a new minimum and maximum
	 * width.
         */
	LOG {
            HtmlTree *pTree = pLayout->pTree;
            HtmlLog(pTree, "LAYOUTENGINE", "%s blockMinMaxWidth() -> "
                "min=%d max=%d",
                Tcl_GetString(HtmlNodeCommand(pTree, pNode)), min, max
            );
        }
    } 

    *pMin = min;
    *pMax = max;

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
void 
borderLayout(pLayout, pNode, pBox, xA, yA, xB, yB)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    BoxContext *pBox;
    int xA;
    int yA;
    int xB;
    int yB;
{
    int x1, y1, x2, y2;

    BoxContext sBox;
    memset(&sBox, 0, sizeof(BoxContext));

    x1 = 0;
    y1 = 0;
    x2 = xB - xA;
    y2 = yB - yA;

    HtmlDrawBox(&pBox->vc, xA, yA, xB-xA, yB-yA, pNode, 0, pLayout->minmaxTest);
}

/*
 *---------------------------------------------------------------------------
 *
 * doConfigureCmd --
 *
 *     Argument pNode must be a pointer to a node that has been replaced (i.e.
 *     using the [$node replace] interface) with a Tk window.  This function
 *     executes the "-configurecmd" script to configure the window based on the
 *     actual CSS property values for the node.
 *
 *     Currently, the configuration array contains the following:
 *
 *         
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     If the -configurecmd script returns an error, Tcl_BackgroundError() is
 *     called.
 *---------------------------------------------------------------------------
 */
static void 
doConfigureCmd(pTree, pNode, iContaining)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int iContaining;
{
    Tcl_Obj *pConfigure;                           /* -configurecmd script */

    assert(pNode && pNode->pReplacement);
    pConfigure = pNode->pReplacement->pConfigure;
    pNode->pReplacement->iOffset = 0;

    if (pConfigure) {
        Tcl_Interp *interp = pTree->interp;
        HtmlComputedValues *pV = pNode->pPropertyValues;
        HtmlNode *pTmp;
        Tcl_Obj *pArray;
        Tcl_Obj *pScript;
        Tcl_Obj *pRes;
        int rc;
        int iWidth;

        pArray = Tcl_NewObj();
        Tcl_ListObjAppendElement(interp, pArray, Tcl_NewStringObj("color",-1));
        Tcl_ListObjAppendElement(interp, pArray, 
                Tcl_NewStringObj(Tk_NameOfColor(pV->cColor->xcolor), -1)
        );

        pTmp = pNode;
        while (pTmp && pTmp->pPropertyValues->cBackgroundColor->xcolor == 0) {
            pTmp = HtmlNodeParent(pTmp);
        }
        if (pTmp) {
            XColor *xcolor = pTmp->pPropertyValues->cBackgroundColor->xcolor;
            Tcl_ListObjAppendElement(interp, pArray, 
                    Tcl_NewStringObj("background-color", -1)
            );
            Tcl_ListObjAppendElement(interp, pArray, 
                    Tcl_NewStringObj(Tk_NameOfColor(xcolor), -1)
            );
        }

        Tcl_ListObjAppendElement(interp, pArray, Tcl_NewStringObj("font",-1));
        Tcl_ListObjAppendElement(interp, pArray, 
                Tcl_NewStringObj(pV->fFont->zFont, -1)
        );

        /* If the 'width' attribute is not PIXELVAL_AUTO, pass it to the
         * replacement window.
         */
        if (PIXELVAL_AUTO != (iWidth = PIXELVAL(pV, WIDTH, iContaining))) {
            Tcl_Obj *pWidth = Tcl_NewStringObj("width",-1);
            iWidth = MAX(iWidth, 1);
            Tcl_ListObjAppendElement(interp, pArray, pWidth);
            Tcl_ListObjAppendElement(interp, pArray, Tcl_NewIntObj(iWidth));
        }

        pScript = Tcl_DuplicateObj(pConfigure);
        Tcl_IncrRefCount(pScript);
        Tcl_ListObjAppendElement(interp, pScript, pArray);
        rc = Tcl_EvalObjEx(interp, pScript, TCL_EVAL_GLOBAL|TCL_EVAL_DIRECT);
        if (rc != TCL_OK) {
            Tcl_BackgroundError(interp);
        }
        Tcl_DecrRefCount(pScript);

        pRes = Tcl_GetObjResult(interp);
        pNode->pReplacement->iOffset = 0;
        Tcl_GetIntFromObj(0, pRes, &pNode->pReplacement->iOffset);
    }
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
layoutReplacement(pLayout, pBox, pNode)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
{
    int width;
    int height;

    HtmlComputedValues *pV = pNode->pPropertyValues;
    assert(nodeIsReplaced(pNode));

    /* Read the values of the 'width' and 'height' properties of the node.
     * PIXELVAL either returns a value in pixels (0 or greater) or the constant
     * PIXELVAL_AUTO. A value of less than 1 pixel that is not PIXELVAL_AUTO
     * is treated as exactly 1 pixel.
     */
    width = PIXELVAL(
        pV, WIDTH, pLayout->minmaxTest ? PIXELVAL_AUTO : pBox->iContaining
    );
    height = PIXELVAL(pV, HEIGHT, pBox->iContaining);
    if (width > MAX_PIXELVAL) width = MAX(width, 1);
    if (height > MAX_PIXELVAL) height = MAX(height, 1);

    if (pNode->pReplacement) {
        CONST char *zReplace = Tcl_GetString(pNode->pReplacement->pReplace);
        Tk_Window win = pNode->pReplacement->win;
        if (win) {
            Tcl_Obj *pWin = 0;
            int iOffset;
            if (!pLayout->minmaxTest) {
                doConfigureCmd(pLayout->pTree, pNode, pBox->iContaining);
                pWin = Tcl_NewStringObj(zReplace, -1);
            }
            width = Tk_ReqWidth(win);
            height = Tk_ReqHeight(win);
            iOffset = pNode->pReplacement->iOffset;
            DRAW_WINDOW(&pBox->vc, pNode, 0, 0, width, height);
        }
    } else {
        int t = pLayout->minmaxTest;
        HtmlImage2 *pImg = pV->imReplacementImage;
        pImg = HtmlImageScale(pImg, &width, &height, (t ? 0 : 1));
        HtmlDrawImage(&pBox->vc, pImg, 0, 0, width, height, pNode, t);
        HtmlImageFree(pImg);
    }

    /* Note that width and height may still be PIXELVAL_AUTO here (if we failed
     * to find the named replacement image or window). This makes no difference
     * because PIXELVAL_AUTO is a large negative number.
     */
    assert(PIXELVAL_AUTO < 0);
    pBox->width = MAX(pBox->width, width);
    pBox->height = MAX(pBox->height, height);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayout --
 * 
 *     Build the internal representation of the formatted document (document
 *     layout). The document layout is stored in the HtmlTree.canvas and
 *     HtmlTree.iCanvasWidth variables.
 *
 * Results:
 *
 * Side effects:
 *     Destroys the existing document layout, if one exists.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlLayout(pTree)
    HtmlTree *pTree;
{
    HtmlNode *pBody = 0;
    int rc = TCL_OK;
    int nWidth;
    BoxContext sBox;               /* The imaginary box <body> is inside */
    LayoutContext sLayout;

    /* Set variable nWidth to the pixel width of the viewport to render 
     * to. This code should use the actual width of the window if the
     * widget is displayed, or the configured width if it is not (i.e. if 
     * the widget is never packed).
     *
     * It would be better to use the Tk_IsMapped() function here, but for
     * some reason I can't make it work. So instead depend on the observed 
     * behaviour that Tk_Width() returns 1 if the window is not mapped.
     */
    nWidth = Tk_Width(pTree->tkwin);
    if (nWidth<5) {
        nWidth = pTree->options.width;
    }

    /* Delete any existing document layout. */
    /* HtmlDrawDeleteControls(pTree, &pTree->canvas); */
    HtmlDrawCleanup(&pTree->canvas);
    memset(&pTree->canvas, 0, sizeof(HtmlCanvas));

    /* Set up the layout context object. */
    memset(&sLayout, 0, sizeof(LayoutContext));
    sLayout.pTree = pTree;
    sLayout.interp = pTree->interp;
    sLayout.tkwin = pTree->win;

    /* Set up the box context object. */
    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = nWidth;
    /* sBox.pFloat = HtmlFloatListNew(); */

    HtmlLog(pTree, "LAYOUTENGINE", "START");

    /* Call blockLayout() to layout the top level box, generated by the
     * root node.
     */
    pBody = pTree->pRoot;
    if (pBody) {
        int xoffset;
        MarginProperties margin;
        nodeGetMargins(&sLayout, pBody, nWidth, &margin);
        sLayout.pTop = pBody;
        drawBlock(&sLayout, &sBox, pBody, nWidth, 0);
        xoffset = MAX(-1 * sBox.vc.left, 0);
        HtmlDrawCanvas(&pTree->canvas, &sBox.vc, xoffset, 0, pBody);
        pTree->canvas.right = sBox.width + xoffset;
        pTree->canvas.bottom = sBox.height;
    }

    if (rc == TCL_OK) {
        pTree->iCanvasWidth = Tk_Width(pTree->tkwin);
        if (pTree->options.shrink) {
            Tk_GeometryRequest(
                pTree->tkwin, pTree->canvas.right, pTree->canvas.bottom
            );
        }
    }
    return rc;
}

void 
HtmlLayoutInvalidateCache(pNode)
    HtmlNode *pNode;
{
    if (pNode->pLayoutCache) {
        HtmlDrawCleanup(&pNode->pLayoutCache->canvas);
        HtmlFree(pNode->pLayoutCache);
        pNode->pLayoutCache = 0;
    }
}

HtmlCanvas *
HtmlLayoutGetCanvas(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    HtmlNode *p;
    for (p = pNode; p; p = HtmlNodeParent(p)) {
        if (p->pLayoutCache && (p->pLayoutCache->flags & CACHE_LAYOUT_VALID)) {
            return &p->pLayoutCache->canvas;
        }
    }
    return &pTree->canvas;
}


