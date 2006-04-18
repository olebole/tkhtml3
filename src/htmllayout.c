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
static const char rcsid[] = "$Id: htmllayout.c,v 1.145 2006/04/18 09:40:07 danielk1977 Exp $";

#include "htmllayout.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define LOG if (pLayout->pTree->options.logcmd && 0 == pLayout->minmaxTest)

/*
 * The code to lay out a "normal-flow" is located in this file:
 *
 * A new normal flow is established by:
 *     - the root node of the document,
 *     - a floating box,
 *     - a table cell.
 *
 */

typedef struct NormalFlowCallback NormalFlowCallback;
typedef struct NormalFlow NormalFlow;

/*
 * This structure is (fairly obviously) used to link node structures into a
 * linked list. This is used as part of the process to layout a node with the
 * 'position' property set to "absolute".
 */
struct NodeList {
    HtmlNode *pNode;
    NodeList *pNext;
    HtmlCanvasItem *pMarker;       /* Static position marker */
};

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
    int nonegative;          /* Do not return negative from Collapse() */
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
 * Public functions:
 *
 *     HtmlLayoutInvalidateCache
 *     HtmlLayout
 *
 * Functions declared in htmllayout.h:
 *
 *     HtmlLayoutNodeContent         * htmltable.c only
 *
 *     blockMinMaxWidth            * This file and htmltable.c
 *     nodeGetMargins              * This file and htmlinline.c
 *     nodeGetBoxProperties        * This file and htmlinline.c
 */

/*
 * These are prototypes for all the static functions in this file. We
 * don't need most of them, but the help with error checking that normally
 * wouldn't happen because of the old-style function declarations. Also
 * they function as a table of contents for this file.
 */

static int markerLayout(LayoutContext*, BoxContext*, NormalFlow *, HtmlNode*, int);

static int inlineLayoutDrawLines
    (LayoutContext*,BoxContext*,InlineContext*,int,int*, NormalFlow*);

/* 
 * nodeIsReplaced() returns a boolean value, true if the node should be
 * layed out as if it were a replaced object (i.e. if there is a value for
 * '-tkhtml-replacement-image' or a replaced window).
 */
static int nodeIsReplaced(HtmlNode *);


typedef int (FlowLayoutFunc) (
    LayoutContext *, 
    BoxContext *, 
    HtmlNode *, 
    int *, 
    InlineContext *, 
    NormalFlow *
);

static void normalFlowLayout(LayoutContext*,BoxContext*,HtmlNode*,NormalFlow*);

static FlowLayoutFunc normalFlowLayoutNode;
static FlowLayoutFunc normalFlowLayoutListItem;
static FlowLayoutFunc normalFlowLayoutFloat;
static FlowLayoutFunc normalFlowLayoutBlock;
static FlowLayoutFunc normalFlowLayoutReplaced;
static FlowLayoutFunc normalFlowLayoutTable;
static FlowLayoutFunc normalFlowLayoutText;
static FlowLayoutFunc normalFlowLayoutInline;
static FlowLayoutFunc normalFlowLayoutReplacedInline;
static FlowLayoutFunc normalFlowLayoutAbsolute;

/* Manage collapsing vertical margins in a normal-flow */
static void normalFlowMarginCollapse(NormalFlow *, int *);
static void normalFlowMarginAdd(NormalFlow *, int);
static int  normalFlowMarginQuery(NormalFlow *);

/* Hooks to attach the list-marker drawing callback to a NormalFlow */
static void normalFlowCbAdd(NormalFlow *, NormalFlowCallback *);
static void normalFlowCbDelete(NormalFlow *, NormalFlowCallback *);

/* Utility function to format an integer as a roman number (I, II, etc.). */
static void getRomanIndex(char *, int, int);

/* Function to invoke the -configurecmd of a replaced object. */
static void doConfigureCmd(HtmlTree *, HtmlNode *, int);

static void drawReplacement(LayoutContext*, BoxContext*, HtmlNode*);
static void drawReplacementContent(LayoutContext*, BoxContext*, HtmlNode*);

/* Add a border to a rendered content area */
static void wrapContent(LayoutContext*, BoxContext*, BoxContext*, HtmlNode*);

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
 *     Four non-negative integer pixel values are returned, the sum of the
 *     padding and border pixel widths for the top, right, bottom and left
 *     sides of the box.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     The four calculated values are written into *pBoxProperties before
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
    int c = iContaining;
    if (pLayout->minmaxTest || c < 0) {
        c = 0;
    }

    pBoxProperties->iTop    = PIXELVAL(pV, PADDING_TOP, c);
    pBoxProperties->iRight  = PIXELVAL(pV, PADDING_RIGHT, c);
    pBoxProperties->iBottom = PIXELVAL(pV, PADDING_BOTTOM, c);
    pBoxProperties->iLeft   = PIXELVAL(pV, PADDING_LEFT, c);

    /* For each border width, use the computed value if border-style is
     * something other than 'none', otherwise use 0. The PIXELVAL macro is not
     * used because 'border-width' properties may not be set to % values.
     */
    pBoxProperties->iTop += (
        (pV->eBorderTopStyle != CSS_CONST_NONE) ? pV->border.iTop : 0);
    pBoxProperties->iRight += (
        (pV->eBorderRightStyle != CSS_CONST_NONE) ? pV->border.iRight : 0);
    pBoxProperties->iBottom += (
        (pV->eBorderBottomStyle != CSS_CONST_NONE) ? pV->border.iBottom : 0);
    pBoxProperties->iLeft += (
        (pV->eBorderLeftStyle != CSS_CONST_NONE) ?  pV->border.iLeft : 0);

    assert(
        pBoxProperties->iTop >= 0 &&
        pBoxProperties->iRight >= 0 &&
        pBoxProperties->iBottom >= 0 &&
        pBoxProperties->iLeft >= 0 
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
    pMargins->topAuto = ((iMarginTop == PIXELVAL_AUTO) ? 1 : 0);
    pMargins->bottomAuto = ((iMarginBottom == PIXELVAL_AUTO) ? 1 : 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowMarginCollapse --
 * normalFlowMarginQuery --
 * normalFlowMarginAdd --
 *
 *     The following three functions are used to manage collapsing vertical
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
    int iMargin = pNormal->iMinMargin + pNormal->iMaxMargin;
    if (pNormal->nonegative) {
        iMargin = MAX(0, iMargin);
    }
    return iMargin;
}
static void 
normalFlowMarginCollapse(pNormal, pY) 
    NormalFlow *pNormal;
    int *pY;
{
    NormalFlowCallback *pCallback = pNormal->pCallbackList;
    int iMargin = pNormal->iMinMargin + pNormal->iMaxMargin;
    if (pNormal->nonegative) {
        iMargin = MAX(0, iMargin);
    }
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
    if (pNormal->isValid && (!pNormal->nonegative || y >= 0)) {
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
 *     This FlowLayoutFunc is called to add a floating box to a normal 
 *     flow. pNode is the node with the 'float' property set.
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
normalFlowLayoutFloat(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;  /* Layout context */
    BoxContext *pBox;        /* Containing box context */
    HtmlNode *pNode;         /* Node that generates floating box */
    int *pY;                 /* IN/OUT: y-coord to draw float at */
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    HtmlComputedValues *pV = pNode->pPropertyValues;
    int eFloat = pV->eFloat;
    int iContaining = pBox->iContaining;
    HtmlFloatList *pFloat = pNormal->pFloat;

    int iTotalHeight;        /* Height of floating box (incl. margins) */
    int iTotalWidth;         /* Width of floating box (incl. margins) */

    int x, y;                /* Coords for content to be drawn */
    int iLeft;               /* Left floating margin where box is drawn */
    int iRight;              /* Right floating margin where box is drawn */
    int iTop;                /* Top of top margin of box */

    MarginProperties margin; /* Margin properties of pNode */
    BoxContext sBox;         /* Box context for content to be drawn into */

    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = iContaining;

    y = (*pY) + normalFlowMarginQuery(pNormal);

    nodeGetMargins(pLayout, pNode, iContaining, &margin);

    /* The code that calculates computed values (htmlprop.c) should have
     * ensured that all floating boxes have a 'display' value of "block",
     * "table" or "list-item".
     */
    assert(
      DISPLAY(pV) == CSS_CONST_BLOCK || 
      DISPLAY(pV) == CSS_CONST_TABLE ||
      DISPLAY(pV) == CSS_CONST_LIST_ITEM
    );
    assert(eFloat == CSS_CONST_LEFT || eFloat == CSS_CONST_RIGHT);

    /* Draw the floating element to sBox. The procedure for determining the
     * width to use for the element is described in sections 10.3.5
     * (non-replaced) and 10.3.6 (replaced) of the CSS 2.1 spec.
     */
    if (nodeIsReplaced(pNode)) {
	/* For a replaced element, the drawReplacement() function takes care of
	 * calculating the actual width and height, and of drawing borders
	 * etc. As usual horizontal margins are included, but vertical are not.
         */
        drawReplacement(pLayout, &sBox, pNode);
    } else {
        /* A non-replaced element. */
        BoxProperties box;   /* Box properties of pNode */
        BoxContext sContent;
        int c = pLayout->minmaxTest ? PIXELVAL_AUTO : iContaining;
        int iWidth = PIXELVAL(pV, WIDTH, c);
        int iHeight = PIXELVAL(pV, HEIGHT, c);

        nodeGetBoxProperties(pLayout, pNode, iContaining, &box);

	/* If the computed value if iWidth is "auto", calculate the
         * shrink-to-fit content width and use that instead.  */
        if (iWidth == PIXELVAL_AUTO) {
            int iMax;            /* Preferred maximum width */
            int iMin;            /* Preferred minimum width */
            int iAvailable;      /* Available width */
    
            iAvailable = sBox.iContaining;
            iAvailable -= (margin.margin_left + margin.margin_right);
            iAvailable -= (box.iLeft + box.iRight);
	    blockMinMaxWidth(pLayout, pNode, &iMin, &iMax);
            iWidth = MIN(MAX(iMin, iAvailable), iMax);
        }

        /* Layout the node content into sContent. Then add the border and
         * transfer the result to sBox. 
         */
        memset(&sContent, 0, sizeof(BoxContext));
        sContent.iContaining = iWidth;
        HtmlLayoutNodeContent(pLayout, &sContent, pNode);
        sContent.width = MAX(iWidth, sContent.width);
        sContent.height = MAX(iHeight, sContent.height);
        wrapContent(pLayout, &sBox, &sContent, pNode);
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
 * HtmlLayoutNodeContent --
 *
 *     Draw the content area of node pNode into the box context pBox. If pNode
 *     has the 'display' property set to "table", it generates a table context.
 *     Otherwise, pNode is assumed to generate a normal flow context.
 *
 *     In particular, it is illegal to pass a replaced element (according to
 *     nodeIsReplaced()) to this
 *     function.
 *
 *     When this function is called, pBox->iContaining should contain the pixel
 *     width available for the content.  The top-left hand corner of the
 *     content is placed at the (0,0) coordinate of canvas pBox->vc.
 *     pBox->width and height are set to the intrinsic width and height of the
 *     content when rendered with containing block width pBox->iContaining.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     If pNode is a replaced element, a Tcl script may be invoked to
 *     configured the element.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlLayoutNodeContent(pLayout, pBox, pNode)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
{
    assert(!nodeIsReplaced(pNode));
    if (DISPLAY(pNode->pPropertyValues) == CSS_CONST_TABLE) {
        /* All the work for tables is done in htmltable.c */
        HtmlTableLayout(pLayout, pBox, pNode);
    } else {
        /* Set up a new NormalFlow for this flow */
        HtmlFloatList *pFloat;
        NormalFlow sNormal;
    
        /* Set up the new normal-flow context */
        memset(&sNormal, 0, sizeof(NormalFlow));
        pFloat = HtmlFloatListNew();
        sNormal.pFloat = pFloat;
    
        /* Layout the contents of the node */
        normalFlowLayout(pLayout, pBox, pNode, &sNormal);
    
	/* Increase the height of the box to cover any floating boxes that
         * extend down past the end of the content. 
         */
        pBox->height = HtmlFloatListClear(pFloat, CSS_CONST_BOTH, pBox->height);
    
        /* Clean up the float list */
        HtmlFloatListDelete(pFloat);
    }
    return 0;
}


/*
 *---------------------------------------------------------------------------
 *
 * drawReplacementContent --
 *
 *     pNode must be a replaced element according to nodeIsReplaced() (either
 *     it has a value set for -tkhtml-replacement-image or the Tcl [nodeHandle
 *     replace] has been used to replace the element with a Tk window).
 *
 *     The replaced content is drawn into pBox, with it's top left corner at
 *     (0, 0). Only content is drawn, no borders or backgrounds. The
 *     pBox->width and pBox->height variables are set to the size of the
 *     content.
 *
 *     If the 'width' property of pNode is set to a percentage it is calculated
 *     with respect to pBox->iContaining.
 *
 *     See drawReplacement() for a wrapper around this function that also 
 *     draws borders and backgrounds.
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
drawReplacementContent(pLayout, pBox, pNode)
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
    height = pV->iHeight;
    if (height != PIXELVAL_AUTO) height = MAX(height, 1);
    if (width != PIXELVAL_AUTO) width = MAX(width, 1);

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
 * drawReplacement --
 *
 *     pNode must be a replaced element. 
 *
 *     The node content, background and borders are drawn into argument pBox.
 *     The pBox coordinates (0, 0) correspond to the outside edge of the left
 *     margin and the upper edge of the top border. That is, horizontal margins
 *     are included but vertical are not.
 *
 *     Any percentage lengths are calculated with respect to pBox->iContaining.
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

    nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);
    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);

    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = pBox->iContaining;
    drawReplacementContent(pLayout, &sBox, pNode);
    wrapContent(pLayout, pBox, &sBox, pNode);
}

static void
drawAbsolute(pLayout, pBox, pStaticCanvas, x, y)
    LayoutContext *pLayout;       /* Layout context */
    BoxContext *pBox;             /* Padding edge box to draw to */
    HtmlCanvas *pStaticCanvas;    /* Canvas containing static positions */
    int x;                        /* Offset of padding edge in pStaticCanvas */
    int y;                        /* Offset of padding edge in pStaticCanvas */
{
    NodeList *pList;
    NodeList *pNext;
    for (pList = pLayout->pAbsolute; pList; pList = pNext) {
        int s_x;               /* Static X offset */
        int s_y;               /* Static Y offset */

        MarginProperties margin;
        BoxProperties box;

        BoxContext sBox;
        BoxContext sContent;

        HtmlNode *pNode = pList->pNode;
        HtmlComputedValues *pV = pNode->pPropertyValues;
        int isReplaced = nodeIsReplaced(pList->pNode);

        int iLeft   = PIXELVAL(pV, LEFT, pBox->iContaining);
        int iRight  = PIXELVAL(pV, RIGHT, pBox->iContaining);
        int iWidth  = PIXELVAL(pV, WIDTH, pBox->iContaining);
        int iTop    = PIXELVAL(pV, TOP, pBox->height);
        int iBottom = PIXELVAL(pV, BOTTOM, pBox->height);
        int iHeight = PIXELVAL(pV, HEIGHT, pBox->height);
        int iSpace;

        pNext = pList->pNext;

        if (HtmlDrawGetMarker(pStaticCanvas, pList->pMarker, &s_x, &s_y)) {
            /* If GetMarker() returns non-zero, then pList->pMarker is not
             * a part of pStaticCanvas. In this case do not draw the box
             * or remove the entry from the list either.
             */
            continue;
        }
        pList->pMarker = 0;

        nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);
        nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);

	/* Correct the static position for the padding edge offset. After the
         * correction the point (s_x, s_y) is the static position in pBox.
         */
        s_x -= x;
        s_y -= y;

        LOG {
            HtmlTree *pTree = pLayout->pTree;
            char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
            HtmlLog(pTree, "LAYOUTENGINE", "%s drawAbsolute() -> "
                "containing block is %dx%d", zNode, pBox->width, pBox->height
            );
            HtmlLog(pTree, "LAYOUTENGINE", "%s "
                "static position is (%d,%d) (includes correction of (%d,%d))", 
                zNode, s_x, s_y, x, y
            );
        }

        memset(&sContent, 0, sizeof(BoxContext));
        if (isReplaced) {
            sContent.iContaining = pBox->iContaining;
            drawReplacementContent(pLayout, &sContent, pNode);
            iWidth = sContent.width;
        }

        /* Determine a content-width for pNode, according to the following:
         *
	 * The sum of the following quantities is equal to the width of the
         * containing block.
         *
         *     + 'left' (may be "auto")
         *     + 'width' (may be "auto")
         *     + 'right' (may be "auto")
         *     + horizontal margins (one or both may be "auto")
         *     + horizontal padding and borders.
         */
        iSpace = pBox->iContaining - box.iLeft - box.iRight;
        if (
            iLeft != PIXELVAL_AUTO && 
            iRight != PIXELVAL_AUTO &&  
            iWidth != PIXELVAL_AUTO
        ) {
            iSpace -= (iLeft + iRight + iWidth);
            if (margin.leftAuto && margin.rightAuto) {
                if (iSpace < 0) {
                    margin.margin_right = iSpace;
                } else {
                    margin.margin_left = iSpace / 2;
                    margin.margin_right = iSpace - margin.margin_left;
                }
            } else if (margin.leftAuto) {
                margin.margin_left = iSpace;
            } else if (margin.rightAuto) {
                margin.margin_right = iSpace;
	    } else {
                iRight = PIXELVAL_AUTO;
            }
        }
        if (iLeft == PIXELVAL_AUTO && iRight == PIXELVAL_AUTO) {
            /* Box is underconstrained. Set 'left' to the static position */
            iLeft = s_x;
        }

        iSpace -= (margin.margin_left + margin.margin_right);
        iSpace -= (iLeft == PIXELVAL_AUTO ? 0: iLeft);
        iSpace -= (iRight == PIXELVAL_AUTO ? 0: iRight);
        iSpace -= (iWidth == PIXELVAL_AUTO ? 0: iWidth);

        if (
            iWidth == PIXELVAL_AUTO &&
            (iLeft == PIXELVAL_AUTO || iRight == PIXELVAL_AUTO)
        ) {
            int min;
            int max;
            assert(iRight != PIXELVAL_AUTO || iLeft != PIXELVAL_AUTO);
            blockMinMaxWidth(pLayout, pNode, &min, &max);
            iWidth = MIN(MAX(min, iSpace), max);
            iSpace -= iWidth;
        }

        /* At this point at most 1 of iWidth, iLeft and iRight can be "auto" */
        if (iWidth == PIXELVAL_AUTO) {
            assert(iLeft != PIXELVAL_AUTO && iRight != PIXELVAL_AUTO);
            iWidth = iSpace;
        } else if (iLeft == PIXELVAL_AUTO) {
            assert(iWidth != PIXELVAL_AUTO && iRight != PIXELVAL_AUTO);
            iLeft = iSpace;
        } else if (iRight == PIXELVAL_AUTO) {
            assert(iWidth != PIXELVAL_AUTO && iLeft != PIXELVAL_AUTO);
            iRight = iSpace;
        }

        /* Layout the content into sContent */
        if (!nodeIsReplaced(pNode)) {
            sContent.iContaining = iWidth;
            HtmlLayoutNodeContent(pLayout, &sContent, pNode);
        }

        /* Determine a content-height for pNode, according to the following:
         *
	 * The sum of the following quantities is equal to the width of the
         * containing block.
         *
         *     + 'top' (may be "auto")
         *     + 'height' (may be "auto")
         *     + 'bottom' (may be "auto")
         *     + vertical margins (one or both may be "auto")
         *     + vertical padding and borders.
         */
        iSpace = pBox->height - box.iTop - box.iBottom;
        if (
            iTop != PIXELVAL_AUTO && 
            iBottom != PIXELVAL_AUTO &&  
            iHeight != PIXELVAL_AUTO
        ) {
            iSpace -= (iTop + iBottom + iHeight);
            if (margin.topAuto && margin.bottomAuto) {
                if (iSpace < 0) {
                    margin.margin_bottom = iSpace;
                } else {
                    margin.margin_top = iSpace / 2;
                    margin.margin_bottom = iSpace - margin.margin_top;
                }
            } else if (margin.topAuto) {
                margin.margin_top = iSpace;
            } else if (margin.bottomAuto) {
                margin.margin_bottom = iSpace;
	    } else {
                iBottom = PIXELVAL_AUTO;
            }
        }
        if (iTop == PIXELVAL_AUTO && iBottom == PIXELVAL_AUTO) {
            /* Box is underconstrained. Set 'top' to the static position */
            iTop = s_y;
        }

        iSpace -= (margin.margin_top + margin.margin_bottom);
        iSpace -= (iTop == PIXELVAL_AUTO ? 0: iTop);
        iSpace -= (iBottom == PIXELVAL_AUTO ? 0: iBottom);
        iSpace -= (iHeight == PIXELVAL_AUTO ? 0: iHeight);

        if (
            iHeight == PIXELVAL_AUTO &&
            (iTop == PIXELVAL_AUTO || iBottom == PIXELVAL_AUTO)
        ) {
            assert(iTop != PIXELVAL_AUTO || iBottom != PIXELVAL_AUTO);
            iHeight = sContent.height;
            iSpace -= iHeight;
        }

        /* At this point at most 1 of iWidth, iLeft and iRight can be "auto" */
        if (iHeight == PIXELVAL_AUTO) {
            assert(iTop != PIXELVAL_AUTO && iBottom != PIXELVAL_AUTO);
            iHeight = iSpace;
        } else if (iTop == PIXELVAL_AUTO) {
            assert(iHeight != PIXELVAL_AUTO && iBottom != PIXELVAL_AUTO);
            iTop = iSpace;
        } else if (iBottom == PIXELVAL_AUTO) {
            assert(iHeight != PIXELVAL_AUTO && iTop != PIXELVAL_AUTO);
            iBottom = iSpace;
        }

        sContent.height = iHeight;
        memset(&sBox, 0, sizeof(BoxContext));
        sBox.iContaining = pBox->iContaining;
        wrapContent(pLayout, &sBox, &sContent, pNode);

        LOG {
            HtmlTree *pTree = pLayout->pTree;
            char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
            HtmlLog(pTree, "LAYOUTENGINE", "%s Calculated values: "
                "left=%d right=%d top=%d bottom=%d width=%d height=%d", 
                zNode, iLeft, iRight, iTop, iBottom, iWidth, iHeight
            );
        }

        DRAW_CANVAS(&pBox->vc, &sBox.vc, iLeft, iTop+margin.margin_top, pNode);

        if (pLayout->pAbsolute == pList) {
            pLayout->pAbsolute = pList->pNext;
        } else {
            NodeList *pTmp = pLayout->pAbsolute;
            for (; pTmp->pNext != pList; pTmp = pTmp->pNext);
            pTmp->pNext = pList->pNext;
        }
        HtmlFree(pList);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * wrapContent --
 *
 *     BORDERS + BACKGROUND
 *
 *         The box context pointed to by pContent contains a content block
 *         with it's top-left at coordinates (0, 0). The width and height
 *         of the conten are given by pContent->width and pContent->height,
 *         respectively. This function adds the background/borders box to
 *         the content and moves the result into box context pBox.
 *
 *         After this function returns, the point (0, 0) in pBox->vc
 *         corresponds to the top-left of the box including padding,
 *         borders and horizontal (but not vertical) margins. The values
 *         stored in pBox->width and pBox->height apply to the same box
 *         (including horizontal, but not vertical, margins).
 *
 *         Any percentage padding or margin values are calculated with
 *         respect to the value in pBox->iContaining. A value of "auto" for
 *         the left or right margin is treated as 0.
 *
 *     RELATIVE POSITIONING
 *
 *         If the value of the 'position' property for pNode is "relative",
 *         then the required offset (if any) is taken into account when
 *         drawing to pBox->vc. The static position of the element is still
 *         described by the (0, 0) point, pBox->width and pBox->height.
 *         Percentage values of properties 'left' and 'right', are
 *         calculated with respect to pBox->iContaining. 
 *
 *         Percentage values for 'top' and 'bottom' are treated as zero.
 *         TODO: This is a bug.
 *
 *     ABSOLUTE POSITIONING
 *
 *         If the value of the 'position' property is anything other than
 *         "static", then any absolutely positioned nodes accumulated in
 *         pLayout->pAbsolute are laid out into pBox with respect to the
 *         padding edge of pNode.
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
wrapContent(pLayout, pBox, pContent, pNode)
    LayoutContext *pLayout;
    BoxContext *pBox;
    BoxContext *pContent;
    HtmlNode *pNode;
{
    HtmlComputedValues *pV = pNode->pPropertyValues;
    MarginProperties margin;      /* Margin properties of pNode */
    BoxProperties box;            /* Box properties of pNode */
    int x;
    int y;
    int w;
    int h;

    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);
    nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);

    x = margin.margin_left;
    y = 0;

    if (pV->ePosition == CSS_CONST_RELATIVE) {
        assert(pV->position.iLeft != PIXELVAL_AUTO);
        assert(pV->position.iTop != PIXELVAL_AUTO);
        assert(pV->position.iLeft == -1 * pV->position.iRight);
        assert(pV->position.iTop == -1 * pV->position.iBottom);
        x += PIXELVAL(pV, LEFT, pBox->iContaining);
        y += PIXELVAL(pV, TOP, 0);
    }

    w = box.iLeft + pContent->width + box.iRight;
    h = box.iTop + pContent->height + box.iBottom;
    HtmlDrawBox(&pBox->vc, x, y, w, h, pNode, 0, pLayout->minmaxTest);

    x += box.iLeft;
    y += box.iTop;
    DRAW_CANVAS(&pBox->vc, &pContent->vc, x, y, pNode);

    pBox->width = MAX(pBox->width, 
        margin.margin_left + box.iLeft + 
        pContent->width + 
        box.iRight + margin.margin_right
    );
    pBox->height = MAX(pBox->height, 
        box.iTop + pContent->height + box.iBottom
    );

    if (pV->ePosition != CSS_CONST_STATIC && pLayout->pAbsolute) {
        BoxContext sAbsolute;
        int iLeftBorder = 0;
        int iTopBorder = 0;

        memset(&sAbsolute, 0, sizeof(BoxContext));
        sAbsolute.height = pContent->height;
        sAbsolute.height += box.iTop;
        sAbsolute.height += box.iBottom;
        if (pV->eBorderTopStyle != CSS_CONST_NONE) {
            iTopBorder = pV->border.iTop;
            sAbsolute.height -= iTopBorder;
        }
        if (pV->eBorderBottomStyle != CSS_CONST_NONE) {
            sAbsolute.height -= pV->border.iBottom;
        }
        sAbsolute.width = pContent->width;
        sAbsolute.width += box.iLeft;
        sAbsolute.width += box.iRight;
        if (pV->eBorderLeftStyle != CSS_CONST_NONE) {
            iLeftBorder = pV->border.iLeft;
            sAbsolute.width -= iLeftBorder;
        }
        if (pV->eBorderRightStyle != CSS_CONST_NONE) {
            sAbsolute.width -= pV->border.iRight;
        }
        sAbsolute.iContaining = sAbsolute.width;
        drawAbsolute(pLayout, &sAbsolute, &pBox->vc,
            PIXELVAL(pV, PADDING_LEFT, pBox->iContaining) + margin.margin_left,
            PIXELVAL(pV, PADDING_TOP, pBox->iContaining)
        );
        DRAW_CANVAS(&pBox->vc, &sAbsolute.vc, 
            margin.margin_left + iLeftBorder, iTopBorder, pNode);
    }
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
    int iMinWidth;                     /* Minimum from blockMinMaxWidth */
    int iContaining = pBox->iContaining;
    HtmlFloatList *pFloat = pNormal->pFloat;

    int iLeftFloat = 0;
    int iRightFloat = pBox->iContaining;

    int iWidth;                        /* Calculated content width */
    int eAlign = CSS_CONST_LEFT;

    int x, y;                     /* Coords for content to be drawn */
    BoxContext sContent;          /* Box context for content to be drawn into */
    BoxContext sBox;              /* Box context for sContent + borders */
    MarginProperties margin;      /* Margin properties of pNode */
    BoxProperties box;            /* Box properties of pNode */

    nodeGetMargins(pLayout, pNode, iContaining, &margin);
    nodeGetBoxProperties(pLayout, pNode, iContaining, &box);

    /* Account for the 'margin-top' property of this node. The margin always
     * collapses for a table element.
     */
    normalFlowMarginAdd(pNormal, margin.margin_top);
    normalFlowMarginCollapse(pNormal, pY);

    /* Note: Passing 10000 as the required height means in some (fairly
     * unlikely) circumstances the table will be placed lower in the flow
     * than would have been necessary. But it's not that big of a deal.
     */
    blockMinMaxWidth(pLayout, pNode, &iMinWidth, 0);
    iMinWidth += margin.margin_left + margin.margin_right;
    iMinWidth += box.iLeft + box.iRight;
    *pY = HtmlFloatListPlace(pFloat, iContaining, iMinWidth, 10000, *pY);
    HtmlFloatListMargins(pFloat, *pY, *pY + 10000, &iLeftFloat, &iRightFloat);

    iWidth = PIXELVAL(
        pNode->pPropertyValues, WIDTH, 
        pLayout->minmaxTest ? PIXELVAL_AUTO : pBox->iContaining
    );
    if (iWidth == PIXELVAL_AUTO) {
        iWidth = iRightFloat - iLeftFloat;
        iMinWidth -= (margin.margin_left + margin.margin_right);
        iMinWidth -= (box.iLeft + box.iRight);
    } 

    memset(&sContent, 0, sizeof(BoxContext));
    memset(&sBox, 0, sizeof(BoxContext));
    sContent.iContaining = iWidth;
    HtmlLayoutNodeContent(pLayout, &sContent, pNode);
    sBox.iContaining = iContaining;
    wrapContent(pLayout, &sBox, &sContent, pNode);

    y = HtmlFloatListPlace(
            pFloat, pBox->iContaining, sBox.width, sBox.height, *pY);
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
    int iMPB;                         /* Sum of margins, padding borders */
    int iWidth;                       /* Content width of pNode in pixels */
    int iWrappedX = 0;                /* X-offset of wrapped content */

    int yBorderOffset;     /* Y offset for top of block border */
    int x, y;              /* Coords for content to be drawn in pBox */
    BoxContext sContent;   /* Box context for content to be drawn into */
    BoxContext sBox;       /* sContent + borders */
    BoxContext sTmp;       /* Used to offset content */

    memset(&sContent, 0, sizeof(BoxContext));
    memset(&sBox, 0, sizeof(BoxContext));
    memset(&sTmp, 0, sizeof(BoxContext));

    nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);
    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);

    /* Calculate iWidth and xBorderLeft. Both are interpreted as pixel values.
     * For a non-replaced block element, the width is always as calculated
     * here, even if the content is not as wide.
     */
    iWidth = PIXELVAL(
        pV, WIDTH, pLayout->minmaxTest ? PIXELVAL_AUTO : pBox->iContaining
    );
    
    iMPB = box.iLeft + box.iRight + margin.margin_left + margin.margin_right;
    if (iWidth == PIXELVAL_AUTO) {
	/* If 'width' is set to "auto", then treat an "auto" value for
	 * 'margin-left' or 'margin-right' as 0. Then calculate the width
	 * available for the content by subtracting the margins, padding and
	 * borders from the width of the containing block.
         */
        sContent.iContaining = pBox->iContaining - iMPB;
    } else {
        int iSpareWidth = pBox->iContaining - iWidth - iMPB;
        if (margin.leftAuto && margin.rightAuto) {
            iWrappedX = iSpareWidth / 2;
        }
        else if (margin.leftAuto) {
            iWrappedX = iSpareWidth;
        } 
        sContent.iContaining = iWidth;
    }
    sContent.width = sContent.iContaining;

    /* Account for the 'margin-top' property of this node. */
    normalFlowMarginAdd(pNormal, margin.margin_top);

    /* If this box has either top-padding or a top border, then collapse the
     * vertical margin between this block and the one above now. 
     */
    if (box.iTop > 0) normalFlowMarginCollapse(pNormal, pY); 
    yBorderOffset = normalFlowMarginQuery(pNormal);

    /* Calculate x and y as pixel values. */
    *pY += box.iTop;
    y = *pY;
    x = iWrappedX + margin.margin_left + box.iLeft;

    /* Set up the box-context used to draw the content. */
    HtmlFloatListNormalize(pNormal->pFloat, -1 * x, -1 * y);

    /* Layout the content of this non-replaced block box. For this kind
     * of box, we treat any computed 'height' value apart from "auto" as a
     * minimum height.
     */
    normalFlowLayout(pLayout, &sContent, pNode, pNormal);
    assert(pV->iHeight >= 0 || pV->iHeight == PIXELVAL_AUTO);
    sContent.height = MAX(pV->iHeight, sContent.height); 
    assert(sContent.height >= pV->iHeight);

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        const char *zFmt = "%s normalFlowLayoutBlock() -> content size: %dx%d";
        const char *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        HtmlLog(pTree, "LAYOUTENGINE", zFmt, zNode, sContent.width, 
            sContent.height);
    }

    /* Re-normalize the float-list. */
    HtmlFloatListNormalize(pNormal->pFloat, x, y);

    if (box.iBottom > 0 && 0) {
        normalFlowMarginCollapse(pNormal, &sContent.height);
    } 
    *pY += sContent.height;
    *pY += box.iBottom;

    sBox.iContaining = pBox->iContaining;
    DRAW_CANVAS(&sTmp.vc, &sContent.vc, 0, -1 * yBorderOffset, pNode);
    sTmp.width = sContent.width;
    sTmp.height = sContent.height - yBorderOffset;
    wrapContent(pLayout, &sBox, &sTmp, pNode);
    DRAW_CANVAS(&pBox->vc, &sBox.vc,iWrappedX, y-box.iTop+yBorderOffset, pNode);
    pBox->width = MAX(pBox->width, sBox.width);
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
        pNormal->iMaxMargin = MAX(pNormal->iMaxMargin - ydiff, 0);
        if (!pNormal->nonegative) pNormal->iMinMargin = 0;
        pNormal->iMinMargin -= ydiff;
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
 * normalFlowLayoutListItem --
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
normalFlowLayoutListItem(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    MarkerLayoutArgs sArgs;
    NormalFlowCallback sCallback;

    sCallback.xCallback = markerLayoutCallback;
    sCallback.clientData = (ClientData) &sArgs;
    sCallback.pNext = 0;
    sArgs.pLayout = pLayout;
    sArgs.pBox = pBox;
    sArgs.pNode = pNode;
    sArgs.y = *pY;
           
    normalFlowCbAdd(pNormal, &sCallback);
    normalFlowLayoutBlock(pLayout, pBox, pNode, pY, pContext, pNormal);
    normalFlowCbDelete(pNormal, &sCallback);

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutText --
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
normalFlowLayoutText(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    HtmlInlineContextAddText(pContext, pNode);
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutReplacedInline --
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
normalFlowLayoutReplacedInline(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
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

    yoffset = -1 * (iHeight - margin.margin_bottom);
    if (pNode->pReplacement) {
      yoffset += pNode->pReplacement->iOffset;
    }

    memset(&sBox2, 0, sizeof(BoxContext));
    DRAW_CANVAS(&sBox2.vc, &sBox.vc, 0, margin.margin_top, pNode);
	    HtmlInlineContextAddBox(pContext, pNode, &sBox2.vc,
        sBox.width, iHeight, yoffset);
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutInline --
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
normalFlowLayoutInline(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    int i;
    InlineBorder *pBorder;
    pBorder = HtmlGetInlineBorder(pLayout, pNode, 0);
    HtmlInlineContextPushBorder(pContext, pBorder);
    for(i=0; i < HtmlNodeNumChildren(pNode); i++) {
        HtmlNode *pChild = HtmlNodeChild(pNode, i);
        normalFlowLayoutNode(pLayout, pBox, pChild, pY, pContext, pNormal);
    }
    HtmlInlineContextPopBorder(pContext, pBorder);
    return 0;
}

static int 
normalFlowLayoutAbsolute(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    int y = *pY + normalFlowMarginQuery(pNormal);
    NodeList *pNew = (NodeList *)HtmlClearAlloc(sizeof(NodeList));
    pNew->pNode = pNode;
    pNew->pNext = pLayout->pAbsolute;
    pNew->pMarker = HtmlDrawAddMarker(&pBox->vc, 0, y, 0);
    pLayout->pAbsolute = pNew;
    return 0;
}

static int 
normalFlowLayoutFixed(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    int y = *pY + normalFlowMarginQuery(pNormal);
    NodeList *pNew = (NodeList *)HtmlClearAlloc(sizeof(NodeList));
    pNew->pNode = pNode;
    pNew->pNext = pLayout->pFixed;
    pNew->pMarker = HtmlDrawAddMarker(&pBox->vc, 0, y, 0);
    pLayout->pFixed = pNew;
    return 0;
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
    typedef struct FlowType FlowType;
    struct FlowType {
        char *z;
        int doDrawLines;          /* True to call inlineLayoutDrawLines() */
        int doClearFloat;         /* True to call normalFlowClearFloat() */
        int doLineBreak;          /* Make a line-break (i.e. for <br>) */
        FlowLayoutFunc *xLayout;  /* Layout function to invoke */
    };

    /* Look-up table used by this function. */
    #define F(z, d, c, l, x) static FlowType FT_ ## z = {#z, d, c, l, x}
    F( NONE,            0, 1, 0, 0);
    F( BLOCK,           1, 1, 0, normalFlowLayoutBlock);
    F( FLOAT,           0, 1, 0, normalFlowLayoutFloat);
    F( TABLE,           1, 1, 0, normalFlowLayoutTable);
    F( BR,              1, 1, 1, normalFlowLayoutBlock);
    F( BLOCK_REPLACED,  1, 1, 0, normalFlowLayoutReplaced);
    F( LIST_ITEM,       1, 1, 0, normalFlowLayoutListItem);
    F( TEXT,            0, 0, 0, normalFlowLayoutText);
    F( INLINE,          0, 0, 0, normalFlowLayoutInline);
    F( INLINE_REPLACED, 0, 0, 0, normalFlowLayoutReplacedInline);
    F( ABSOLUTE,        0, 0, 0, normalFlowLayoutAbsolute);
    F( FIXED,           0, 0, 0, normalFlowLayoutFixed);
    #undef F

    /* 
     * Note about FT_NONE : The CSS 2.1 spec says, in section 9.2.4,
     * that an element with display 'none' has no effect on layout at all.
     * But rendering of http://slashdot.org depends on honouring the
     * 'clear' property on an element with display 'none'. And Mozilla,
     * KHTML and Opera do so.  Find out about this and if there are any
     * other properties that need handling here.
     */

    HtmlComputedValues *pV = pNode->pPropertyValues;
    int eDisplay   = DISPLAY(pV);
    FlowType *pFlow = &FT_NONE;

    if (HtmlNodeIsText(pNode)) {
        pFlow = &FT_TEXT;
    } else if (eDisplay == CSS_CONST_INLINE) {
        pFlow = &FT_INLINE;
        if (nodeIsReplaced(pNode)) {
            pFlow = &FT_INLINE_REPLACED;
        } 
    } else if (pNode->pPropertyValues->ePosition == CSS_CONST_ABSOLUTE) {
        pFlow = &FT_ABSOLUTE;
    } else if (pNode->pPropertyValues->ePosition == CSS_CONST_FIXED) {
        pFlow = &FT_FIXED;
    } else if (pNode->pPropertyValues->eFloat != CSS_CONST_NONE) {
        pFlow = &FT_FLOAT;
    } else if (nodeIsReplaced(pNode)) {
        pFlow = &FT_BLOCK_REPLACED;
    } else if (eDisplay == CSS_CONST_BLOCK) {
        pFlow = &FT_BLOCK;
        if (HtmlNodeTagType(pNode) == Html_BR) {
            pFlow = &FT_BR;
        } 
    } else if (eDisplay == CSS_CONST_LIST_ITEM) {
        pFlow = &FT_LIST_ITEM;
    } else if (eDisplay == CSS_CONST_TABLE) {
        pFlow = &FT_TABLE;
    }

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        const char *zFmt = "%s normalFlowLayoutNode() -> layout as type \"%s\"";
        const char *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        HtmlLog(pTree, "LAYOUTENGINE", zFmt, zNode, pFlow->z);
    }

    if (pFlow->doLineBreak && HtmlInlineContextIsEmpty(pContext)) {
        *pY += pV->fFont->em_pixels;
    }
    if (pFlow->doDrawLines) {
        inlineLayoutDrawLines(pLayout, pBox, pContext, 1, pY, pNormal);
    }
    if (pFlow->doClearFloat) {
        *pY = normalFlowClearFloat(pBox, pNode, pNormal, *pY);
    }
    if (pFlow->xLayout) {
        pFlow->xLayout(pLayout, pBox, pNode, pY, pContext, pNormal);
    }

    /* See if there are any complete line-boxes to copy to the main canvas. */
    inlineLayoutDrawLines(pLayout, pBox, pContext, 0, pY, pNormal);

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayout --
 *
 *     This function is called in two circumstances:
 *
 *         1. When pNode creates a new normal flow context and
 *         2. When pNode is a non-replaced, non-floating block box in a normal
 *            flow (recursively from normalFlowLayoutBlock()).
 *
 *     In either case, the content of pNode is drawn to pBox->vc with the
 *     top-left hand corner at canvas coordinates (0, 0). It is the callers
 *     responsibility to deal with margins, border padding and background. 
 *
 *     When this function is called, pBox->iContaining should contain the width
 *     available for the content of pNode (*not* the width of pNode's
 *     containing block as for FlowLayoutFunc functions).
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
    NodeList *pAbsolute = pLayout->pAbsolute;
    NodeList *pFixed = pLayout->pFixed;

    int left = 0; 
    int right = pBox->iContaining;
    int overhang;
    int hasNormalCb = (pNormal->pCallbackList ? 1 : 0);

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
     * will be). A cached layout is used only if all of the following
     * conditions are met:
     *
     *      1. The widget -layoutcache option is set to true.
     *      2. Function is not being called as part of a min-max width test.
     *      3. A valid layout cache exists.
     *      4. The width of the containing block is the same as it was when 
     *         the cache was generated.
     *      5. The vertical margins that will collapse with the top margin of 
     *         the first block in this flow are the same as they were when the
     *         cache was generated.
     *      6. There are no list marker boxes waiting to be positioned based on      *         the layout of this node.
     *      7. The current floating margins are the same as they were when 
     *         the cache was generated and there are no new floating margins
     *         in the float list that affect the area where the cached 
     *         layout is to be placed.
     */
    HtmlFloatListMargins(pFloat, 0, 1, &left, &right);
    if (
        pLayout->pTree->options.layoutcache &&                         /* 1 */
        !pLayout->minmaxTest &&                                        /* 2 */
        pCache && (pCache->flags & CACHE_LAYOUT_VALID) &&              /* 3 */
        pBox->iContaining == pCache->iContaining &&                    /* 4 */
        pNormal->isValid    == pCache->normalFlowIn.isValid &&         /* 5 */
        pNormal->iMinMargin == pCache->normalFlowIn.iMinMargin &&   
        pNormal->iMaxMargin == pCache->normalFlowIn.iMaxMargin &&
        !hasNormalCb &&                                                /* 6 */
        left == pCache->iFloatLeft && right == pCache->iFloatRight &&  /* 7 */
        HtmlFloatListIsConstant(pFloat, 0, pCache->iHeight)
    ) {
        /* Hooray! A cached layout can be used. */
        assert(!pBox->vc.pFirst);
        assert(!pLayout->minmaxTest);
        HtmlDrawCopyCanvas(&pBox->vc, &pCache->canvas);
        pBox->width = pCache->iWidth;
        assert(pCache->iHeight >= pBox->height);
        pBox->height = pCache->iHeight;
        pNormal->iMaxMargin = pCache->normalFlowOut.iMaxMargin;
        pNormal->iMinMargin = pCache->normalFlowOut.iMinMargin;
        pNormal->isValid = pCache->normalFlowOut.isValid;
        return;
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

    /* TODO: Danger! elements with "position:relative" might break this? */
    overhang = MAX(0, pBox->vc.bottom - pBox->height);

    if (
        pLayout->pTree->options.layoutcache && 
        !pLayout->minmaxTest && 
        pCache->iFloatLeft == left &&
        pCache->iFloatRight == right &&
        HtmlFloatListIsConstant(pFloat, pBox->height, overhang) &&
        !pNormal->pCallbackList && !hasNormalCb &&
        pLayout->pAbsolute == pAbsolute &&
        pLayout->pFixed == pFixed
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

    return;
}

/*
 *---------------------------------------------------------------------------
 *
 * blockMinMaxWidth --
 *
 *     Figure out the minimum and maximum widths that the content generated by
 *     pNode may use. This is used during table floating box layout.
 *
 *     The returned widths do not include the borders, padding or margins of
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
        HtmlLayoutNodeContent(pLayout, &sBox, pNode);
        HtmlDrawCleanup(&sBox.vc);
        min = sBox.width;
    
        /* Figure out the maximum width of the box by pretending to lay it
         * out with a very large parent width. It is not expected to
         * be a problem that tables may be layed out incorrectly on
         * displays wider than 10000 pixels.
         */
        memset(&sBox, 0, sizeof(BoxContext));
        sBox.iContaining = 10000;
        HtmlLayoutNodeContent(pLayout, &sBox, pNode);
        HtmlDrawCleanup(&sBox.vc);
        max = sBox.width;

        assert(max>=min);
        pCache->iMinWidth = min;
        pCache->iMaxWidth = max;
        pCache->flags |= CACHE_MINMAX_VALID;

        pLayout->minmaxTest = minmaxTestOrig;

	/* Log the fact that we just calculated a new minimum and maximum
	 * width. Don't use the LOG macro, as this message should be logged
	 * regardless of the value of pLayout->minmaxTest.
         */
	if (pLayout->pTree->options.logcmd) {
            HtmlTree *pTree = pLayout->pTree;
            HtmlLog(pTree, "LAYOUTENGINE", "%s blockMinMaxWidth() -> "
                "min=%d max=%d",
                Tcl_GetString(HtmlNodeCommand(pTree, pNode)), min, max
            );
        }
    } 

    if (pMin) *pMin = min;
    if (pMax) *pMax = max;

    return TCL_OK;
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
    if (nWidth < 5) {
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
        int x;
        int y;

        MarginProperties margin;
        BoxProperties box;
        BoxContext sContent;
        BoxContext sFixed;

        nodeGetMargins(&sLayout, pBody, nWidth, &margin);
        nodeGetBoxProperties(&sLayout, pBody, nWidth, &box);
        
        memset(&sContent, 0, sizeof(BoxContext));
        sContent.iContaining = sBox.iContaining - 
            margin.margin_left - margin.margin_right - box.iLeft - box.iRight;

        sLayout.pTop = pBody;
        HtmlLayoutNodeContent(&sLayout, &sContent, pBody);

        x = MAX(-1 * sBox.vc.left, 0) + margin.margin_left + box.iLeft;
        y = MAX(-1 * sBox.vc.top, 0) + margin.margin_top + box.iTop;

        drawAbsolute(&sLayout, &sContent, &sContent.vc, -1 * x, -1 * y);

        memset(&sFixed, 0, sizeof(BoxContext));

        assert(sLayout.pAbsolute == 0);
        sLayout.pAbsolute = sLayout.pFixed;
        sLayout.pFixed = 0;
        sFixed.iContaining = sContent.iContaining;
        sFixed.height = Tk_Height(pTree->tkwin);
        if (sFixed.height < 5) sFixed.height = pTree->options.height;
        HtmlDrawAddMarker(&sFixed.vc, 0, 0, 1);
        drawAbsolute(&sLayout, &sFixed, &sContent.vc, -1 * x, -1 * y);

        HtmlDrawCanvas(&pTree->canvas, &sContent.vc, x, y, pBody);
        HtmlDrawCanvas(&pTree->canvas, &sFixed.vc, x, y, pBody);

        pTree->canvas.right = MAX(
            pTree->canvas.right,
            margin.margin_left + box.iLeft + 
            sContent.width + 
            box.iRight + margin.margin_right
        );
        pTree->canvas.bottom = MAX(
            pTree->canvas.bottom,
            margin.margin_top + box.iTop + 
            sContent.height + 
            box.iBottom + margin.margin_bottom
        );
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

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutInvalidateCache --
 * 
 *     Invalidate the layout-cache for the specified node. 
 *
 *     Note that the layout-caches of the parent and ancestor nodes are NOT
 *     invalidated. If the caller wants the layout to be recomputed next time
 *     HtmlLayout() is called, the layout-caches of the parent and ancestor
 *     nodes must also be invalidated (by calling this function).
 * 
 * Results:
 *     None.
 *
 * Side effects:
 *     Deletes the layout cache for node pNode.
 *
 *---------------------------------------------------------------------------
 */
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
