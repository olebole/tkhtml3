
/*
 * htmlstyle.c ---
 *
 *     This file applies the cascade algorithm using the stylesheet
 *     code in css.c to the tree built with code in htmltree.c
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
static const char rcsid[] = "$Id: htmlstyle.c,v 1.50 2006/12/23 09:01:53 danielk1977 Exp $";

#include "html.h"
#include <assert.h>
#include <string.h>

void
HtmlDelScrollbars(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    HtmlElementNode *pElem = (HtmlElementNode *)pNode;

    if (!HtmlNodeIsText(pNode) && pElem->pScrollbar) {
        HtmlNodeScrollbars *p = pElem->pScrollbar;
        if (p->vertical.win) {
	    /* Remove any entry from the HtmlTree.pMapped list. */
            if (&p->vertical == pTree->pMapped) {
                pTree->pMapped = p->vertical.pNext;
            } else {
                HtmlNodeReplacement *pCur = pTree->pMapped; 
                while( pCur && pCur->pNext != &p->vertical ) pCur = pCur->pNext;
                if (pCur) {
                    pCur->pNext = p->vertical.pNext;
                }
            }

            Tk_DestroyWindow(p->vertical.win);
            Tcl_DecrRefCount(p->vertical.pReplace);
        }
        if (p->horizontal.win) {
	    /* Remove any entry from the HtmlTree.pMapped list. */
            if (&p->horizontal == pTree->pMapped) {
                pTree->pMapped = p->horizontal.pNext;
            } else {
                HtmlNodeReplacement *pCur = pTree->pMapped; 
                while( pCur && pCur->pNext != &p->horizontal ) {
                    pCur = pCur->pNext;
                }
                if (pCur) {
                    pCur->pNext = p->horizontal.pNext;
                }
            }

            Tk_DestroyWindow(p->horizontal.win);
            Tcl_DecrRefCount(p->horizontal.pReplace);
        }
        HtmlFree(p);
        pElem->pScrollbar = 0;
    }
}

void
HtmlDelStackingInfo(pTree, pElem)
    HtmlTree *pTree;
    HtmlElementNode *pElem;
{
    HtmlNodeStack *pStack = pElem->pStack;
    if (pStack && pStack->pElem == pElem){
        if (pStack->pPrev) {
            pStack->pPrev->pNext = pStack->pNext;
        } 
        if (pStack->pNext) {
            pStack->pNext->pPrev = pStack->pPrev;
        } 
        if (pStack==pTree->pStack) {
          pTree->pStack = pStack->pNext;
        }
        assert(!pTree->pStack || !pTree->pStack->pPrev);

        HtmlFree(pStack);
        pTree->nStack--;
    }
    pElem->pStack = 0;
}


#define STACK_NONE      0
#define STACK_FLOAT     1
#define STACK_AUTO      2
#define STACK_CONTEXT   3
static int 
stackType(p) 
    HtmlNode *p;
{
    HtmlComputedValues *pV = HtmlNodeComputedValues(p);
    if (!HtmlNodeParent(p)) {
        return STACK_CONTEXT;
    }
    if (pV->ePosition != CSS_CONST_STATIC) {
        if (pV->iZIndex == PIXELVAL_AUTO) return STACK_AUTO;
        return STACK_CONTEXT;
    }
    assert(pV->eFloat != CSS_CONST_NONE);
    return STACK_FLOAT;
}

static void
addStackingInfo(pTree, pElem)
    HtmlTree *pTree;
    HtmlElementNode *pElem;
{
    HtmlComputedValues *pV = pElem->pPropertyValues;
    HtmlNode *pNode = (HtmlNode *)pElem;
    
    /* A node forms a new stacking context if it is positioned or floating.
     * We only need create an HtmlNodeStack if this is the case.
      */
    if (
        !HtmlNodeParent(pNode) ||
        pV->eFloat != CSS_CONST_NONE ||
        pV->ePosition != CSS_CONST_STATIC
    ) {
        HtmlNodeStack *pStack;
        int nByte = sizeof(HtmlNodeStack);

        pStack = (HtmlNodeStack *)HtmlClearAlloc("HtmlNodeStack", nByte);
        pStack->pElem = pElem;
        pStack->eType = stackType(pNode);
        pStack->pNext = pTree->pStack;
        if( pStack->pNext ){
            pStack->pNext->pPrev = pStack;
        }
        pTree->pStack = pStack;
        pElem->pStack = pStack;
        pTree->cb.flags |= HTML_STACK;
        pTree->nStack++;
    } else {
      pElem->pStack = ((HtmlElementNode *)HtmlNodeParent(pNode))->pStack;
    }
    assert(pElem->pStack);
}

#define STACK_STACKING  1
#define STACK_BLOCK     3
#define STACK_INLINE    5
typedef struct StackCompare StackCompare;
struct StackCompare {
    HtmlNodeStack *pStack;
    int eStack;
};

#define IS_STACKING_CONTEXT(x) (                                    \
         x == x->pStack->pElem && x->pStack->eType == STACK_CONTEXT \
)

/*
 *---------------------------------------------------------------------------
 *
 * scoreStack --
 *
 * Results:
 *     1 -> Border and background of stacking context box.
 *     2 -> Descendants with negative z-index values.
 *     3 -> In-flow, non-inline descendants.
 *     4 -> Floats and their contents.
 *     5 -> In-flow, inline descendants.
 *     6 -> Positioned descendants with z-index values of "auto" or "0".
 *     7 -> Descendants with positive z-index values.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
scoreStack(pParentStack, pStack, eStack)
    HtmlNodeStack *pParentStack;
    HtmlNodeStack *pStack;
    int eStack;
{
    int z;
    if (pStack == pParentStack) {
        return eStack;
    }
    assert(pStack->pElem->node.pParent);
    if (pStack->eType == STACK_FLOAT) return 4;
    if (pStack->eType == STACK_AUTO) return 6;
    z = pStack->pElem->pPropertyValues->iZIndex;
    assert(z != PIXELVAL_AUTO);
    if (z == 0) return 6;
    if (z < 0) return 2;
    return 7;
}

static int
stackCompare(pVoidLeft, pVoidRight)
    const void *pVoidLeft;
    const void *pVoidRight;
{
    StackCompare *pLeft = (StackCompare *)pVoidLeft;
    StackCompare *pRight = (StackCompare *)pVoidRight;

    HtmlNodeStack *pLeftStack = pLeft->pStack;
    HtmlNodeStack *pRightStack = pRight->pStack;
    HtmlNodeStack *pParentStack = 0;

    int nLeftDepth = -1;        /* Tree depth of node pLeftStack->pNode */
    int nRightDepth = -1;       /* Tree depth of node pRightStack->pNode */

    int iLeft;
    int iRight;
    int iRes;
    int iTreeOrder = 0;

    int ii;
    HtmlElementNode *pL;
    HtmlElementNode *pR;

    /* There are three scenarios:
     *
     *     1) pLeft and pRight are associated with the same HtmlNodeStack
     *        structure. In this case "inline" beats "block" and "block"
     *        beats "stacking".
     *
     *     2) pLeft is descended from pRight, or vice versa.
     *
     *     3) Both are descended from a common stacking context.
     */

    /* Calculate pLeftStack, pRightStack and pParentStack */
    for (pL = pLeftStack->pElem; pL; pL = HtmlElemParent(pL)) nLeftDepth++;
    for (pR = pRightStack->pElem; pR; pR = HtmlElemParent(pR)) nRightDepth++;
    pL = pLeftStack->pElem;
    pR = pRightStack->pElem;
    for (ii = 0; ii < MAX(0, nLeftDepth - nRightDepth); ii++) {
        if (IS_STACKING_CONTEXT(pL)) pLeftStack = pL->pStack;
        pL = HtmlElemParent(pL);
        iTreeOrder = +1;
    }
    for (ii = 0; ii < MAX(0, nRightDepth - nLeftDepth); ii++) {
        if (IS_STACKING_CONTEXT(pR)) pRightStack = pR->pStack;
        pR = HtmlElemParent(pR);
        iTreeOrder = -1;
    }
    while (pR != pL) {
        HtmlElementNode *pParentL = HtmlElemParent(pL);
        HtmlElementNode *pParentR = HtmlElemParent(pR);
        if (IS_STACKING_CONTEXT(pL)) pLeftStack = pL->pStack;
        if (IS_STACKING_CONTEXT(pR)) pRightStack = pR->pStack;
        if (pParentL == pParentR) {
            iTreeOrder = 0;
            for (ii = 0; 0 == iTreeOrder; ii++) {
                HtmlNode *pChild = HtmlNodeChild(&pParentL->node, ii);
                if (pChild == (HtmlNode *)pL) {
                    iTreeOrder = -1;
                }
                if (pChild == (HtmlNode *)pR) {
                    iTreeOrder = +1;
                }
            }
            assert(iTreeOrder != 0);
        }
        pL = pParentL;
        pR = pParentR;
        assert(pL && pR);
    }
    while (!IS_STACKING_CONTEXT(pR)) {
        pR = HtmlElemParent(pR);
        assert(pR);
    }
    pParentStack = pR->pStack;

    iLeft = scoreStack(pParentStack, pLeftStack, pLeft->eStack);
    iRight = scoreStack(pParentStack, pRightStack, pRight->eStack);

    iRes = iLeft - iRight;
    if (iRes == 0 && (iRight == 2 || iRight == 6 || iRight == 7)) {
        int z1 = pLeftStack->pElem->pPropertyValues->iZIndex;
        int z2 = pRightStack->pElem->pPropertyValues->iZIndex;
        if (z1 == PIXELVAL_AUTO) z1 = 0;
        if (z2 == PIXELVAL_AUTO) z2 = 0;
        iRes = z1 - z2;
    }
    /* if (iRes == 0 && (iRight == 4 || pLeftStack == pRightStack)) { */
    if (iRes == 0 && pLeftStack == pRightStack) {
        iRes = (pLeft->eStack - pRight->eStack);
    }
    if (iRes == 0) {
        assert(iTreeOrder != 0);
        iRes = iTreeOrder;
    }
    return iRes;
}
#undef IS_STACKING_CONTEXT

/*
 *---------------------------------------------------------------------------
 *
 * checkStackSort --
 *
 *     This function is equivalent to an assert() statement. It is not
 *     part of the functionality of Tkhtml3, but is used to check the
 *     integrity of internal data structures.
 *
 *     The first argument, aStack, should be an array of nStack (the
 *     second arg) StackCompare structure. An assert fails inside
 *     this function if the array is not sorted in ascending order.
 *
 *     The primary purpose of this test is to ensure that the stackCompare()
 *     comparision function is stable. It is quite an expensive check,
 *     so is normally disabled at compile time. Change the "#if 0"
 *     below to reenable the checking.
 *
 *     NOTE: If you got this file from tkhtml.tcl.tk and there is an 
 *           "#if 1" in the code below, I have checked it in by mistake.
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
checkStackSort(pTree, aStack, nStack)
    HtmlTree *pTree;
    StackCompare *aStack;
    int nStack;
{
#if 0
    int ii;
    int jj;
    for (ii = 0; ii < nStack; ii++) {
      for (jj = ii + 1; jj < nStack; jj++) {
        int r1 = stackCompare(&aStack[ii], &aStack[jj]);
        int r2 = stackCompare(&aStack[jj], &aStack[ii]);
        assert(r1 && r2);
        assert((r1 * r2) < 0);
        assert(r1 < 0);
      }
    }
#endif
}

void
HtmlRestackNodes(pTree)
    HtmlTree *pTree;
{
    HtmlNodeStack *pStack;
    StackCompare *apTmp;
    int iTmp = 0;
    if (0 == (pTree->cb.flags & HTML_STACK)) return;

    apTmp = (StackCompare *)HtmlAlloc(0, sizeof(StackCompare)*pTree->nStack*3);

    for (pStack = pTree->pStack; pStack; pStack = pStack->pNext) {
        apTmp[iTmp].pStack = pStack;
        apTmp[iTmp].eStack = STACK_BLOCK;
        apTmp[iTmp+1].pStack = pStack;
        apTmp[iTmp+1].eStack = STACK_INLINE;
        apTmp[iTmp+2].pStack = pStack;
        apTmp[iTmp+2].eStack = STACK_STACKING;
        iTmp += 3;
    }
    assert(iTmp == pTree->nStack * 3);

    qsort(apTmp, pTree->nStack * 3, sizeof(StackCompare), stackCompare);

    for (iTmp = 0; iTmp < pTree->nStack * 3; iTmp++) {
#if 0
printf("Stack %d: %s %s\n", iTmp, 
    Tcl_GetString(HtmlNodeCommand(pTree, apTmp[iTmp].pStack->pNode)),
    (apTmp[iTmp].eStack == STACK_INLINE ? "inline" : 
     apTmp[iTmp].eStack == STACK_BLOCK ? "block" : "stacking")
);
#endif
        switch (apTmp[iTmp].eStack) {
            case STACK_INLINE:
                apTmp[iTmp].pStack->iInlineZ = iTmp;
                break;
            case STACK_BLOCK:
                apTmp[iTmp].pStack->iBlockZ = iTmp;
                break;
            case STACK_STACKING:
                apTmp[iTmp].pStack->iStackingZ = iTmp;
                break;
        }
    }
    checkStackSort(pTree, apTmp, pTree->nStack * 3);

    pTree->cb.flags &= (~HTML_STACK);
    HtmlFree(apTmp);
}

/*
 *---------------------------------------------------------------------------
 *
 * styleNode --
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
styleNode(pTree, pNode, clientData)
    HtmlTree *pTree; 
    HtmlNode *pNode;
    ClientData clientData;
{
    CONST char *zStyle;      /* Value of "style" attribute for node */
    int trashDynamics = (int)clientData;

    if (!HtmlNodeIsText(pNode)) {
        HtmlElementNode *pElem = (HtmlElementNode *)pNode;
        int redrawmode = 0;
        HtmlComputedValues *pV = pElem->pPropertyValues;
        pElem->pPropertyValues = 0;
        HtmlDelStackingInfo(pTree, pElem);

        /* If the clientData was set to a non-zero value, then the 
         * stylesheet configuration has changed. In this case we need to
         * recalculate the nodes list of dynamic conditions.
         */
        if (trashDynamics) {
            HtmlCssFreeDynamics(pElem);
        }
    
        /* If there is a "style" attribute on this node, parse the attribute
         * value and put the resulting mini-stylesheet in pNode->pStyle. 
         *
         * We assume that if the pStyle attribute is not NULL, then this node
         * has been styled before. The stylesheet configuration may have
         * changed since then, so we have to recalculate pNode->pProperties,
         * but the "style" attribute is constant so pStyle is never invalid.
         */
        if (!pElem->pStyle) {
            zStyle = HtmlNodeAttr(pNode, "style");
            if (zStyle) {
                HtmlCssInlineParse(-1, zStyle, &pElem->pStyle);
            }
        }
    
        /* Recalculate the properties for this node */
        HtmlCssStyleSheetApply(pTree, pNode);
        HtmlComputedValuesRelease(pTree, pElem->pPreviousValues);
        pElem->pPreviousValues = pV;

        redrawmode = HtmlComputedValuesCompare(pElem->pPropertyValues, pV);

        /* Regenerate any :before and :after content */
        if (pElem->pBefore || pElem->pAfter) {
            HtmlCallbackLayout(pTree, pNode);
            HtmlNodeClearGenerated(pTree, pElem);
            redrawmode = 2;
        }
        HtmlCssStyleSheetGenerated(pTree, pElem);
        if (pElem->pBefore || pElem->pAfter) {
            redrawmode = 2;
        }

        if (!pV || redrawmode == 2) {
            HtmlCallbackLayout(pTree, pNode);
        } else if (redrawmode == 1 && !(pTree->cb.flags & HTML_LAYOUT)) {
            int x, y, w, h;
            HtmlWidgetNodeBox(pTree, pNode, &x, &y, &w, &h);
            HtmlCallbackDamage(pTree,x-pTree->iScrollX,y-pTree->iScrollY,w,h,0);
        }

        addStackingInfo(pTree, pElem);

        if (pElem->pBefore) {
            ((HtmlElementNode *)(pElem->pBefore))->pStack = pElem->pStack;
            pElem->pBefore->pParent = pNode;
            pElem->pBefore->iNode = -1;
        }
        if (pElem->pAfter) {
            ((HtmlElementNode *)(pElem->pAfter))->pStack = pElem->pStack;
            pElem->pAfter->pParent = pNode;
            pElem->pAfter->iNode = -1;
        }

        /* If there has been a style-callback configured (-stylecmd option to
         * the [nodeHandle replace] command) for this node, invoke it now.
         */
        if (pElem->pReplacement && pElem->pReplacement->pStyle) {
            int rc = Tcl_EvalObjEx(
                pTree->interp, pElem->pReplacement->pStyle, TCL_EVAL_GLOBAL
            );
            if (rc != TCL_OK) {
                Tcl_BackgroundError(pTree->interp);
            }
        }
    }

    return HTML_WALK_DESCEND;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlStyleApply --
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
HtmlStyleApply(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    int isRoot = ((pNode == pTree->pRoot) ? 1 : 0);
    HtmlLog(pTree, "STYLEENGINE", "START");
    HtmlWalkTree(pTree, pNode, styleNode, (ClientData)isRoot);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlStyleSyntaxErrs --
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
HtmlStyleSyntaxErrs(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    int nSyntaxErrs = 0;
    if( pTree->pStyle ){
        nSyntaxErrs = HtmlCssStyleSheetSyntaxErrs(pTree->pStyle);
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(nSyntaxErrs));
    return TCL_OK;
}

