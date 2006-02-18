
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
static const char rcsid[] = "$Id: htmldraw.c,v 1.80 2006/02/18 14:43:55 danielk1977 Exp $";

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

#define CANVAS_IMAGE2  8

typedef struct CanvasText CanvasText;
typedef struct CanvasWindow CanvasWindow;
typedef struct CanvasOrigin CanvasOrigin;
typedef struct CanvasQuad CanvasQuad;
typedef struct CanvasBackground CanvasBackground;
typedef struct CanvasComment CanvasComment;

typedef struct CanvasImage2 CanvasImage2;

struct CanvasText {
    int x;                   /* Relative x coordinate to render at */
    int y;                   /* Relative y coordinate to render at */

    Tcl_Obj *pText;          /* Text to render */
    HtmlFont *pFont;         /* Font to render text with */
    XColor *color;           /* Color to draw in */
    HtmlNode *pNode;         /* Color to draw in */
    int iIndex;              /* Index in pNode text of this item */
};

struct CanvasImage2 {
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
        CanvasImage2 i2;
        CanvasWindow w;
        CanvasQuad q;
        CanvasOrigin o;
        CanvasBackground b;
        CanvasComment c;
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
            case CANVAS_IMAGE2:
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
            assert(pLast2->x.o.pNode == 0);
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
            pItem2 = (HtmlCanvasItem *)HtmlAlloc(sizeof(HtmlCanvasItem));
            pItem2->type = CANVAS_ORIGIN;
            pItem2->x.o.x = x*-1;
            pItem2->x.o.y = y*-1;
            pItem2->x.o.pSkip = 0;
            pItem2->x.o.pNode = 0;
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
        pItem->type = CANVAS_IMAGE2;
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
            case CANVAS_IMAGE2:
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

/*
 *---------------------------------------------------------------------------
 *
 * drawImage2 --
 *
 *     This function is used to draw a CANVAS_IMAGE2 primitive to the drawable
 *     *pDrawable.
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
drawImage2(pTree, pI2, pDrawable, x, y, w, h)
    HtmlTree *pTree;
    CanvasImage2 *pI2;
    Drawable *pDrawable;
    int x;                 /* X-coord in *pDrawable */
    int y;                 /* Y-coord in *pDrawable */
    int w;                 /* Total width of *pDrawable */
    int h;                 /* Total height of *pDrawable */
{
    if (pI2->pImage) {
        int iw;                /* Intrinsic width of image */
        int ih;                /* Intrinsic height of image */

        Tk_Image img = HtmlImageImage(pI2->pImage);
        Tk_SizeOfImage(img, &iw, &ih);
        if (
            (pI2->eRepeat != CSS_CONST_NO_REPEAT ||
             pI2->iPositionX != 0 || 
             pI2->iPositionY != 0) &&
            iw > 0 && 
            ih > 0
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
                bw = bw + x1;
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
                Pixmap ipix;             /* Pixmap of image */
                ipix = Tk_GetPixmap(pDisplay, Tk_WindowId(win),iw,ih,depth);
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
        } else if (ih > 0 && iw > 0) {
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
            Tk_RedrawImage(img, ix, iy, iw, ih, *pDrawable, dx, dy);
        }
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

            case CANVAS_IMAGE2: {
                drawImage2(pTree, &pItem->x.i2, &pmap, x, y, w, h);
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
    HtmlCanvas *pCanvas = &pTree->canvas;
    int origin_x = 0;
    int origin_y = 0;
    int rc = 0;

    for (pItem = pCanvas->pFirst; pItem; pItem = pItem->pNext) {
        if (pItem->type == CANVAS_ORIGIN) {
            CanvasOrigin *pOrigin = &pItem->x.o;
            origin_x += pOrigin->x;
            origin_y += pOrigin->y;
            if (pOrigin->pSkip && (
                (ymax >= 0 && (origin_y + pOrigin->top) > ymax) ||
                (ymin >= 0 && (origin_y + pOrigin->bottom) < ymin))
            ) {
               pItem = pOrigin->pSkip;
               assert(pItem->type == CANVAS_ORIGIN);
               assert(!pItem->x.o.pSkip);
               origin_x -= pOrigin->x;
               origin_y -= pOrigin->y;
            }
        } else if (0 != (rc = xFunc(pItem, origin_x, origin_y, clientData))) {
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
 *     Pointer to HtmlNode, or NULL.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static HtmlNode *
layoutNodeIndexCmd(pTree, x, y, piIndex)
    HtmlTree *pTree;        /* Widget tree */
    int x;                  /* Document (not viewport) X coordinate */
    int y;                  /* Document (not viewport) Y coordinate */
    int *piIndex;           /* OUT: Write the index integer here */
{
    int iIndex = 0;
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
        int dummy;
        int n;
        const char *z;
        Tk_Font font = sQuery.pClosest->pFont->tkfont;
        z = Tcl_GetStringFromObj(sQuery.pClosest->pText, &n);
        iIndex = Tk_MeasureChars(font, z, n, x - sQuery.closest_x, 0, &dummy);
        iIndex += sQuery.pClosest->iIndex;
    }

    *piIndex = iIndex;
    return (sQuery.pClosest ? sQuery.pClosest->pNode : 0);
}

static HtmlNode *
layoutNodeCmd(pTree, x, y)
    HtmlTree *pTree;
    int x;
    int y;
{
    int origin_x = 0;
    int origin_y = 0;
    HtmlCanvas *pCanvas = &pTree->canvas;
    HtmlNode *pNode = 0;
    HtmlCanvasItem *pItem;

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
                     pNode = pOrigin->pNode;
                 }
            }
        }
    }

    return pNode;
}
  

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutNode --
 *
 *     <widget> node -index? X Y?
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
    int isIndex = 0;
    int iIndex = 0;

    HtmlNode *pNode = 0;
    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc == 2){
        pNode = pTree->pRoot;
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
            pNode = layoutNodeCmd(pTree, x, y);
        } else {
            isIndex = 1;
            pNode = layoutNodeIndexCmd(pTree, x, y, &iIndex);
        }
    } else {
        Tcl_WrongNumArgs(interp, 2, objv, "?-index ?X Y??");
        return TCL_ERROR;
    }

    if (pNode) {
        Tcl_Obj *pCmd = Tcl_DuplicateObj(HtmlNodeCommand(pTree, pNode));
        if (isIndex) {
            Tcl_ListObjAppendElement(interp, pCmd, Tcl_NewIntObj(iIndex));
        }
        Tcl_SetObjResult(interp, pCmd);
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
    HtmlNode *pNode = 0;
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlCanvas *pCanvas = &pTree->canvas;
    HtmlCanvasItem *pItem;
    int bbox[4];
    int x = 0;
    int y = 0;

    HtmlCallbackForce(pTree);

    bbox[0] = pCanvas->right;  /* x1 */
    bbox[1] = pCanvas->bottom; /* y1 */
    bbox[2] = pCanvas->left;   /* x2 */
    bbox[3] = pCanvas->top;    /* y2 */

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 3, objv, "NODE");
        return TCL_ERROR;
    }

    if (0 == Tcl_GetCommandInfo(interp, Tcl_GetString(objv[2]), &info)) {
        Tcl_AppendResult(interp, "no such node: ", Tcl_GetString(objv[2]), 0);
        return TCL_ERROR;
    }

    pNode = (HtmlNode *)info.objClientData;
    assert(pNode);

    for (pItem=pCanvas->pFirst; pItem; pItem=pItem->pNext) {
        if (pItem->type == CANVAS_ORIGIN) {
            CanvasOrigin *pOrigin = &pItem->x.o;
            HtmlNode *pN;
            x += pOrigin->x;
            y += pOrigin->y;
            for (pN = pOrigin->pNode; pN; pN = HtmlNodeParent(pN)) { 
                if (pN == pNode) {
                    bbox[0] = MIN(bbox[0], x + pOrigin->left);
                    bbox[1] = MIN(bbox[1], y + pOrigin->top);
                    bbox[2] = MAX(bbox[2], x + pOrigin->right);
                    bbox[3] = MAX(bbox[3], y + pOrigin->bottom);
                    break;
                }
            }
        }
    }

    if (bbox[0] < bbox[2] && bbox[1] < bbox[3]) {
        char zBuf[128];
        sprintf(zBuf, "%d %d %d %d", bbox[0], bbox[1], bbox[2], bbox[3]);
        Tcl_SetResult(interp, zBuf, TCL_VOLATILE);
    }

    return TCL_OK;
}

