
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
static const char rcsid[] = "$Id: htmldraw.c,v 1.90 2006/03/10 07:42:00 danielk1977 Exp $";

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
 *         * Rect       - Solid rectangle
 *         * Border     - Border segment
 *         * Text       - Single line of text
 *         * Image      - An image (or region to be tiled with an image)
 *         * Window     - A Tk widget window (set by [node replace])
 *
 *     All web documents are reduced by the layout engine to zero or more 
 *     of these primitives. Todo: Possibly a document has a background 
 *     image also?
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
 */

#define CANVAS_QUAD    1
#define CANVAS_TEXT    2
#define CANVAS_WINDOW  4
#define CANVAS_ORIGIN  5
#define CANVAS_BACKGROUND  6
#define CANVAS_COMMENT  7


#define CANVAS_IMAGE  8

#define CANVAS_BOX  9

typedef struct CanvasText CanvasText;
typedef struct CanvasWindow CanvasWindow;
typedef struct CanvasOrigin CanvasOrigin;
typedef struct CanvasQuad CanvasQuad;
typedef struct CanvasBackground CanvasBackground;
typedef struct CanvasComment CanvasComment;
typedef struct CanvasImage CanvasImage;
typedef struct CanvasBox CanvasBox;

struct CanvasText {
    int x;                   /* Relative x coordinate to render at */
    int y;                   /* Relative y coordinate to render at */

    Tcl_Obj *pText;          /* Text to render */
    HtmlFont *pFont;         /* Font to render text with */
    XColor *color;           /* Color to draw in */
    HtmlNode *pNode;         /* Text node, if any */
    int iIndex;              /* Index in pNode text of this item */
};

struct CanvasBox {
    int x;                   /* Relative x coordinate to render at */
    int y;                   /* Relative y coordinate to render at */
    int w;                   /* Width of box area */
    int h;                   /* Height of box area */
    HtmlNode *pNode;         /* Use computed properties from this node */
};

struct CanvasImage {
    int x;                   /* Relative x coordinate to render at */
    int y;                   /* Relative y coordinate to render at */

    HtmlImage2 *pImage;
    int w;                   /* Width of image region */
    int h;                   /* Height of image region */
    int iPositionX;
    int iPositionY;
    unsigned char isPositionPercent;
    unsigned char eRepeat;
};

struct CanvasWindow {
    int x;                   /* Relative x coordinate */
    int y;                   /* Relative y coordinate */

    Tcl_Obj *pWindow;
    int absx;
    int absy;
    HtmlCanvasItem *pNext;
};

struct CanvasQuad {
    Tcl_Obj *pWindow;
    int x1, y1;
    int x2, y2;
    int x3, y3;
    int x4, y4;
    XColor *color;
};

struct CanvasOrigin {
    int x;
    int y;
    int left, right;
    int top, bottom;
    HtmlCanvasItem *pSkip;
    HtmlNode *pNode;
};

struct CanvasComment {
    Tcl_Obj *pComment;
};

struct CanvasBackground {
    XColor *color;
};

struct HtmlCanvasItem {
    int type;
    union {
        CanvasText t;
        CanvasImage i2;
        CanvasWindow w;
        CanvasQuad q;
        CanvasOrigin o;
        CanvasBackground b;
        CanvasComment c;
        CanvasBox box;
    } x;
    HtmlCanvasItem *pNext;
};

/*
 *---------------------------------------------------------------------------
 *
 * MAX5 --
 *
 *     Return the maxmimum value passed as one of the 5 integer arguments.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int MAX5(a, b, c, d, e)
    int a, b, c, d, e;
{
    int max = a;
    max = MAX(max, b);
    max = MAX(max, c);
    max = MAX(max, d);
    max = MAX(max, e);
    return max;
}

/*
 *---------------------------------------------------------------------------
 *
 * MIN5 --
 * 
 *     Return the minimum value passed as one of the 5 integer arguments.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int MIN5(a, b, c, d, e)
    int a, b, c, d, e;
{
    int min = a;
    min = MIN(min, b);
    min = MIN(min, c);
    min = MIN(min, d);
    min = MIN(min, e);
    return min;
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
HtmlDrawCleanup(pCanvas)
    HtmlCanvas *pCanvas;
{
    HtmlCanvasItem *pItem;
    HtmlCanvasItem *pPrev = 0;

    for (pItem = pCanvas->pFirst; pItem; pItem=pItem->pNext) {
        Tcl_Obj *pObj = 0;
        switch (pItem->type) {
            case CANVAS_TEXT:
                pObj = pItem->x.t.pText;
                break;
            case CANVAS_IMAGE:
                HtmlImageFree(pItem->x.i2.pImage);
                break;
            case CANVAS_WINDOW:
                pObj = pItem->x.w.pWindow;
                break;
            case CANVAS_COMMENT:
                pObj = pItem->x.c.pComment;
                break;
        }
        if (pObj) {
            Tcl_DecrRefCount(pObj);
        }
        if (pPrev) {
            HtmlFree((char *)pPrev);
        }
        pPrev = pItem;
    }
    if (pPrev) {
        HtmlFree((char *)pPrev);
    }
    pCanvas->pFirst = 0;
    pCanvas->pLast = 0;

    if (pCanvas->pPrimitives) {
        Tcl_DecrRefCount(pCanvas->pPrimitives);
        pCanvas->pPrimitives = 0;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawDeleteControls --
 *
 *     Unmap and delete all the control widgets contained in this canvas.
 *     This must be called *before* HtmlDrawCleanup() (because it uses a
 *     data structure that DrawCleanup() deletes).
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
HtmlDrawDeleteControls(pTree, pCanvas)
    HtmlTree *pTree;
    HtmlCanvas *pCanvas;
{
    HtmlCanvasItem *pItem;
    
    for (pItem = pCanvas->pWindow; pItem; pItem = pItem->x.w.pNext) {
        Tk_Window control = Tk_NameToWindow(
                pTree->interp, Tcl_GetString(pItem->x.w.pWindow), pTree->tkwin);
        if (control) {
            if (Tk_IsMapped(control)) {
                Tk_UnmapWindow(control);
            }
            /* Tk_DestroyWindow(control); */
        }
    }
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
    if (pCanvas2->pFirst) {
        HtmlCanvasItem *pItem;
        HtmlCanvasItem *pItem2;
        int requireOrigin = 1;

        HtmlCanvasItem *pFirst2;            /* First item in child canvas */
        HtmlCanvasItem *pLast2;             /* First item in child canvas */

	/* Special case: If the canvas being draw into the parent consists
         * of a single text item, then see if it can be combined with a
         * text item already in the parent canvas.
         */
        if (pCanvas2->pFirst == pCanvas2->pLast && 
            pCanvas2->pFirst->type == CANVAS_TEXT &&
            pCanvas->pFirst && pCanvas->pFirst->pNext &&
            pCanvas->pFirst->pNext->pNext
        ) {
            HtmlCanvasItem *pO;
            HtmlCanvasItem *pI;

            HtmlCanvasItem *pNew = pCanvas2->pFirst;
            for (pO = pCanvas->pFirst; pO->pNext->pNext->pNext; pO = pO->pNext);
            pI = pO->pNext;
            if (pO->type == CANVAS_ORIGIN &&
                pI->type == CANVAS_TEXT && 
                pI->x.t.pFont == pNew->x.t.pFont &&
                pI->x.t.color == pNew->x.t.color && 
                pI->x.t.pNode == pNew->x.t.pNode && 
                (pI->x.t.y + pO->x.o.y) == (pNew->x.t.y + y)
            ) {
                int xi = pO->x.o.x;
                int xn;

                xi += pO->x.o.right;
                xn = pNew->x.t.x + x;

                if ((xn - xi) == pI->x.t.pFont->space_pixels) {
                    Tcl_AppendToObj(pI->x.t.pText, " ", 1);
                    Tcl_AppendObjToObj(pI->x.t.pText, pNew->x.t.pText);
                    pO->x.o.right = (x + pCanvas2->right) - pO->x.o.x;
                    HtmlDrawCleanup(pCanvas2);
                    goto draw_canvas_out;
                }
            } 
        }

        /* Figure out if we require a new CanvasOrigin primitive. This 
         * primitive is required if the new child canvas contains any
         * items drawn in directly (not by another call to HtmlDrawCanvas()).
         */
        pFirst2 = pCanvas2->pFirst;
        pLast2 = pCanvas2->pLast;
        if (
            0 &&
            pFirst2->type == CANVAS_ORIGIN && 
            pLast2 == pFirst2->x.o.pSkip
        ) {
            requireOrigin = 0;
            pFirst2->x.o.x += x;
            pFirst2->x.o.left += x;
            pFirst2->x.o.right += x;
            pFirst2->x.o.top += y;
            pFirst2->x.o.bottom += y;
            pFirst2->x.o.y += y;

            assert(pLast2->x.o.pSkip == 0);
            assert(pLast2->type == CANVAS_ORIGIN);
            pLast2->x.o.x -= x;
            pLast2->x.o.y -= y;
        }
 
        if (requireOrigin) {
            pItem = (HtmlCanvasItem *)HtmlAlloc(sizeof(HtmlCanvasItem));
            pItem->type = CANVAS_ORIGIN;
            pItem->x.o.x = x;
            pItem->x.o.y = y;
            pItem->x.o.left = pCanvas2->left;
            pItem->x.o.right = pCanvas2->right;
            pItem->x.o.bottom = pCanvas2->bottom;
            pItem->x.o.top = pCanvas2->top;
            pItem->x.o.pNode = pNode;
            linkItem(pCanvas, pItem);
        }

        if (pCanvas->pFirst) {
            pCanvas->pLast->pNext = pCanvas2->pFirst;
        } else {
            pCanvas->pFirst = pCanvas2->pFirst;
        }
        pCanvas->pLast = pCanvas2->pLast;
        pCanvas2->pFirst = 0;
        pCanvas2->pLast = 0;

        if (requireOrigin) {
            pItem2 = (HtmlCanvasItem *)HtmlClearAlloc(sizeof(HtmlCanvasItem));
            pItem2->type = CANVAS_ORIGIN;
            pItem2->x.o.x = x*-1;
            pItem2->x.o.y = y*-1;
            pItem->x.o.pSkip = pItem2;
            linkItem(pCanvas, pItem2);
        }

        pItem2 = 0;
        for (pItem = pCanvas2->pWindow; pItem; pItem = pItem->x.w.pNext) {
            pItem->x.w.absx += x;
            pItem->x.w.absy += y;
            pItem2 = pItem;
        }
        if (pItem2) {
            assert(!pItem2->x.w.pNext);
            assert(pCanvas2->pWindow);
            pItem2->x.w.pNext = pCanvas->pWindow;
            pCanvas->pWindow = pCanvas2->pWindow;
        }
    }

draw_canvas_out:
    pCanvas->left = MIN(pCanvas->left, x+pCanvas2->left);
    pCanvas->top = MIN(pCanvas->top, y+pCanvas2->top);
    pCanvas->bottom = MAX(pCanvas->bottom, y+pCanvas2->bottom);
    pCanvas->right = MAX(pCanvas->right, x+pCanvas2->right);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawComment --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#ifdef HTML_DEBUG
void 
HtmlDrawComment(pCanvas, zComment, size_only)
    HtmlCanvas *pCanvas;
    CONST char *zComment;
    int size_only;
{
    if (!size_only) {
        HtmlCanvasItem *pItem;
        pItem = (HtmlCanvasItem *)HtmlAlloc(sizeof(HtmlCanvasItem));
        pItem->type = CANVAS_COMMENT;
        pItem->x.c.pComment = Tcl_NewStringObj(zComment, -1);
        Tcl_IncrRefCount(pItem->x.c.pComment);
        linkItem(pCanvas, pItem);
    }
}
#endif

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
 * Results:
 *     None.
 *
 * Side effects:
 *     Adds an item to the canvas pCanvas.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlDrawBox(pCanvas, x, y, w, h, pNode, size_only)
    HtmlCanvas *pCanvas;
    int x;
    int y;
    int w;
    int h;
    HtmlNode *pNode;
    int size_only;
{
    if (!size_only) {
        HtmlCanvasItem *pItem; 
        pItem = (HtmlCanvasItem *)HtmlAlloc(sizeof(HtmlCanvasItem));
        pItem->type = CANVAS_BOX;
        pItem->x.box.x = x;
        pItem->x.box.y = y;
        pItem->x.box.w = w;
        pItem->x.box.h = h;
        pItem->x.box.pNode = pNode;
        linkItem(pCanvas, pItem);
    }

    pCanvas->left = MIN(pCanvas->left, x);
    pCanvas->right = MAX(pCanvas->right, x + w);
    pCanvas->bottom = MAX(pCanvas->bottom, y + h);
    pCanvas->top = MIN(pCanvas->top, y);
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
void HtmlDrawText(pCanvas, pText, x, y, w, pFont, color, size_only, pNode, iIndex)
    HtmlCanvas *pCanvas; 
    Tcl_Obj *pText; 
    int x;
    int y;
    int w;
    HtmlFont *pFont;
    XColor *color;
    int size_only;
    HtmlNode *pNode;
    int iIndex;
{
    if (!size_only) {
        HtmlCanvasItem *pItem; 
        pItem = (HtmlCanvasItem *)HtmlAlloc(sizeof(HtmlCanvasItem));
        pItem->type = CANVAS_TEXT;
        pItem->x.t.pText = pText;
        pItem->x.t.x = x;
        pItem->x.t.y = y;
        pItem->x.t.color = color;
        pItem->x.t.pFont = pFont;
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
HtmlDrawImage2(
        pCanvas, pImage, 
        iPositionX, iPositionY, isPositionPercent, eRepeat, 
        x, y, w, h, 
        size_only
)
    HtmlCanvas *pCanvas;
    HtmlImage2 *pImage;               /* Image name or NULL */
    int iPositionX;
    int iPositionY;
    unsigned char isPositionPercent;
    unsigned char eRepeat;           /* e.g. CSS_CONST_REPEAT_X */
    int x; 
    int y;
    int w;                      /* Width of image */
    int h;                      /* Height of image */
    int size_only;
{
    HtmlImageCheck(pImage);
    if (!size_only) {
        HtmlCanvasItem *pItem; 
        pItem = (HtmlCanvasItem *)HtmlAlloc(sizeof(HtmlCanvasItem));
        pItem->type = CANVAS_IMAGE;
        pItem->x.i2.pImage = pImage;
        HtmlImageRef(pImage);
        pItem->x.i2.eRepeat = eRepeat;
        pItem->x.i2.x = x;
        pItem->x.i2.y = y;
        pItem->x.i2.w = w;
        pItem->x.i2.h = h;
        pItem->x.i2.iPositionX = iPositionX;
        pItem->x.i2.iPositionY = iPositionY;
        pItem->x.i2.isPositionPercent = isPositionPercent;
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
HtmlDrawWindow(pCanvas, pWindow, x, y, w, h, size_only)
    HtmlCanvas *pCanvas;
    Tcl_Obj *pWindow;
    int x; 
    int y;
    int w;       /* Width of window */
    int h;       /* Height of window */
    int size_only;
{
    if (!size_only) {
        HtmlCanvasItem *pItem; 
        pItem = (HtmlCanvasItem *)HtmlAlloc(sizeof(HtmlCanvasItem));
        pItem->type = CANVAS_WINDOW;
        pItem->x.w.pWindow = pWindow;
        pItem->x.w.x = x;
        pItem->x.w.y = y;
        pItem->x.w.absx = x;
        pItem->x.w.absy = y;
        if (pWindow) {
            Tcl_IncrRefCount(pWindow);
        }
        pItem->x.w.pNext = pCanvas->pWindow;
        pCanvas->pWindow = pItem;
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
 * HtmlDrawQuad --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlDrawQuad(pCanvas, x1, y1, x2, y2, x3, y3, x4, y4, color, size_only)
    HtmlCanvas *pCanvas; 
    int x1, y1; 
    int x2, y2; 
    int x3, y3; 
    int x4, y4; 
    XColor *color;
    int size_only;
{
    if (!size_only) {
        HtmlCanvasItem *pItem;
        pItem = (HtmlCanvasItem *)HtmlAlloc(sizeof(HtmlCanvasItem));
        pItem->type = CANVAS_QUAD;
        pItem->x.q.x1 = x1;
        pItem->x.q.y1 = y1;
        pItem->x.q.x2 = x2;
        pItem->x.q.y2 = y2;
        pItem->x.q.x3 = x3;
        pItem->x.q.y3 = y3;
        pItem->x.q.y4 = y4;
        pItem->x.q.x4 = x4;
        pItem->x.q.x4 = x4;
        pItem->x.q.color = color;
        linkItem(pCanvas, pItem);
    }

    pCanvas->left = MIN5(pCanvas->left, x1, x2, x3, x4);
    pCanvas->top = MIN5(pCanvas->top, y1, y2, y3, y4);
    pCanvas->bottom = MAX5(pCanvas->bottom, y1, y2, y3, y4);
    pCanvas->right = MAX5(pCanvas->right, x1, x2, x3, x4);

}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawBackground --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlDrawBackground(pCanvas, color, size_only)
    HtmlCanvas *pCanvas;
    XColor *color;
    int size_only;
{
    if (!size_only) {
        HtmlCanvasItem *pItem;
        pItem = (HtmlCanvasItem *)HtmlAlloc(sizeof(HtmlCanvasItem));
        pItem->type = CANVAS_BACKGROUND;
        pItem->x.b.color = color;
        linkItem(pCanvas, pItem);
    }
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

    if (pCanvas->pPrimitives) {
        Tcl_SetObjResult(interp, pCanvas->pPrimitives);
        return TCL_OK;
    }

    pPrimitives = Tcl_NewObj();
    Tcl_IncrRefCount(pPrimitives);

    for (pItem=pCanvas->pFirst; pItem; pItem=pItem->pNext) {
        Tcl_Obj *pList = 0;
        nObj = 0;
        switch (pItem->type) {
            case CANVAS_ORIGIN:
                nObj = 3;
                aObj[0] = Tcl_NewStringObj("draw_origin", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.o.x);
                aObj[2] = Tcl_NewIntObj(pItem->x.o.y);
                break;
            case CANVAS_TEXT:
                nObj = 6;
                aObj[0] = Tcl_NewStringObj("draw_text", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.t.x);
                aObj[2] = Tcl_NewIntObj(pItem->x.t.y);
                aObj[3] = Tcl_NewStringObj(
                    Tk_NameOfFont(pItem->x.t.pFont->tkfont), -1
                ); 
                aObj[4] = Tcl_NewStringObj(Tk_NameOfColor(pItem->x.t.color),-1);
                aObj[5] = pItem->x.t.pText;
                break;
            case CANVAS_IMAGE:
                if (pItem->x.i2.pImage) {
                    nObj = 9;
                    aObj[0] = Tcl_NewStringObj("draw_image2", -1);
                    aObj[1] = Tcl_NewStringObj(
                            HtmlCssConstantToString(pItem->x.i2.eRepeat), -1);
                    if (pItem->x.i2.isPositionPercent) {
                        char zBuf[128];
                        sprintf(zBuf, "%.2f%%", 
                            (double)pItem->x.i2.iPositionX / 100.0
                        );
                        aObj[2] = Tcl_NewStringObj(zBuf, -1);
                        sprintf(zBuf, "%.2f%%", 
                            (double)pItem->x.i2.iPositionY / 100.0
                        );
                        aObj[3] = Tcl_NewStringObj(zBuf, -1);
                    } else {
                        char zBuf[128];
                        sprintf(zBuf, "%dpx", pItem->x.i2.iPositionX);
                        aObj[2] = Tcl_NewStringObj(zBuf, -1);
                        sprintf(zBuf, "%dpx", pItem->x.i2.iPositionY);
                        aObj[3] = Tcl_NewStringObj(zBuf, -1);
                    }
                    aObj[4] = Tcl_NewIntObj(pItem->x.i2.x);
                    aObj[5] = Tcl_NewIntObj(pItem->x.i2.y);
                    aObj[6] = Tcl_NewIntObj(pItem->x.i2.w);
                    aObj[7] = Tcl_NewIntObj(pItem->x.i2.h);
                    aObj[8] = HtmlImageUnscaledName(pItem->x.i2.pImage);
                }
                break;
            case CANVAS_WINDOW:
                nObj = 4;
                aObj[0] = Tcl_NewStringObj("draw_window", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.w.x);
                aObj[2] = Tcl_NewIntObj(pItem->x.w.y);
                aObj[3] = pItem->x.w.pWindow;
                break;
            case CANVAS_QUAD:
                nObj = 10;
                aObj[0] = Tcl_NewStringObj("draw_quad", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.q.x1);
                aObj[2] = Tcl_NewIntObj(pItem->x.q.y1);
                aObj[3] = Tcl_NewIntObj(pItem->x.q.x2);
                aObj[4] = Tcl_NewIntObj(pItem->x.q.y2);
                aObj[5] = Tcl_NewIntObj(pItem->x.q.x3);
                aObj[6] = Tcl_NewIntObj(pItem->x.q.y3);
                aObj[7] = Tcl_NewIntObj(pItem->x.q.x4);
                aObj[8] = Tcl_NewIntObj(pItem->x.q.y4);
                aObj[9] = Tcl_NewStringObj(Tk_NameOfColor(pItem->x.q.color),-1);
                break;
            case CANVAS_BACKGROUND:
                nObj = 2;
                aObj[0] = Tcl_NewStringObj("draw_background", -1);
                aObj[1] = Tcl_NewStringObj(Tk_NameOfColor(pItem->x.b.color),-1);
                break;
            case CANVAS_COMMENT:
                pList = Tcl_NewStringObj("# ", 2);
                Tcl_AppendObjToObj(pList, pItem->x.c.pComment);
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
        }
        if (nObj>0) {
            pList = Tcl_NewObj();
            Tcl_SetListObj(pList, nObj, aObj);
        }
        if (pList) {
            Tcl_ListObjAppendElement(interp, pPrimitives, pList);
        }
    }
    pCanvas->pPrimitives = pPrimitives;
    Tcl_SetObjResult(interp, pPrimitives);
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
    int rc;

    gc_values.foreground = xcolor->pixel;
    gc = Tk_GetGC(win, GCForeground, &gc_values);
    points[0].x = x1; points[0].y = y1;
    points[1].x = x2; points[1].y = y2;
    points[2].x = x3; points[2].y = y3;
    points[3].x = x4; points[3].y = y4;
    rc = XFillPolygon(display, d, gc, points, 4, Convex, CoordModePrevious);
    Tk_FreeGC(display, gc);

    return rc;
}

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
static void 
drawBox(pTree, pBox, drawable, x, y, w, h)
    HtmlTree *pTree;
    CanvasBox *pBox;
    Drawable drawable;
    int x;                 /* X-coord in *pDrawable */
    int y;                 /* Y-coord in *pDrawable */
    int w;                 /* Total width of *pDrawable */
    int h;                 /* Total height of *pDrawable */
{
    HtmlComputedValues *pV = pBox->pNode->pPropertyValues;

    /* Figure out the widths of the top, bottom, right and left borders */
    int tw = ((pV->eBorderTopStyle != CSS_CONST_NONE) ? pV->border.iTop : 0);
    int bw = ((pV->eBorderBottomStyle != CSS_CONST_NONE)?pV->border.iBottom:0);
    int rw = ((pV->eBorderRightStyle != CSS_CONST_NONE) ? pV->border.iRight :0);
    int lw = ((pV->eBorderLeftStyle != CSS_CONST_NONE) ? pV->border.iLeft : 0);

    int bg_x = x + pBox->x + lw;
    int bg_y = y + pBox->y + tw;
    int bg_w = pBox->w - lw - rw;
    int bg_h = pBox->h - tw - bw;

    /* Figure out the colors of the top, bottom, right and left borders */
    XColor *tc = pV->cBorderTopColor->xcolor;
    XColor *rc = pV->cBorderRightColor->xcolor;
    XColor *bc = pV->cBorderBottomColor->xcolor;
    XColor *lc = pV->cBorderLeftColor->xcolor;

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
    if (lw > 0 && lc) {
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

    /* Solid background, if required */
    if (pV->cBackgroundColor->xcolor) {
        fill_quad(pTree->win, drawable, pV->cBackgroundColor->xcolor,
            bg_x, bg_y,
            bg_w, 0,
            0, bg_h,
            -1 * bg_w, 0
        );
    }

    /* Image background, if required. This bit's a little tricky. */
    if (pV->imBackgroundImage) {
        Tk_Image img;
        Pixmap ipix;
        GC gc;
        XGCValues gc_values;
        int iWidth;
        int iHeight;
        int iPosX;
        int iPosY;
        Tk_Window win = pTree->win;
        Display *display = Tk_Display(win);
        int dep = Tk_Depth(win);
        int eR = pV->eBackgroundRepeat;
 
        img = HtmlImageImage(pV->imBackgroundImage);
        Tk_SizeOfImage(img, &iWidth, &iHeight);

        if (iWidth > 0 && iHeight > 0) {
            HtmlNode *pBgNode = pBox->pNode;
    
            /* Create a pixmap of the image */
            ipix = Tk_GetPixmap(display, Tk_WindowId(win),iWidth, iHeight, dep);
            for ( ; pBgNode; pBgNode = HtmlNodeParent(pBgNode)) {
                HtmlComputedValues *pV2 = pBgNode->pPropertyValues;
                if (pV2->cBackgroundColor->xcolor) {
                    fill_quad(pTree->win, ipix, pV2->cBackgroundColor->xcolor,
                        0, 0, iWidth, 0, 0, iHeight, -1 * iWidth, 0
                    );
                    break;
                }
            }
            Tk_RedrawImage(img, 0, 0, iWidth, iHeight, ipix, 0, 0);
    
            iPosX = pV->iBackgroundPositionX;
            iPosY = pV->iBackgroundPositionY;
            if ( pV->mask & PROP_MASK_BACKGROUND_POSITION_X ){
                iPosX = (double)iPosX * (double)(bg_w - iWidth) / 10000.0;
                iPosY = (double)iPosY * (double)(bg_h - iHeight) / 10000.0;
            }
            iPosX += bg_x;
            iPosY += bg_y;
    
            gc_values.ts_x_origin = iPosX;
            gc_values.ts_y_origin = iPosY;
    
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
    
            /* Draw a rectangle to the drawable with origin (bg_x, bg_y). The
             * size of the rectangle is (bg_w *  bg_h). The background image
             * is tiled across the region with a relative origin point as
             * defined by (gc_values.ts_x_origin, gc_values.ts_y_origin).
             */
            gc_values.tile = ipix;
            gc_values.fill_style = FillTiled;
            gc = Tk_GetGC(pTree->win, 
                GCTile | GCTileStipXOrigin | GCTileStipYOrigin | GCFillStyle, 
                &gc_values
            );
            if (bg_h > 0 && bg_w > 0) {
                XFillRectangle(display, drawable, gc, bg_x, bg_y, bg_w, bg_h);
            }
            Tk_FreePixmap(display, ipix);
            Tk_FreeGC(display, gc);
        }
    }
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
drawImage(pTree, pI2, pDrawable, x, y, w, h)
    HtmlTree *pTree;
    CanvasImage *pI2;
    Drawable *pDrawable;
    int x;                 /* X-coord in *pDrawable */
    int y;                 /* Y-coord in *pDrawable */
    int w;                 /* Total width of *pDrawable */
    int h;                 /* Total height of *pDrawable */
{
    if (pI2->pImage) {
        int iw;                /* Intrinsic width of image */
        int ih;                /* Intrinsic height of image */
        int bw;                /* Width of block to draw */
        int bh;                /* Height of block to draw */
        int bx;                /* X-coordinate of block in drawable */
        int by;                /* Y-coordinate of block in drawable */
        int ix = 0;
        int iy = 0;

        int clipx1, clipx2;
        int clipy1, clipy2;

        int iPosX = pI2->iPositionX;
        int iPosY = pI2->iPositionY;

        const int eR = pI2->eRepeat;          /* Copy of pI2->eRepeat */

        /* Retrieve the Tk-image and measure it. */
        Tk_Image img = HtmlImageImage(pI2->pImage);
        Tk_SizeOfImage(img, &iw, &ih);
        if (iw <= 0 || ih <= 0) {
            /* Nothing to draw for an empty image */
            return;
        }

        /* Make sure iPosX and iPosY are pixel values.
         */
        if (pI2->isPositionPercent) {
            iPosX = (double)iPosX * (double)(pI2->w - iw) / 10000.0;
            iPosY = (double)iPosY * (double)(pI2->h - ih) / 10000.0;
        }

        /* Figure out the X and Y coordinates of the block origin. */
        if (eR == CSS_CONST_REPEAT || eR == CSS_CONST_REPEAT_X) {
            bx = x + pI2->x;
        } else {
            bx = x + iPosX + pI2->x;
            bw = iw;
        }
        if (eR == CSS_CONST_REPEAT || eR == CSS_CONST_REPEAT_Y) {
            by = y + pI2->y;
        } else {
            by = y + iPosY + pI2->y;
            bh = ih;
        }

        clipx1 = MAX(0, pI2->x + x);
        clipy1 = MAX(0, pI2->y + y);

        clipx2 = MIN(w, pI2->x + x + pI2->w);
        clipy2 = MIN(h, pI2->y + y + pI2->h);

        /* Clip the values to make sure we don't paint outside the drawable */
        if (bx < clipx1) {
            bw = bw - (clipx1 - bx);
            iPosX = (clipx1 - bx);
            bx = clipx1;
        }
        if (by < clipy1) {
            bh = bh - (clipy1 - by);
            iPosY = (clipy1 - by);
            by = clipy1;
        }
        if (bx + bw > clipx2)  bw = clipx2 - bx;
        if (by + bh > clipy2)  bh = clipy2 - by;

        /* There are two ways to draw an image to the drawable. The first
         * is to draw it directly using Tk_RedrawImage(). The second is to
         * draw the image to a new pixmap, request a graphics context with 
         * the newly allocated pixmap as the background tile and call 
         * XFillRectangle().
         *
         * The second method can be more efficient for background images. If
         * a small (perhaps 1x1) image is tiled over a large area, hundreds
         * or even thousands of copies are made. Doing this in client code
         * via multiple calls to Tk_RedrawImage() is too slow in some cases.
         * The disadvantage is that images featuring transparency are 
         * handled incorrectly.
         *
         * We must use the second method if the CanvasImage.eRepeat variable
         * is set to other than 'no-repeat'.
         */
        if (eR == CSS_CONST_NO_REPEAT) {
            Tk_RedrawImage(img, iPosX, iPosY, bw, bh, *pDrawable, bx, by);
        } else {
            GC gc;                 /* Graphics context to draw with */
            XGCValues gc_values;   /* Structure used to specify gc */

            Tk_Window win = pTree->tkwin;
            Display *pDisplay = Tk_Display(win);
            int depth = Tk_Depth(win);

            /* Create a pixmap of the image */
            Pixmap ipix;
            ipix = Tk_GetPixmap(pDisplay, Tk_WindowId(win), iw, ih, depth);
            Tk_RedrawImage(img, 0, 0, iw, ih, ipix, 0, 0);
        
            gc_values.tile = ipix;
            gc_values.fill_style = FillTiled;
            gc_values.ts_x_origin = iPosX;
            gc_values.ts_y_origin = iPosY;
            gc = Tk_GetGC(pTree->win, 
                GCTile|GCTileStipXOrigin|
                GCTileStipYOrigin|GCFillStyle, 
                &gc_values
            );
            XFillRectangle(pDisplay, *pDrawable, gc, bx, by, bw, bh);
            Tk_FreePixmap(pDisplay, ipix);
            Tk_FreeGC(pDisplay, gc);
        }

#if 0
        if (
            pI2->eRepeat != CSS_CONST_NO_REPEAT ||
            pI2->iPositionX != 0 || 
            pI2->iPositionY != 0
        ) {
            GC gc;                 /* Graphics context to draw with */
            XGCValues gc_values;   /* Structure used to specify gc */
            int bw;                /* Width of rectangle to paint */
            int bh;                /* Height of rectangle to paint */
            Tk_Window win = pTree->tkwin;
            Display *pDisplay = Tk_Display(win);
            int depth = Tk_Depth(win);

            int x1 = pI2->iPositionX;   /* Drawable coordinate */
            int y1 = pI2->iPositionY;   /* Drawable coordinate */
            if (pI2->isPositionPercent) {
                x1 = (double)x1 * (double)(pI2->w - iw) / 10000.0;
                y1 = (double)y1 * (double)(pI2->h - ih) / 10000.0;
            }
            bw = iw;
            bh = ih;

            gc_values.ts_x_origin = x1 + pI2->x + x;
            gc_values.ts_y_origin = y1 + pI2->y + y;

            if (
                pI2->eRepeat == CSS_CONST_REPEAT || 
                pI2->eRepeat == CSS_CONST_REPEAT_X
            ) {
                x1 = 0;
                bw = pI2->w;
            }
            if (
                pI2->eRepeat == CSS_CONST_REPEAT || 
                pI2->eRepeat == CSS_CONST_REPEAT_Y
            ) {
                y1 = 0;
                bh = pI2->h;
            }
            x1 += (pI2->x + x);
            y1 += (pI2->y + y);

            if (x1 < 0) {
                bw += x1;
                x1 = 0;
            }
            if (y1 < 0) {
                bh = bh + y1;
                y1 = 0;
            }
            if ((x1 + bw) > w) {
                bw = (w - x1);
            }
            if ((y1 + bh) > h) {
                bh = (h - y1);
            }

            if (bh > 0 && bw > 0) {
                /* Create a pixmap of the image */
                Pixmap ipix;
                ipix = Tk_GetPixmap(pDisplay, Tk_WindowId(win), iw, ih, depth);
                Tk_RedrawImage(img, 0, 0, iw, ih, ipix, 0, 0);
        
                gc_values.tile = ipix;
                gc_values.fill_style = FillTiled;
                gc = Tk_GetGC(pTree->win, 
                    GCTile|GCTileStipXOrigin|
                    GCTileStipYOrigin|GCFillStyle, 
                    &gc_values
                );
                XFillRectangle(pDisplay, *pDrawable, gc, x1, y1, bw, bh);
                Tk_FreePixmap(pDisplay, ipix);
                Tk_FreeGC(pDisplay, gc);
            }
        } else {
            int ix;              /* Image x */
            int iy;              /* Image y */
            int dx;              /* Drawable x */
            int dy;              /* Drawable y */

            dx = MAX(0, pI2->x + x);
            dy = MAX(0, pI2->y + y);
            ix = MAX(0, -1 * (pI2->x + x));
            iy = MAX(0, -1 * (pI2->y + y));

            iw = MIN(iw, w - dx);
            ih = MIN(ih, h - dy);
            iw = MIN(iw, pI2->w);
            ih = MIN(ih, pI2->h);

            Tk_RedrawImage(img, ix, iy, iw, ih, *pDrawable, dx, dy);
        }
#endif
    }
}

#define SWAPINT(x,y) {int tmp = x; x = y; y = tmp;}

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
    CONST char *z;

    GC gc = 0;
    XGCValues gc_values;
    int mask;

    int n;
    Tk_Font font = pT->pFont->tkfont;

    int iSelFrom;      /* Index in this string where the selection starts */
    int iSelTo = 0;    /* Index in this string where the selection ends */

    z = Tcl_GetStringFromObj(pT->pText, &n);
    iSelFrom = n;

    if (pTree->pFromNode && pT->pNode) {
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
            iSelFrom = 0;
            iSelTo = n;
            if (iToNode == iThis && iToIndex >= 0) {
                iSelTo = iToIndex - pT->iIndex;
            }
            if (iFromNode == iThis && iFromIndex >= 0) {
                iSelFrom = iFromIndex - pT->iIndex;
            }
            iSelFrom = MAX(0, iSelFrom);
            iSelTo = MIN(n, iSelTo);
        }
    }

    /* Unless the entire line is selected, draw the text in the regular way */
    if (iSelTo < n || iSelFrom > 0) {
        mask = GCForeground | GCFont;
        gc_values.foreground = pT->color->pixel;
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
    if (iSelTo > 0 && iSelFrom < n) {
        CONST char *zSel = &z[iSelFrom];
        int nSel;
        int w;                               /* Pixels of selected text */
        int xs = x;                          /* Pixel offset of selected text */
        int h;                               /* Height of text line */
        int ybg;                             /* Y coord for bg rectangle */

        nSel     = iSelTo - iSelFrom;
        if (iSelFrom > 0) {
            xs += Tk_TextWidth(font, z, iSelFrom);
        }
        w = Tk_TextWidth(font, zSel, nSel);
        h = pT->pFont->metrics.ascent + pT->pFont->metrics.descent;
        ybg = pT->y + y - pT->pFont->metrics.ascent;

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
getPixmap(pTree, xcanvas, ycanvas, w, h)
    HtmlTree *pTree;
    int xcanvas;
    int ycanvas;
    int w;
    int h;
{
    Pixmap pmap;
    Display *pDisplay;
    Tk_Window win = pTree->win;
    HtmlCanvasItem *pItem;

    GC gc = 0;
    XGCValues gc_values;
    int mask;

    HtmlCanvas *pCanvas = &pTree->canvas;
    int x = xcanvas * -1;
    int y = ycanvas * -1;

    Tk_MakeWindowExist(win);

    pDisplay = Tk_Display(win);
    pmap = Tk_GetPixmap(pDisplay, Tk_WindowId(win), w, h, Tk_Depth(win));

    for (pItem=pCanvas->pFirst; pItem; pItem=pItem->pNext) {
        switch (pItem->type) {

            case CANVAS_ORIGIN: {
                int skip = 1;
                while (skip) {
                    CanvasOrigin *pOrigin = &pItem->x.o;
                    x += pOrigin->x;
                    y += pOrigin->y;
    
                    /* If the contents of this canvas is completely outside
		     * of the clipping border, then we can skip to the
		     * 'pSkip' member of pItem.
                     */
                    skip = 0;
                    if (pOrigin->pSkip) {
                        if (((x + pOrigin->right) < 0) ||
                            ((x + pOrigin->left) > w) ||
                            ((y + pOrigin->top) > h) ||
                            ((y + pOrigin->bottom) < 0)
                        ) {
                            pItem = pOrigin->pSkip;
                            assert(pItem->type == CANVAS_ORIGIN);
                            assert(!pItem->x.o.pSkip);
                            skip = 1;
                        }
                    }
                }

                break;
            }

            case CANVAS_TEXT: {
                drawText(pTree, pItem, pmap, x, y);
                break;
            }

            case CANVAS_IMAGE: {
                drawImage(pTree, &pItem->x.i2, &pmap, x, y, w, h);
                break;
            }

            case CANVAS_BOX: {
                drawBox(pTree, &pItem->x.box, pmap, x, y, w, h);
                break;
            }

            case CANVAS_WINDOW:
                break;

            case CANVAS_QUAD: {
                XPoint points[4];

                gc_values.foreground = pItem->x.q.color->pixel;
                mask = GCForeground;
                gc = Tk_GetGC(pTree->win, mask, &gc_values);

                points[0].x = pItem->x.q.x1 + x;
                points[1].x = pItem->x.q.x2 + x;
                points[2].x = pItem->x.q.x3 + x;
                points[3].x = pItem->x.q.x4 + x;

                points[0].y = pItem->x.q.y1 + y;
                points[1].y = pItem->x.q.y2 + y;
                points[2].y = pItem->x.q.y3 + y;
                points[3].y = pItem->x.q.y4 + y;

                XFillPolygon(
                    pDisplay, pmap, gc, points, 4, Convex, CoordModeOrigin);
                break;
            }
            case CANVAS_BACKGROUND: {
                gc_values.foreground = pItem->x.b.color->pixel;
                mask = GCForeground;
                gc = Tk_GetGC(pTree->win, mask, &gc_values);
                XFillRectangle(pDisplay, pmap, gc, 0, 0, w, h);
                break;
            }
        }

        if (gc) {
            Tk_FreeGC(pDisplay, gc);
            gc = 0;
        }
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
        pixmap = getPixmap(pTree, 0, 0, w, h);
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

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutSize --
 *
 *     <widget> layout size:
 * 
 *     Return the horizontal and vertical size of the layout as a
 *     two-element list.
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
HtmlLayoutSize(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    Tcl_Obj *pRet;
    int width;
    int height;
    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 3, objv, "");
        return TCL_ERROR;
    }

    pRet = Tcl_NewObj();
    Tcl_IncrRefCount(pRet);

    width = pTree->canvas.right;
    height = pTree->canvas.bottom;

    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewIntObj(width));
    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewIntObj(height));
    Tcl_SetObjResult(interp, pRet);
    Tcl_DecrRefCount(pRet);
    return TCL_OK;
}

static int    
searchCanvas(pTree, ymin, ymax, xFunc, clientData)
    HtmlTree *pTree;
    int ymin;
    int ymax;
    int (*xFunc)(HtmlCanvasItem *, int, int, ClientData);
    ClientData clientData;
{
    HtmlCanvasItem *pItem;
    HtmlCanvasItem *pSkip = 0;
    HtmlCanvas *pCanvas = &pTree->canvas;
    int origin_x = 0;
    int origin_y = 0;
    int rc = 0;
     
    for (pItem = pCanvas->pFirst; pItem; pItem = (pSkip?pSkip:pItem->pNext)) {
        pSkip = 0;
        if (pItem->type == CANVAS_ORIGIN) {
            CanvasOrigin *pOrigin = &pItem->x.o;
            origin_x += pOrigin->x;
            origin_y += pOrigin->y;
            if (pOrigin->pSkip && (
                (ymax >= 0 && (origin_y + pOrigin->top) > ymax) ||
                (ymin >= 0 && (origin_y + pOrigin->bottom) < ymin))
            ) {
               pSkip = pOrigin->pSkip;
            }
        } 
        if (0 != (rc = xFunc(pItem, origin_x, origin_y, clientData))) {
            return rc;
        }
    }
 
    return 0;
}

typedef struct NodeIndexQuery NodeIndexQuery;
struct NodeIndexQuery {
    int x;
    int y;
    CanvasText *pClosest;
    HtmlNode *pFlow;
    int mindist;
    int closest_x;
};

static HtmlNode *
findFlowNode(pNode)
    HtmlNode *pNode;
{
    HtmlNode *p;
    for (p = pNode; p; p = HtmlNodeParent(p)) {
        HtmlComputedValues *pV = p->pPropertyValues;
        if (pV && 
            (pV->eDisplay == CSS_CONST_TABLE_CELL ||
             pV->eFloat != CSS_CONST_NONE)
        ) {
            break;
        }
    }
    return p;
}

static int
layoutNodeIndexCb(pItem, origin_x, origin_y, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    ClientData clientData;
{
    if (pItem->type == CANVAS_TEXT) {
        NodeIndexQuery *pQuery = (NodeIndexQuery *)clientData;
        CanvasText *pT = &pItem->x.t;
        int left   = pT->x + origin_x;
        int top    = origin_y + pT->y - pT->pFont->metrics.ascent;
        int bottom = origin_y + pT->y + pT->pFont->metrics.descent;

        if (pT->pNode && left <= pQuery->x && top <= pQuery->y) {
            int n;
            const char *z;
            int right;
            int dist = 0;

            z = Tcl_GetStringFromObj(pT->pText, &n);
            right = left + Tk_TextWidth(pT->pFont->tkfont, z, n);

            dist += MAX(pQuery->y - bottom, 0);
            dist += MAX(top - pQuery->y, 0);
            dist += MAX(pQuery->x - right, 0);
            dist += MAX(left - pQuery->x, 0);

            if (dist < pQuery->mindist || !pQuery->pClosest) {
                pQuery->pClosest = pT;
                pQuery->mindist = dist;
                pQuery->closest_x = left;
                pQuery->pFlow = findFlowNode(pT->pNode);
            } else if (pQuery->pFlow == findFlowNode(pT->pNode)) {
                pQuery->pClosest = pT;
                pQuery->closest_x = left;
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
    ClientData cd = (ClientData *)&sQuery;

    memset(&sQuery, 0, sizeof(NodeIndexQuery));
    sQuery.x = x;
    sQuery.y = y;

    searchCanvas(pTree, y-100, y, layoutNodeIndexCb, (ClientData)&sQuery);
    if (!sQuery.pClosest) {
        int ymin = y - pTree->iScrollY;
        searchCanvas(pTree, ymin, y, layoutNodeIndexCb, cd);
    }
    if (!sQuery.pClosest) {
        searchCanvas(pTree, -1, y, layoutNodeIndexCb, cd);
    }

    if (sQuery.pClosest) {
        HtmlNode *pNode = sQuery.pClosest->pNode;     /* Node to return */
        int iIndex = 0;                               /* Index to return */

        /* Calculate the index to return */
        int dummy;
        int n;
        const char *z;
        Tk_Font font = sQuery.pClosest->pFont->tkfont;
        z = Tcl_GetStringFromObj(sQuery.pClosest->pText, &n);
        iIndex = Tk_MeasureChars(font, z, n, x - sQuery.closest_x, 0, &dummy);
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
HtmlNode *
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
    int origin_x = 0;
    int origin_y = 0;
    HtmlCanvas *pCanvas = &pTree->canvas;
    HtmlCanvasItem *pItem;

    HtmlNode **apNode = 0;
    int nNodeAlloc = 0;
    int nNode = 0;

    for (pItem=pCanvas->pFirst; pItem; pItem=pItem->pNext) {
        if (pItem->type == CANVAS_ORIGIN) {
            CanvasOrigin *pOrigin = &pItem->x.o;
            origin_x += pOrigin->x;
            origin_y += pOrigin->y;
            if (pOrigin->pSkip && 
                (x < (pOrigin->left + origin_x) ||
                 x > (pOrigin->right + origin_x) ||
                 y < (pOrigin->top + origin_y) ||
                 y > (pOrigin->bottom + origin_y))
            ) {
                pItem = pOrigin->pSkip;
                origin_x -= pOrigin->x;
                origin_y -= pOrigin->y;
            } else {
                 if (pOrigin->pNode) {
                     int i;
                     HtmlNode *pDesc;
                     for (i = 0; i < nNode; i++) {
                         pDesc = returnDescNode(pOrigin->pNode, apNode[i]);
                         if (pDesc) {
                             apNode[i] = pDesc;
                             break;
                         }
                     }
                     if (i == nNode) {
                         nNode++;
                         if (nNode > nNodeAlloc) {
                             int nByte;
                             nNodeAlloc += 16;
                             nByte = nNodeAlloc * sizeof(HtmlNode *);
                             apNode = (HtmlNode **)HtmlRealloc(apNode, nByte);
                         }
                         apNode[nNode - 1] = pOrigin->pNode;
                     }
                 }
            }
        }
    }

    if (nNode > 0) {
        int i;
        Tcl_Obj *pRet = Tcl_NewObj();
        for (i = 0; i < nNode; i++) {
            Tcl_Obj *pCmd = HtmlNodeCommand(pTree, apNode[i]);
            Tcl_ListObjAppendElement(0, pRet, pCmd);
        }
        Tcl_SetObjResult(pTree->interp, pRet);
    }
    HtmlFree(apNode);
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
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetPaint --
 *
 *     <widget> widget paint CANVAS-X CANVAS-Y X Y WIDTH HEIGHT
 *
 *     This command updates a rectangular portion of the window contents.
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
HtmlWidgetPaint(pTree, canvas_x, canvas_y, x, y, width, height)
    HtmlTree *pTree;
    int canvas_x;
    int canvas_y;
    int x;
    int y;
    int width;
    int height;
{
    Pixmap pixmap;
    GC gc;
    XGCValues gc_values;
    Display *pDisp; 
    Tk_Window win;                      /* Window to draw to */

    if (width <= 0 || height <= 0) {
        return TCL_OK;
    }

    win = pTree->tkwin;
    Tk_MakeWindowExist(win);
    pDisp = Tk_Display(win);

    pixmap = getPixmap(pTree, canvas_x, canvas_y, width, height);

    memset(&gc_values, 0, sizeof(XGCValues));
    gc = Tk_GetGC(pTree->win, 0, &gc_values);

    XCopyArea(pDisp, pixmap, Tk_WindowId(win), gc, 0, 0, width, height, x, y);
    Tk_FreePixmap(pDisp, pixmap);

    Tk_FreeGC(pDisp, gc);
    return TCL_OK;
}

/*
 * A pointer to an instance of the following structure is passed by 
 * HtmlLayoutPaintText() to paintNodesSearchCb() as the client-data
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
 *     The callback for the canvas search performed by HtmlLayoutPaintText().
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
        if (pT->pNode) {
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
                    int top    = origin_y + pT->y - pT->pFont->metrics.ascent;
                    int bottom = origin_y + pT->y + pT->pFont->metrics.descent;
                    int left   = origin_x + pT->x;
                    int right;
                    int nFin = n;

                    if (iNode == p->iNodeFin && p->iIndexFin >= 0) {
                        nFin = MIN(n, 1 + p->iIndexFin - pT->iIndex);
                    }
                    right = Tk_TextWidth(pT->pFont->tkfont, z, nFin) + left;
                    if (iNode == p->iNodeStart && p->iIndexStart > 0) {
                        int nStart = MAX(0, p->iIndexStart - pT->iIndex);
                        if (nStart > 0) {
                            assert(nStart <= n);
                            left += Tk_TextWidth(pT->pFont->tkfont, z, nStart);
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
 * HtmlLayoutPaintText --
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
HtmlLayoutPaintText(pTree, iNodeStart, iIndexStart, iNodeFin, iIndexFin)
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

    searchCanvas(pTree, ymin, ymax, paintNodesSearchCb, (ClientData)&sQuery);

    x = sQuery.left - pTree->iScrollX;
    x = MAX(x, 0);

    w = (sQuery.right - pTree->iScrollX) - x;
    w = MIN(w, Tk_Width(pTree->tkwin) - x);

    y = sQuery.top - pTree->iScrollY;
    y = MAX(y, 0);

    h = (sQuery.bottom - pTree->iScrollY) - y;
    h = MIN(h, Tk_Height(pTree->tkwin) - y);

    if (w > 0 && h > 0) {
        HtmlCallbackSchedule(pTree, HTML_CALLBACK_DAMAGE);
        HtmlCallbackExtents(pTree, x, y, w, h);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetScroll --
 *
 *     <widget> widget scroll X Y
 *
 *     Scroll the widget X pixels in the X direction and Y pixels in the Y
 *     direction. Y is positive if the user is scrolling the document from
 *     top to bottom (i.e. presses PgDn). X is positive if the user scrolls
 *     the document from left to right.
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
HtmlWidgetScroll(pTree, x, y)
    HtmlTree *pTree;
    int x;
    int y;
{
    Tk_Window win;
    Display *display;
    GC gc;
    XGCValues gc_values;

    int source_x, source_y;
    int dest_x, dest_y;
    int width, height;

    win = pTree->tkwin; 
    display = Tk_Display(win);
    memset(&gc_values, 0, sizeof(XGCValues));
    gc = Tk_GetGC(pTree->win, 0, &gc_values);

    dest_x = MIN(x, 0) * -1;
    source_x = MAX(x, 0);
    width = Tk_Width(win) - MAX(source_x, dest_x);

    dest_y = MIN(y, 0) * -1;
    source_y = MAX(y, 0);
    height = Tk_Height(win) - MAX(dest_y, source_y);

    if (height > 0) {
        XCopyArea(display, Tk_WindowId(win), Tk_WindowId(win), gc, 
                source_x, source_y, width, height, dest_x, dest_y);
    }

    Tk_FreeGC(display, gc);
    /* XFlush(display); */

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetMapControls --
 *
 *     <widget> widget mapcontrols X Y
 *
 *     This command updates the position and visibility of all Tk windows
 *     mapped by the widget (i.e. as <form> controls). The parameters X and
 *     Y are the coordinates of the point on the virtual canvas that
 *     corresponds to the top-left corner of the viewport. 
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
HtmlWidgetMapControls(pTree)
    HtmlTree *pTree;
{
    int x = pTree->iScrollX;
    int y = pTree->iScrollY;
    int w;
    int h;
    Tcl_Interp *interp = pTree->interp;
    HtmlCanvas *pCanvas = &pTree->canvas;
    HtmlCanvasItem *pItem;

    Tk_Window win = pTree->tkwin;
    w = Tk_Width(pTree->tkwin);
    h = Tk_Height(pTree->tkwin);

    for (pItem = pCanvas->pWindow; pItem; pItem = pItem->x.w.pNext) {
        Tk_Window control;
        CanvasWindow *pWin = &pItem->x.w;

        control = Tk_NameToWindow(interp, Tcl_GetString(pWin->pWindow), win);
        if (control) {
            int winwidth = Tk_ReqWidth(control);
            int winheight = Tk_ReqHeight(control);
    
            /* See if this window can be skipped because it is not visible */
            if ((pWin->absx + winwidth) < x ||
                 pWin->absx > (x + w) ||
                (pWin->absy + winheight) < y ||
                 pWin->absy > (y + h) 
            ) {
                if (Tk_IsMapped(control)) {
                    Tk_UnmapWindow(control);
                }
            } else {
                Tk_MoveResizeWindow(control, pWin->absx - x, pWin->absy - y, 
                        winwidth, winheight);
                if (!Tk_IsMapped(control)) {
                    Tk_MapWindow(control);
                }
            }
        }
    }

    return TCL_OK;
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
layoutBboxCb(pItem, x, y, clientData)
    HtmlCanvasItem *pItem;
    int x;
    int y;
    ClientData clientData;
{
    if (pItem->type == CANVAS_ORIGIN) {
        CanvasOrigin *pOrigin = &pItem->x.o;
        LayoutBboxQuery *pQuery = (LayoutBboxQuery *)clientData;
        if (pOrigin->pSkip) {
            HtmlNode *pN;
            for (pN = pOrigin->pNode; pN; pN = HtmlNodeParent(pN)) { 
                if (pN == pQuery->pNode) {
                    pQuery->left = MIN(pQuery->left, x + pOrigin->left);
                    pQuery->top = MIN(pQuery->top, y + pOrigin->top);
                    pQuery->right = MAX(pQuery->right,x + pOrigin->right);
                    pQuery->bottom = MAX(pQuery->bottom, y + pOrigin->bottom);
                    break;
                }
            }
        }
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutBbox --
 *
 *     <widget> bbox NODE
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
HtmlLayoutBbox(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    Tcl_CmdInfo info;
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlCanvas *pCanvas = &pTree->canvas;
    LayoutBboxQuery sQuery;

    HtmlCallbackForce(pTree);

    sQuery.left = pCanvas->right;
    sQuery.right = pCanvas->left;
    sQuery.top = pCanvas->bottom;
    sQuery.bottom = pCanvas->top;

    if (objc != 3 && objc != 2) {
        Tcl_WrongNumArgs(interp, 3, objv, "?NODE?");
        return TCL_ERROR;
    }

    if (objc == 3) {
        const char *zNode = Tcl_GetString(objv[2]);
        if (0 == Tcl_GetCommandInfo(interp, zNode, &info)) {
            Tcl_AppendResult(interp, "no such node: ", zNode, 0);
            return TCL_ERROR;
        }
    
        sQuery.pNode = (HtmlNode *)info.objClientData;
        assert(sQuery.pNode);
        searchCanvas(pTree, -1, -1, layoutBboxCb, (ClientData)&sQuery);
    } else {
        sQuery.left   = pTree->canvas.left;
        sQuery.right  = pTree->canvas.right;
        sQuery.top    = pTree->canvas.top;
        sQuery.bottom = pTree->canvas.bottom;
    }

    if (sQuery.left < sQuery.right && sQuery.top < sQuery.bottom) {
        char zBuf[128];
        sprintf(zBuf, "%d %d %d %d", sQuery.left, sQuery.top, 
            sQuery.right, sQuery.bottom);
        Tcl_SetResult(interp, zBuf, TCL_VOLATILE);
    }

    return TCL_OK;
}

typedef struct ScrollToQuery ScrollToQuery;
struct ScrollToQuery {
    HtmlTree *pTree;
    int iNode;
    int iReturn;
};

static int
scrollToNodeCb(pItem, x, y, clientData)
    HtmlCanvasItem *pItem;
    int x;
    int y;
    ClientData clientData;
{
    CanvasOrigin *p = &pItem->x.o;
    ScrollToQuery *pQuery = (ScrollToQuery *)clientData;
    int iNode = pQuery->iNode;
    if (pItem->type == CANVAS_ORIGIN && p->pNode && p->pNode->iNode <= iNode) {
        pQuery->iReturn = y + p->top;
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutScrollToNode --
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
HtmlLayoutScrollToNode(pTree, iNode)
    HtmlTree *pTree;
    int iNode;
{
    ScrollToQuery sQuery;
    HtmlCallbackForce(pTree);
    sQuery.iNode = iNode;
    sQuery.iReturn = 0;
    sQuery.pTree = pTree;
    searchCanvas(pTree, -1, -1, scrollToNodeCb, (ClientData)&sQuery);
    return sQuery.iReturn;
}

