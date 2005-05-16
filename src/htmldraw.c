
#include "html.h"
#include <assert.h>

#define CANVAS_QUAD    1
#define CANVAS_TEXT    2
#define CANVAS_IMAGE   3
#define CANVAS_WINDOW  4
#define CANVAS_ORIGIN  5
#define CANVAS_BACKGROUND  6

typedef struct CanvasText CanvasText;
typedef struct CanvasImage CanvasImage;
typedef struct CanvasWindow CanvasWindow;
typedef struct CanvasOrigin CanvasOrigin;
typedef struct CanvasQuad CanvasQuad;
typedef struct CanvasBackground CanvasBackground;

struct CanvasText {
    Tcl_Obj *pText;
    int x;
    int y;
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
};

struct CanvasBackground {
    XColor *color;
};

struct HtmlCanvasItem {
    int type;
#ifdef HTML_DEBUG
    Tcl_Obj *pComment;
#endif
    union {
        CanvasText t;
        CanvasImage i;
        CanvasWindow w;
        CanvasQuad q;
        CanvasOrigin o;
        CanvasBackground b;
    } x;
    HtmlCanvasItem *pNext;
};

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
                Tcl_DecrRefCount(pItem->x.i.pImage);
                break;
            case CANVAS_WINDOW:
                Tcl_DecrRefCount(pItem->x.w.pWindow);
                break;
        }
#ifdef HTML_DEBUG
        if (pItem->pComment) {
            Tcl_DecrRefCount(pItem->pComment);
        }
#endif
        ckfree((char *)pPrev);
        pPrev = pItem;
    }
    ckfree((char *)pPrev);
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
void HtmlDrawComment(pCanvas, zComment)
    HtmlCanvas *pCanvas; 
    CONST char *zComment;
{
    assert(pCanvas->pLast);
    pCanvas->pLast->pComment = Tcl_NewStringObj(zComment, -1);
    Tcl_IncrRefCount(pCanvas->pLast->pComment);
}
#endif

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
#ifdef HTML_DEBUG
    pItem->pComment = 0;
#endif
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
void HtmlDrawCanvas(pCanvas, pCanvas2, x, y)
    HtmlCanvas *pCanvas;
    HtmlCanvas *pCanvas2;
    int x;
    int y;
{
    if (pCanvas2->pFirst) {
        HtmlCanvasItem *pItem;
 
        pItem = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));
        pItem->type = CANVAS_ORIGIN;
        pItem->x.o.x = x;
        pItem->x.o.y = y;
        linkItem(pCanvas, pItem);

        pCanvas->pLast->pNext = pCanvas2->pFirst;
        pCanvas->pLast = pCanvas2->pLast;
        pCanvas2->pFirst = 0;
        pCanvas2->pLast = 0;

        pItem = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));
        pItem->type = CANVAS_ORIGIN;
        pItem->x.o.x = x*-1;
        pItem->x.o.y = y*-1;
        linkItem(pCanvas, pItem);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawText --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlDrawText(pCanvas, pText, x, y, font, color)
    HtmlCanvas *pCanvas; 
    Tcl_Obj *pText; 
    int x;
    int y;
    Tk_Font font;
    XColor *color;
{
    HtmlCanvasItem *pItem = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));
    pItem->type = CANVAS_TEXT;
    pItem->x.t.pText = pText;
    pItem->x.t.x = x;
    pItem->x.t.y = y;
    pItem->x.t.font = font;
    pItem->x.t.color = color;
    Tcl_IncrRefCount(pText);
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
HtmlDrawImage(pCanvas, pImage, x, y)
    HtmlCanvas *pCanvas;
    Tcl_Obj *pImage;
    int x; 
    int y;
{
    HtmlCanvasItem *pItem = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));
    pItem->type = CANVAS_IMAGE;
    pItem->x.i.pImage = pImage;
    pItem->x.i.x = x;
    pItem->x.i.y = y;
    Tcl_IncrRefCount(pImage);
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
HtmlDrawWindow(pCanvas, pWindow, x, y)
    HtmlCanvas *pCanvas;
    Tcl_Obj *pWindow;
    int x; 
    int y;
{
    HtmlCanvasItem *pItem = (HtmlCanvasItem *)ckalloc(sizeof(HtmlCanvasItem));
    pItem->type = CANVAS_WINDOW;
    pItem->x.w.pWindow = pWindow;
    pItem->x.w.x = x;
    pItem->x.w.y = y;
    Tcl_IncrRefCount(pWindow);
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
        Tcl_Obj *pList;
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
        }
        pList = Tcl_NewObj();
        Tcl_SetListObj(pList, nObj, aObj);
#ifdef HTML_DEBUG
        if (pItem->pComment) {
            Tcl_AppendToObj(pList, " ;# ", 4);
            Tcl_AppendObjToObj(pList, pItem->pComment);
        }
#endif
        Tcl_ListObjAppendElement(interp, pPrimitives, pList);
    }
    pCanvas->pPrimitives = pPrimitives;
    Tcl_SetObjResult(interp, pPrimitives);
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

