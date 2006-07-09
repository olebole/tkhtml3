
/*----------------------------------------------------------------------------
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
static const char rcsid[] = "$Id: htmldraw.c,v 1.135 2006/07/09 17:06:56 danielk1977 Exp $";

#include "html.h"
#include <assert.h>
#include <X11/Xutil.h>

/*-------------------------------------------------------------------------
 * OVERVIEW:
 *
 *     This file contains code and data structures for the runtime 
 *     document.
 *
 *     A runtime document consists of a set of primitives and a background
 *     color. Rendering the page is achieved by iterating through all
 *     primitives (in an order defined by the layout engine) and painting or
 *     positioning them relative to the widget window. A primitive may be one
 *     of the following:
 *
 *         * Box        - CSS style border and background.
 *         * Line       - Horizontal line used for 'text-decoration'.
 *         * Text       - Single line of text
 *         * Image      - An image. Used for replacement + list-marker images.
 *         * Window     - A Tk widget window (set via [node replace])
 *
 *     All web documents are reduced by the layout engine to zero or more 
 *     of these primitives. 
 *
 * USE CASES
 *
 *     The data structures in this module are designed to account for the
 *     the following four use cases:
 *
 *         1. Paint region. 
 *         2. Selection modification
 *         3. Find window coordinates (bbox) given node.
 *         4. Find node given window coordinates (a single point).
 *
 *     There are a few different variations on case (3).
 *
 */

/*
 * EXPORTED FUNCTIONS:
 *
 * Functions for keeping the display up to date:
 *
 *     HtmlWidgetRepair
 *     HtmlWidgetSetViewport
 *
 *         Repair() is used when a rectangular region of the viewport must
 *         be repainted (i.e.  to repair window damage). SetViewport() is
 *         used to scroll the window. It recalculates the positions of
 *         mapped windows.
 *
 *
 * Functions to query a canvas layout:
 *     HtmlWidgetDamageText
 *     HtmlWidgetNodeBox
 *     HtmlWidgetNodeTop
 *
 *         The NodeBox() function returns the canvas coordinates of a
 *         bounding-box for a supplied node. The NodeTop() function returns a
 *         single coordinate - the offset from the top of the canvas for a
 *         nominated node. 
 *       
 *         DamageText() is used to query for the bounding box of a region of
 *         text. However instead of returning coordinates, it invokes
 *         HtmlCallbackDamage() on the identified region.
 *
 * Tcl command functions:
 *     HtmlLayoutNode
 *     HtmlLayoutPrimitives
 *     HtmlLayoutImage
 *
 *         Implementations of the [widget node] [widget image] and [widget
 *         primiitives] commands. Note that the latter two are intended for
 *         debugging only and so are not really part of the public interface.
 *
 * Canvas management:
 *     HtmlDrawCanvas
 *     HtmlDrawCleanup
 *     HtmlDrawCopyCanvas
 *     HtmlDrawIsEmpty
 *
 * Functions for drawing primitives to a canvas:
 *     HtmlDrawOrigin
 *     HtmlDrawImage
 *     HtmlDrawWindow
 *     HtmlDrawText
 *     HtmlDrawBox
 *     HtmlDrawLine
 *     HtmlDrawOverflow
 *
 * Markers:
 *     HtmlDrawAddMarker
 *     HtmlDrawGetMarker
 */
#define CANVAS_TEXT     1
#define CANVAS_WINDOW   2
#define CANVAS_ORIGIN   3
#define CANVAS_IMAGE    4
#define CANVAS_BOX      5
#define CANVAS_LINE     6
#define CANVAS_MARKER   7
#define CANVAS_OVERFLOW 8

typedef struct CanvasText CanvasText;
typedef struct CanvasImage CanvasImage;
typedef struct CanvasBox CanvasBox;
typedef struct CanvasWindow CanvasWindow;
typedef struct CanvasOrigin CanvasOrigin;
typedef struct CanvasLine   CanvasLine;
typedef struct CanvasMarker CanvasMarker;
typedef struct CanvasOverflow CanvasOverflow;

typedef struct CanvasItemSorter CanvasItemSorter;
typedef struct CanvasItemSorterLevel CanvasItemSorterLevel;
typedef struct CanvasItemSorterSlot CanvasItemSorterSlot;

/* A single line of text. The relative coordinates (x, y) are as required
 * by Tk_DrawChars() - the far left-edge of the text baseline. The color
 * and font of the text are determined by the properties of CanvasText.pNode.
 */
struct CanvasText {
    int x;                   /* Relative x coordinate to render at */
    int y;                   /* Relative y coordinate to render at */
    int w;                   /* Width of the text */
    Tcl_Obj *pText;          /* Text to render */
    HtmlNode *pNode;         /* Text node */
    int iIndex;              /* Index in pNode text of this item (or -1) */
};

/* A square box, with borders, background color and image as determined
 * by the properties of pNode. Top-left hand corner is at (x, y). The
 * width and height of the box, as measured from the outer edge of the
 * borders, are w and h pixels, respectively. 
 */
struct CanvasBox {
    int x;                   /* Relative x coordinate to render at */
    int y;                   /* Relative y coordinate to render at */
    int w;                   /* Width of box area */
    int h;                   /* Height of box area */
    int flags;               /* Combination of CANVAS_BOX flags */
    HtmlNode *pNode;         /* Use computed properties from this node */
};

/* An image. Nothing to see here. */
struct CanvasImage {
    int x;                   /* Relative x coordinate to render at */
    int y;                   /* Relative y coordinate to render at */
    int w;                   /* Width of image region */
    int h;                   /* Height of image region */
    HtmlImage2 *pImage;      /* Image pointer */
    HtmlNode *pNode;         /* Associate document node */
};

/* This primitive is used to implement the 'text-decoration' property.
 * It draws zero or more horizontal lines of width CanvasLine.w starting 
 * at relative x-coordinate CanvasLine.x.
 *
 * The lines draw depend on the 'text-decoration' property of 
 * CanvasLine.pNode. If the property is set to 'overline', then the
 * relative y-coordinate of the line is CanvasLine.y_overline. Similarly
 * if the text-decoration property of the node is 'line-through' or 
 * 'underline', then CanvasLine.y_linethrough or CanvasLine.y_underline
 * is used as the relative y-coordinate of the rendered line.
 *
 * The color of the line is determined by the 'color' property of 
 * CanvasLine.pNode.
 *
 * Todo: Above, it says "zero or more" horizontal lines. At the moment, the
 * truth is that zero or one line is drawn. This is a limitation of
 * the HtmlComputedValues structure. Once that code is fixed, this primitive
 * may draw (for example) both an overline and a linethrough decoration.
 */
struct CanvasLine {
    int x;                   /* Relative x coordinate to render at */
    int y;                   /* Relative y coordinate for overline */
    int w;                   /* Width of line */
    int y_underline;         /* y coordinate for underline relative to "y" */
    int y_linethrough;       /* y coordinate for linethrough relative to "y" */
    HtmlNode *pNode;         /* Node pointer */
};

struct CanvasWindow {
    int x;                   /* Relative x coordinate */
    int y;                   /* Relative y coordinate */
    HtmlNode *pNode;         /* Node replaced by this window */
};

/*
 * CanvasOrigin primitives are used for two purposes:
 *
 *     1. To facilitate layout caching, and
 *     2. To speed up searches of the primitives list.
 *
 * CanvasOrigin primitives are added to the display list in pairs. One
 * primitive is added to the start of the display list, the other to the end
 * (ususually, primitives are only added to the end of the list).
 */
struct CanvasOrigin {
    int x;
    int y;
    int horizontal;
    int vertical;
    int nRef;
    HtmlCanvasItem *pSkip;
};

/*
 * A CanvasOverflow primitive is used to deal with blocks that have the
 * 'overflow' property set to something other than "visible".
 *
 * Currently, only "hidden" is handled (not "scroll" or "auto").
 */
struct CanvasOverflow {
    int x;
    int y;
    int w;
    int h;
    HtmlNode *pNode;
    HtmlCanvasItem *pEnd;          /* Region ends *after* this item */
};

/*
 * Markers are used for two unrelated purposes:
 *
 *     * They are inserted into the display list to record the static position
 *       of fixed or absolutely positioned elements. This sort of marker is
 *       always removed from the display list during the layout and has a
 *       CanvasMarker.flags value of 0. 
 *
 *     * To show where the "position:fixed" section of the display list begins.
 *       The x and y variables are ignored and the flags variable is set to
 *       MARKER_FIXED.
 */
#define MARKER_FIXED 0x00000001
struct CanvasMarker {
    int x;
    int y;
    int flags;
};

struct HtmlCanvasItem {
    int type;
    union {
        struct GenericItem {
            int x;
            int y; 
        } generic;
        CanvasText   t;
        CanvasWindow w;
        CanvasOrigin o;
        CanvasImage  i2;
        CanvasBox    box;
        CanvasLine   line;
        CanvasMarker marker;
        CanvasOverflow overflow;
    } x;
    HtmlCanvasItem *pNext;
};

#if 0
static void
CHECK_CANVAS(pCanvas) 
    HtmlCanvas *pCanvas;
{
    HtmlCanvasItem *pItem; 
    HtmlCanvasItem *pPrev = 0; 
    for (pItem = pCanvas->pFirst; pItem; pItem = pItem->pNext) {
        assert(!pPrev       || pPrev->pNext == pItem);
        assert(pPrev        || pCanvas->pFirst == pItem);
        assert(pItem->pNext || pItem == pCanvas->pLast);
        pPrev = pItem;
    }
}
#else
  #define CHECK_CANVAS(x)
#endif

/*
 * Every item in the canvas has an associated z-level (not to be confused with
 * the CSS property 'z-index'). A z-coord is an integer close to zero
 * calculated for each item as follows:
 *
 *     1. The z-level is initially 0.
 *     2. Add 10 for each ancestor that is positioned.
 *     2. Add 2 for each ancestor that is a floating box.
 *     3. Add 1 if the element is inline.
 *
 * This algorithm will have to change when the 'z-index' property is supported.
 * Right now it is not.
 */
struct CanvasItemSorter {
    int nLevel;                         /* Number of allocated levels */
    CanvasItemSorterLevel *aLevel;      /* Array of levels */  
};
struct CanvasItemSorterLevel {
    int iSlot;                       /* Index of next free entry in aSlot */
    int nSlot;                       /* Allocated size of aSlot */
    CanvasItemSorterSlot *aSlot;     /* Array of slots to store items */
};
struct CanvasItemSorterSlot {
    int x;                           /* item x-coord is relative to this */
    int y;                           /* item y-coord is relative to this */
    HtmlCanvasItem *pItem;           /* The item itself */
};

static void
sorterInsert(pSorter, pItem, x, y)
    CanvasItemSorter *pSorter;
    HtmlCanvasItem *pItem;
    int x;
    int y;
{
    int z = 0;
    HtmlNode *pNode = 0;
    CanvasItemSorterLevel *pLevel;
    CanvasItemSorterSlot *pSlot;
    switch( pItem->type) {
        case CANVAS_TEXT:
            pNode = pItem->x.t.pNode;
            break;
        case CANVAS_IMAGE:
            pNode = pItem->x.i2.pNode;
            break;
        case CANVAS_BOX:
            pNode = pItem->x.box.pNode;
            break;
        case CANVAS_LINE:
            pNode = pItem->x.line.pNode;
            break;
        case CANVAS_WINDOW:
            break;
        default:
            assert(!"bad type value");
    }

    if (pNode && !pNode->pPropertyValues) {
        pNode = HtmlNodeParent(pNode);
    }
    if (pNode) z = pNode->iZLevel;

    if (z >= pSorter->nLevel) {
        int n = pSorter->nLevel + 128;
        pSorter->aLevel = (CanvasItemSorterLevel *)HtmlRealloc(0, 
            pSorter->aLevel, n * sizeof(CanvasItemSorterLevel)
        );
        memset(&pSorter->aLevel[pSorter->nLevel], 0, 
            sizeof(CanvasItemSorterLevel) * 128
        );
        pSorter->nLevel = n;
    }
    pLevel = &pSorter->aLevel[z];

    assert(pLevel->nSlot >= pLevel->iSlot);
    if (pLevel->nSlot == pLevel->iSlot) {
        int n = pLevel->nSlot + 128;
        pLevel->aSlot = (CanvasItemSorterSlot *)HtmlRealloc(0,
            pLevel->aSlot, n * sizeof(CanvasItemSorterSlot)
        );
        memset(&pLevel->aSlot[pLevel->nSlot], 0, 
            sizeof(CanvasItemSorterSlot) * 128
        );
        pLevel->nSlot = n;
    }
    pSlot = &pLevel->aSlot[pLevel->iSlot];
    pLevel->iSlot++;
    
    pSlot->x = x;
    pSlot->y = y;
    pSlot->pItem = pItem;
}
static void
sorterIterate(pSorter, xCallback, clientData)
    CanvasItemSorter *pSorter;
    int (*xCallback)(HtmlCanvasItem *, int, int, ClientData);
    ClientData clientData;
{
    int ii;
    for (ii = 0; ii < pSorter->nLevel; ii++) {
        CanvasItemSorterLevel *pLevel = &pSorter->aLevel[ii];
        int jj;
        for (jj = 0; jj < pLevel->iSlot; jj++) {
            CanvasItemSorterSlot *pSlot = &pLevel->aSlot[jj];
            xCallback(pSlot->pItem, pSlot->x, pSlot->y, clientData);
        }
    }
}
static void
sorterReset(pSorter)
    CanvasItemSorter *pSorter;
{
    int ii;
    for (ii = 0; ii < pSorter->nLevel; ii++) {
        HtmlFree(0, pSorter->aLevel[ii].aSlot);
    }
    HtmlFree(0, pSorter->aLevel);
}



static HtmlCanvasItem *
allocateCanvasItem()
{
    return (HtmlCanvasItem *)HtmlClearAlloc(
        "Screen-graph item", sizeof(HtmlCanvasItem)
    );
}
static void
freeCanvasItem(p)
    HtmlCanvasItem *p;
{
    return HtmlFree("Screen-graph item", p);
}

static void 
windowsRepair(pTree, pCanvas)
    HtmlTree *pTree;
    HtmlCanvas *pCanvas;
{
    Tk_Window win = (pTree ? pTree->tkwin : 0);
    int w         = (win ? Tk_Width(win) : 0);
    int h         = (win ? Tk_Height(win): 0);

    HtmlNodeReplacement *p = pTree->pMapped;
    HtmlNodeReplacement *pPrev = 0;

    /* Loop through the HtmlCanvas.pMapped list. For each mapped window
     * that is clipped by the viewport, unmap the window (if mapped) and
     * remove it from the list. For each mapped window that is not clipped
     * by the viewport, reposition and map it (if unmapped).
     */
    while (p) {
        HtmlNodeReplacement *pNext = p->pNext;
        Tk_Window control = p->win;
        int iViewY; 
        int iWidth; 
        int iHeight; 
        int iViewX; 

        if (pTree) {
            iViewX = p->iCanvasX - pTree->iScrollX;
            iViewY = p->iCanvasY - pTree->iScrollY;
            iWidth = Tk_ReqWidth(control);
            iHeight = Tk_ReqHeight(control);
        }

        if (
            !pTree ||
            iViewX > w || iViewY > h || 
            (iViewX + iWidth) <= 0 || (iViewY + iHeight) <= 0
        ) {
            if (Tk_IsMapped(control)) {
                Tk_UnmapWindow(control);
            }
            if (pPrev) {
                assert(pPrev->pNext == p);
                pPrev->pNext = pNext;
            } else {
                assert(pTree->pMapped == p);
                pTree->pMapped = pNext;
            }
            p->pNext = 0;
        } else {
            Tk_MoveResizeWindow(control, iViewX, iViewY, iWidth, iHeight);
            if (!Tk_IsMapped(control)) {
                Tk_MapWindow(control);
            }
            pPrev = p;
        }

        p = pNext;
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawCleanup --
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
HtmlDrawCleanup(pTree, pCanvas)
    HtmlTree *pTree;
    HtmlCanvas *pCanvas;
{
    HtmlCanvasItem *pItem;
    HtmlCanvasItem *pPrev = 0;

    assert(pTree || !pCanvas->pFirst);

    if (&pTree->canvas == pCanvas) {
        HtmlNodeReplacement *p;
CHECK_CANVAS(pCanvas);
        for (p = pTree->pMapped; p; p = p->pNext) {
            p->iCanvasX = -10000;
            p->iCanvasY = -10000;
            assert(p->iCanvasX < 0 && p->iCanvasY < 0);
        }
    }

    pItem = pCanvas->pFirst;
    while (pItem) {
        Tcl_Obj *pObj = 0;
        int save = 0;
        switch (pItem->type) {
            case CANVAS_TEXT:
                pObj = pItem->x.t.pText;
                break;
            case CANVAS_IMAGE:
                HtmlImageFree(pItem->x.i2.pImage);
                break;
            case CANVAS_ORIGIN:
                assert(pItem->x.o.nRef >= 1 || !pItem->x.o.pSkip);
                if (pItem->x.o.pSkip) {
                    pItem->x.o.nRef--;
                    assert(pItem->x.o.pSkip->type == CANVAS_ORIGIN);
                    if (pItem->x.o.nRef > 0) {
                        assert(pItem->x.o.nRef == 1);
                        pItem = pItem->x.o.pSkip;
                        save = 1;
                    }
                }
                break;
            case CANVAS_MARKER:
                assert(pItem->x.marker.flags == MARKER_FIXED);
                break;
            case CANVAS_WINDOW:
            case CANVAS_BOX:
            case CANVAS_LINE:
            case CANVAS_OVERFLOW:
                break;
            default:
                assert(!"Canvas corruption");
        }
        if (pObj) {
            Tcl_DecrRefCount(pObj);
        }
        if (pPrev) {
            freeCanvasItem(pPrev);
        }
        pPrev = pItem;
        pItem = (pItem == pCanvas->pLast ? 0 : pItem->pNext);

        if (save) {
            assert(pPrev->type == CANVAS_ORIGIN && !pPrev->x.o.pSkip);
            if (pItem) pPrev->pNext = 0;
            pPrev = 0;
        }
    }

    if (pPrev) {
        freeCanvasItem(pPrev);
    }
    memset(pCanvas, 0, sizeof(HtmlCanvas));
}

/*
 *---------------------------------------------------------------------------
 *
 * linkItem --
 *
 *     Link the item pItem into the end of the pCanvas link-list.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void linkItem(pCanvas, pItem)
    HtmlCanvas *pCanvas;
    HtmlCanvasItem *pItem;
{
    pItem->pNext = 0;
    if (pCanvas->pFirst) {
        pCanvas->pLast->pNext = pItem;
    } else {
        pCanvas->pFirst = pItem;
    }
    pCanvas->pLast = pItem;
}

static HtmlFont *
fontFromNode(pNode)
    HtmlNode *pNode;
{
    HtmlNode *p = pNode;
    if (!p->pPropertyValues) {
        p = HtmlNodeParent(p);
    }
    assert(p && p->pPropertyValues);
    return p->pPropertyValues->fFont;
}
static HtmlColor *
colorFromNode(pNode)
    HtmlNode *pNode;
{
    HtmlNode *p = pNode;
    if (!p->pPropertyValues) {
        p = HtmlNodeParent(p);
    }
    assert(p && p->pPropertyValues);
    return p->pPropertyValues->cColor;
}

/*
 *---------------------------------------------------------------------------
 *
 * countPrimitives --
 *
 *     Count the number of primitives drawn directly into pCanvas.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#if 0
static int
countPrimitives(pCanvas)
    HtmlCanvas *pCanvas;
{
    HtmlCanvasItem *p;
    int n = 0;
    for (p = pCanvas->pFirst; p; p = p->pNext) {
        n++;
        if (p->type == CANVAS_ORIGIN) {
            p = p->x.o.pSkip;
        }
    }
    return n;
}
#endif

static void 
movePrimitives(pCanvas, x, y)
    HtmlCanvas *pCanvas;
    int x;
    int y;
{
    HtmlCanvasItem *p;

    /* Optimization. Do nothing for a +0+0 translation. */
    if (x == 0 && y == 0) {
        return;
    }

    for (p = pCanvas->pFirst; p; p = p->pNext) {
        p->x.generic.x += x;
        p->x.generic.y += y;
        if (p->type == CANVAS_ORIGIN) {
            p = p->x.o.pSkip;
            p->x.generic.x -= x;
            p->x.generic.y -= y;
        }
    }
}

void HtmlDrawOrigin(pCanvas)
    HtmlCanvas *pCanvas;
{
    HtmlCanvasItem *pItem;
    HtmlCanvasItem *pItem2;
    if (!pCanvas->pFirst) return;
    assert(pCanvas->pLast);

    /* Allocate the first CANVAS_ORIGIN item */
    pItem = allocateCanvasItem();
    memset(pItem, 0, sizeof(HtmlCanvasItem));
    pItem->x.o.horizontal = pCanvas->left;
    pItem->x.o.vertical = pCanvas->top;
    pItem->x.o.nRef = 1;
    pItem->type = CANVAS_ORIGIN;

    /* Add the first CANVAS_ORIGIN item to the start of the list */
    pItem->pNext = pCanvas->pFirst;
    pCanvas->pFirst = pItem;

    /* Allocate the second CANVAS_ORIGIN item */
    pItem2 = allocateCanvasItem();
    memset(pItem2, 0, sizeof(HtmlCanvasItem));
    pItem->x.o.pSkip = pItem2;
    pItem2->type = CANVAS_ORIGIN;
    pItem2->x.o.horizontal = pCanvas->right;
    pItem2->x.o.vertical = pCanvas->bottom;

    /* Add the first CANVAS_ORIGIN item to the end of the list */
    pCanvas->pLast->pNext = pItem2;
    pCanvas->pLast = pItem2;
}

void HtmlDrawOverflow(pCanvas, pNode, w, h)
    HtmlCanvas *pCanvas;
    HtmlNode *pNode;
    int w;
    int h;
{
    HtmlCanvasItem *pLast = pCanvas->pLast;
    HtmlCanvasItem *pItem;

    if (!pLast) return;

    pItem = allocateCanvasItem();
    pItem->type = CANVAS_OVERFLOW;
    pItem->x.overflow.pNode = pNode;
    pItem->x.overflow.w = w;
    pItem->x.overflow.h = h;
    pItem->x.overflow.pEnd = pLast;

    pItem->pNext = pCanvas->pFirst;
    pCanvas->pFirst = pItem;
}

void HtmlDrawCopyCanvas(pTo, pFrom)
    HtmlCanvas *pTo;
    HtmlCanvas *pFrom;
{
    assert(!pFrom->pFirst || pFrom->pFirst->type == CANVAS_ORIGIN);
    assert(!pFrom->pLast || 
       (pFrom->pLast->type == CANVAS_ORIGIN && pFrom->pLast->pNext == 0));

    memcpy(pTo, pFrom, sizeof(HtmlCanvas));

    if (pTo->pFirst) {
        assert(pTo->pFirst->x.o.nRef == 1);
        pTo->pFirst->x.o.nRef++;
        pTo->pFirst->x.o.x = 0;
        pTo->pFirst->x.o.y = 0;
        pTo->pLast->x.o.x = 0;
        pTo->pLast->x.o.y = 0;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * combineText --
 *
 *     This function is used to determine if two document tree nodes should be
 *     considered sequential for the purposes of drawing the selection
 *     background.
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
combineText(pNodeA, pNodeB) 
    HtmlNode *pNodeA;
    HtmlNode *pNodeB;
{
    HtmlNode *pA = 0;
    HtmlNode *pB = 0;
    HtmlNode *pCommon = 0;

    assert(HtmlNodeIsText(pNodeA));
    assert(HtmlNodeIsText(pNodeB));
    /* assert(pNodeA->iNode <= pNodeB->iNode); */
    if (pNodeA->iNode > pNodeB->iNode) return 0;

    for (pA = pNodeA; pA && !pCommon; pA = HtmlNodeParent(pA)) {
        for (pB = pNodeB; pB && !pCommon; pB = HtmlNodeParent(pB)) {
            if (pB == pA) {
                pCommon = pB;
            }
        }
    }

    if (!pCommon) return 0;

    assert(pCommon);
    for (pA = pNodeA; pA != pCommon; pA = HtmlNodeParent(pA)) {
        if (
            pA->pPropertyValues && (
                pA->pPropertyValues->eDisplay != CSS_CONST_INLINE ||
                pA->pPropertyValues->padding.iRight > 0 ||
                pA->pPropertyValues->margin.iRight > 0 ||
                (
                    pA->pPropertyValues->eBorderRightStyle != CSS_CONST_NONE &&
                    pA->pPropertyValues->border.iRight > 0
                )
            )
        ) {
            return 0;
        }
    }
    for (pB = pNodeB; pB != pCommon; pB = HtmlNodeParent(pB)) {
        if (
            pB->pPropertyValues && (
                pB->pPropertyValues->eDisplay != CSS_CONST_INLINE ||
                pB->pPropertyValues->padding.iLeft > 0 ||
                pB->pPropertyValues->margin.iLeft > 0 ||
                (
                    pB->pPropertyValues->eBorderLeftStyle != CSS_CONST_NONE &&
                    pB->pPropertyValues->border.iLeft > 0
                )
            )
        ) {
            return 0;
        }
    }
    
    return 1;
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawCanvas --
 *
 *     Transfer the contents one canvas (pCanvas2) to another (pCanvas) at
 *     location (x,y). i.e. a geometric primitive at location (a,b) in
 *     pCanvas2 is transfered to location (x+a,y+b) in pCanvas.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlDrawCanvas(pCanvas, pCanvas2, x, y, pNode)
    HtmlCanvas *pCanvas;
    HtmlCanvas *pCanvas2;
    int x;
    int y;
    HtmlNode *pNode;
{
CHECK_CANVAS(pCanvas);
CHECK_CANVAS(pCanvas2);
    if (pCanvas2->pFirst) {
        movePrimitives(pCanvas2, x, y);

        if (pCanvas->pLast) {
            HtmlCanvasItem *pT1 = pCanvas->pLast;
            HtmlCanvasItem *pT2 = pCanvas2->pFirst;
            int combined = 0;
            assert(pCanvas->pFirst);

            /* Special case: combine two text primitives: */
            if (
                pT1 && pT1->type == CANVAS_TEXT && 
                pT2 && pT2->type == CANVAS_TEXT &&
                pT1->x.t.pNode == pT2->x.t.pNode
            ) {
                int sw = fontFromNode(pT1->x.t.pNode)->space_pixels;
                if ((pT1->x.t.x + pT1->x.t.w + sw) == pT2->x.t.x) {
                    Tcl_AppendToObj(pT1->x.t.pText, " ", 1);
                    Tcl_AppendObjToObj(pT1->x.t.pText, pT2->x.t.pText);
                    pT1->x.t.w += (pT2->x.t.w + sw);

                    pCanvas2->pFirst = pT2->pNext;
                    Tcl_DecrRefCount(pT2->x.t.pText);
                    if (pCanvas2->pFirst == 0) {
                        assert(pCanvas2->pLast == pT2);
                        pCanvas2->pLast = 0;
                    } else {
                        assert(pCanvas2->pLast);
                    }
                    freeCanvasItem(pT2);
                    combined = 1;
                }
            }

            if (pT2->type == CANVAS_TEXT && !combined) {
                HtmlCanvasItem *p;
                HtmlCanvasItem *pT = 0;
                for (p = pCanvas->pFirst; p; p = p->pNext) {
                    if (p->type == CANVAS_TEXT) {
                        pT = p;
		    } else if (p->type==CANVAS_IMAGE||p->type==CANVAS_WINDOW) {
                        pT = 0;
                    } else if (p->type == CANVAS_ORIGIN) {
                        pT = 0; 
                        break;
                    }
                }
                if (
                    pT && 
                    (pT->x.t.x + pT->x.t.w) < pT2->x.t.x && 
                    combineText(pT->x.t.pNode, pT2->x.t.pNode)
                ) {
                    pT->x.t.w = (pT2->x.t.x - pT->x.t.x);
                }
            }

            pCanvas->pLast->pNext = pCanvas2->pFirst;
            if (pCanvas2->pLast) {
                pCanvas->pLast = pCanvas2->pLast;
            }

        } else {
            assert(!pCanvas->pFirst);
            pCanvas->pFirst = pCanvas2->pFirst;
            pCanvas->pLast = pCanvas2->pLast;
        }
    }

    pCanvas->left = MIN(pCanvas->left, x+pCanvas2->left);
    pCanvas->top = MIN(pCanvas->top, y+pCanvas2->top);
    pCanvas->bottom = MAX(pCanvas->bottom, y+pCanvas2->bottom);
    pCanvas->right = MAX(pCanvas->right, x+pCanvas2->right);
CHECK_CANVAS(pCanvas);
}

static int 
requireBox(pNode)
    HtmlNode *pNode;
{
    HtmlComputedValues *pV = pNode->pPropertyValues;
    if (
        pNode->pDynamic ||
        pV->cBackgroundColor->xcolor ||
        pV->imBackgroundImage ||
        pV->eBorderTopStyle != CSS_CONST_NONE ||
        pV->eBorderBottomStyle != CSS_CONST_NONE ||
        pV->eBorderRightStyle != CSS_CONST_NONE ||
        pV->eBorderLeftStyle != CSS_CONST_NONE
    ) {
        return 1;
    }
    return 0;
}

static HtmlNode *
itemToBox(pItem, origin_x, origin_y, pX, pY, pW, pH)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    int *pX;
    int *pY;
    int *pW;
    int *pH;
{
    switch (pItem->type) {
        case CANVAS_BOX: {
            int ow = 0;
/*
            HtmlComputedValues *pV = pItem->x.box.pNode->pPropertyValues;
            if (pV->eOutlineStyle != CSS_CONST_NONE) {
                ow = MAX(0, pV->iOutlineWidth);
            }
*/
            *pX = pItem->x.box.x + origin_x - ow;
            *pY = pItem->x.box.y + origin_y - ow;
            *pW = pItem->x.box.w + ow + ow;
            *pH = pItem->x.box.h + ow + ow;
            return pItem->x.box.pNode;
        }
        case CANVAS_TEXT: {
            HtmlFont *pFont = fontFromNode(pItem->x.t.pNode);
            *pX = pItem->x.t.x + origin_x;
            *pY = pItem->x.t.y + origin_y - pFont->metrics.ascent;
            *pW = pItem->x.t.w;
            *pH = pFont->metrics.ascent + pFont->metrics.descent;
            return pItem->x.t.pNode;
        }
        case CANVAS_IMAGE:
            *pX = pItem->x.i2.x + origin_x;
            *pY = pItem->x.i2.y + origin_y;
            *pW = pItem->x.i2.w;
            *pH = pItem->x.i2.h;
            return pItem->x.i2.pNode;
        case CANVAS_LINE:
            *pX = pItem->x.line.x + origin_x;
            *pY = pItem->x.line.y + origin_y;
            *pW = pItem->x.line.w;
            *pH = pItem->x.line.y_underline + 1;
            return pItem->x.line.pNode;
        case CANVAS_WINDOW: {
            HtmlNodeReplacement *pR = pItem->x.w.pNode->pReplacement;
            if (pR && pR->win) {
                Tk_Window control = pR->win;
                *pW = Tk_ReqWidth(control);
                *pH = Tk_ReqHeight(control);
            } else {
                *pW = 1;
                *pH = 1;
            }
            *pX = pItem->x.w.x + origin_x;
            *pY = pItem->x.w.y + origin_y;
            return 0;
        }
        default:
            assert(
                pItem->type==CANVAS_ORIGIN || 
                pItem->type==CANVAS_MARKER ||
                pItem->type==CANVAS_OVERFLOW 
            );
            return 0;
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawBox --
 *
 *     Draw a "box" based on the computed properties of node pNode. A "box"
 *     consists of zero or more of the following optional components:
 *
 *         - A border,
 *         - A solid background color,
 *         - A background image.
 *
 *     The (x,y) coordinate specifies the top-left hand corner of the box (the
 *     outer border pixel, if a border is defined). The specified width and
 *     height include any borders and padding, but do NOT include any space 
 *     for the outline (if any).
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Adds an item to the canvas pCanvas.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlDrawBox(pCanvas, x, y, w, h, pNode, flags, size_only)
    HtmlCanvas *pCanvas;
    int x;
    int y;
    int w;
    int h;
    HtmlNode *pNode;
    int flags;
    int size_only;
{
    if (!size_only && requireBox(pNode)) {
        int x1, y1, w1, h1;
        HtmlCanvasItem *pItem; 
        pItem = allocateCanvasItem();
        pItem->type = CANVAS_BOX;
        pItem->x.box.x = x;
        pItem->x.box.y = y;
        pItem->x.box.w = w;
        pItem->x.box.h = h;
        pItem->x.box.pNode = pNode;
        pItem->x.box.flags = flags;
        linkItem(pCanvas, pItem);

        itemToBox(pItem, 0, 0, &x1, &y1, &w1, &h1);
        pCanvas->left = MIN(pCanvas->left, x1);
        pCanvas->right = MAX(pCanvas->right, x1 + w1);
        pCanvas->bottom = MAX(pCanvas->bottom, y1 + h1);
        pCanvas->top = MIN(pCanvas->top, y1);
    } else {
        pCanvas->left = MIN(pCanvas->left, x);
        pCanvas->right = MAX(pCanvas->right, x + w);
        pCanvas->bottom = MAX(pCanvas->bottom, y + h);
        pCanvas->top = MIN(pCanvas->top, y);
    }
}

void 
HtmlDrawLine(pCanvas, x, w, y_over, y_through, y_under, pNode, size_only)
    HtmlCanvas *pCanvas;
    int x;
    int w;
    int y_over;
    int y_through;
    int y_under;
    HtmlNode *pNode;
    int size_only;
{
    if (!size_only) {
        HtmlCanvasItem *pItem; 
        pItem = allocateCanvasItem();
        pItem->type = CANVAS_LINE;
        pItem->x.line.x = x;
        pItem->x.line.w = w;
        pItem->x.line.y = y_over;
        pItem->x.line.y_underline = (y_under - y_over);
        pItem->x.line.y_linethrough = (y_through - y_over);
        pItem->x.line.pNode = pNode;
        linkItem(pCanvas, pItem);
    }

    pCanvas->left = MIN(pCanvas->left, x);
    pCanvas->right = MAX(pCanvas->right, x + w);
    pCanvas->bottom = MAX(pCanvas->bottom, y_under);
    pCanvas->top = MIN(pCanvas->top, y_over);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawText --
 *
 *     Add a single line of text drawn in a single font to a canvas.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlDrawText(pCanvas, pText, x, y, w, size_only, pNode, iIndex)
    HtmlCanvas *pCanvas; 
    Tcl_Obj *pText; 
    int x;
    int y;
    int w;
    int size_only;
    HtmlNode *pNode;
    int iIndex;
{
    HtmlFont *pFont = fontFromNode(pNode);

    if (!size_only) {
        HtmlCanvasItem *pItem; 
        pItem = allocateCanvasItem();
        pItem->type = CANVAS_TEXT;
        pItem->x.t.pText = pText;
        pItem->x.t.x = x;
        pItem->x.t.y = y;
        pItem->x.t.w = w;
        pItem->x.t.pNode = pNode;
        pItem->x.t.iIndex = iIndex;
        Tcl_IncrRefCount(pText);
        linkItem(pCanvas, pItem);
    }

    pCanvas->left = MIN(pCanvas->left, x);
    pCanvas->right = MAX(pCanvas->right, x + w);
    pCanvas->bottom = MAX(pCanvas->bottom, y + pFont->metrics.descent);
    pCanvas->top = MIN(pCanvas->top, y - pFont->metrics.ascent);
}

void 
HtmlDrawImage(
        pCanvas, pImage, 
        x, y, w, h, 
        pNode,
        size_only
)
    HtmlCanvas *pCanvas;
    HtmlImage2 *pImage;               /* Image name or NULL */
    HtmlNode *pNode;
    int x; 
    int y;
    int w;                      /* Width of image */
    int h;                      /* Height of image */
    int size_only;
{
    HtmlImageCheck(pImage);
    if (!size_only) {
        HtmlCanvasItem *pItem; 
        pItem = allocateCanvasItem();
        pItem->type = CANVAS_IMAGE;
        pItem->x.i2.pImage = pImage;
        HtmlImageRef(pImage);
        pItem->x.i2.x = x;
        pItem->x.i2.y = y;
        pItem->x.i2.w = w;
        pItem->x.i2.h = h;
        pItem->x.i2.pNode = pNode;
        linkItem(pCanvas, pItem);
    }

    pCanvas->left = MIN(pCanvas->left, x);
    pCanvas->right = MAX(pCanvas->right, x+w);
    pCanvas->bottom = MAX(pCanvas->bottom, y+h);
    pCanvas->top = MIN(pCanvas->top, y);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawWindow --
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
HtmlDrawWindow(pCanvas, pNode, x, y, w, h, size_only)
    HtmlCanvas *pCanvas;
    HtmlNode *pNode;
    int x; 
    int y;
    int w;       /* Width of window */
    int h;       /* Height of window */
    int size_only;
{
    if (!size_only) {
        HtmlCanvasItem *pItem; 
        pItem = allocateCanvasItem();
        memset(pItem, 0, sizeof(HtmlCanvasItem));
        pItem->type = CANVAS_WINDOW;
        pItem->x.w.pNode = pNode;
        pItem->x.w.x = x;
        pItem->x.w.y = y;
        linkItem(pCanvas, pItem);
    }

    pCanvas->left = MIN(pCanvas->left, x);
    pCanvas->right = MAX(pCanvas->right, x+w);
    pCanvas->bottom = MAX(pCanvas->bottom, y+h);
    pCanvas->top = MIN(pCanvas->top, y);

}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutPrimitives --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlLayoutPrimitives(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlCanvasItem *pItem;
    Tcl_Obj *aObj[13];
    int nObj = 0;
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlCanvas *pCanvas = &pTree->canvas;
    Tcl_Obj *pPrimitives;

    pPrimitives = Tcl_NewObj();
    Tcl_IncrRefCount(pPrimitives);

    for (pItem=pCanvas->pFirst; pItem; pItem=pItem->pNext) {
        Tcl_Obj *pList = 0;
        nObj = 0;
        switch (pItem->type) {
            case CANVAS_ORIGIN:
                if (pItem->x.o.pSkip) {
                    nObj = 5;
                    aObj[0] = Tcl_NewStringObj("draw_origin_start", -1);
                    aObj[1] = Tcl_NewIntObj(pItem->x.o.x);
                    aObj[2] = Tcl_NewIntObj(pItem->x.o.y);
                    aObj[3] = Tcl_NewIntObj(pItem->x.o.horizontal);
                    aObj[4] = Tcl_NewIntObj(pItem->x.o.vertical);
                } else {
                    nObj = 3;
                    aObj[0] = Tcl_NewStringObj("draw_origin_end", -1);
                    aObj[1] = Tcl_NewIntObj(pItem->x.o.x);
                    aObj[2] = Tcl_NewIntObj(pItem->x.o.y);
                    aObj[3] = Tcl_NewIntObj(pItem->x.o.horizontal);
                    aObj[4] = Tcl_NewIntObj(pItem->x.o.vertical);
                }
                break;
            case CANVAS_TEXT: {
                nObj = 7;
                aObj[0] = Tcl_NewStringObj("draw_text", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.t.x);
                aObj[2] = Tcl_NewIntObj(pItem->x.t.y);
                aObj[3] = Tcl_NewIntObj(pItem->x.t.w);
                aObj[4] = HtmlNodeCommand(pTree, pItem->x.t.pNode);
                aObj[5] = Tcl_NewIntObj(pItem->x.t.iIndex);
                aObj[6] = pItem->x.t.pText;
                break;
            }
            case CANVAS_IMAGE:
                if (pItem->x.i2.pImage) {
                    nObj = 7;
                    aObj[0] = Tcl_NewStringObj("draw_image", -1);
                    aObj[1] = Tcl_NewIntObj(pItem->x.i2.x);
                    aObj[2] = Tcl_NewIntObj(pItem->x.i2.y);
                    aObj[3] = Tcl_NewIntObj(pItem->x.i2.w);
                    aObj[4] = Tcl_NewIntObj(pItem->x.i2.h);
                    aObj[5] = HtmlNodeCommand(pTree, pItem->x.i2.pNode);
                    aObj[6] = HtmlImageUnscaledName(pItem->x.i2.pImage);
                }
                break;
            case CANVAS_WINDOW:
                nObj = 4;
                aObj[0] = Tcl_NewStringObj("draw_window", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.w.x);
                aObj[2] = Tcl_NewIntObj(pItem->x.w.y);
                aObj[3] = pItem->x.w.pNode->pReplacement->pReplace;
                break;
            case CANVAS_BOX:
                nObj = 6;
                aObj[0] = Tcl_NewStringObj("draw_box", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.box.x);
                aObj[2] = Tcl_NewIntObj(pItem->x.box.y);
                aObj[3] = Tcl_NewIntObj(pItem->x.box.w);
                aObj[4] = Tcl_NewIntObj(pItem->x.box.h);
                aObj[5] = HtmlNodeCommand(pTree, pItem->x.box.pNode);
                break;
            case CANVAS_LINE:
                nObj = 7;
                aObj[0] = Tcl_NewStringObj("draw_line", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.line.x);
                aObj[2] = Tcl_NewIntObj(pItem->x.line.y);
                aObj[3] = Tcl_NewIntObj(pItem->x.line.w);
                aObj[4] = Tcl_NewIntObj(pItem->x.line.y_linethrough);
                aObj[5] = Tcl_NewIntObj(pItem->x.line.y_underline);
                aObj[6] = HtmlNodeCommand(pTree, pItem->x.line.pNode);
                break;
        }
        if (nObj>0) {
            pList = Tcl_NewObj();
            Tcl_SetListObj(pList, nObj, aObj);
        }
        if (pList) {
            Tcl_ListObjAppendElement(interp, pPrimitives, pList);
        }
    }
    Tcl_SetObjResult(interp, pPrimitives);
    Tcl_DecrRefCount(pPrimitives);
    return TCL_OK;
}

static int
fill_quad(win, d, xcolor, x1, y1, x2, y2, x3, y3, x4, y4)
    Tk_Window win;
    Drawable d;
    XColor *xcolor;
    int x1; int y1;
    int x2; int y2;
    int x3; int y3;
    int x4; int y4;
{
    XPoint points[4];
    Display *display = Tk_Display(win);
    GC gc;
    XGCValues gc_values;
    int rc = 0;

    gc_values.foreground = xcolor->pixel;
    gc = Tk_GetGC(win, GCForeground, &gc_values);

    /* The coordinates provided to this function are suitable for
     * passing to XFillPolygon() with the "mode" argument set to 
     * CoordModePrevious. However not all Tk platforms (notably MacOSX,
     * but probably others too) support this mode. So manipulate the
     * supplied coordinates here so that they can be passed with
     * the mode set to CoordModeOrigin.
     */
    points[0].x = x1; 
    points[0].y = y1;
    points[1].x = points[0].x + x2; 
    points[1].y = points[0].y + y2;
    points[2].x = points[1].x + x3; 
    points[2].y = points[1].y + y3;
    points[3].x = points[2].x + x4; 
    points[3].y = points[2].y + y4;

    XFillPolygon(display, d, gc, points, 4, Convex, CoordModeOrigin);

    Tk_FreeGC(display, gc);
    return rc;
}

static int
fill_rectangle(win, d, xcolor, x, y, w, h)
    Tk_Window win;
    Drawable d;
    XColor *xcolor;
    int x; int y;
    int w; int h;
{
    if (w > 0 && h > 0){
        Display *display = Tk_Display(win);
        GC gc;
        XGCValues gc_values;
    
        gc_values.foreground = xcolor->pixel;
        gc = Tk_GetGC(win, GCForeground, &gc_values);
        XFillRectangle(display, d, gc, x, y, w, h);
        Tk_FreeGC(display, gc);
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutPrimitives --
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
tileimage(
drawable, d_w, d_h, pImage, bg_x, bg_y, bg_w, bg_h, iPosX, iPosY)
    Drawable drawable;        /* Where to draw */
    int d_w; int d_h;         /* Total width and height of drawable */
    HtmlImage2 *pImage;
    int bg_x; int bg_y;       /* Drawable coords for drawn block */
    int bg_w; int bg_h;       /* Width and height of drawn block */
    int iPosX; int iPosY;     /* Origin of image in drawable */
{
    int x1, y1;

    int clip_x1 = MAX(0, bg_x);
    int clip_y1 = MAX(0, bg_y);
    int clip_x2 = MIN(d_w, bg_x + bg_w);
    int clip_y2 = MIN(d_h, bg_y + bg_h);

    Tk_Image img;
    int i_w;
    int i_h;

    img = HtmlImageImage(pImage);
    Tk_SizeOfImage(img, &i_w, &i_h);
    if (bg_h > (i_h * 2) && bg_w > (i_w * 2)) {
        img = HtmlImageTile(pImage);
        Tk_SizeOfImage(img, &i_w, &i_h);
    }
    assert(i_w > 0);
    assert(i_h > 0);

    x1 = iPosX;
    if (iPosX != bg_x) {
        x1 -= (1 + (iPosX - bg_x) / i_w) * i_w;
    }

    for (; x1 < bg_x + bg_w; x1 += i_w) {
        y1 = iPosY;
        if (iPosY != bg_y) {
            y1 -= (1 + (iPosY - bg_y) / i_h) * i_h;
        }
        for (; y1 < bg_y + bg_h; y1 += i_h) {

            int w = i_w;
            int h = i_h;
            int im_x = 0;
            int im_y = 0;
            int x = x1;
            int y = y1;

            if (x + w > clip_x2) {
                w = (clip_x2 - x);
            }
            if (y + h > clip_y2) {
                h = (clip_y2 - y);
            }

            if (x < clip_x1) {
                im_x = clip_x1 - x;
                w -= (clip_x1 - x);
                x = clip_x1;
            }
            if (y < clip_y1) {
                im_y = clip_y1 - y;
                h -= im_y;
                y = clip_y1;
            }

            if (w > 0 && h > 0) {
                Tk_RedrawImage(img, im_x, im_y, w, h, drawable, x, y);
            }
        }
    }
}

typedef struct Outline Outline;
struct Outline {
    int x;
    int y;
    int w;
    int h;
    HtmlNode *pNode;
    Outline *pNext;
};

/*
 *---------------------------------------------------------------------------
 *
 * drawBox --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Outline* 
drawBox(pTree, pBox, drawable, x, y, w, h, xview, yview)
    HtmlTree *pTree;
    CanvasBox *pBox;
    Drawable drawable;
    int x;                 /* X-coord in *pDrawable */
    int y;                 /* Y-coord in *pDrawable */
    int w;                 /* Total width of *pDrawable */
    int h;                 /* Total height of *pDrawable */
    int xview;             /* X-coord of drawable in viewport */
    int yview;             /* Y-coord of drawable in viewport */
{
    HtmlComputedValues *pV = pBox->pNode->pPropertyValues;

    /* Figure out the widths of the top, bottom, right and left borders */
    int tw = ((pV->eBorderTopStyle != CSS_CONST_NONE) ? pV->border.iTop : 0);
    int bw = ((pV->eBorderBottomStyle != CSS_CONST_NONE)?pV->border.iBottom:0);
    int rw = ((pV->eBorderRightStyle != CSS_CONST_NONE) ? pV->border.iRight :0);
    int lw = ((pV->eBorderLeftStyle != CSS_CONST_NONE) ? pV->border.iLeft : 0);
    int ow = ((pV->eOutlineStyle != CSS_CONST_NONE) ? pV->iOutlineWidth : 0);

    int bg_x = x + pBox->x + lw;      /* Drawable x coord for background */
    int bg_y = y + pBox->y + tw;      /* Drawable y coord for background */
    int bg_w = pBox->w - lw - rw;     /* Width of background rectangle */
    int bg_h = pBox->h - tw - bw;     /* Height of background rectangle */

    /* Figure out the colors of the top, bottom, right and left borders */
    XColor *tc = pV->cBorderTopColor->xcolor;
    XColor *rc = pV->cBorderRightColor->xcolor;
    XColor *bc = pV->cBorderBottomColor->xcolor;
    XColor *lc = pV->cBorderLeftColor->xcolor;
    XColor *oc = pV->cOutlineColor->xcolor;

    int isInline = (pV->eDisplay == CSS_CONST_INLINE);

    if (pBox->pNode == pTree->pBgRoot) return 0;

    if (pBox->flags & CANVAS_BOX_OPEN_LEFT) {
        lw = 0;
    }
    if (pBox->flags & CANVAS_BOX_OPEN_RIGHT) {
        rw = 0;
    }

    /* Solid background, if required */
    if (pV->cBackgroundColor->xcolor) {
        int boxw = pBox->w + MIN((x + pBox->x), 0);
        int boxh = pBox->h + MIN((y + pBox->y), 0);
        fill_rectangle(pTree->win, 
            drawable, pV->cBackgroundColor->xcolor,
            MAX(0, x + pBox->x), MAX(0, y + pBox->y),
            MIN(boxw, w), MIN(boxh, h)
        );
    }

    /* Top border */
    if (tw > 0 && tc) {
        fill_quad(pTree->win, drawable, tc,
            x + pBox->x, y + pBox->y,
            lw, tw,
            pBox->w - lw - rw, 0,
            rw, -1 * tw
        );
    }

    /* Left border, if required */
    if (lw > 0 && lc) {
        fill_quad(pTree->win, drawable, lc,
            x + pBox->x, y + pBox->y,
            lw, tw,
            0, pBox->h - tw - bw,
            -1 * lw, bw
        );
    }

    /* Bottom border, if required */
    if (bw > 0 && bc) {
        fill_quad(pTree->win, drawable, bc,
            x + pBox->x, y + pBox->y + pBox->h,
            lw, - 1 * bw,
            pBox->w - lw - rw, 0,
            rw, bw
        );
    }

    /* Right border, if required */
    if (rw > 0 && rc) {
        fill_quad(pTree->win, drawable, rc,
            x + pBox->x + pBox->w, y + pBox->y,
            -1 * rw, tw,
            0, pBox->h - tw - bw,
            rw, bw
        );
    }

    /* Image background, if required and the generating node is not inline. 
     * Tkhtml does not draw background images for inline nodes. That's Ok
     * for now, because they're not terribly common.
     */
    if (/* !isInline && */ pV->imBackgroundImage) {
        Tk_Image img;
        Pixmap ipix;
        GC gc;
        XGCValues gc_values;
        int iWidth;
        int iHeight;
        Tk_Window win = pTree->win;
        Display *display = Tk_Display(win);
        int dep = Tk_Depth(win);
        int eR = pV->eBackgroundRepeat;

 
        img = HtmlImageImage(pV->imBackgroundImage);
        Tk_SizeOfImage(img, &iWidth, &iHeight);

        if (iWidth > 0 && iHeight > 0) {
            int iPosX;
            int iPosY;
            HtmlNode *pBgNode = pBox->pNode;

#ifdef WIN32
            /*
	     * Todo: On windows, using XFillRectangle() to draw the image
	     * doesn't seem to work. This is probably a shortcoming of the Tk
	     * porting layer, but this hasn't been checked properly yet. For
             * now, disable the XFillRectangle() optimization. 
             */
            int isAlpha = 1;
#else
            int isAlpha = HtmlImageAlphaChannel(pTree, pV->imBackgroundImage);
#endif
    
            iPosX = pV->iBackgroundPositionX;
            iPosY = pV->iBackgroundPositionY;
            if (pV->eBackgroundAttachment == CSS_CONST_SCROLL) {
                if ( pV->mask & PROP_MASK_BACKGROUND_POSITION_X ){
                    iPosX = (double)iPosX * (double)(bg_w - iWidth) / 10000.0;
                }
                if ( pV->mask & PROP_MASK_BACKGROUND_POSITION_Y ){
                    iPosY = (double)iPosY * (double)(bg_h - iHeight) / 10000.0;
                }
                iPosX += bg_x;
                iPosY += bg_y;
            } else {
                /* 'background-attachment' is "fixed" */
                int rw = Tk_Width(pTree->tkwin);
                int rh = Tk_Height(pTree->tkwin);
                if ( pV->mask & PROP_MASK_BACKGROUND_POSITION_X ){
                    iPosX = (double)iPosX * (double)(rw - iWidth) / 10000.0;
                }
                if ( pV->mask & PROP_MASK_BACKGROUND_POSITION_Y ){
                    iPosY = (double)iPosY * (double)(rh - iHeight) / 10000.0;
                }
                iPosX -= xview;
                iPosY -= yview;
            }

            if (eR != CSS_CONST_REPEAT && eR != CSS_CONST_REPEAT_X) {
                int draw_x1 = MAX(bg_x, iPosX);
                int draw_x2 = MIN(bg_x + bg_w, iPosX + iWidth);
                bg_x = draw_x1;
                bg_w = draw_x2 - draw_x1;
            } 
            if (eR != CSS_CONST_REPEAT && eR != CSS_CONST_REPEAT_Y) {
                int draw_y1 = MAX(bg_y, iPosY);
                int draw_y2 = MIN(bg_y + bg_h, iPosY + iHeight);
                bg_y = draw_y1;
                bg_h = draw_y2 - draw_y1;
            }

            if (isAlpha) {
                tileimage(
                    drawable, w, h, 
                    pV->imBackgroundImage,
                    bg_x, bg_y, bg_w, bg_h, 
                    iPosX, iPosY
                );
            } else {

                /* Create a pixmap of the image */
                ipix = Tk_GetPixmap(
                    display, Tk_WindowId(win),iWidth, iHeight, dep
                );
                for ( ; pBgNode; pBgNode = HtmlNodeParent(pBgNode)) {
                    HtmlComputedValues *pV2 = pBgNode->pPropertyValues;
                    if (pV2->cBackgroundColor->xcolor) {
                        fill_quad(pTree->win, ipix, 
                            pV2->cBackgroundColor->xcolor,
                            0, 0, iWidth, 0, 0, iHeight, -1 * iWidth, 0
                        );
                        break;
                    }
                }
                Tk_RedrawImage(img, 0, 0, iWidth, iHeight, ipix, 0, 0);
        
		/* Draw a rectangle to the drawable with origin (bg_x, bg_y).
		 * The size of the rectangle is (bg_w *  bg_h). The background
		 * image is tiled across the region with a relative origin
		 * point as defined by (gc_values.ts_x_origin,
		 * gc_values.ts_y_origin).
                 */
                gc_values.ts_x_origin = iPosX;
                gc_values.ts_y_origin = iPosY;
                gc_values.tile = ipix;
                gc_values.fill_style = FillTiled;
                gc = Tk_GetGC(pTree->win, 
                    GCTile|GCTileStipXOrigin|GCTileStipYOrigin|GCFillStyle, 
                    &gc_values
                );
                if (bg_h > 0 && bg_w > 0) {
                    XFillRectangle(display,drawable,gc,bg_x,bg_y,bg_w,bg_h);
                }
                Tk_FreePixmap(display, ipix);
                Tk_FreeGC(display, gc);
            }
        }
    }

    /* Outline, if required */
    if (ow > 0 && oc) {
        Outline *pOutline = (Outline *)HtmlClearAlloc(0, sizeof(Outline));
        pOutline->x = x + pBox->x;
        pOutline->y = y + pBox->y;
        pOutline->w = pBox->w;
        pOutline->h = pBox->h;
        pOutline->pNode = pBox->pNode;
        return pOutline;
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * drawImage --
 *
 *     This function is used to draw a CANVAS_IMAGE primitive to the 
 *     drawable *pDrawable.
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
drawImage(pTree, pI2, drawable, x, y, w, h)
    HtmlTree *pTree;
    CanvasImage *pI2;
    Drawable drawable;
    int x;                 /* X-coord in *pDrawable */
    int y;                 /* Y-coord in *pDrawable */
    int w;                 /* Total width of *pDrawable */
    int h;                 /* Total height of *pDrawable */
{
    if (pI2->pImage) {
        int imW;                   /* Image width */
        int imH;                   /* Image height */
        Tk_Image img;              /* Tk Image */

        img = HtmlImageImage(pI2->pImage);
        Tk_SizeOfImage(img, &imW, &imH);

        tileimage(
            drawable, w, h, 
            pI2->pImage,
            x + pI2->x, y + pI2->y,
            imW, imH,
            x + pI2->x, y + pI2->y
        );
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * drawLine --
 *
 *     This function is used to draw a CANVAS_LINE primitive to the 
 *     drawable.
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
drawLine(pTree, pLine, drawable, x, y, w, h)
    HtmlTree *pTree;
    CanvasLine *pLine;
    Drawable drawable;
    int x;                 /* X-coord in *pDrawable */
    int y;                 /* Y-coord in *pDrawable */
    int w;                 /* Total width of *pDrawable */
    int h;                 /* Total height of *pDrawable */
{
    XColor *xcolor;
    int yrel;

    switch (pLine->pNode->pPropertyValues->eTextDecoration) {
        case CSS_CONST_LINE_THROUGH:
            yrel = pLine->y + pLine->y_linethrough; 
            break;
        case CSS_CONST_UNDERLINE:
            yrel = pLine->y + pLine->y_underline; 
            break;
        case CSS_CONST_OVERLINE:
            yrel = pLine->y; 
            break;
        default:
            return;
    }
    xcolor = pLine->pNode->pPropertyValues->cColor->xcolor;

    fill_quad(pTree->tkwin, drawable, xcolor, 
        x + pLine->x, y + yrel, pLine->w, 0, 0, 1, -1 * pLine->w, 0
    );
}

#define SWAPINT(x,y) {int tmp = x; x = y; y = tmp;}

/*
 *---------------------------------------------------------------------------
 *
 * drawLine --
 *
 *     This function draws a CANVAS_TEXT primitive on the supplied drawable.
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
drawText(pTree, pItem, drawable, x, y)
    HtmlTree *pTree;
    HtmlCanvasItem *pItem;
    Drawable drawable;
    int x;
    int y;
{
    Display *disp = Tk_Display(pTree->win);
    CanvasText *pT = &pItem->x.t;

    GC gc = 0;
    XGCValues gc_values;
    int mask;

    CONST char *z;          /* String to render */
    int n;                  /* Length of string z in (Todo: bytes? chars?) */

    HtmlFont *pFont = fontFromNode(pT->pNode);
    HtmlColor *pColor = colorFromNode(pT->pNode);
    Tk_Font font = pFont->tkfont;

    int iSelFrom;      /* Index in this string where the selection starts */
    int iSelTo = 0;    /* Index in this string where the selection ends */
    int eContinue = 0; /* True if this is not the last text selected */

    z = Tcl_GetStringFromObj(pT->pText, &n);
    iSelFrom = n;

    /* This block sets the iSelTo and iSelFrom variables according to the
     * portion (if any) of the text string that is "selected".
     */
    if (pTree->pFromNode && pT->iIndex >= 0) {
        int iToNode    = pTree->pToNode->iNode;
        int iFromNode  = pTree->pFromNode->iNode;
        int iToIndex   = pTree->iToIndex;
        int iFromIndex = pTree->iFromIndex;

        int iThis = pT->pNode->iNode;

        if (
            iFromNode > iToNode || 
            (iFromNode==iToNode && iFromIndex > iToIndex)
        ) {
            SWAPINT(iFromNode, iToNode);
            SWAPINT(iFromIndex, iToIndex);
        }

        if (iToNode>=iThis && iFromNode<=iThis) {
	    /* If this condition is true, then part of the node that this  
             * text belongs to is selected.
             */
	    iSelFrom = 0;
            iSelTo = n;
            if (iToNode == iThis && iToIndex >= 0) {
                iSelTo = iToIndex - pT->iIndex;
            }
            if (iFromNode == iThis && iFromIndex >= 0) {
                iSelFrom = iFromIndex - pT->iIndex;
            }

            if (iToNode > iThis || iSelTo > n) {
                HtmlCanvasItem *p = pItem->pNext;
                while (p && p->type != CANVAS_TEXT) p = p->pNext;
                if (p && p->x.t.pNode && p->x.t.pNode->iNode <= iToNode) {
                    eContinue = 1;
                }
            }
            iSelFrom = MAX(0, iSelFrom);
            iSelTo = MIN(n, iSelTo);
        }
    }

    /* Unless the entire line is selected, draw the text in the regular way */
    if (iSelTo < n || iSelFrom > 0) {
        mask = GCForeground | GCFont;
        gc_values.foreground = pColor->xcolor->pixel;
        gc_values.font = Tk_FontId(font);
        gc = Tk_GetGC(pTree->win, mask, &gc_values);

	/* Todo: There seems to be a bug in Tk_DrawChars triggered by
         * attempting to draw a string that lies wholly outside the drawable
         * region. So avoid this...
         */ 
        Tk_DrawChars(disp, drawable, gc, font, z, n, pT->x + x, pT->y + y);
        Tk_FreeGC(disp, gc);
    }

    /* If any text at all is selected, draw that text */
    if (iSelTo > 0 && iSelFrom <= n && iSelTo >= iSelFrom) {
        CONST char *zSel = &z[iSelFrom];
        int nSel;
        int w;                               /* Pixels of selected text */
        int xs = x;                          /* Pixel offset of selected text */
        int h;                               /* Height of text line */
        int ybg;                             /* Y coord for bg rectangle */

        nSel = iSelTo - iSelFrom;
        if (iSelFrom > 0) {
            xs += Tk_TextWidth(font, z, iSelFrom);
        }
        if (eContinue) {
            w = pT->w + x - xs;
        } else {
            w = Tk_TextWidth(font, zSel, nSel);
        }

        h = pFont->metrics.ascent + pFont->metrics.descent;
        ybg = pT->y + y - pFont->metrics.ascent;

        mask = GCForeground;
        gc_values.foreground = pTree->options.selectbackground->pixel;
        gc = Tk_GetGC(pTree->win, mask, &gc_values);
        XFillRectangle(disp, drawable, gc, pT->x + xs, ybg, w, h);
        Tk_FreeGC(disp, gc);

        mask = GCForeground | GCFont;
        gc_values.foreground = pTree->options.selectforeground->pixel;
        gc_values.font = Tk_FontId(font);
        gc = Tk_GetGC(pTree->win, mask, &gc_values);
        Tk_DrawChars(disp, drawable, gc, font, zSel, nSel, pT->x+xs, pT->y+y);
        Tk_FreeGC(disp, gc);
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * searchCanvas --
 *
 *     Iterate through a subset of the drawing primitives in the
 *     canvas associated with widget pTree. For each primitive, invoke
 *     the callback function provided as argument xFunc.
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
searchCanvas(pTree, ymin, ymax, pNode, xFunc, clientData)
    HtmlTree *pTree;
    int ymin;                    /* Minimum y coordinate, or INT_MIN */
    int ymax;                    /* Maximum y coordinate, or INT_MAX */
    HtmlNode *pNode;             /* Node to search subtree of, or NULL */
    int (*xFunc)(HtmlCanvasItem *, int, int, ClientData);
    ClientData clientData;
{
    HtmlCanvasItem *pItem;
    HtmlCanvasItem *pSkip = 0;
    HtmlCanvas *pCanvas = &pTree->canvas;
    int origin_x = 0;
    int origin_y = 0;
    int rc = 0;

    int nTest = 0;
    int nCallback = 0;
     
    for (pItem = pCanvas->pFirst; pItem; pItem = (pSkip?pSkip:pItem->pNext)) {
        pSkip = 0;
        if (pItem->type == CANVAS_ORIGIN) {
            CanvasOrigin *pOrigin1 = &pItem->x.o;
            CanvasOrigin *pOrigin2 = 0;
            if (pOrigin1->pSkip) pOrigin2 = &pItem->x.o.pSkip->x.o;

            origin_x += pOrigin1->x;
            origin_y += pOrigin1->y;
            if (pOrigin2 && (
                (ymax >= 0 && (origin_y + pOrigin1->vertical) > ymax) ||
                (ymin >= 0 && (origin_y + pOrigin2->vertical) < ymin))
            ) {
               pSkip = pOrigin1->pSkip;
            }
        } else if (pItem->type == CANVAS_MARKER) {
            assert(pItem->x.marker.flags == MARKER_FIXED);
            assert(origin_x == 0);
            assert(origin_y == 0);
            origin_x = pTree->iScrollX;
            origin_y = pTree->iScrollY;
        } else {
            int x, y, w, h;
            nTest++;

            itemToBox(pItem, origin_x, origin_y, &x, &y, &w, &h);

            if ((ymax < 0 || y <= ymax) && (ymin < 0 || (y + h) >= ymin)) {
                if (0 != (rc = xFunc(pItem, origin_x, origin_y, clientData))) {
                    return rc;
                }
                nCallback++;
            }
        }
    }

#if 0
printf("Search(%d, %d) -> %d tests %d callbacks\n",ymin,ymax,nTest,nCallback);
#endif
 
    return 0;
}

static int
sorterCb(pItem, x, y, clientData)
    HtmlCanvasItem *pItem;
    int x;
    int y;
    ClientData clientData;
{
    CanvasItemSorter *pSorter = (CanvasItemSorter *)clientData;
    sorterInsert(pSorter, pItem, x, y);
    return 0;
}
static void    
searchSortedCanvas(pTree, ymin, ymax, pNode, xFunc, clientData)
    HtmlTree *pTree;
    int ymin;                    /* Minimum y coordinate, or INT_MIN */
    int ymax;                    /* Maximum y coordinate, or INT_MAX */
    HtmlNode *pNode;             /* Node to search subtree of, or NULL */
    int (*xFunc)(HtmlCanvasItem *, int, int, ClientData);
    ClientData clientData;
{
    CanvasItemSorter sSorter;
    memset(&sSorter, 0, sizeof(CanvasItemSorter));

    searchCanvas(pTree, ymin, ymax, pNode, sorterCb, (ClientData)&sSorter);
    sorterIterate(&sSorter, xFunc, clientData);
    sorterReset(&sSorter);
}


typedef struct GetPixmapQuery GetPixmapQuery;
struct GetPixmapQuery {
    HtmlTree *pTree;
    int x;
    int y;
    int w;
    int h;
    int getwin;
    Outline *pOutline;
    Pixmap pmap;
};

static int
pixmapQueryCb(pItem, origin_x, origin_y, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    ClientData clientData;
{
    GetPixmapQuery *pQuery = (GetPixmapQuery *)clientData;
    int x = origin_x + pQuery->x;
    int y = origin_y + pQuery->y;

    int w = pQuery->w;
    int h = pQuery->h;

    switch (pItem->type) {
        case CANVAS_TEXT: {
            drawText(pQuery->pTree, pItem, pQuery->pmap, x, y);
            break;
        }

        case CANVAS_IMAGE: {
            drawImage(pQuery->pTree, &pItem->x.i2, pQuery->pmap, x, y, w, h);
            break;
        }

        case CANVAS_BOX: {
            Outline *p;
            int xv = -1 * (pQuery->x + pQuery->pTree->iScrollX);
            int yv = -1 * (pQuery->y + pQuery->pTree->iScrollY);
            p = drawBox(pQuery->pTree,&pItem->x.box,pQuery->pmap,x,y,w,h,xv,yv);
            if (p) {
                p->pNext = pQuery->pOutline;
                pQuery->pOutline = p;
            }
            break;
        }

        case CANVAS_LINE: {
            drawLine(pQuery->pTree, &pItem->x.line, pQuery->pmap, x, y, w, h);
            break;
        }
        case CANVAS_WINDOW: {
            if (pQuery->getwin) {
                HtmlTree *pTree = pQuery->pTree;
                HtmlNodeReplacement *pRep = pItem->x.w.pNode->pReplacement;
                HtmlNodeReplacement *p;

                pRep->iCanvasX = origin_x + pItem->x.w.x;
                pRep->iCanvasY = origin_y + pItem->x.w.y;

                for (p = pTree->pMapped; p && p != pRep; p = p->pNext);
                if (!p) {
                    pRep->pNext = pTree->pMapped;
                    pTree->pMapped = pRep;
                }
            }
            break;
        }
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * getPixmap --
 *
 *    Return a Pixmap containing the rendered document. The caller is
 *    responsible for calling Tk_FreePixmap() on the returned value.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Pixmap 
getPixmap(pTree, xcanvas, ycanvas, w, h, getwin)
    HtmlTree *pTree;
    int xcanvas;
    int ycanvas;
    int w;
    int h;
    int getwin;
{
    Pixmap pmap;
    Display *pDisplay;
    Tk_Window win = pTree->win;
    XColor *bg_color = 0;
    GetPixmapQuery sQuery;
    Outline *pOutline;
    ClientData clientData;

    HtmlNode *pBgRoot = 0;

    Tk_MakeWindowExist(win);
    pDisplay = Tk_Display(win);
    pmap = Tk_GetPixmap(pDisplay, Tk_WindowId(win), w, h, Tk_Depth(win));

    /* Paint the canvas background on this pixmap. */
    pBgRoot = pTree->pRoot;
    if (pBgRoot) {
        HtmlComputedValues *pV = pBgRoot->pPropertyValues;
        if (!pV->cBackgroundColor->xcolor && !pV->imBackgroundImage) {
            int i;
            pBgRoot = 0;
            for (i = 0; i < HtmlNodeNumChildren(pTree->pRoot); i++) {
                HtmlNode *pChild = HtmlNodeChild(pTree->pRoot, i);
                if (HtmlNodeTagType(pChild) == Html_BODY) {
                    HtmlComputedValues *pV = pChild->pPropertyValues;
                    if (pV->cBackgroundColor->xcolor || pV->imBackgroundImage) {
                        pBgRoot = pChild;
                    }
                    break;
                }
            }
        } 
    }
    pTree->pBgRoot = 0;
    if (!pBgRoot || !pBgRoot->pPropertyValues->cBackgroundColor->xcolor) {
        Tcl_HashEntry *pEntry;
        pEntry = Tcl_FindHashEntry(&pTree->aColor, "white");
        assert(pEntry);
        bg_color = ((HtmlColor *)Tcl_GetHashValue(pEntry))->xcolor;
        fill_quad(win, pmap, bg_color, 0, 0, w, 0, 0, h, -1 * w, 0);
    }
    if (pBgRoot) {
        CanvasBox sBox;
        int xv = xcanvas - pTree->iScrollX;
        int yv = ycanvas - pTree->iScrollY;
        memset(&sBox, 0, sizeof(CanvasBox));
        sBox.pNode = pBgRoot;
        sBox.w = MAX(Tk_Width(pTree->tkwin), pTree->canvas.right);
        sBox.h = MAX(Tk_Height(pTree->tkwin), pTree->canvas.bottom);
        drawBox(pTree, &sBox, pmap, -1 * xcanvas, -1 * ycanvas, w, h, xv, yv);
    } 
    pTree->pBgRoot = pBgRoot;

    sQuery.pTree = pTree;
    sQuery.pmap = pmap;
    sQuery.x = xcanvas * -1;
    sQuery.y = ycanvas * -1;
    sQuery.w = w;
    sQuery.h = h;
    sQuery.pOutline = 0;
    sQuery.getwin = getwin;

    clientData = (ClientData)&sQuery;
#if 0
    searchCanvas(pTree, ycanvas, ycanvas+h, 0, pixmapQueryCb, clientData);
#else
    searchSortedCanvas(pTree, ycanvas, ycanvas+h, 0, pixmapQueryCb, clientData);
#endif

    pOutline = sQuery.pOutline;
    while (pOutline) {
        int ow = pOutline->pNode->pPropertyValues->iOutlineWidth;
        XColor *oc = pOutline->pNode->pPropertyValues->cOutlineColor->xcolor;
        int x1 = pOutline->x;
        int y1 = pOutline->y;
        int w1 = pOutline->w;
        int h1 = pOutline->h;
        Outline *pPrev = pOutline;
        fill_quad(pTree->win, pmap, oc, x1,y1, w1,0, 0,ow, -w1,0);
        fill_quad(pTree->win, pmap, oc, x1,y1+h1, w1,0, 0,-ow, -w1,0);
        fill_quad(pTree->win, pmap, oc, x1,y1, 0,h1, ow,0, 0,-h1);
        fill_quad(pTree->win, pmap, oc, x1+w1,y1, 0,h1, -ow,0, 0,-h1);
        pOutline = pOutline->pNext;
        HtmlFree(0, pPrev);
    }
  
    return pmap;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutImage --
 *
 *     <widget> image
 * 
 *     Render the document to a Tk image and return the name of the image
 *     as the Tcl result. The calling script is responsible for deleting
 *     the image. The image has blank space where controls would be mapped
 *     in a live display.
 *
 * Results:
 *     Standard Tcl return code.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlLayoutImage(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    Display *pDisplay = Tk_Display(pTree->win);

    int x = 0;
    int y = 0;
    int w;
    int h;

    /* Force any pending style and/or layout operations to run. */
    HtmlCallbackForce(pTree);

    w = pTree->canvas.right;
    h = pTree->canvas.bottom;
    assert(w >= 0 && h >= 0);
    if (w>0 && h>0) {
        Pixmap pixmap;
        Tcl_Obj *pImage;
        XImage *pXImage;
        pixmap = getPixmap(pTree, 0, 0, w, h, 0);
        pXImage = XGetImage(pDisplay, pixmap, x, y, w, h, AllPlanes, ZPixmap);
        pImage = HtmlXImageToImage(pTree, pXImage, w, h);
        XDestroyImage(pXImage);
        Tcl_SetObjResult(interp, pImage);
        Tcl_DecrRefCount(pImage);
        Tk_FreePixmap(Tk_Display(pTree->win), pixmap);
    } else {
        /* If the width or height is zero, then the image is empty. So just
	 * run the following simple script to set the interpreter result to
	 * an empty image.
         */
        Tcl_Eval(interp, "image create photo");
    }
  
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawIsEmpty --
 *
 *     Return true if the canvas object pCanvas is empty - i.e. contains no
 *     canvas items.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlDrawIsEmpty(pCanvas)
    HtmlCanvas *pCanvas;
{
    return (pCanvas->left==pCanvas->right && pCanvas->top==pCanvas->bottom);
}

static HtmlNode *
findFlowNode(pNode)
    HtmlNode *pNode;
{
    HtmlNode *p;
    for (p = pNode; p; p = HtmlNodeParent(p)) {
        HtmlComputedValues *pV = p->pPropertyValues;
        if (pV && (
                pV->eDisplay == CSS_CONST_TABLE_CELL ||
                pV->eFloat != CSS_CONST_NONE ||
                pV->ePosition != CSS_CONST_STATIC
            )
        ) {
            break;
        }
    }
    return p;
}

typedef struct NodeIndexQuery NodeIndexQuery;
struct NodeIndexQuery {
    int x;
    int y;
    CanvasText *pClosest;
    int closest_x;
    HtmlNode *pFlow;
};

/*
 *---------------------------------------------------------------------------
 *
 * layoutNodeIndexCb --
 *
 *     The searchCanvas() callback used by the implementation of the 
 *     [$html node -index X Y] command.
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
layoutNodeIndexCb(pItem, origin_x, origin_y, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    ClientData clientData;
{
    CanvasText *pT = &pItem->x.t;
    if (pItem->type == CANVAS_TEXT && pT->iIndex >= 0 && pT->pNode->iNode >= 0){
        NodeIndexQuery *pQuery = (NodeIndexQuery *)clientData;

        /* Calculate the bounding-box of the item. Store the coordinates 
         * of the top-left corner in variables x and y, and the size of
         * the box in variables w and h.
         */
        int x, y, w, h;
        itemToBox(pItem, origin_x, origin_y, &x, &y, &w, &h);

        /* If our point is actually inside the bounding box of this
         * text item, then this item is returned as the "closest text".
         */
        if (
            pQuery->x >= x && pQuery->x <= (x + w) &&
            pQuery->y >= y && pQuery->y <= (y + h)
        ) {
            pQuery->pClosest = pT;
            pQuery->closest_x = x;
            return 1;
        }
        
        /* If a text item for which the bounding box encapsulates the 
         * search point cannot be found, then we are looking for the
         * closest text item that is "above" the search point.
         */
        else {
            if (
                y <= pQuery->y && (
                    (x <= pQuery->x && pQuery->x <= (x + w)) ||  
                    (x <= pQuery->x && pQuery->y < (y + h) &&
                         x > pQuery->closest_x
                    ) ||
                    (pQuery->pFlow == findFlowNode(pT->pNode))
                )
            ) {
                pQuery->pFlow = findFlowNode(pT->pNode);
                pQuery->pClosest = pT;
                pQuery->closest_x = x;
            }
        }
    }
    return 0;
}


/*
 *---------------------------------------------------------------------------
 *
 * layoutNodeIndexCmd --
 *
 *     This function is called to process a command of the form:
 *
 *         <widget> node -index X Y
 *
 * Results:
 *     No results returned.
 *
 * Side effects:
 *     The tcl interpreter HtmlTree.interp is loaded with the result of
 *     the [<widget> node -index X Y] command.
 *
 *---------------------------------------------------------------------------
 */
static void
layoutNodeIndexCmd(pTree, x, y)
    HtmlTree *pTree;        /* Widget tree */
    int x;                  /* Document (not viewport) X coordinate */
    int y;                  /* Document (not viewport) Y coordinate */
{
    NodeIndexQuery sQuery;
    ClientData cd = (ClientData)&sQuery;
    int rc;

    memset(&sQuery, 0, sizeof(NodeIndexQuery));
    sQuery.x = x;
    sQuery.y = y;

    rc = searchCanvas(pTree, y-100, y, 0, layoutNodeIndexCb, cd);
    if (!sQuery.pClosest) {
        int ymin = y - pTree->iScrollY;
        rc = searchCanvas(pTree, ymin, y, 0, layoutNodeIndexCb, cd);
    }
    if (!sQuery.pClosest) {
        rc = searchCanvas(pTree, -1, y, 0, layoutNodeIndexCb, cd);
    }

    if (sQuery.pClosest) {
        HtmlNode *pNode = sQuery.pClosest->pNode;     /* Node to return */
        int iIndex = 0;                               /* Index to return */
        const char *z;
        int n;
        z = Tcl_GetStringFromObj(sQuery.pClosest->pText, &n);

        iIndex = n;
        if (rc) {
            /* Calculate the index to return */
            int dum;
            Tk_Font font = fontFromNode(sQuery.pClosest->pNode)->tkfont;
            iIndex = Tk_MeasureChars(font, z, n, x - sQuery.closest_x, 0, &dum);
        }
        iIndex += sQuery.pClosest->iIndex;

        /* Load the result into the Tcl interpreter */
        Tcl_Obj *pCmd = Tcl_DuplicateObj(HtmlNodeCommand(pTree, pNode));
        Tcl_ListObjAppendElement(0, pCmd, Tcl_NewIntObj(iIndex));
        Tcl_SetObjResult(pTree->interp, pCmd);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * returnDescNode --
 *
 *     Arguments pNode1 and pNode2 are two document nodes. If one of the 
 *     nodes is a descendant of another (is part of the subtree rooted 
 *     at the other node), then return the pointer to the descendant node.
 *     Otherwise return NULL.
 *
 *    
 * Results:
 *     See above.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static HtmlNode *
returnDescNode(pNode1, pNode2)
    HtmlNode *pNode1;
    HtmlNode *pNode2;
{
    HtmlNode *pN;
    for (pN = pNode1; pN && pN != pNode2; pN = HtmlNodeParent(pN));
    if (pN) {
        return pNode1;
    }
    for (pN = pNode2; pN && pN != pNode1; pN = HtmlNodeParent(pN));
    if (pN) {
        return pNode2;
    }
    return 0;
}

typedef struct NodeQuery NodeQuery;
struct NodeQuery {
    /* Query parameters */
    int x;
    int y;

    /* Variables for building up the result set in */
    HtmlNode **apNode;
    int nNodeAlloc;
    int nNode;
};

static int
layoutNodeCb(pItem, origin_x, origin_y, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    ClientData clientData;
{
    int x, y, w, h;
    NodeQuery *pQuery = (NodeQuery *)clientData;
    HtmlNode *pNode;

    pNode = itemToBox(pItem, origin_x, origin_y, &x, &y, &w, &h);

    if (
        pNode && pNode->iNode >= 0 && 
        x <= pQuery->x && (x + w) >= pQuery->x &&
        y <= pQuery->y && (y + h) >= pQuery->y
    ) {
        int i;
        for (i = 0; i < pQuery->nNode; i++) {
            HtmlNode *pDesc = returnDescNode(pNode, pQuery->apNode[i]);
            if (pDesc) {
                pQuery->apNode[i] = pDesc;
                return 0;
            }
        }

        pQuery->nNode++;
        if (pQuery->nNode > pQuery->nNodeAlloc) {
            int nByte;
            pQuery->nNodeAlloc += 16;
            nByte = pQuery->nNodeAlloc * sizeof(HtmlNode *);
            pQuery->apNode = (HtmlNode**)HtmlRealloc(0, pQuery->apNode, nByte);
        }
        assert(i == pQuery->nNode - 1);
        pQuery->apNode[i] = pNode;
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * layoutNodeCmd --
 *
 *     This function is called to process a command of the form:
 *
 *         <widget> node X Y
 *    
 * Results:
 *     No results returned.
 *
 * Side effects:
 *     The tcl interpreter HtmlTree.interp is loaded with the result of
 *     the [<widget> node X Y] command.
 *
 *---------------------------------------------------------------------------
 */
static void
layoutNodeCmd(pTree, x, y)
    HtmlTree *pTree;
    int x;
    int y;
{
    NodeQuery sQuery;
    memset(&sQuery, 0, sizeof(NodeQuery));

    sQuery.x = x;
    sQuery.y = y;

    searchSortedCanvas(pTree, y-1, y+1, 0, layoutNodeCb, (ClientData)&sQuery);

    if (sQuery.nNode > 0) {
        int i;
        Tcl_Obj *pRet = Tcl_NewObj();
        for (i = 0; i < sQuery.nNode; i++) {
            Tcl_Obj *pCmd = HtmlNodeCommand(pTree, sQuery.apNode[i]);
            Tcl_ListObjAppendElement(0, pRet, pCmd);
        }
        Tcl_SetObjResult(pTree->interp, pRet);
    }
    HtmlFree(0, sQuery.apNode);
}
  

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutNode --
 *
 *     <widget> node ??-index? X Y?
 *
 *     Return the Tcl handle for the document node that lies at coordinates
 *     (X, Y), relative to the viewport. Or, if no node populates the given
 *     point, return an empty string.
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
HtmlLayoutNode(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int x;
    int y;

    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc == 2){
        if (pTree->pRoot) {
            Tcl_Obj *pCmd = HtmlNodeCommand(pTree, pTree->pRoot);
            Tcl_SetObjResult(interp, pCmd);
        }
    } else if (objc == 4 || objc == 5) {
        if (TCL_OK != Tcl_GetIntFromObj(interp, objv[objc - 2], &x) ||
            TCL_OK != Tcl_GetIntFromObj(interp, objv[objc - 1], &y) 
        ) {
            return TCL_ERROR;
        }

        /* Transform x and y from viewport to document coordinates */
        x += pTree->iScrollX;
        y += pTree->iScrollY;

        if (objc == 4){
            layoutNodeCmd(pTree, x, y);
        } else {
            layoutNodeIndexCmd(pTree, x, y);
        }
    } else {
        Tcl_WrongNumArgs(interp, 2, objv, "?-index ?X Y??");
        return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 * A pointer to an instance of the following structure is passed by 
 * HtmlWidgetDamageText() to paintNodesSearchCb() as the client-data
 * parameter. 
 */
typedef struct PaintNodesQuery PaintNodesQuery;
struct PaintNodesQuery {
    int iNodeStart;
    int iIndexStart;
    int iNodeFin;
    int iIndexFin;
    int left;
    int right;
    int top;
    int bottom;
};

/*
 *---------------------------------------------------------------------------
 *
 * paintNodesSearchCb --
 *
 *     The callback for the canvas search performed by HtmlWidgetDamageText().
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
paintNodesSearchCb(pItem, origin_x, origin_y, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    ClientData clientData;
{
    PaintNodesQuery *p = (PaintNodesQuery *)clientData;

    if (pItem->type == CANVAS_TEXT) {
        CanvasText *pT = &(pItem->x.t);
        HtmlFont *pFont = fontFromNode(pT->pNode);
        if (pT->iIndex >= 0) {
            int iNode = pT->pNode->iNode;
            if (iNode >= p->iNodeStart && iNode <= p->iNodeFin) {
                int n;
                const char *z;
                int iIndex = pT->iIndex;
                int iIndex2;

                z = Tcl_GetStringFromObj(pT->pText, &n);
                iIndex2 = iIndex + n;

                if ( 
                    (iNode != p->iNodeStart || iIndex2 >= p->iIndexStart) &&
                    (iNode != p->iNodeFin || iIndex <= p->iIndexFin)
                ) {
                    int top    = origin_y + pT->y - pFont->metrics.ascent;
                    int bottom = origin_y + pT->y + pFont->metrics.descent;
                    int left   = origin_x + pT->x;
                    int right;
                    int nFin = n;

                    if (iNode == p->iNodeFin && p->iIndexFin >= 0) {
                        nFin = MIN(n, 1 + p->iIndexFin - pT->iIndex);
                        right = Tk_TextWidth(pFont->tkfont, z, nFin) + left;
                    } else {
                        right = pT->w + left;
                    }
                    if (iNode == p->iNodeStart && p->iIndexStart > 0) {
                        int nStart = MAX(0, p->iIndexStart - pT->iIndex);
                        if (nStart > 0) {
                            assert(nStart <= n);
                            left += Tk_TextWidth(pFont->tkfont, z, nStart);
                        }
                    }
    
                    p->left   = MIN(left, p->left);
                    p->right  = MAX(right, p->right);
                    p->top    = MIN(top, p->top);
                    p->bottom = MAX(bottom, p->bottom);
                }
            }
        }
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetDamageText --
 *
 *     This function is used to repaint the area covered by the text
 *     associated with a series of sequential nodes. It is used to update 
 *     the display when the selection changes. This function does not do any
 *     drawing itself, it schedules a callback using HtmlCallbackSchedule()
 *     to do the actual work.
 *
 *     At most, a single rectangular area is redrawn. This is the minimum
 *     rectangle that is visible in the viewport that includes all text
 *     between node iStartNode, index iStartIndex and node iNodeFin, 
 *     iIndexFin.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May schedule a HTML_CALLBACK_DAMAGE callback, as described above.
 *
 *---------------------------------------------------------------------------
 */
void
HtmlWidgetDamageText(pTree, iNodeStart, iIndexStart, iNodeFin, iIndexFin)
    HtmlTree *pTree;         /* Widget tree */
    int iNodeStart;          /* First node to repaint */
    int iIndexStart;         /* First node to repaint */
    int iNodeFin;            /* Last node to repaint */
    int iIndexFin;           /* Last node to repaint */
{
    PaintNodesQuery sQuery;
    int ymin, ymax;
    int x, y;
    int w, h;

    if (iNodeStart > iNodeFin || 
        (iNodeStart == iNodeFin && iIndexStart > iIndexFin)
    ) {
        SWAPINT(iNodeStart, iNodeFin);
        SWAPINT(iIndexStart, iIndexFin);
    }

    sQuery.iNodeStart = iNodeStart;
    sQuery.iNodeFin = iNodeFin;
    sQuery.iIndexStart = iIndexStart;
    sQuery.iIndexFin = iIndexFin;
    sQuery.left = pTree->canvas.right;
    sQuery.right = pTree->canvas.left;
    sQuery.top = pTree->canvas.bottom;
    sQuery.bottom = pTree->canvas.top;

    ymin = pTree->iScrollY;
    ymax = pTree->iScrollY + Tk_Height(pTree->tkwin);

    searchCanvas(pTree, ymin, ymax, 0, paintNodesSearchCb, (ClientData)&sQuery);

    x = sQuery.left - pTree->iScrollX;
    w = (sQuery.right - pTree->iScrollX) - x;
    y = sQuery.top - pTree->iScrollY;
    h = (sQuery.bottom - pTree->iScrollY) - y;
    HtmlCallbackDamage(pTree, x, y, w, h);
}

/*
 * The client-data for the search-callback used by HtmlWidgetNodeTop()
 */
typedef struct ScrollToQuery ScrollToQuery;
struct ScrollToQuery {
    HtmlTree *pTree;
    int iMinNode;
    int iMaxNode;
    int iReturn;
};

/*
 *---------------------------------------------------------------------------
 *
 * scrollToNodeCb --
 *     
 *     This function is the search-callback for HtmlWidgetNodeTop().
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
scrollToNodeCb(pItem, origin_x, origin_y, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    ClientData clientData;
{
    int x, y, w, h;
    ScrollToQuery *pQuery = (ScrollToQuery *)clientData;
    HtmlNode *pNode;
    int iMaxNode = pQuery->iMaxNode;

    pNode = itemToBox(pItem, origin_x, origin_y, &x, &y, &w, &h);

    /* If we have found a CANVAS_BOX for the node sought, then 
     * unconditionally return the pixel offset of the top-border edge
     * of the box. This is defined in CSS2.1.
     */
    if (pNode && pItem->type == CANVAS_BOX && pNode->iNode == iMaxNode){
        pQuery->iReturn = y;
        return 1;
    }
 
    if (
        pNode && 
        pNode->iNode <= pQuery->iMaxNode && 
        pNode->iNode >= pQuery->iMinNode
    ) {
        pQuery->iReturn = y;
        pQuery->iMinNode = pNode->iNode;
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetNodeTop --
 *
 *     The second argument, iNode, must be the node-number for some node 
 *     in the document tree pTree. This function returns the canvas 
 *     y-coordinate, in pixels of the top of the content generated by
 *     the node.
 *
 *     This is used in the implementation of the [widget yview nodeHandle]
 *     command. 
 *
 * Results:
 *     Pixels from the top of the canvas to the top of the content generated 
 *     by node iNode. Or, if node iNode does not generate content, then
 *     the content generated by node (iNode - 1). And so on. If no node
 *     with a node number less than iNode generated content, 0 is returned.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int
HtmlWidgetNodeTop(pTree, iNode)
    HtmlTree *pTree;
    int iNode;
{
    ScrollToQuery sQuery;
    HtmlCallbackForce(pTree);
    sQuery.iMaxNode = iNode;
    sQuery.iMinNode = 0;
    sQuery.iReturn = 0;
    sQuery.pTree = pTree;
    HtmlCallbackForce(pTree);
    searchCanvas(pTree, -1, -1, 0, scrollToNodeCb, (ClientData)&sQuery);
    return sQuery.iReturn;
}

typedef struct LayoutBboxQuery LayoutBboxQuery;
struct LayoutBboxQuery {
    HtmlNode *pNode;
    int left;
    int right;
    int top;
    int bottom;
};

static int
layoutBboxCb(pItem, origin_x, origin_y, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    ClientData clientData;
{
    int x, y, w, h;
    LayoutBboxQuery *pQuery = (LayoutBboxQuery *)clientData;
    HtmlNode *pNode;

    pNode = itemToBox(pItem, origin_x, origin_y, &x, &y, &w, &h);
    for (; pNode; pNode = HtmlNodeParent(pNode)) {
        if (pNode == pQuery->pNode) {
            pQuery->left = MIN(pQuery->left, x);
            pQuery->top = MIN(pQuery->top, y);
            pQuery->right = MAX(pQuery->right, x + w);
            pQuery->bottom = MAX(pQuery->bottom, y + h);
        }
    }
    return 0;
}

void 
HtmlWidgetNodeBox(pTree, pNode, pX, pY, pW, pH)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int *pX;
    int *pY;
    int *pW;
    int *pH;
{
    HtmlCanvas *pCanvas = &pTree->canvas;
    LayoutBboxQuery sQuery;

    HtmlCallbackForce(pTree);

    sQuery.left = pCanvas->right;
    sQuery.right = pCanvas->left;
    sQuery.top = pCanvas->bottom;
    sQuery.bottom = pCanvas->top;
    sQuery.pNode = pNode;

    searchCanvas(pTree, -1, -1, 0, layoutBboxCb, (ClientData)&sQuery);

    if (sQuery.left < sQuery.right && sQuery.top < sQuery.bottom) {
        *pX = sQuery.left;
        *pY = sQuery.top;
        *pW = sQuery.right - *pX;
        *pH = sQuery.bottom - *pY;
    } else {
        *pX = 0;
        *pY = 0;
        *pW = 0;
        *pH = 0;
    }
}

static void 
widgetRepair(pTree, x, y, w, h, g)
    HtmlTree *pTree;
    int x;
    int y;
    int w;
    int h;
    int g;
{
    Pixmap pixmap;
    GC gc;
    XGCValues gc_values;
    Tk_Window win = pTree->tkwin;
    Display *pDisp = Tk_Display(win); 

    if (w <= 0 || h <= 0) {
        return;
    }

    pixmap = getPixmap(pTree, pTree->iScrollX+x, pTree->iScrollY+y, w, h, g);

    memset(&gc_values, 0, sizeof(XGCValues));
    gc = Tk_GetGC(pTree->win, 0, &gc_values);
    assert(Tk_WindowId(win));
    XCopyArea(pDisp, pixmap, Tk_WindowId(win), gc, 0, 0, w, h, x, y);
    Tk_FreePixmap(pDisp, pixmap);
    Tk_FreeGC(pDisp, gc);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetRepair --
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlWidgetRepair(pTree, x, y, w, h)
    HtmlTree *pTree;
    int x;
    int y;
    int w;
    int h;
{
    /* Make sure the widget main window exists before painting anything */
    Tk_MakeWindowExist(pTree->tkwin);
    widgetRepair(pTree, x, y, w, h, 0);
}

#if 0
#ifdef WIN32
#include <windows.h>
static void
queryVisibility(pTree)
    HtmlTree *pTree;
{
    HWND hWindow = (HWND)Tk_WindowId(pTree->tkwin);
    HDC hDc = GetDC(hWindow);
    RECT clip;                 /* Clipped rectangle */
    RECT client;               /* Client window rectangle */
    int rc;

    GetClientRect(hWindow,  &client);
    rc = GetClipBox(hDc, &clip);
    ReleaseDC(hWindow, hDc);

    if (rc == SIMPLEREGION && EqualRect(&clip, &client)) {
        pTree->eVisibility = VisibilityUnobscured; 
    } else {
        pTree->eVisibility = VisibilityPartiallyObscured; 
    }

    HtmlLog(pTree, "EVENT", "queryVisibility: state=%s", 
                pTree->eVisibility == VisibilityFullyObscured ?
                        "VisibilityFullyObscured":
                pTree->eVisibility == VisibilityPartiallyObscured ?
                        "VisibilityPartiallyObscured":
                pTree->eVisibility == VisibilityUnobscured ?
                        "VisibilityUnobscured":
                "N/A"
    );
}
#else
  #define queryVisibility(x)
#endif
#endif


/*
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetSetViewport --
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlWidgetSetViewport(pTree, scroll_x, scroll_y, force_redraw)
    HtmlTree *pTree;            /* Tree structure */
    int scroll_x;               /* New value for pTree->iScrollX */
    int scroll_y;               /* New value for pTree->iScrollY */
    int force_redraw;           /* Redraw the entire viewport regardless */
{
    int w;
    int h;
    Tk_Window win = pTree->tkwin;

    int delta_x = scroll_x - pTree->iScrollX;
    int delta_y = scroll_y - pTree->iScrollY;

    /* Make sure the widget main window exists before doing anything */
    Tk_MakeWindowExist(win);
    w = Tk_Width(win);
    h = Tk_Height(win);

#if 0
    queryVisibility(pTree);
#endif

    assert(pTree->nFixedBackground >= 0);
    if (pTree->nFixedBackground || pTree->eVisibility != VisibilityUnobscured) {
        force_redraw = 1;
    }

    pTree->iScrollY = scroll_y;
    pTree->iScrollX = scroll_x;
    if (force_redraw || delta_x != 0 || abs(delta_y) >= h) {
        widgetRepair(pTree, 0, 0, w, h, 1);
    } else {
        XGCValues gc_values;
        GC gc;
        Display *pDisp = Tk_Display(win);
        Window xwin = Tk_WindowId(win);

        memset(&gc_values, 0, sizeof(XGCValues));
        gc = Tk_GetGC(pTree->win, 0, &gc_values);

        if (delta_y > 0) {
            XCopyArea(pDisp, xwin, xwin, gc, 0, delta_y, w, h-delta_y, 0, 0);
            widgetRepair(pTree, 0, h-delta_y, w, delta_y, 1);
        } else if (delta_y < 0) {
            XCopyArea(pDisp, xwin, xwin, gc, 0, 0, w, h+delta_y, 0, 0-delta_y);
            widgetRepair(pTree, 0, 0, w, 0 - delta_y, 1);
        }

        Tk_FreeGC(pDisp, gc);
    }

    windowsRepair(pTree, &pTree->canvas);
}

HtmlCanvasItem *
HtmlDrawAddMarker(pCanvas, x, y, fixed) 
    HtmlCanvas *pCanvas;
    int x;
    int y;
    int fixed;
{
    HtmlCanvasItem *pItem; 
CHECK_CANVAS(pCanvas);
    pItem = allocateCanvasItem();
    pItem->type = CANVAS_MARKER;
    pItem->x.marker.x = x;
    pItem->x.marker.y = y;
    pItem->x.marker.flags = (fixed ? MARKER_FIXED : 0);
    linkItem(pCanvas, pItem);
CHECK_CANVAS(pCanvas);
    return pItem;
}

int
HtmlDrawGetMarker(pCanvas, pMarker, pX, pY)
    HtmlCanvas *pCanvas;
    HtmlCanvasItem *pMarker;
    int *pX;
    int *pY;
{
    int origin_x = 0;
    int origin_y = 0;
    HtmlCanvasItem *pItem; 
    HtmlCanvasItem *pPrev = 0; 
    CHECK_CANVAS(pCanvas);
    for (pItem = pCanvas->pFirst; pItem && pMarker; pItem = pItem->pNext) {
        if (pItem->type == CANVAS_ORIGIN) {
            CanvasOrigin *pOrigin = &pItem->x.o;
            origin_x += pOrigin->x;
            origin_y += pOrigin->y;
        } else if (pItem == pMarker) {
            *pX = origin_x + pItem->x.marker.x;
            *pY = origin_y + pItem->x.marker.y;
            if (pPrev) {
                assert(pPrev->pNext == pMarker);
                pPrev->pNext = pMarker->pNext;
            } else {
                assert(pCanvas->pFirst == pMarker);
                pCanvas->pFirst = pMarker->pNext;
            }
            if (pCanvas->pLast == pMarker) {
                pCanvas->pLast = pPrev ? pPrev : pCanvas->pFirst;
            }
            freeCanvasItem(pMarker);
            CHECK_CANVAS(pCanvas);
            return 0;
        }
        pPrev = pItem;
    }
    return 1;
}

