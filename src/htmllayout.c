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
static const char rcsid[] = "$Id: htmllayout.c,v 1.109 2005/11/19 07:43:49 danielk1977 Exp $";

#include "htmllayout.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/*
 * Notes on layout-engine logging:
 */
#define LOG if (pLayout->pTree->options.logcmd)

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

struct NormalFlow {
    int iMaxMargin;
    int iMinMargin;
    int isValid;
    NormalFlowCallback *pCallbackList;
};

struct NormalFlowCallback {
    void (*xCallback)(NormalFlow *, NormalFlowCallback *, int);
    ClientData clientData;
    NormalFlowCallback *pNext;
};

/*
 * These are prototypes for all the static functions in this file. We
 * don't need most of them, but the help with error checking that normally
 * wouldn't happen because of the old-style function declarations. Also
 * they function as a table of contents for this file.
 */

static int markerLayout(LayoutContext*, BoxContext*, HtmlNode*, int);
static void layoutReplacement(LayoutContext*,BoxContext*,HtmlNode*,CONST char*);

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
void 
nodeGetBorderProperties(pLayout, pNode, pBorderProperties)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    BorderProperties *pBorderProperties;
{
    HtmlComputedValues *pValues = pNode->pPropertyValues;
    pBorderProperties->color_top = pValues->cBorderTopColor->xcolor;
    pBorderProperties->color_right = pValues->cBorderRightColor->xcolor;
    pBorderProperties->color_bottom = pValues->cBorderBottomColor->xcolor;
    pBorderProperties->color_left = pValues->cBorderLeftColor->xcolor;
    pBorderProperties->color_bg = pValues->cBackgroundColor->xcolor;

    assert(
        ((pValues->mask & PROP_MASK_BACKGROUND_POSITION_X) ? 1 : 0) ==
        ((pValues->mask & PROP_MASK_BACKGROUND_POSITION_Y) ? 1 : 0)
    );
    pBorderProperties->pBgImage = pValues->imBackgroundImage;
    pBorderProperties->eBgRepeat = pValues->eBackgroundRepeat;
    pBorderProperties->iPositionX = pValues->iBackgroundPositionX;
    pBorderProperties->iPositionY = pValues->iBackgroundPositionY;
    pBorderProperties->isPositionPercent = 
        ((pValues->mask & PROP_MASK_BACKGROUND_POSITION_X) ? 1 : 0);
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
}
static void 
normalFlowMarginAdd(pNormal, y) 
    NormalFlow *pNormal;
    int y;
{
    if (pNormal->isValid) {
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
    int isReplaced = (pNode->pReplacement ? 1 : 0);
    int eFloat = pV->eFloat;
    int iContaining = pBox->iContaining;
    HtmlFloatList *pFloat = pBox->pFloat;

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
    iTop = HtmlFloatListClear(pFloat, pV->eClear, y);
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

    /* Fix the float list in the parent block so that nothing overlaps
     * this floating box.
     */
    HtmlFloatListAdd(pBox->pFloat, eFloat, 
        ((eFloat == CSS_CONST_LEFT) ? x + iTotalWidth : x),
        iTop, iTop + iTotalHeight);

    LOG if (!pLayout->minmaxTest) {
        HtmlTree *pTree = pLayout->pTree;
        char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        HtmlLog(pTree, "LAYOUTENGINE", "%s (Float) %dx%d (%d,%d)", 
            zNode, iTotalWidth, iTotalHeight, x, iTop);
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
markerLayout(pLayout, pBox, pNode, y)
    LayoutContext *pLayout;
    BoxContext *pBox;
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
        int iMarginLeft = PIXELVAL(pComputed, MARGIN_LEFT, pBox->iContaining);
        iMarginLeft = MAX(40, iMarginLeft);
        width = iMarginLeft - pComputed->fFont->ex_pixels;

        if (pComputed->eListStylePosition == CSS_CONST_OUTSIDE) {
            HtmlDrawImage2(&sCanvas, pComputed->imListStyleImage,
                10000, 10000, 1, CSS_CONST_NO_REPEAT,
                -1 * iMarginLeft, y,
                width, yoffset, pLayout->minmaxTest
            );
        } else {
            HtmlDrawImage2(&sCanvas, pComputed->imListStyleImage,
                0, 10000, 1, CSS_CONST_NO_REPEAT,
                0, y,
                width, yoffset, pLayout->minmaxTest
            );
            HtmlFloatListAdd(pBox->pFloat, FLOAT_LEFT, 
                  iMarginLeft, y, y + yoffset);
        }
    } else {
        XColor *color;
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
        color = pComputed->cColor->xcolor;
        pMarker = Tcl_NewStringObj(zMarker, -1);
        Tcl_IncrRefCount(pMarker);
        width = Tk_TextWidth(font, zMarker, strlen(zMarker));
    
        if (pComputed->eListStylePosition == CSS_CONST_OUTSIDE) {
	    /* It's not specified in CSS 2.1 exactly where the list marker
	     * should be drawn when the 'list-style-position' property is
	     * 'outside'.  The algorithm used is to draw it the width of 1 'x'
	     * character in the current font to the left of the content box.
             */
            offset = pComputed->fFont->ex_pixels + width;
        } else {
            assert(pComputed->eListStylePosition == CSS_CONST_INSIDE);
            offset = 0;
            HtmlFloatListAdd(pBox->pFloat, FLOAT_LEFT, 
                  pComputed->fFont->ex_pixels + width, y, y + yoffset);
        }
        DRAW_TEXT(&sCanvas, pMarker, -1*offset, y+yoffset, width, 0,font,color);
    
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
        HtmlFloatListMargins(pBox->pFloat, y, y+10, &leftFloat, &rightFloat);
        forcebox = (rightFloat==pBox->iContaining && leftFloat==0);

        memset(&lc, 0, sizeof(HtmlCanvas));
        w = rightFloat - leftFloat;
        f = (forcebox ? LINEBOX_FORCEBOX : 0) | 
            (forceflag ? LINEBOX_FORCELINE : 0) |
            (closeborders ? LINEBOX_CLOSEBORDERS : 0);
        have = inlineContextGetLineBox(pLayout, pContext, &w, f, &lc, &nV, &nA);

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
            y = HtmlFloatListPlace(pBox->pFloat, pBox->iContaining, w, 1, y);
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
    int isReplaced = pNode->pReplacement ? 1 : 0;
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
 *     independant of a normal flow context (possibly creating it's own
 *     context). For example if pNode:
 *
 *         + is a floating box
 *         + is the root node of a document
 *         + is a table cell
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
 *                 block, ont the width between the inside and outside margins. 
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
    sBox.pFloat = HtmlFloatListNew();

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
            tableLayout(pLayout, &sBox, pNode);
            break;
        default:
            assert(!"Bad display value in drawBlock()");
    }

    if (flags & DRAWBLOCK_ENFORCEHEIGHT) {
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
    pBox->height = HtmlFloatListClear(sBox.pFloat, CSS_CONST_BOTH,pBox->height);
    pBox->width = MAX(pBox->width, xBorderRight + margin.margin_right);

    if (pNode == pLayout->pTop) {
        normalFlowMarginCollapse(&sNormal, &pBox->height);
    }

    HtmlFloatListDelete(sBox.pFloat);
}

int 
HtmlLayoutTableCell(pLayout, pBox, pNode, iAvailable)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int iAvailable;
{
    drawBlock(pLayout, pBox, pNode, iAvailable, DRAWBLOCK_OMITBORDER);
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
    CONST char *zReplace;             /* Name of replacement object */
    BoxContext sBox;

    int yBorderBottom; /* Y-coordinate for bottom of block border */
    int xBorderLeft;   /* X-coordinate for left of block border */
    int xBorderRight;  /* X-coordinate for right of block border */
    int x, y;          /* Coords for content to be drawn */

    nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);
    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);

    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = pBox->iContaining;
    zReplace = Tcl_GetString(pNode->pReplacement->pReplace);
    assert(zReplace);
    layoutReplacement(pLayout, &sBox, pNode, zReplace);

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
    iWidth = PIXELVAL(
        pNode->pPropertyValues, WIDTH, 
        pLayout->minmaxTest ? PIXELVAL_AUTO : pBox->iContaining
    );
    if (iWidth == PIXELVAL_AUTO) {
        blockMinMaxWidth(pLayout, pNode, &iMinWidth, &iMaxWidth);
        *pY = HtmlFloatListPlace(
            pBox->pFloat, pBox->iContaining, iMinWidth, 10000, *pY);
        HtmlFloatListMargins(
            pBox->pFloat, *pY, *pY + 10000, &iLeftFloat, &iRightFloat);
        iWidth = iRightFloat - iLeftFloat;
        flags = 0;
    } else {
        flags = DRAWBLOCK_ENFORCEWIDTH|DRAWBLOCK_CONTENTWIDTH;
    }

    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = pBox->iContaining;
    drawBlock(pLayout, &sBox, pNode, iWidth, flags);

    y = *pY;
    *pY += sBox.height;

    if (margin.leftAuto && margin.rightAuto) {
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
         * Todo: Find out more about this.
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
	pBox->pFloat, pBox->iContaining, sBox.width, sBox.height, *pY);
    HtmlFloatListMargins(
        pBox->pFloat, *pY, *pY + sBox.height, &iLeftFloat, &iRightFloat);

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
    sBox.pFloat = pBox->pFloat;
    HtmlFloatListNormalize(sBox.pFloat, -1 * x, -1 * y);
    sBox.iContaining = iWidth;

    /* Layout the box content and copy it into the parent box-context. */
    normalFlowLayout(pLayout, &sBox, pNode, pNormal);
    *pY += sBox.height;

    /* Re-normalize the float-list. */
    HtmlFloatListNormalize(sBox.pFloat, x, y);

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
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void
normalFlowClearFloat(pBox, pNode, pNormal, y)
    BoxContext *pBox;
    HtmlNode *pNode;
    NormalFlow *pNormal;
    int y;
{
    int y2 = HtmlFloatListClear(pBox->pFloat,pNode->pPropertyValues->eClear,y);
    normalFlowMarginAdd(pNormal, y2 - y);
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
    markerLayout(pArgs->pLayout, pArgs->pBox, pArgs->pNode, pArgs->y + iMargin);
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

    switch (eFlowType) {

        /* A text node */
        case FLOWTYPE_TEXT:
            rc = HtmlInlineContextAddText(pContext, pNode);
            break;

        /* A non-replaced block box.  */
	case FLOWTYPE_BLOCK: {
            inlineLayoutDrawLines(pLayout, pBox, pContext, 1, pY, pNormal);
            normalFlowClearFloat(pBox, pNode, pNormal, *pY);
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
           
            normalFlowClearFloat(pBox, pNode, pNormal, *pY);
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
            normalFlowClearFloat(pBox, pNode, pNormal, *pY);
            normalFlowLayoutBlock(pLayout, pBox, pNode, pY, pContext, pNormal);
            break;
        }

        /* A floating box. */
        case FLOWTYPE_FLOAT: {
            int y = *pY;
            normalFlowLayoutFloat(pLayout, pBox, pNode, &y, pContext, pNormal);
            break;
        }

        /* A replaced block element. */
        case FLOWTYPE_BLOCK_REPLACED: {
            inlineLayoutDrawLines(pLayout, pBox, pContext, 1, pY, pNormal);
            normalFlowClearFloat(pBox, pNode, pNormal, *pY);
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
            sBox.pFloat = 0;
            sBox.iContaining = pBox->iContaining;
            drawReplacement(pLayout, &sBox, pNode);

            nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);
            iHeight = sBox.height + margin.margin_top + margin.margin_bottom;

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

            memset(&sBox2, 0, sizeof(BoxContext));
            sBox2.pFloat = 0;
            DRAW_CANVAS(&sBox2.vc, &sBox.vc, 0, margin.margin_top, pNode);
	    HtmlInlineContextAddBox(pContext, pNode, &sBox2.vc,
                sBox.width, iHeight, yoffset);
            break;
        }

        /* An inline element. Recurse through the child nodes.  */
        case FLOWTYPE_INLINE: {
            int i;
            InlineBorder *pBorder;
            pBorder = inlineContextGetBorder(pLayout, pNode, 0);
            if (pBorder) {
                inlineContextPushBorder(pContext, pBorder);
            }
            for(i=0; i<HtmlNodeNumChildren(pNode) && 0==rc; i++) {
                HtmlNode *pChild = HtmlNodeChild(pNode, i);
                rc = normalFlowLayoutNode(pLayout, pBox, pChild, pY, pContext,
                    pNormal);
            }
            if (pBorder) {
                inlineContextPopBorder(pContext, pBorder);
            }
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
            normalFlowClearFloat(pBox, pNode, pNormal, *pY);
            break;

        case FLOWTYPE_TABLE: {
            inlineLayoutDrawLines(pLayout, pBox, pContext, 1, pY, pNormal);
            normalFlowClearFloat(pBox, pNode, pNormal, *pY);
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
    int rc;                       /* Return Code */
    InlineBorder *pBorder;
    int ii;

    assert( 
        DISPLAY(pNode->pPropertyValues) == CSS_CONST_BLOCK ||
        DISPLAY(pNode->pPropertyValues) == CSS_CONST_TABLE_CELL ||
        DISPLAY(pNode->pPropertyValues) == CSS_CONST_LIST_ITEM ||
 
        /* TODO: Should this case really be here? */
        DISPLAY(pNode->pPropertyValues) == CSS_CONST_INLINE
    );
    assert(!pNode->pReplacement);

    /* Create the InlineContext object for this containing box */
    pContext = HtmlInlineContextNew(pNode, pLayout->minmaxTest);

    /* Add any inline-border created by the node that generated this
     * normal-flow to the InlineContext. Actual border attributes do not apply
     * in this case, but the 'text-decoration' attribute may.
     */
    if ((pBorder = inlineContextGetBorder(pLayout, pNode, 1))) {
        inlineContextPushBorder(pContext, pBorder);
    }

    /* Layout each of the child nodes into BoxContext. */
    for(ii = 0; ii < HtmlNodeNumChildren(pNode) ; ii++) {
        HtmlNode *p = HtmlNodeChild(pNode, ii);
        normalFlowLayoutNode(pLayout, pBox, p, &y, pContext, pNormal);
    }

    /* Finish the inline-border started by the parent, if any. */
    if (pBorder) {
        inlineContextPopBorder(pContext, pBorder);
    }

    rc = inlineLayoutDrawLines(pLayout, pBox, pContext, 1, &y, pNormal);
    HtmlInlineContextCleanup(pContext);

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

        pCache = (int *)HtmlAlloc(sizeof(int)*2);
        pCache[0] = min;
        pCache[1] = max;
        Tcl_SetHashValue(pEntry, pCache);

        pLayout->minmaxTest = minmaxTestOrig;

        /* Log the fact that we just calculated a new minimum and maximum
	 * width.
         */
	LOG if (0 == pLayout->minmaxTest) {
            HtmlTree *pTree = pLayout->pTree;
            HtmlLog(pTree, "LAYOUTENGINE", "%s blockMinMaxWidth() "
                "min=%d max=%d",
                Tcl_GetString(HtmlNodeCommand(pTree, pNode)), 
                min, max
            );
        }

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
    BoxProperties boxproperties;
    BorderProperties borderproperties;
    int tw, rw, bw, lw;
    XColor *tc, *rc, *bc, *lc;
    int x1, y1, x2, y2;

    BoxContext sBox;
    memset(&sBox, 0, sizeof(BoxContext));

    x1 = 0;
    y1 = 0;
    x2 = xB - xA;
    y2 = yB - yA;

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
        DRAW_QUAD(&sBox.vc, x1, y1, x1+lw, y1+tw, x2-rw, y1+tw, x2, y1, tc);
    }
    if (rw>0) {
        DRAW_QUAD(&sBox.vc, x2, y1, x2-rw, y1+tw, x2-rw, y2-bw, x2, y2, rc);
    }
    if (bw>0) {
        DRAW_QUAD(&sBox.vc, x2, y2, x2-rw, y2-bw, x1+lw, y2-bw, x1, y2, bc);
    }
    if (lw>0) {
        DRAW_QUAD(&sBox.vc, x1, y2, x1+lw, y2-bw, x1+lw, y1+tw, x1, y1, lc);
    }

    if (pNode != pLayout->pTop) {
        if (borderproperties.color_bg) {
            DRAW_QUAD(&sBox.vc, 
                x1+lw, y1+tw, 
                x2-rw, y1+tw, 
                x2-rw, y2-bw, 
                x1+lw, y2-bw, borderproperties.color_bg);
        }
        if (borderproperties.pBgImage) {
            HtmlDrawImage2(
                &sBox.vc,                            /* canvas */
                borderproperties.pBgImage,           /* Html image */
                borderproperties.iPositionX,         /* 'background-position' */
                borderproperties.iPositionY, 
                borderproperties.isPositionPercent, 
                borderproperties.eBgRepeat,          /* 'background-repeat' */
                x1+lw, y1+tw,                        /* x, y */
                x2-rw, y2-bw,                        /* width, height */
                pLayout->minmaxTest                  /* Size-only mode */
            );
        }
    }

    DRAW_CANVAS(&pBox->vc, &sBox.vc, xA, yA, pNode);
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
 * Results:
 *     None.
 *
 * Side effects:
 *     If the -configurecmd script returns an error, Tcl_BackgroundError() is
 *     called.
 *---------------------------------------------------------------------------
 */
static void 
doConfigureCmd(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    Tcl_Obj *pConfigure;                           /* -configurecmd script */

    assert(pNode && pNode->pReplacement);
    pConfigure = pNode->pReplacement->pConfigure;

    if (pConfigure) {
        Tcl_Interp *interp = pTree->interp;
        HtmlComputedValues *pV = pNode->pPropertyValues;
        HtmlNode *pTmp;
        Tcl_Obj *pArray;
        Tcl_Obj *pScript;
        int rc;

        pArray = Tcl_NewObj();
        Tcl_ListObjAppendElement(interp, pArray, Tcl_NewStringObj("color",-1));
        Tcl_ListObjAppendElement(interp, pArray, 
                Tcl_NewStringObj(pV->cColor->zColor, -1)
        );

        pTmp = pNode;
        while (pTmp && pTmp->pPropertyValues->cBackgroundColor->xcolor == 0) {
            pTmp = HtmlNodeParent(pTmp);
        }
        if (pTmp) {
            Tcl_ListObjAppendElement(interp, pArray, 
                    Tcl_NewStringObj("background-color", -1)
            );
            Tcl_ListObjAppendElement(interp, pArray, 
                    Tcl_NewStringObj(
                            pTmp->pPropertyValues->cBackgroundColor->zColor, -1
                    )
            );
        }

        Tcl_ListObjAppendElement(interp, pArray, Tcl_NewStringObj("font",-1));
        Tcl_ListObjAppendElement(interp, pArray, 
                Tcl_NewStringObj(pV->fFont->zFont, -1)
        );

        pScript = Tcl_DuplicateObj(pConfigure);
        Tcl_IncrRefCount(pScript);
        Tcl_ListObjAppendElement(interp, pScript, pArray);
        rc = Tcl_EvalObjEx(interp, pScript, TCL_EVAL_GLOBAL|TCL_EVAL_DIRECT);
        if (rc != TCL_OK) {
            Tcl_BackgroundError(interp);
        }
        Tcl_DecrRefCount(pScript);
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
layoutReplacement(pLayout, pBox, pNode, zReplace)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    CONST char *zReplace;
{
    int width;
    int height;

    HtmlComputedValues *pV = pNode->pPropertyValues;
    Tk_Window tkwin = pLayout->tkwin;
    Tcl_Interp *interp = pLayout->interp;

    /* Read the values of the 'width' and 'height' properties of the node.
     * PIXELVAL either returns a value in pixels (0 or greater) or the constant
     * PIXELVAL_AUTO.
     */
    width = PIXELVAL(
        pV, WIDTH, pLayout->minmaxTest ? PIXELVAL_AUTO : pBox->iContaining
    );
    height = PIXELVAL(pV, HEIGHT, pBox->iContaining);

    if (width > MAX_PIXELVAL) width = MAX(width, 1);
    if (height > MAX_PIXELVAL) height = MAX(height, 1);

    if (zReplace[0]=='.') {
        Tk_Window win = Tk_NameToWindow(interp, zReplace, tkwin);
        if (win) {
            Tcl_Obj *pWin = 0;
            if (!pLayout->minmaxTest) {
                doConfigureCmd(pLayout->pTree, pNode);
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
    int rc;
    int width = Tk_Width(pTree->tkwin); /* Default width if no -width option */
    BoxContext sBox;               /* The imaginary box <body> is inside */
    LayoutContext sLayout;
    Tcl_HashSearch s;
    Tcl_HashEntry *p;

    /* Delete any existing document layout. */
    /* HtmlDrawDeleteControls(pTree, &pTree->canvas); */
    HtmlDrawCleanup(&pTree->canvas);
    memset(&pTree->canvas, 0, sizeof(HtmlCanvas));

    /* Set up the layout context object. */
    memset(&sLayout, 0, sizeof(LayoutContext));
    sLayout.pTree = pTree;
    sLayout.interp = pTree->interp;
    sLayout.tkwin = pTree->win;
    Tcl_InitHashTable(&sLayout.widthCache, TCL_ONE_WORD_KEYS);

    /* Set up the box context object. */
    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = width;
    sBox.pFloat = HtmlFloatListNew();

    HtmlLog(pTree, "LAYOUTENGINE", "START");

    /* Call blockLayout() to layout the top level box, generated by the
     * root node.
     */
    pBody = pTree->pRoot;
    if (pBody) {
        XColor *c = pBody->pPropertyValues->cBackgroundColor->xcolor;
        MarginProperties margin;
        nodeGetMargins(&sLayout, pBody, width, &margin);
        if (c) {
            HtmlDrawBackground(&pTree->canvas, c, 0);
        }
        sLayout.pTop = pBody;
        drawBlock(&sLayout, &sBox, pBody, width, 0);
        HtmlDrawCanvas(&pTree->canvas, &sBox.vc, 0, 0, pBody);
        pTree->canvas.right = sBox.width;
        pTree->canvas.bottom = sBox.height;
    } else {
        HtmlColor *pColor;
        Tcl_HashEntry *pEntry;
        pEntry = Tcl_FindHashEntry(&pTree->aColor, "white");
        assert(pEntry);
        pColor = Tcl_GetHashValue(pEntry);
        HtmlDrawBackground(&pTree->canvas, pColor->xcolor, 0);
        rc = TCL_OK;
    }

    /* Clear the width cache and delete the float-list. */
    HtmlFloatListDelete(sBox.pFloat);
    for (
        p = Tcl_FirstHashEntry(&sLayout.widthCache, &s); 
        p; 
        p = Tcl_NextHashEntry(&s)) 
    {
        HtmlFree((char *)Tcl_GetHashValue(p));
    }
    Tcl_DeleteHashTable(&sLayout.widthCache);

    if (rc == TCL_OK) {
        pTree->iCanvasWidth = Tk_Width(pTree->tkwin);
    }
    return rc;
}
