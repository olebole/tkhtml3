
#include "html.h"
#include <assert.h>

#define CANVAS_QUAD    1
#define CANVAS_TEXT    2
#define CANVAS_IMAGE   3
#define CANVAS_WINDOW  4
#define CANVAS_ORIGIN  5
#define CANVAS_BACKGROUND  6
#define CANVAS_COMMENT  7

typedef struct CanvasText CanvasText;
typedef struct CanvasImage CanvasImage;
typedef struct CanvasWindow CanvasWindow;
typedef struct CanvasOrigin CanvasOrigin;
typedef struct CanvasQuad CanvasQuad;
typedef struct CanvasBackground CanvasBackground;
typedef struct CanvasComment CanvasComment;

struct CanvasText {
    Tcl_Obj *pText;
    int x;
    int y;
    int sw;
    Tk_Font font;
    XColor *color;
};

struct CanvasImage {
    Tcl_Obj *pImage;
    int x;
    int y;
};

struct CanvasWindow {
    Tcl_Obj *pWindow;
    int x;
    int y;
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
        CanvasImage i;
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
        switch (pItem->type) {
            case CANVAS_TEXT:
                Tcl_DecrRefCount(pItem->x.t.pText);
                break;
            case CANVAS_IMAGE:
                if (pItem->x.i.pImage) {
                    Tcl_DecrRefCount(pItem->x.i.pImage);
                }
                break;
            case CANVAS_WINDOW:
                Tcl_DecrRefCount(pItem->x.w.pWindow);
                break;
            case CANVAS_COMMENT:
                Tcl_DecrRefCount(pItem->x.c.pComment);
                break;
        }
        if (pPrev) {
            ckfree((char *)pPrev);
        }
        pPrev = pItem;
    }
    if (pPrev) {
        ckfree((char *)pPrev);
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
            Tk_DestroyWindow(control);
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

	/* Special case: If the canvas being draw into the parent consists
         * of a single text item, then see if it can be combined with a
         * text item already in the parent canvas.
         */
#if 1
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
                pI->x.t.font == pNew->x.t.font &&
                pI->x.t.color == pNew->x.t.color && 
                (pI->x.t.y + pO->x.o.y) == (pNew->x.t.y + y)
            ) {
                int xi = pO->x.o.x;
                int xn;

                xi += pO->x.o.right;
                xn = pNew->x.t.x + x;

                if ((xn - xi) == pI->x.t.sw) {
                    Tcl_AppendToObj(pI->x.t.pText, " ", 1);
                    Tcl_AppendObjToObj(pI->x.t.pText, pNew->x.t.pText);
                    pO->x.o.right = (x + pCanvas2->right) - pO->x.o.x;
                    HtmlDrawCleanup(pCanvas2);
                    goto draw_canvas_out;
                }
            } 
        }
#endif
 
        pItem = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));
        pItem->type = CANVAS_ORIGIN;
        pItem->x.o.x = x;
        pItem->x.o.y = y;
        pItem->x.o.left = pCanvas2->left;
        pItem->x.o.right = pCanvas2->right;
        pItem->x.o.bottom = pCanvas2->bottom;
        pItem->x.o.top = pCanvas2->top;
        pItem->x.o.pNode = pNode;
        linkItem(pCanvas, pItem);

        pCanvas->pLast->pNext = pCanvas2->pFirst;
        pCanvas->pLast = pCanvas2->pLast;
        pCanvas2->pFirst = 0;
        pCanvas2->pLast = 0;

        pItem2 = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));
        pItem2->type = CANVAS_ORIGIN;
        pItem2->x.o.x = x*-1;
        pItem2->x.o.y = y*-1;
        pItem2->x.o.pSkip = 0;
        pItem2->x.o.pNode = 0;
        pItem->x.o.pSkip = pItem2;
        linkItem(pCanvas, pItem2);

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
void HtmlDrawComment(pCanvas, zComment)
    HtmlCanvas *pCanvas; 
    CONST char *zComment;
{
    HtmlCanvasItem *pItem = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));
    pItem->type = CANVAS_COMMENT;
    pItem->x.c.pComment = Tcl_NewStringObj(zComment, -1);
    Tcl_IncrRefCount(pItem->x.c.pComment);
    linkItem(pCanvas, pItem);
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
void HtmlDrawText(pCanvas, pText, x, y, w, sw, font, color)
    HtmlCanvas *pCanvas; 
    Tcl_Obj *pText; 
    int x;
    int y;
    int w;
    int sw;
    Tk_Font font;
    XColor *color;
{
    HtmlCanvasItem *pItem = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));
    Tk_FontMetrics fontMetrics;

    pItem->type = CANVAS_TEXT;
    pItem->x.t.pText = pText;
    pItem->x.t.x = x;
    pItem->x.t.y = y;
    pItem->x.t.font = font;
    pItem->x.t.color = color;
    pItem->x.t.sw = sw;
    Tcl_IncrRefCount(pText);

    Tk_GetFontMetrics(font, &fontMetrics);
    pCanvas->left = MIN(pCanvas->left, x);
    pCanvas->right = MAX(pCanvas->right, x + w);
    pCanvas->bottom = MAX(pCanvas->bottom, y + fontMetrics.descent);
    pCanvas->top = MIN(pCanvas->top, y - fontMetrics.ascent);

    linkItem(pCanvas, pItem);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawImage --
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
HtmlDrawImage(pCanvas, pImage, x, y, w, h)
    HtmlCanvas *pCanvas;
    Tcl_Obj *pImage;          /* Image name or NULL */
    int x; 
    int y;
    int w;       /* Width of image */
    int h;       /* Height of image */
{
    HtmlCanvasItem *pItem = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));

    pItem->type = CANVAS_IMAGE;
    pItem->x.i.pImage = pImage;
    pItem->x.i.x = x;
    pItem->x.i.y = y;
    if (pImage) {
        Tcl_IncrRefCount(pImage);
    }

    pCanvas->left = MIN(pCanvas->left, x);
    pCanvas->right = MAX(pCanvas->right, x+w);
    pCanvas->bottom = MAX(pCanvas->bottom, y+h);
    pCanvas->top = MIN(pCanvas->top, y);

    linkItem(pCanvas, pItem);
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
HtmlDrawWindow(pCanvas, pWindow, x, y, w, h)
    HtmlCanvas *pCanvas;
    Tcl_Obj *pWindow;
    int x; 
    int y;
    int w;       /* Width of window */
    int h;       /* Height of window */
{
    HtmlCanvasItem *pItem = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));
    pItem->type = CANVAS_WINDOW;
    pItem->x.w.pWindow = pWindow;
    pItem->x.w.x = x;
    pItem->x.w.y = y;
    pItem->x.w.absx = x;
    pItem->x.w.absy = y;
    Tcl_IncrRefCount(pWindow);

    pCanvas->left = MIN(pCanvas->left, x);
    pCanvas->right = MAX(pCanvas->right, x+w);
    pCanvas->bottom = MAX(pCanvas->bottom, y+h);
    pCanvas->top = MIN(pCanvas->top, y);

    pItem->x.w.pNext = pCanvas->pWindow;
    pCanvas->pWindow = pItem;

    linkItem(pCanvas, pItem);
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
void HtmlDrawQuad(pCanvas, x1, y1, x2, y2, x3, y3, x4, y4, color)
    HtmlCanvas *pCanvas; 
    int x1, y1; 
    int x2, y2; 
    int x3, y3; 
    int x4, y4; 
    XColor *color;
{
    HtmlCanvasItem *pItem = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));

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

    pCanvas->left = MIN5(pCanvas->left, x1, x2, x3, x4);
    pCanvas->top = MIN5(pCanvas->top, y1, y2, y3, y4);
    pCanvas->bottom = MAX5(pCanvas->bottom, y1, y2, y3, y4);
    pCanvas->right = MAX5(pCanvas->right, x1, x2, x3, x4);

    linkItem(pCanvas, pItem);
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
void HtmlDrawBackground(pCanvas, color)
    HtmlCanvas *pCanvas;
    XColor *color;
{
    HtmlCanvasItem *pItem = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));
    pItem->type = CANVAS_BACKGROUND;
    pItem->x.b.color = color;
    linkItem(pCanvas, pItem);
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
                aObj[3] = Tcl_NewStringObj(Tk_NameOfFont(pItem->x.t.font), -1);
                aObj[4] = Tcl_NewStringObj(Tk_NameOfColor(pItem->x.t.color),-1);
                aObj[5] = pItem->x.t.pText;
                break;
            case CANVAS_IMAGE:
                nObj = 4;
                aObj[0] = Tcl_NewStringObj("draw_image", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.i.x);
                aObj[2] = Tcl_NewIntObj(pItem->x.i.y);
                aObj[3] = pItem->x.i.pImage;
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
static Pixmap getPixmap(pTree, xcanvas, ycanvas, w, h)
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

    GC gc;
    XGCValues gc_values;
    int mask;

    HtmlCanvas *pCanvas = &pTree->canvas;
    int x = xcanvas * -1;
    int y = ycanvas * -1;

    Tk_MakeWindowExist(win);

    pDisplay = Tk_Display(win);
    pmap = Tk_GetPixmap(pDisplay, Tk_WindowId(win), w, h, Tk_Depth(win));

    gc_values.foreground = Tk_GetColor(pTree->interp, win, "white")->pixel;
    mask = GCForeground;
    gc = Tk_GetGC(pTree->win, mask, &gc_values);
    XFillRectangle(pDisplay, pmap, gc, 0, 0, w, h);

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
                CanvasText *pT = &pItem->x.t;
                CONST char *z;

                int n;
                z = Tcl_GetStringFromObj(pT->pText, &n);

                gc_values.foreground = pT->color->pixel;
                gc_values.font = Tk_FontId(pT->font);
                mask = GCForeground | GCFont;
                gc = Tk_GetGC(pTree->win, mask, &gc_values);

                Tk_DrawChars(
                    pDisplay, pmap, gc, pT->font, z, n, pT->x + x, pT->y + y);
                break;
            }

            case CANVAS_IMAGE: {
                CanvasImage *pI = &pItem->x.i;
                CONST char *zImage = Tcl_GetString(pI->pImage);
                Tk_Image tkimg;
                int iw;
                int ih;

                gc = Tk_GetGC(pTree->win, 0, &gc_values);
                tkimg = Tk_GetImage(pTree->interp, pTree->win, zImage, 0, 0);
                Tk_SizeOfImage(tkimg, &iw, &ih);
                Tk_RedrawImage(tkimg, 0, 0, iw, ih, pmap, pI->x+x, pI->y+y);
                Tk_FreeImage(tkimg);
                
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
    }

    return pmap;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutImage --
 *
 *     <widget> layout image
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
    HtmlCanvas *pCanvas = &pTree->canvas;
    Display *pDisplay = Tk_Display(pTree->win);

    int x = 0;
    int y = 0;
    int w = pCanvas->right;
    int h = pCanvas->bottom;

    if (w>0 && h>0) {
        Pixmap pixmap;
        Tcl_Obj *pImage;
        XImage *pXImage;
        pixmap = getPixmap(pTree, 0, 0, w, h);
        pXImage = XGetImage(pDisplay, pixmap, x, y, w, h, XAllPlanes(),ZPixmap);
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
    return (pCanvas->pFirst==0);
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

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutNode --
 *
 *     <widget> layout node X Y
 *
 *     Return the Tcl handle for the document node that lies at coordinates
 *     (X, Y), relative to the layout. Or, if no node populates the given
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
    int origin_x = 0;
    int origin_y = 0;
    HtmlNode *pNode = 0;
    HtmlCanvasItem *pItem;
    HtmlTree *pTree = (HtmlTree *)clientData;

    HtmlCanvas *pCanvas = &pTree->canvas;

    if (objc != 5) {
        Tcl_WrongNumArgs(interp, 3, objv, "X Y");
        return TCL_ERROR;
    }
    if (TCL_OK != Tcl_GetIntFromObj(interp, objv[3], &x) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[4], &y) 
    ) {
        return TCL_ERROR;
    }

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

    if (pNode) {
        Tcl_Obj *pCmd = HtmlNodeCommand(interp, pNode);
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
HtmlWidgetPaint(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int x;
    int y;
    int canvas_x;
    int canvas_y;
    int width;
    int height;
    Pixmap pixmap;
    GC gc;
    XGCValues gc_values;
    HtmlTree *pTree = (HtmlTree *)clientData;
    Display *display; 
    Tk_Window win;                      /* Window to draw to */
 
    win = pTree->tkwin;
    Tk_MakeWindowExist(win);
    display = Tk_Display(win);
 
    if (objc != 9) {
        Tcl_WrongNumArgs(interp, 3, objv, 
                "CANVAS-X CANVAS-Y X Y WIDTH HEIGHT"); 
        return TCL_ERROR;
    }
    if (TCL_OK != Tcl_GetIntFromObj(interp, objv[3], &canvas_x) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[4], &canvas_y) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[5], &x) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[6], &y) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[7], &width) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[8], &height)
    ) {
        return TCL_ERROR;
    }

    pixmap = getPixmap(pTree, canvas_x, canvas_y, width, height);

    memset(&gc_values, 0, sizeof(XGCValues));
    gc = Tk_GetGC(pTree->win, 0, &gc_values);

    XCopyArea(display, pixmap, Tk_WindowId(win), gc, 0, 0, width, height, x, y);
    Tk_FreePixmap(display, pixmap);
    Tk_FreeGC(display, gc);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetScroll --
 *
 *     <widget> widget scroll X Y
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
HtmlWidgetScroll(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int x;
    int y;
    Tk_Window win;
    Display *display;
    GC gc;
    XGCValues gc_values;

    int source_x, source_y;
    int dest_x, dest_y;
    int width, height;

    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc != 5) {
        Tcl_WrongNumArgs(interp, 3, objv, "X Y");
        return TCL_ERROR;
    }

    if (TCL_OK != Tcl_GetIntFromObj(interp, objv[3], &x) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[4], &y) 
    ) {
        return TCL_ERROR;
    }

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
HtmlWidgetMapControls(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int x;
    int y;
    int w;
    int h;
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlCanvas *pCanvas = &pTree->canvas;
    HtmlCanvasItem *pItem;

    Tk_Window win = pTree->tkwin;

    /* Check that the arguments are correct and copy the X and Y parameters
     * into native C variables x and y. Then set w and h to the width and
     * height of the viewport respectively.
     */
    if (objc != 5) {
        Tcl_WrongNumArgs(interp, 3, objv, "X Y");
        return TCL_ERROR;
    }
    if (TCL_OK != Tcl_GetIntFromObj(interp, objv[3], &x) ||
        TCL_OK != Tcl_GetIntFromObj(interp, objv[4], &y) 
    ) {
        return TCL_ERROR;
    }
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
 *     <widget> internal bbox NODE
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
    int rc;
    Tcl_CmdInfo info;
    HtmlNode *pNode = 0;
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlCanvas *pCanvas = &pTree->canvas;
    HtmlCanvasItem *pItem;
    int bbox[4];
    int x = 0;
    int y = 0;

    bbox[0] = pCanvas->right;  /* x1 */
    bbox[1] = pCanvas->bottom; /* y1 */
    bbox[2] = pCanvas->left;   /* x2 */
    bbox[3] = pCanvas->top;    /* y2 */

    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 3, objv, "NODE");
        return TCL_ERROR;
    }

    if (0 == Tcl_GetCommandInfo(interp, Tcl_GetString(objv[3]), &info)) {
        Tcl_AppendResult(interp, "no such node: ", Tcl_GetString(objv[3]), 0);
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

