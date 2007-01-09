/*
 *--------------------------------------------------------------------------
 * Copyright (c) 2005 Dan Kennedy.
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
 *     * Neither the name of Eolas Technologies Inc. nor the names of its
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
static char const rcsid[] = "@(#) $Id: htmltcl.c,v 1.150 2007/01/09 10:05:37 danielk1977 Exp $";

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "html.h"
#include "swproc.h"

#include <time.h>

#include "htmldefaultstyle.c"

#define LOG if (pTree->options.logcmd)

#define SafeCheck(interp,str) if (Tcl_IsSafe(interp)) { \
    Tcl_AppendResult(interp, str, " invalid in safe interp", 0); \
    return TCL_ERROR; \
}

#ifndef NDEBUG
static int 
allocCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return Rt_AllocCommand(0, interp, objc, objv);
}
static int 
heapdebugCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlHeapDebug(0, interp, objc, objv);
}
static int 
hashstatsCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    Tcl_HashEntry *p;
    Tcl_HashSearch search;
    int nObj = 0;
    int nRef = 0;
    char zRes[128];

    for (
        p = Tcl_FirstHashEntry(&pTree->aValues, &search); 
        p; 
        p = Tcl_NextHashEntry(&search)
    ) {
        HtmlComputedValues *pV = 
            (HtmlComputedValues *)Tcl_GetHashKey(&pTree->aValues, p);
        nObj++;
        nRef += pV->nRef;
    }

    sprintf(zRes, "%d %d", nObj, nRef);
    Tcl_SetResult(interp, zRes, TCL_VOLATILE);
    return TCL_OK;
}
#endif


/*
 *---------------------------------------------------------------------------
 *
 * HtmlLog --
 * HtmlTimer --
 *
 *     This function is used by various parts of the widget to output internal
 *     information that may be useful in debugging. 
 *
 *     The first argument is the HtmlTree structure. The second is a string
 *     identifying the sub-system logging the message. The third argument is a
 *     printf() style format string and subsequent arguments are substituted
 *     into it to form the logged message.
 *
 *     If -logcmd is set to an empty string, this function is a no-op.
 *     Otherwise, the name of the subsystem and the formatted error message are
 *     appended to the value of the -logcmd option and the result executed as a
 *     Tcl script.
 *
 *     This function is replaced with an empty macro if NDEBUG is defined at
 *     compilation time. If the "-logcmd" option is set to an empty string it
 *     returns very quickly.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Invokes the -logcmd script, if it is not "".
 *
 *---------------------------------------------------------------------------
 */
void 
logCommon(
    HtmlTree *pTree, 
    Tcl_Obj *pLogCmd, 
    CONST char *zSubject, 
    CONST char *zFormat, 
    va_list ap
)
{
    if (pLogCmd) {
        char *zDyn = 0;
        char zStack[200];
        char *zBuf = zStack;
        int nBuf;
        Tcl_Obj *pCmd;

        nBuf = vsnprintf(zBuf, 200, zFormat, ap);
        if (nBuf >= 200) {
            zDyn = HtmlAlloc(0, nBuf + 10);
            zBuf = zDyn;
            nBuf = vsnprintf(zBuf, nBuf + 1, zFormat, ap);
        }

        pCmd = Tcl_DuplicateObj(pLogCmd);
        Tcl_IncrRefCount(pCmd);
        Tcl_ListObjAppendElement(0, pCmd, Tcl_NewStringObj(zSubject, -1));
        Tcl_ListObjAppendElement(0, pCmd, Tcl_NewStringObj(zBuf, nBuf));

        if (Tcl_EvalObjEx(pTree->interp, pCmd, TCL_GLOBAL_ONLY)) {
            Tcl_BackgroundError(pTree->interp);
        }

        Tcl_DecrRefCount(pCmd);
        HtmlFree(zDyn);
    }
}

void 
HtmlTimer(HtmlTree *pTree, CONST char *zSubject, CONST char *zFormat, ...) {
    va_list ap;
    va_start(ap, zFormat);
    logCommon(pTree, pTree->options.timercmd, zSubject, zFormat, ap);
}
void 
HtmlLog(HtmlTree *pTree, CONST char *zSubject, CONST char *zFormat, ...) {
    va_list ap;
    va_start(ap, zFormat);
    logCommon(pTree, pTree->options.logcmd, zSubject, zFormat, ap);
}

/*
 *---------------------------------------------------------------------------
 *
 * doLoadDefaultStyle --
 *
 *     Load the default-style sheet into the current stylesheet configuration.
 *     The text of the default stylesheet is stored in the -defaultstyle
 *     option.
 *
 *     This function is called once when the widget is created and each time
 *     [.html reset] is called thereafter.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Loads the default style.
 *
 *---------------------------------------------------------------------------
 */
static void
doLoadDefaultStyle(pTree)
    HtmlTree *pTree;
{
    Tcl_Obj *pObj = pTree->options.defaultstyle;
    Tcl_Obj *pId = Tcl_NewStringObj("agent", 5);
    assert(pObj);
    Tcl_IncrRefCount(pId);
    HtmlStyleParse(pTree, pTree->interp, pObj, pId, 0, 0);
    Tcl_DecrRefCount(pId);
}

/*
 *---------------------------------------------------------------------------
 *
 * doSingleScrollCallback --
 *
 *     Helper function for doScrollCallback().
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May invoke the script in *pScript.
 *
 *---------------------------------------------------------------------------
 */
static void
doSingleScrollCallback(interp, pScript, iOffScreen, iTotal, iPage)
    Tcl_Interp *interp;
    Tcl_Obj *pScript;
    int iOffScreen;
    int iTotal;
    int iPage;
{
    if (pScript) {
        double fArg1;
        double fArg2;
        int rc;
        Tcl_Obj *pEval; 

        if (iTotal == 0){ 
            fArg1 = 0.0;
            fArg2 = 1.0;
        } else {
            fArg1 = (double)iOffScreen / (double)iTotal;
            fArg2 = (double)(iOffScreen + iPage) / (double)iTotal;
            fArg2 = MIN(1.0, fArg2);
        }

        pEval = Tcl_DuplicateObj(pScript);
        Tcl_IncrRefCount(pEval);
        Tcl_ListObjAppendElement(interp, pEval, Tcl_NewDoubleObj(fArg1));
        Tcl_ListObjAppendElement(interp, pEval, Tcl_NewDoubleObj(fArg2));
        rc = Tcl_EvalObjEx(interp, pEval, TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL);
        if (TCL_OK != rc) {
            Tcl_BackgroundError(interp);
        }
        Tcl_DecrRefCount(pEval);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * doScrollCallback --
 *
 *     Invoke both the -xscrollcommand and -yscrollcommand scripts (unless they
 *     are set to empty strings). The arguments passed to the two scripts are
 *     calculated based on the current values of HtmlTree.iScrollY,
 *     HtmlTree.iScrollX and HtmlTree.canvas.
 *
 * Results:
 *     None.
 *
 * Side effects: 
 *     May invoke either or both of the -xscrollcommand and -yscrollcommand
 *     scripts.
 *
 *---------------------------------------------------------------------------
 */
static void 
doScrollCallback(pTree)
    HtmlTree *pTree;
{
    Tcl_Interp *interp = pTree->interp;
    Tk_Window win = pTree->tkwin;

    Tcl_Obj *pScrollCommand;
    int iOffScreen;
    int iTotal;
    int iPage;

    pScrollCommand = pTree->options.yscrollcommand;
    iOffScreen = pTree->iScrollY;
    iTotal = pTree->canvas.bottom;
    iPage = Tk_Height(win);
    doSingleScrollCallback(interp, pScrollCommand, iOffScreen, iTotal, iPage);

    pScrollCommand = pTree->options.xscrollcommand;
    iOffScreen = pTree->iScrollX;
    iTotal = pTree->canvas.right;
    iPage = Tk_Width(win);
    doSingleScrollCallback(interp, pScrollCommand, iOffScreen, iTotal, iPage);
}


/*
 *---------------------------------------------------------------------------
 *
 * callbackHandler --
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
callbackHandler(clientData)
    ClientData clientData;
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlCallback *p = &pTree->cb;

    clock_t styleClock = 0;              
    clock_t layoutClock = 0;

    int offscreen;

    assert(
        !pTree->pRoot ||
        HtmlNodeComputedValues(pTree->pRoot) ||
        pTree->cb.pRestyle
    );

    HtmlLog(pTree, "CALLBACK", 
        "flags=( %s%s%s%s%s) pDynamic=%s pRestyle=%s scroll=(+%d+%d) ",
        (p->flags & HTML_DYNAMIC ? "Dynamic " : ""),
        (p->flags & HTML_RESTYLE ? "Style " : ""),
        (p->flags & HTML_LAYOUT ? "Layout " : ""),
        (p->flags & HTML_DAMAGE ? "Damage " : ""),
        (p->flags & HTML_SCROLL ? "Scroll " : ""),
        (p->pDynamic?Tcl_GetString(HtmlNodeCommand(pTree,p->pDynamic)):"N/A"),
        (p->pRestyle?Tcl_GetString(HtmlNodeCommand(pTree,p->pRestyle)):"N/A"),
         p->iScrollX, p->iScrollY
    );

    assert(!pTree->cb.inProgress);
    pTree->cb.inProgress = 1;

    /* If the HTML_DYNAMIC flag is set, then call HtmlCssCheckDynamic()
     * to recalculate all the dynamic CSS rules that may apply to 
     * the sub-tree rooted at HtmlCallback.pDynamic. CssCheckDynamic() may
     * call either HtmlCallbackDamage() or HtmlCallbackRestyle() if any
     * computed style values are modified.
     */
    if (!pTree->delayToken && (pTree->cb.flags & HTML_DYNAMIC)) {
        assert(pTree->cb.pDynamic);
        HtmlCssCheckDynamic(pTree);
    }

    /* If the HtmlCallback.pRestyle variable is set, then recalculate 
     * style information for the sub-tree rooted at HtmlCallback.pRestyle. 
     * Note that restyling a node may invoke the -imagecmd callback.
     *
     * Todo: This seems dangerous.  What happens if the -imagecmd calls
     * [.html parse] or something?
     */
    if (pTree->cb.flags & HTML_RESTYLE) {
        HtmlNode *pParent = HtmlNodeParent(pTree->cb.pRestyle);
        HtmlNode *pRestyle = pTree->cb.pRestyle;

        pTree->cb.pRestyle = 0;
        assert(pRestyle);
        styleClock = clock();

        if (pParent) {
            int i;
            int nChild = HtmlNodeNumChildren(pParent);
            for (i = 0; HtmlNodeChild(pParent, i) != pRestyle; i++);
            for ( ; i < nChild; i++) {
                 HtmlStyleApply(pTree, HtmlNodeChild(pParent, i));
            }
        } else {
            HtmlStyleApply(pTree, pRestyle);
        }
        HtmlRestackNodes(pTree);

        if (!pTree->options.imagecache) {
            HtmlImageServerDoGC(pTree);
        }

        styleClock = clock() - styleClock;
    }
    pTree->cb.flags &= ~HTML_RESTYLE;

    if (pTree->delayToken) {
        pTree->cb.inProgress = 0;
        return;
    }

    /* If the HTML_LAYOUT flag is set, run the layout engine. If the layout
     * engine is run, then also set the HTML_SCROLL bit in the
     * HtmlCallback.flags bitmask. This ensures that the entire display is
     * redrawn and that the Tk windows for any replaced nodes are correctly
     * mapped, unmapped or moved.
     */
    assert(pTree->cb.pDamage == 0 || pTree->cb.flags & HTML_DAMAGE);
    if (pTree->cb.flags & HTML_LAYOUT) {
        HtmlDamage *pD = pTree->cb.pDamage;

        layoutClock = clock();
        HtmlLayout(pTree);
        layoutClock = clock() - layoutClock;
        pTree->cb.flags |= HTML_SCROLL;

        /* Discard any damage info, as the whole viewport will be redrawn */
        pTree->cb.flags &= ~HTML_DAMAGE;
        while (pD) {
            HtmlDamage *pNext = pD->pNext;
            HtmlFree(pD);
            pD = pNext;
        }
        pTree->cb.pDamage = 0;
    }

    /* If the HTML_DAMAGE flag is set, repaint a window region. */
    assert(pTree->cb.pDamage == 0 || pTree->cb.flags & HTML_DAMAGE);
    if (pTree->cb.flags & HTML_DAMAGE) {
        HtmlDamage *pD = pTree->cb.pDamage;
        pTree->cb.pDamage = 0;
        while (pD) {
            HtmlDamage *pNext = pD->pNext;
            HtmlLog(pTree, 
                "ACTION", "Repair: %dx%d +%d+%d", pD->w, pD->h, pD->x, pD->y
            );
            HtmlWidgetRepair(pTree, pD->x, pD->y, pD->w, pD->h, 
                pD->pixmapok, (pTree->cb.flags & HTML_NODESCROLL)?1:0
            );
            HtmlFree(pD);
            pD = pNext;
        }
    }

    /* If the HTML_SCROLL flag is set, scroll the viewport. */
    if (pTree->cb.flags & HTML_SCROLL) {
        clock_t scrollClock = 0;              
        int force_redraw = (pTree->cb.flags & HTML_LAYOUT);
        HtmlLog(pTree, "ACTION", "SetViewport: x=%d y=%d force=%d nFixed=%d", 
            p->iScrollX, p->iScrollY, force_redraw, pTree->nFixedBackground
        );
        scrollClock = clock();
        HtmlWidgetSetViewport(pTree, p->iScrollX, p->iScrollY, force_redraw);
        scrollClock = clock() - scrollClock;
        HtmlLog(pTree, "TIMING", "SetViewport: clicks=%d", scrollClock);
    }

    if (pTree->cb.flags & (HTML_SCROLL|HTML_LAYOUT)) {
      doScrollCallback(pTree);
    }

    pTree->cb.flags = 0;
    assert(pTree->cb.inProgress);
    pTree->cb.inProgress = 0;

    if (pTree->cb.pDamage) {
        pTree->cb.flags = HTML_DAMAGE;
        Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
    }

    offscreen = MAX(0, 
        MIN(pTree->canvas.bottom - Tk_Height(pTree->tkwin), pTree->iScrollY)
    );
    if (offscreen != pTree->iScrollY) {
        HtmlCallbackScrollY(pTree, offscreen);
    }
    offscreen = MAX(0, 
        MIN(pTree->canvas.right - Tk_Width(pTree->tkwin), pTree->iScrollX)
    );
    if (offscreen != pTree->iScrollX) {
        HtmlCallbackScrollX(pTree, offscreen);
    }
}

static void
delayCallbackHandler(clientData)
    ClientData clientData;
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    pTree->delayToken = 0;
    if (pTree->cb.flags) {
        callbackHandler(clientData);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCallbackForce --
 *
 *     If there is a callback scheduled, execute it now instead of waiting 
 *     for the idle loop.
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
HtmlCallbackForce(pTree)
    HtmlTree *pTree;
{
    if (pTree->cb.flags && !pTree->cb.inProgress) {
        ClientData clientData = (ClientData)pTree;
        Tcl_CancelIdleCall(callbackHandler, clientData);
        callbackHandler(clientData);
    }
}

static void
upgradeRestylePoint(ppRestyle, pNode)
    HtmlNode **ppRestyle;
    HtmlNode *pNode;
{
    HtmlNode *pA;
    HtmlNode *pB;
    assert(pNode && ppRestyle);

    for (pA = pNode; pA; pA = HtmlNodeParent(pA)) {
        if (pA->iNode == HTML_NODE_ORPHAN) return;
    }

    for (pA = *ppRestyle; pA; pA = HtmlNodeParent(pA)) {
        HtmlNode *pParentA = HtmlNodeParent(pA);
        for (pB = pNode; pB; pB = HtmlNodeParent(pB)) {
            if (pB == pA) {
                *ppRestyle = pB;
                return;
            }  
            if (HtmlNodeParent(pB) == pParentA) {
                int i;
                for (i = 0; i < HtmlNodeNumChildren(pParentA); i++) {
                    HtmlNode *pChild = HtmlNodeChild(pParentA, i);
                    if (pChild == pB) {
                        *ppRestyle = pB;
                        return;
                    }
                    if (pChild == pA) {
                        *ppRestyle = pA;
                        return;
                    }
                }
            }
        }
    }

    assert(!*ppRestyle);
    *ppRestyle = pNode;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCallbackRestyle --
 *
 *     Next widget idle-callback, recalculate style information for the
 *     sub-tree rooted at pNode. This function is a no-op if (pNode==0).
 *     If pNode is the root of the document, then the list of dynamic
 *     conditions (HtmlNode.pDynamic) that apply to each node is also
 *     recalculated.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May modify HtmlTree.cb and/or register for an idle callback with
 *     the Tcl event loop.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlCallbackRestyle(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if (pNode) {
        if (!pTree->cb.flags) {
            Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
        }
        pTree->cb.flags |= HTML_RESTYLE;
        upgradeRestylePoint(&pTree->cb.pRestyle, pNode);
    }

    /* This is also where the text-representation of the document is
     * invalidated. If the style of a node is to change, or a new node
     * that has no style is added, then the current text-representation
     * is clearly suspect.
     */
    HtmlTextInvalidate(pTree);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCallbackDynamic --
 *
 *     Next widget idle-callback, check if any dynamic CSS conditions
 *     attached to nodes that are part of the sub-tree rooted at pNode
 *     have changed. If so, restyle the affected nodes.  This function
 *     is a no-op if (pNode==0).
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May modify HtmlTree.cb and/or register for an idle callback with
 *     the Tcl event loop.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlCallbackDynamic(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if (pNode) {
        if (!pTree->cb.flags) {
            Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
        }
        pTree->cb.flags |= HTML_DYNAMIC;
        upgradeRestylePoint(&pTree->cb.pDynamic, pNode);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCallbackLayout --
 *
 *     Ensure the layout of node pNode is recalculated next idle
 *     callback. This is a no-op if (pNode==0).
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May modify HtmlTree.cb and/or register for an idle callback with
 *     the Tcl event loop. May expire layout-caches belonging to pNode
 *     and it's ancestor nodes.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlCallbackLayout(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if (pNode) {
        HtmlNode *p;
        if (!pTree->cb.flags) {
            Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
        }
        pTree->cb.flags |= HTML_LAYOUT;
        for (p = pNode; p; p = HtmlNodeParent(p)) {
            HtmlLayoutInvalidateCache(pTree, p);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCallbackDamage --
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
HtmlCallbackDamage(pTree, x, y, w, h, pixmapok)
    HtmlTree *pTree;
    int x; 
    int y;
    int w; 
    int h;
    int pixmapok;
{
    HtmlDamage *pNew;
    HtmlDamage *p;

    /* Clip the values to the viewport */
    if (x < 0) {w += x; x = 0;}
    if (y < 0) {h += y; y = 0;}
    w = MIN(w, Tk_Width(pTree->tkwin) - x);
    h = MIN(h, Tk_Height(pTree->tkwin) - y);
    
    /* If the damaged region is not currently visible, do nothing */
    if (w <= 0 || h <= 0) {
        return;
    }

    /* Loop through the current list of damaged rectangles. If possible
     * clip the new damaged region so that the same part of the display
     * is not painted more than once.
     */
    for (p = pTree->cb.pDamage; p; p = p->pNext) {
        /* Check if region p completely encapsulates the new region. If so,
         * we need do nothing. 
         */
        assert(pTree->cb.flags & HTML_DAMAGE);
        if (
            p->x <= x && p->y <= y && 
            (p->x + p->w) >= (x + w) && (p->y + p->h) >= (y + h)
        ) {
            return;
        }
    }

#if 0
    if (pTree->cb.flags & HTML_DAMAGE) {
        int x2 = MAX(x + w, pTree->cb.x + pTree->cb.w);
        int y2 = MAX(y + h, pTree->cb.y + pTree->cb.h);
        pTree->cb.x = MIN(pTree->cb.x, x);
        pTree->cb.y = MIN(pTree->cb.y, y);
        pTree->cb.w = x2 - pTree->cb.x;
        pTree->cb.h = y2 - pTree->cb.y;
    } else {
        pTree->cb.x = x;
        pTree->cb.y = y;
        pTree->cb.w = w;
        pTree->cb.h = h;
    }

    assert(pTree->cb.x >= 0);
    assert(pTree->cb.y >= 0);
    assert(pTree->cb.w > 0);
    assert(pTree->cb.h > 0);
#endif
 
    pNew = HtmlNew(HtmlDamage);
    pNew->x = x;
    pNew->y = y;
    pNew->w = w;
    pNew->h = h;
    pNew->pNext = pTree->cb.pDamage;
    pTree->cb.pDamage = pNew;

    if (!pTree->cb.flags) {
        Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
    }
    pTree->cb.flags |= HTML_DAMAGE;
}

void 
HtmlCallbackScrollY(pTree, y)
    HtmlTree *pTree;
    int y; 
{
    if (!pTree->cb.flags) {
        Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
    }
    pTree->cb.flags |= HTML_SCROLL;
    pTree->cb.iScrollY = y;
}

void 
HtmlCallbackScrollX(pTree, x)
    HtmlTree *pTree;
    int x; 
{
    if (!pTree->cb.flags) {
        Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
    }
    pTree->cb.flags |= HTML_SCROLL;
    pTree->cb.iScrollX = x;
}

/*
 *---------------------------------------------------------------------------
 *
 * cleanupHandlerTable --
 *
 *      This function is called to delete the contents of one of the
 *      HtmlTree.aScriptHandler, aNodeHandler or aParseHandler tables.
 *      It is called as the tree is being deleted.
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
cleanupHandlerTable(pHash)
    Tcl_HashTable *pHash;
{
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch search;

    for (
        pEntry = Tcl_FirstHashEntry(pHash, &search); 
        pEntry; 
        pEntry = Tcl_NextHashEntry(&search)
    ) {
        Tcl_DecrRefCount((Tcl_Obj *)Tcl_GetHashValue(pEntry));
    }
    Tcl_DeleteHashTable(pHash);
}

/*
 *---------------------------------------------------------------------------
 *
 * deleteWidget --
 *
 *     destroy $html
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
deleteWidget(clientData)
    ClientData clientData;
{
    HtmlDamage *pDamage;
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlTreeClear(pTree);

    /* Delete the contents of the three "handler" hash tables */
    cleanupHandlerTable(&pTree->aNodeHandler);
    cleanupHandlerTable(&pTree->aParseHandler);
    cleanupHandlerTable(&pTree->aScriptHandler);

    /* Clear any widget tags */
    HtmlTagCleanupTree(pTree);

    /* Clear the remaining colors etc. from the styler code hash tables */
    HtmlComputedValuesCleanupTables(pTree);

    /* Delete the image-server */
    HtmlImageServerDoGC(pTree);
    HtmlImageServerShutdown(pTree);

    /* Cancel any pending idle callback */
    Tcl_CancelIdleCall(callbackHandler, (ClientData)pTree);
    if (pTree->delayToken) {
        Tcl_DeleteTimerHandler(pTree->delayToken);
    }
    pTree->delayToken = 0;
    while ((pDamage = pTree->cb.pDamage)) {
        pTree->cb.pDamage = pDamage->pNext;
        HtmlFree(pDamage);
    }

    /* Delete the structure itself */
    HtmlFree(pTree);
}

/*
 *---------------------------------------------------------------------------
 *
 * widgetCmdDel --
 *
 *     This command is invoked by Tcl when a widget object command is
 *     deleted. This can happen under two circumstances:
 *
 *         * A script deleted the command. In this case we should delete
 *           the widget window (and everything else via the window event
 *           callback) as well.
 *         * A script deleted the widget window. In this case we need
 *           do nothing.
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
widgetCmdDel(clientData)
    ClientData clientData;
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    if( !pTree->isDeleted ){
      pTree->cmd = 0;
      Tk_DestroyWindow(pTree->tkwin);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * eventHandler --
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
eventHandler(clientData, pEvent)
    ClientData clientData;
    XEvent *pEvent;
{
    HtmlTree *pTree = (HtmlTree *)clientData;

    switch (pEvent->type) {
        case ConfigureNotify: {
            /* XConfigureEvent *p = (XConfigureEvent*)pEvent; */
            HtmlLog(pTree, "EVENT", "ConfigureNotify:");
            if (Tk_Width(pTree->tkwin) != pTree->iCanvasWidth) {
                HtmlCallbackLayout(pTree, pTree->pRoot);
            }
            break;
        }

        case Expose: {
            XExposeEvent *p = (XExposeEvent *)pEvent;

            HtmlLog(pTree, "EVENT", "Expose: x=%d y=%d width=%d height=%d",
                p->x, p->y, p->width, p->height
            );

            HtmlCallbackDamage(pTree, p->x, p->y, p->width, p->height, 1);
            break;
        }

        case VisibilityNotify: {
            XVisibilityEvent *p = (XVisibilityEvent *)pEvent;

            HtmlLog(pTree, "EVENT", "VisibilityNotify: state=%s", 
                p->state == VisibilityFullyObscured ?
                        "VisibilityFullyObscured":
                p->state == VisibilityPartiallyObscured ?
                        "VisibilityPartiallyObscured":
                p->state == VisibilityUnobscured ?
                        "VisibilityUnobscured":
                "N/A"
            );


#ifndef WIN32
            pTree->eVisibility = p->state;
#endif
            break;
        }

        case UnmapNotify: {
            if (pTree->pixmap) {
                /* If the window has a double-buffer pixmap, free it now. */
                Tk_FreePixmap(Tk_Display(pTree->tkwin), pTree->pixmap);
                pTree->pixmap = 0;
            }
            break;
        }

        case DestroyNotify: {
            pTree->isDeleted = 1;
            Tcl_DeleteCommandFromToken(pTree->interp, pTree->cmd);
            deleteWidget(pTree);
            break;
        }

    }
}

static int 
relayoutCb(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    HtmlCallbackLayout(pTree, pNode);
    return HTML_WALK_DESCEND;
}

static int 
worldChangedCb(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    if (!HtmlNodeIsText(pNode)) {
        HtmlElementNode *pElem = (HtmlElementNode *)pNode;
        HtmlLayoutInvalidateCache(pTree, pNode);
        HtmlNodeClearStyle(pTree, pElem);
    }
    return HTML_WALK_DESCEND;
}

/*
 *---------------------------------------------------------------------------
 *
 * configureCmd --
 *
 *     Implementation of the standard Tk "configure" command.
 *
 *         <widget> configure -OPTION VALUE ?-OPTION VALUE? ...
 *
 *     TODO: Handle configure of the forms:
 *         <widget> configure -OPTION 
 *         <widget> configure
 *
 * Results:
 *     Standard tcl result.
 *
 * Side effects:
 *     May set values of HtmlTree.options struct. May call
 *     Tk_GeometryRequest().
 *
 *---------------------------------------------------------------------------
 */
static int 
configureCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    static const char *azModes[] = {"quirks","almost standards","standards",0};
    /*
     * Mask bits for options declared in htmlOptionSpec.
     */
    #define GEOMETRY_MASK  0x00000001
    #define FT_MASK        0x00000002    
    #define DS_MASK        0x00000004    
    #define S_MASK         0x00000008    

    /*
     * Macros to generate static Tk_OptionSpec structures for the
     * htmlOptionSpec() array.
     */
    #define PIXELS(v, s1, s2, s3) \
        {TK_OPTION_PIXELS, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, 0, 0}
    #define GEOMETRY(v, s1, s2, s3) \
        {TK_OPTION_PIXELS, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, 0, GEOMETRY_MASK}
    #define STRING(v, s1, s2, s3) \
        {TK_OPTION_STRING, "-" #v, s1, s2, s3, \
         Tk_Offset(HtmlOptions, v), -1, TK_OPTION_NULL_OK, 0, 0}
    #define XCOLOR(v, s1, s2, s3) \
        {TK_OPTION_COLOR, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, 0, 0}
    #define BOOLEAN(v, s1, s2, s3, flags) \
        {TK_OPTION_BOOLEAN, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, 0, flags}
    #define OBJ(v, s1, s2, s3, f) \
        {TK_OPTION_STRING, "-" #v, s1, s2, s3, \
         Tk_Offset(HtmlOptions, v), -1, 0, 0, f}
    #define DOUBLE(v, s1, s2, s3, f) \
        {TK_OPTION_DOUBLE, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, 0, f}
    
    /* Option table definition for the html widget. */
    static Tk_OptionSpec htmlOptionSpec[] = {

        /* Standard geometry interface */
        GEOMETRY(height, "height", "Height", "600"),
        GEOMETRY(width, "width", "Width", "800"),

BOOLEAN(shrink, "shrink", "Shrink", "0", S_MASK),
BOOLEAN(layoutcache, "layoutCache", "LayoutCache", "1", S_MASK),
BOOLEAN(forcefontmetrics, "forceFontMetrics", "ForceFontMetrics", "1", S_MASK),
#ifdef WIN32
BOOLEAN(doublebuffer, "doubleBuffer", "DoubleBuffer", "1", 0),
#else
BOOLEAN(doublebuffer, "doubleBuffer", "DoubleBuffer", "0", 0),
#endif
BOOLEAN(forcewidth, "forceWidth", "ForceWidth", "0", S_MASK),

        DOUBLE(fontscale, "fontScale", "FontScale", "1.0", S_MASK),
        DOUBLE(zoom, "zoom", "Zoom", "1.0", S_MASK),

        /* Standard scroll interface - same as canvas, text */
        PIXELS(yscrollincrement, "yScrollIncrement", "ScrollIncrement", "20"),
        PIXELS(xscrollincrement, "xScrollIncrement", "ScrollIncrement", "20"),
        STRING(xscrollcommand, "xScrollCommand", "ScrollCommand", ""),
        STRING(yscrollcommand, "yScrollCommand", "ScrollCommand", ""),

        /* Non-debugging widget specific options */
        OBJ(defaultstyle, "defaultStyle", "DefaultStyle", 
            HTML_DEFAULT_CSS, FT_MASK),
        STRING(imagecmd, "imageCmd", "ImageCmd", ""),
        BOOLEAN(imagecache, "imageCache", "ImageCache", "1", S_MASK),
    
        /* Options for logging info to debugging scripts */
        STRING(logcmd, "logCmd", "LogCmd", ""),
        STRING(timercmd, "timerCmd", "TimerCmd", ""),

        OBJ(fonttable, "fontTable", "FontTable", "8 9 10 11 13 15 17", FT_MASK),

        {TK_OPTION_STRING_TABLE, "-mode", "mode", "Mode", "standards", 
             -1, Tk_Offset(HtmlOptions, mode), 0, (ClientData)azModes, S_MASK
        },
    
        {TK_OPTION_END, 0, 0, 0, 0, 0, 0, 0, 0}
    };
    #undef PIXELS
    #undef STRING
    #undef XCOLOR
    #undef BOOLEAN

    HtmlTree *pTree = (HtmlTree *)clientData;
    char *pOptions = (char *)&pTree->options;
    Tk_Window win = pTree->tkwin;
    Tk_OptionTable otab = pTree->optionTable;
    Tk_SavedOptions saved;

    int mask = 0;
    int init = 0;                /* True if Tk_InitOptions() is called */
    int rc;

    if (!otab) {
        pTree->optionTable = Tk_CreateOptionTable(interp, htmlOptionSpec);
        Tk_InitOptions(interp, pOptions, pTree->optionTable, win);
        init = 1;
        otab = pTree->optionTable;
    }

    rc = Tk_SetOptions(
        interp, pOptions, otab, objc-2, &objv[2], win, (init?0:&saved), &mask
    );
    if (TCL_OK == rc) {
        /* Hard-coded minimum values for width and height */
        pTree->options.height = MAX(pTree->options.height, 0);
        pTree->options.width = MAX(pTree->options.width, 0);

        if (init || (mask & GEOMETRY_MASK)) {
            int w = pTree->options.width;
            int h = pTree->options.height;
            Tk_GeometryRequest(pTree->tkwin, w, h);
        }
    
        if (init || mask & FT_MASK) {
            int nSize;
            Tcl_Obj **apSize;
            int aFontSize[7];
            Tcl_Obj *pFT = pTree->options.fonttable;
            if (
                Tcl_ListObjGetElements(interp, pFT, &nSize, &apSize) ||
                nSize != 7 ||
                Tcl_GetIntFromObj(interp, apSize[0], &aFontSize[0]) ||
                Tcl_GetIntFromObj(interp, apSize[1], &aFontSize[1]) ||
                Tcl_GetIntFromObj(interp, apSize[2], &aFontSize[2]) ||
                Tcl_GetIntFromObj(interp, apSize[3], &aFontSize[3]) ||
                Tcl_GetIntFromObj(interp, apSize[4], &aFontSize[4]) ||
                Tcl_GetIntFromObj(interp, apSize[5], &aFontSize[5]) ||
                Tcl_GetIntFromObj(interp, apSize[6], &aFontSize[6])
            ) {
                Tcl_ResetResult(interp);
                Tcl_AppendResult(interp, 
                    "expected list of 7 integers but got ", 
                    "\"", Tcl_GetString(pFT), "\"", 0
                );
                rc = TCL_ERROR;
            } else {
                memcpy(pTree->aFontSizeTable, aFontSize, sizeof(aFontSize));
                HtmlCallbackRestyle(pTree, pTree->pRoot);
            }
        }

        if (mask & S_MASK) {
            HtmlImageServerSuspendGC(pTree);
            HtmlWalkTree(pTree, pTree->pRoot, worldChangedCb, 0);
            HtmlCallbackRestyle(pTree, pTree->pRoot);
            HtmlCallbackLayout(pTree, pTree->pRoot);
        }

        if (rc != TCL_OK) {
            assert(!init);
            Tk_RestoreSavedOptions(&saved);
        } else if (!init) {
            Tk_FreeSavedOptions(&saved);
        }
    }

    return rc;
    #undef GEOMETRY_MASK
    #undef FT_MASK
}

/*
 *---------------------------------------------------------------------------
 *
 * cgetCmd --
 *
 *     Standard Tk "cget" command for querying options.
 *
 *     <widget> cget -OPTION
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int cgetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    Tcl_Obj *pRet;
    Tk_OptionTable otab = pTree->optionTable;
    Tk_Window win = pTree->tkwin;
    char *pOptions = (char *)&pTree->options;

    assert(otab);

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "-OPTION");
        return TCL_ERROR;
    }

    pRet = Tk_GetOptionValue(interp, pOptions, otab, objv[2], win);
    if( pRet ) {
        Tcl_SetObjResult(interp, pRet);
    } else {
        char * zOpt = Tcl_GetString(objv[2]);
        Tcl_AppendResult( interp, "unknown option \"", zOpt, "\"", 0);
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * resetCmd --
 * 
 *         widget reset
 *
 *     Reset the widget so that no document or stylesheet is loaded.
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
resetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    Tk_Window win = pTree->tkwin;

    HtmlTreeClear(pTree);

    HtmlImageServerDoGC(pTree);
    if (pTree->options.imagecache) {
        HtmlImageServerSuspendGC(pTree);
    }
    assert(HtmlImageServerCount(pTree) == 0);

    HtmlCallbackScrollY(pTree, 0);
    HtmlCallbackScrollX(pTree, 0);
    HtmlCallbackDamage(pTree, 0, 0, Tk_Width(win), Tk_Height(win), 0);
    doLoadDefaultStyle(pTree);
    pTree->isParseFinished = 0;
    pTree->isSequenceOk = 1;
    pTree->eWriteState = HTML_WRITE_NONE;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * relayoutCmd --
 * 
 *         $html relayout ?-layout|-style? ?NODE?
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
static int 
relayoutCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc == 2) {
        HtmlCallbackRestyle(pTree, pTree->pRoot);
        HtmlWalkTree(pTree, pTree->pRoot, relayoutCb, 0);
    } else {
        char *zArg3 = ((objc >= 3) ? Tcl_GetString(objv[2]) : 0);
        char *zArg4 = ((objc >= 4) ? Tcl_GetString(objv[3]) : 0);
        HtmlNode *pNode;

        pNode = HtmlNodeGetPointer(pTree, zArg4 ? zArg4 : zArg3);
        if (!zArg4) {
            HtmlCallbackRestyle(pTree, pNode);
            HtmlCallbackLayout(pTree, pNode);
        } else if (0 == strcmp(zArg3, "-layout")) {
            HtmlCallbackLayout(pTree, pNode);
        } else if (0 == strcmp(zArg3, "-style")) {
            HtmlCallbackRestyle(pTree, pNode);
        } else {
            Tcl_AppendResult(interp, 
                "Bad option \"", zArg3, "\": must be -layout or -style", 0
            );
            return TCL_ERROR;
        }
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * parseCmd --
 *
 *         $widget parse ?-final? HTML-TEXT
 * 
 *     Appends the given HTML text to the end of any HTML text that may have
 *     been inserted by prior calls to this command. See Tkhtml man page for
 *     further details.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
parseCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlNode *pCurrent = pTree->state.pCurrent;

    int isFinal;
    char *zHtml;
    int nHtml;

    Tcl_Obj *aObj[2];
    SwprocConf aConf[3] = {
        {SWPROC_SWITCH, "final", "0", "1"},   /* -final */
        {SWPROC_ARG, 0, 0, 0},                /* HTML-TEXT */
        {SWPROC_END, 0, 0, 0}
    };

    if (
        SwprocRt(interp, (objc - 2), &objv[2], aConf, aObj) ||
        Tcl_GetBooleanFromObj(interp, aObj[0], &isFinal)
    ) {
        return TCL_ERROR;
    }

    /* zHtml = Tcl_GetByteArrayFromObj(aObj[1], &nHtml); */
    zHtml = Tcl_GetStringFromObj(aObj[1], &nHtml);

    assert(Tcl_IsShared(aObj[1]));
    Tcl_DecrRefCount(aObj[0]);
    Tcl_DecrRefCount(aObj[1]);

    if (pTree->isParseFinished) {
        const char *zWidget = Tcl_GetString(objv[0]);
        Tcl_ResetResult(interp);
        Tcl_AppendResult(interp, 
            "Cannot call [", zWidget, " parse]" 
            "until after [", zWidget, "] reset", 0
        );
        return TCL_ERROR;
    }

    /* Add the new text to the internal cache of the document. Also tokenize
     * it and add the new HtmlToken objects to the HtmlTree.pFirst/pLast 
     * linked list.
     */
    HtmlTokenizerAppend(pTree, zHtml, nHtml, isFinal);
    if (isFinal) {
        pTree->isParseFinished = 1;
        assert(
            pTree->eWriteState == HTML_WRITE_NONE ||
            pTree->eWriteState == HTML_WRITE_WAIT
        );
        if (pTree->eWriteState == HTML_WRITE_NONE) {
            HtmlFinishNodeHandlers(pTree);
        }
    }

    HtmlCallbackRestyle(pTree, pCurrent ? pCurrent : pTree->pRoot);
    HtmlCallbackRestyle(pTree, pTree->state.pCurrent);
    HtmlCallbackRestyle(pTree, pTree->state.pFoster);
    HtmlCallbackLayout(pTree, pCurrent);

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * preloadCmd --
 *
 *         $widget preload URI
 * 
 *     Preload the image located at the specified URI.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
preloadCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlImage2 *pImg2 = 0;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "URI");
        return TCL_ERROR;
    }

    pImg2 = HtmlImageServerGet(pTree->pImageServer, Tcl_GetString(objv[2]));
    HtmlImageFree(pImg2);

    Tcl_ResetResult(interp);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * fragmentCmd --
 *
 *         $widget fragment HTML-TEXT
 *
 *     Parse the supplied markup test and return a list of node handles.
 * 
 * Results:
 *     List of node-handles.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
fragmentCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "HTML-TEXT");
        return TCL_ERROR;
    }
    HtmlParseFragment(pTree, Tcl_GetString(objv[2]));
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * viewCommon --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
viewCommon(pTree, isXview, objc, objv)
    HtmlTree *pTree;
    int isXview;               /* True for [xview], zero for [yview] */
    int objc;
    Tcl_Obj * CONST objv[];
{
    Tcl_Interp *interp = pTree->interp;
    Tk_Window win = pTree->tkwin;

    int iUnitPixels;           /* Value of -[xy]scrollincrement in pixels */
    int iPagePixels;           /* Width or height of the viewport */
    int iMovePixels;           /* Width or height of canvas */
    int iOffScreen;            /* Current scroll position */
    double aRet[2];
    Tcl_Obj *pRet;
    Tcl_Obj *pScrollCommand;

    if (isXview) { 
        iPagePixels = Tk_Width(win);
        iUnitPixels = pTree->options.xscrollincrement;
        iMovePixels = pTree->canvas.right;
        iOffScreen = pTree->iScrollX;
        pScrollCommand = pTree->options.xscrollcommand;
    } else {
        iPagePixels = Tk_Height(win);
        iUnitPixels = pTree->options.yscrollincrement;
        iMovePixels = pTree->canvas.bottom;
        iOffScreen = pTree->iScrollY;
        pScrollCommand = pTree->options.yscrollcommand;
    }

    if (objc > 2) {
        double fraction;
        int count;
        int iNewVal = 0;     /* New value of iScrollY or iScrollX */

        /* The [widget yview] command also supports "scroll-to-node" */
        if (!isXview && objc == 3) {
            const char *zCmd = Tcl_GetString(objv[2]);
            HtmlNode *pNode = HtmlNodeGetPointer(pTree, zCmd);
            if (!pNode) {
                return TCL_ERROR;
            }
            iNewVal = HtmlWidgetNodeTop(pTree, pNode);
            iMovePixels = pTree->canvas.bottom;
        } else {
            int eType;       /* One of the TK_SCROLL_ symbols */
            eType = Tk_GetScrollInfoObj(interp, objc, objv, &fraction, &count);
            switch (eType) {
                case TK_SCROLL_MOVETO:
                    iNewVal = (int)((double)iMovePixels * fraction);
                    break;
                case TK_SCROLL_PAGES:
                    iNewVal = iOffScreen + ((count * iPagePixels) * 0.9);
                    break;
                case TK_SCROLL_UNITS:
                    iNewVal = iOffScreen + (count * iUnitPixels);
                    break;
                case TK_SCROLL_ERROR:
                    return TCL_ERROR;
        
                default: assert(!"Not possible");
            }
        }

        /* Clip the new scrolling value for the window size */
        iNewVal = MIN(iNewVal, iMovePixels - iPagePixels);
        iNewVal = MAX(iNewVal, 0);

        if (isXview) { 
            HtmlCallbackScrollX(pTree, iNewVal);
        } else {
            HtmlCallbackScrollY(pTree, iNewVal);
        }
    }

    /* Construct the Tcl result for this command. */
    if (iMovePixels <= iPagePixels) {
        aRet[0] = 0.0;
        aRet[1] = 1.0;
    } else {
        assert(iMovePixels > 0);
        assert(iOffScreen  >= 0);
        assert(iPagePixels >= 0);
        aRet[0] = (double)iOffScreen / (double)iMovePixels;
        aRet[1] = (double)(iOffScreen + iPagePixels) / (double)iMovePixels;
        aRet[1] = MIN(aRet[1], 1.0);
    }
    pRet = Tcl_NewObj();
    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewDoubleObj(aRet[0]));
    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewDoubleObj(aRet[1]));
    Tcl_SetObjResult(interp, pRet);

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * xviewCmd --
 * yviewCmd --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
xviewCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    return viewCommon((HtmlTree *)clientData, 1, objc, objv);
}
static int 
yviewCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    return viewCommon((HtmlTree *)clientData, 0, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * writeCmd --
 *
 *     $widget write wait
 *     $widget write text TEXT
 *     $widget write continue
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
writeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    int eChoice;

    enum SubOptType {
        OPT_WAIT, OPT_TEXT, OPT_CONTINUE
    };
    struct SubOpt {
        char *zSubOption;             /* Name of sub-command */
        enum SubOptType eType;        /* Corresponding OPT_XXX value */
        int iExtraArgs;               /* Number of args following sub-command */
        char *zWrongNumArgsTail;      /* 4th arg to Tcl_WrongNumArgs() */
    } aSub[] = {
        {"wait", OPT_WAIT, 0, ""}, 
        {"text", OPT_TEXT, 1, "TEXT"}, 
        {"continue", OPT_CONTINUE, 0, ""}, 
        {0, 0, 0}
    };

    /* All commands must consist of at least three words - the widget name,
     * "write", and the sub-command name. Otherwise, it's a Tcl error.
     */
    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "OPTION");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObjStruct(
        interp, objv[2], aSub, sizeof(struct SubOpt), "option", 0, &eChoice) 
    ){
        return TCL_ERROR;
    }
    if ((objc - 3) != aSub[eChoice].iExtraArgs) {
        Tcl_WrongNumArgs(interp, 3, objv, aSub[eChoice].zWrongNumArgsTail);
        return TCL_ERROR;
    }

    assert(pTree->interp == interp);
    switch (aSub[eChoice].eType) {
        case OPT_WAIT:
            return HtmlWriteWait(pTree);
            break;
        case OPT_TEXT:
            return HtmlWriteText(pTree, objv[3]);
            break;
        case OPT_CONTINUE:
            return HtmlWriteContinue(pTree);
            break;
    }

    assert(!"Cannot happen");
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * handlerCmd --
 *
 *     $widget handler [node|script|parse] TAG SCRIPT
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
handlerCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int tag;
    Tcl_Obj *pScript;
    Tcl_HashEntry *pEntry;
    Tcl_HashTable *pHash = 0;
    int newentry;
    HtmlTree *pTree = (HtmlTree *)clientData;
    char *zTag;

    if (objc!=5) {
        Tcl_WrongNumArgs(interp, 3, objv, "TAG SCRIPT");
        return TCL_ERROR;
    }

    zTag = Tcl_GetString(objv[3]);
    tag = HtmlNameToType(0, zTag);
    switch (Tcl_GetString(objv[2])[0]) {
        case 'n':
            pHash = &pTree->aNodeHandler;
            break;
        case 's':
            pHash = &pTree->aScriptHandler;
            break;
        case 'p': {
            pHash = &pTree->aParseHandler;
            if (0 == zTag[0]) {
                tag = Html_Text;
            } else if ('/' == zTag[0]) {
                tag = HtmlNameToType(0, &zTag[1]);
                if (tag != Html_Unknown) tag = tag * -1;
            }
            break;
        }
        default:
            assert(!"Illegal objv[2] value in handlerCmd()");
    }

    assert(pHash);
    pScript = objv[4];

    if (tag==Html_Unknown) {
        Tcl_AppendResult(interp, "Unknown tag type: ", zTag, 0);
        return TCL_ERROR;
    }

    if (Tcl_GetCharLength(pScript) == 0) {
        pEntry = Tcl_FindHashEntry(pHash, (char *)tag);
        if (pEntry) {
            Tcl_DeleteHashEntry(pEntry);
        }
    } else {
        pEntry = Tcl_CreateHashEntry(pHash,(char*)tag,&newentry);
        if (!newentry) {
            Tcl_Obj *pOld = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
            Tcl_DecrRefCount(pOld);
        }
        Tcl_IncrRefCount(pScript);
        Tcl_SetHashValue(pEntry, (ClientData)pScript);
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * styleCmd --
 *
 *         $widget style ?options? HTML-TEXT
 *
 *             -importcmd IMPORT-CMD
 *             -id ID
 *             -urlcmd URL-CMD
 *
 * Results:
 *     Tcl result (i.e. TCL_OK, TCL_ERROR).
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
styleCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    SwprocConf aConf[4 + 1] = {
        {SWPROC_OPT, "id", "author", 0},      /* -id <style-sheet id> */
        {SWPROC_OPT, "importcmd", 0, 0},      /* -importcmd <cmd> */
        {SWPROC_OPT, "urlcmd", 0, 0},         /* -urlcmd <cmd> */
        {SWPROC_ARG, 0, 0, 0},                /* STYLE-SHEET-TEXT */
        {SWPROC_END, 0, 0, 0}
    };
    Tcl_Obj *apObj[4];
    int rc = TCL_OK;
    int n;
    HtmlTree *pTree = (HtmlTree *)clientData;

    /* First assert() that the sizes of the aConf and apObj array match. Then
     * call SwprocRt() to parse the arguments. If the parse is successful then
     * apObj[] contains the following: 
     *
     *     apObj[0] -> Value passed to -id option (or default "author")
     *     apObj[1] -> Value passed to -importcmd option (or default "")
     *     apObj[2] -> Value passed to -urlcmd option (or default "")
     *     apObj[3] -> Text of stylesheet to parse
     *
     * Pass these on to the HtmlStyleParse() command to actually parse the
     * stylesheet.
     */
    assert(sizeof(apObj)/sizeof(apObj[0])+1 == sizeof(aConf)/sizeof(aConf[0]));
    if (TCL_OK != SwprocRt(interp, objc - 2, &objv[2], aConf, apObj)) {
        return TCL_ERROR;
    }

    Tcl_GetStringFromObj(apObj[3], &n);
    if (n > 0) {
        rc = HtmlStyleParse(pTree,interp,apObj[3],apObj[0],apObj[1],apObj[2]);
    }

    /* Clean up object references created by SwprocRt() */
    SwprocCleanup(apObj, sizeof(apObj)/sizeof(Tcl_Obj *));

    if (rc == TCL_OK) {
        HtmlCallbackRestyle(pTree, pTree->pRoot);
    }
    return rc;
}

static int
tagAddCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTagAddRemoveCmd(clientData, interp, objc, objv, HTML_TAG_ADD);
}
static int
tagRemoveCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTagAddRemoveCmd(clientData, interp, objc, objv, HTML_TAG_REMOVE);
}
static int
tagCfgCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTagConfigureCmd(clientData, interp, objc, objv);
}

static int
tagDeleteCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTagDeleteCmd(clientData, interp, objc, objv);
}

static int
textTextCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTextTextCmd(clientData, interp, objc, objv);
}
static int
textIndexCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTextIndexCmd(clientData, interp, objc, objv);
}
static int
textBboxCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTextBboxCmd(clientData, interp, objc, objv);
}
static int
textOffsetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTextOffsetCmd(clientData, interp, objc, objv);
}


static int 
forceCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlCallbackForce((HtmlTree *)clientData);
    return TCL_OK;
}

static int 
delayCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    int iMilli;
    Tcl_TimerToken t;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "MILLI-SECONDS");
        return TCL_ERROR;
    }
    if (TCL_OK != Tcl_GetIntFromObj(interp, objv[2], &iMilli)) {
        return TCL_ERROR;
    }

    if (pTree->delayToken) {
        Tcl_DeleteTimerHandler(pTree->delayToken);
    }
    pTree->delayToken = 0;

    if (iMilli > 0) {
        t = Tcl_CreateTimerHandler(iMilli, delayCallbackHandler, clientData);
        pTree->delayToken = t;
    } else if (pTree->cb.flags) {
        Tcl_DoWhenIdle(callbackHandler, clientData);
    }
  
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * imageCmd --
 * nodeCmd --
 * primitivesCmd --
 *
 *     New versions of gcc don't allow pointers to non-local functions to
 *     be used as constant initializers (which we need to do in the
 *     aSubcommand[] array inside widgetCmd(). So the following
 *     functions are wrappers around Tcl_ObjCmdProc functions implemented
 *     in other files.
 *
 * Results:
 *     Tcl result (i.e. TCL_OK, TCL_ERROR).
 *
 * Side effects:
 *     Whatever the called function does.
 *
 *---------------------------------------------------------------------------
 */
static int 
imageCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutImage(clientData, interp, objc, objv);
}
static int 
nodeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutNode(clientData, interp, objc, objv);
}
static int 
primitivesCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutPrimitives(clientData, interp, objc, objv);
}
static int 
searchCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlCssSearch(clientData, interp, objc, objv);
}
static int 
styleconfigCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlCssStyleConfigDump(clientData, interp, objc, objv);
}
static int 
stylereportCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlCssStyleReport(clientData, interp, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * bboxCmd --
 *
 *     html bbox ?node-handle?
 *
 * Results:
 *     Tcl result (i.e. TCL_OK, TCL_ERROR).
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
bboxCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlNode *pNode;
    int x, y, w, h;
    HtmlTree *pTree = (HtmlTree *)clientData;
    Tcl_Obj *pRet = Tcl_NewObj();

    if (objc != 2 && objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?NODE-HANDLE?");
        return TCL_ERROR;
    }

    if (objc == 3) {
        pNode = HtmlNodeGetPointer(pTree, Tcl_GetString(objv[2]));
        if (!pNode) return TCL_ERROR;
    } else {
        pNode = pTree->pRoot;
    }

    HtmlWidgetNodeBox(pTree, pNode, &x, &y, &w, &h);
    if (w > 0 && h > 0) {
        Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(x));
        Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(y));
        Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(x + w));
        Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(y + h));
    }

    Tcl_SetObjResult(interp, pRet);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * widgetCmd --
 *
 *     This is the C function invoked for a widget command.
 *
 *         cget
 *         configure
 *         handler
 *         image
 *         node
 *         parse
 *         reset
 *         style
 *         tag
 *         text
 *         xview
 *         yview
 *
 * Results:
 *     Tcl result.
 *
 * Side effects:
 *     Whatever the command does.
 *
 *---------------------------------------------------------------------------
 */
int widgetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    /* The following array defines all the built-in widget commands.  This
     * function just parses the first one or two arguments and vectors control
     * to one of the command service routines defined in the following array.
     */
    static struct SC {
        char *zCmd1;                /* First-level subcommand.  Required */
        char *zCmd2;                /* Second-level subcommand.  May be NULL */
        Tcl_ObjCmdProc *xFuncObj;   /* Object cmd */
    } aSubcommand[] = {
        {"bbox",       0,           bboxCmd},
        {"cget",       0,           cgetCmd},
        {"configure",  0,           configureCmd},
        {"fragment",   0,           fragmentCmd},
        {"handler",    "node",      handlerCmd},
        {"handler",    "parse",     handlerCmd},
        {"handler",    "script",    handlerCmd},
        {"image",      0,           imageCmd},
        {"node",       0,           nodeCmd},
        {"parse",      0,           parseCmd},
        {"preload",    0,           preloadCmd},
        {"reset",      0,           resetCmd},
        {"search",     0,           searchCmd},
        {"style",      0,           styleCmd},

        {"tag",        "add",       tagAddCmd},
        {"tag",        "remove",    tagRemoveCmd},
        {"tag",        "configure", tagCfgCmd},
        {"tag",        "delete",    tagDeleteCmd},

        {"text",       "text",      textTextCmd},
        {"text",       "index",     textIndexCmd},
        {"text",       "bbox",      textBboxCmd},
        {"text",       "offset",    textOffsetCmd},

        {"write",      0,           writeCmd},

        {"xview",      0,           xviewCmd},
        {"yview",      0,           yviewCmd},

        /* The following are for debugging only. May change at any time.
	 * They are not included in the documentation. Just don't touch Ok? :)
         */
        {"delay",       0,          delayCmd},
        {"force",       0,          forceCmd},
        {"primitives",  0,          primitivesCmd},
        {"relayout",    0,          relayoutCmd},
        {"styleconfig", 0,          styleconfigCmd},
        {"stylereport", 0,          stylereportCmd},
#ifndef NDEBUG
        {"_hashstats", 0, hashstatsCmd},
#endif
    };

    int i;
    CONST char *zArg1 = 0;
    CONST char *zArg2 = 0;
    Tcl_Obj *pError;

    int matchone = 0; /* True if the first argument matched something */
    int multiopt = 0; /* True if their were multiple options for second match */
    CONST char *zBad;

    if (objc>1) {
        zArg1 = Tcl_GetString(objv[1]);
    } else {
        Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
        return TCL_ERROR;
    }
    if (objc>2) {
        zArg2 = Tcl_GetString(objv[2]);
    }

    /* Search the array of built-in commands */
    for (i=0; i<sizeof(aSubcommand)/sizeof(struct SC); i++) {
         struct SC *pCommand = &aSubcommand[i];
         if (zArg1 && 0==strcmp(zArg1, pCommand->zCmd1)) {
             matchone = 1;
             if (!pCommand->zCmd2 || 
                 (zArg2 && 0==strcmp(zArg2, pCommand->zCmd2))) 
             {
                 return pCommand->xFuncObj(clientData, interp, objc, objv);
             }
         }
    }

    /* Failed to find a matching sub-command. The remainder of this routine
     * is generating an error message. 
     */
    zBad = matchone?zArg2:zArg1;
    pError = Tcl_NewStringObj("bad option \"", -1);
    Tcl_IncrRefCount(pError);
    Tcl_AppendToObj(pError, zBad, -1);
    Tcl_AppendToObj(pError, "\" must be ", -1);
    zBad = 0;
    for (i=0; i<sizeof(aSubcommand)/sizeof(struct SC); i++) {
        struct SC *pCommand = &aSubcommand[i];
        CONST char *zAdd = 0;

        if (matchone) { 
            if (0==strcmp(pCommand->zCmd1, zArg1)) {
                zAdd = pCommand->zCmd2;
            }
        } else if (!zBad || strcmp(pCommand->zCmd1, zBad)) {
            zAdd = pCommand->zCmd1;
        }

        if (zAdd) {
            if (zBad) {
                Tcl_AppendToObj(pError, zBad, -1);
                Tcl_AppendToObj(pError, ", ", -1);
                multiopt = 1;
            }
            zBad = zAdd;
        }
    }
    if (zBad) {
        if (multiopt) {
            Tcl_AppendToObj(pError, "or ", -1);
        }
        Tcl_AppendToObj(pError, zBad, -1);
    }
    Tcl_SetObjResult(interp, pError);
    Tcl_DecrRefCount(pError);
    
    return TCL_ERROR;
}



/*
 *---------------------------------------------------------------------------
 *
 * newWidget --
 *
 *     Create a new Html widget command:
 *
 *         html PATH ?<options>?
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
newWidget(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree;
    CONST char *zCmd;
    int rc;
    Tk_Window mainwin;           /* Main window of application */

    if (objc<2) {
        Tcl_WrongNumArgs(interp, 1, objv, "WINDOW-PATH ?OPTIONS?");
        return TCL_ERROR;
    }
    
    zCmd = Tcl_GetString(objv[1]);
    pTree = HtmlNew(HtmlTree);
    pTree->eVisibility = VisibilityPartiallyObscured;

    /* Create the Tk window.
     */
    mainwin = Tk_MainWindow(interp);
    pTree->tkwin = Tk_CreateWindowFromPath(interp, mainwin, zCmd, NULL); 
    if (!pTree->tkwin) {
        HtmlFree(pTree);
        return TCL_ERROR;
    }
    Tk_SetClass(pTree->tkwin, "Html");

    pTree->interp = interp;
    Tcl_InitHashTable(&pTree->aParseHandler, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aScriptHandler, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aNodeHandler, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aOrphan, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aTag, TCL_STRING_KEYS);
    pTree->cmd = Tcl_CreateObjCommand(interp,zCmd,widgetCmd,pTree,widgetCmdDel);

    /* Initialise the hash tables used by styler code */
    HtmlComputedValuesSetupTables(pTree);

    /* Set up an event handler for the widget window */
    Tk_CreateEventHandler(pTree->tkwin, 
            ExposureMask|StructureNotifyMask|VisibilityChangeMask, 
            eventHandler, (ClientData)pTree
    );

    /* Create the image-server */
    HtmlImageServerInit(pTree);

    /* TODO: Handle the case where configureCmd() returns an error. */
    rc = configureCmd(pTree, interp, objc, objv);
    if (rc != TCL_OK) {
        Tk_DestroyWindow(pTree->tkwin);
        return TCL_ERROR;
    }
    assert(!pTree->options.logcmd);
    assert(!pTree->options.timercmd);

    /* Load the default style-sheet, ready for the first document. */
    doLoadDefaultStyle(pTree);
    pTree->isSequenceOk = 1;

    /* Return the name of the widget just created. */
    Tcl_SetObjResult(interp, objv[1]);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * htmlstyleCmd --
 *
 *     ::tkhtml::htmlstyle ?-quirks?
 *
 * Results:
 *     Built-in html style-sheet, including quirks if the -quirks option
 *     was specified.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
htmlstyleCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    if (objc > 1 && objc != 2 && strcmp(Tcl_GetString(objv[1]), "-quirks")) {
        Tcl_WrongNumArgs(interp, 1, objv, "?-quirks?");
        return TCL_ERROR;
    }

    Tcl_SetResult(interp, HTML_DEFAULT_CSS, TCL_STATIC);
    if (objc == 2) {
        Tcl_AppendResult(interp, HTML_DEFAULT_QUIRKS);
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * htmlVersionCmd --
 *
 *     ::tkhtml::version
 *
 * Results:
 *     Returns a string containing the versions of the *.c files used
 *     to build the library
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
htmlVersionCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    if (objc > 1) {
        Tcl_WrongNumArgs(interp, 1, objv, "");
        return TCL_ERROR;
    }
    Tcl_SetResult(interp, HTML_SOURCE_FILES, TCL_STATIC);
    return TCL_OK;
}


static int 
htmlDecodeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlDecode(clientData, interp, objc, objv);
}

static int 
htmlEscapeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlEscapeUriComponent(clientData, interp, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * htmlByteOffsetCmd --
 * htmlCharOffsetCmd --
 *
 *     ::tkhtml::charoffset STRING BYTE-OFFSET
 *     ::tkhtml::byteoffset STRING CHAR-OFFSET
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
htmlByteOffsetCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int iCharOffset;
    int iRet;
    char *zArg;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "STRING CHAR-OFFSET");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[2], &iCharOffset)) return TCL_ERROR;
    zArg = Tcl_GetString(objv[1]);

    iRet = (Tcl_UtfAtIndex(zArg, iCharOffset) - zArg);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(iRet));
    return TCL_OK;
}
static int 
htmlCharOffsetCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int iByteOffset;
    int iRet;
    char *zArg;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "STRING BYTE-OFFSET");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[2], &iByteOffset)) return TCL_ERROR;
    zArg = Tcl_GetString(objv[1]);

    iRet = Tcl_NumUtfChars(zArg, iByteOffset);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(iRet));
    return TCL_OK;
}


/*
 * Define the DLL_EXPORT macro, which must be set to something or other in
 * order to export the Tkhtml_Init and Tkhtml_SafeInit symbols from a win32
 * DLL file. I don't entirely understand the ins and outs of this, the
 * block below was copied verbatim from another program.
 */
#if INTERFACE
#define DLL_EXPORT
#endif
#if defined(USE_TCL_STUBS) && defined(__WIN32__)
# undef DLL_EXPORT
# define DLL_EXPORT __declspec(dllexport)
#endif
#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

/*
 *---------------------------------------------------------------------------
 *
 * Tkhtml_Init --
 *
 *     Load the package into an interpreter.
 *
 * Results:
 *     Tcl result.
 *
 * Side effects:
 *     Loads the tkhtml package into interpreter interp.
 *
 *---------------------------------------------------------------------------
 */
DLL_EXPORT int Tkhtml_Init(interp)
    Tcl_Interp *interp;
{
    int rc;

    /* Require stubs libraries version 8.4 or greater. */
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.4", 0) == 0) {
        return TCL_ERROR;
    }
    if (Tk_InitStubs(interp, "8.4", 0) == 0) {
        return TCL_ERROR;
    }
#endif

    if (0 == Tcl_PkgRequire(interp, "Tk", "8.4", 0)) {
        return TCL_ERROR;
    }
    Tcl_PkgProvide(interp, "Tkhtml", "3.0");

    Tcl_CreateObjCommand(interp, "html", newWidget, 0, 0);
    Tcl_CreateObjCommand(interp, "::tkhtml::htmlstyle", htmlstyleCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "::tkhtml::version", htmlVersionCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "::tkhtml::decode", htmlDecodeCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "::tkhtml::escape_uri", htmlEscapeCmd, 0, 0);

    Tcl_CreateObjCommand(interp, "::tkhtml::byteoffset", htmlByteOffsetCmd,0,0);
    Tcl_CreateObjCommand(interp, "::tkhtml::charoffset", htmlCharOffsetCmd,0,0);

#ifndef NDEBUG
    Tcl_CreateObjCommand(interp, "::tkhtml::htmlalloc", allocCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "::tkhtml::heapdebug", heapdebugCmd, 0, 0);
#endif

    SwprocInit(interp);

    rc = Tcl_EvalEx(interp, HTML_DEFAULT_TCL, -1, TCL_EVAL_GLOBAL);
    assert(rc == TCL_OK);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tkhtml_SafeInit --
 *
 *     Load the package into a safe interpreter.
 *
 *     Note that this function has to be below the Tkhtml_Init()
 *     implementation. Otherwise the Tkhtml_Init() invocation in this
 *     function counts as an implicit declaration which causes problems for
 *     MSVC somewhere on down the line.
 *
 * Results:
 *     Tcl result.
 *
 * Side effects:
 *     Loads the tkhtml package into interpreter interp.
 *
 *---------------------------------------------------------------------------
 */
DLL_EXPORT int Tkhtml_SafeInit(interp)
    Tcl_Interp *interp;
{
    return Tkhtml_Init(interp);
}
