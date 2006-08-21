
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
static const char rcsid[] = "$Id: htmlstyle.c,v 1.37 2006/08/21 15:44:38 danielk1977 Exp $";

#include "html.h"
#include <assert.h>
#include <string.h>

void
HtmlDelStackingInfo(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    HtmlNodeStack *pStack = pNode->pStack;
    if (pStack && pStack->pNode == pNode){
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

        HtmlFree("HtmlNodeStack", pStack);
    }
    pNode->pStack = 0;
}

static void
addStackingInfo(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    HtmlComputedValues *pV = pNode->pPropertyValues;
    if (
        !HtmlNodeParent(pNode) || 
        pV->eFloat != CSS_CONST_NONE || 
        pV->ePosition != CSS_CONST_STATIC
    ) {
        HtmlNodeStack *pStack;
        int nByte = sizeof(HtmlNodeStack);

        pStack = (HtmlNodeStack *)HtmlClearAlloc("HtmlNodeStack", nByte);
        pStack->pNode = pNode;
        pStack->pNext = pTree->pStack;
        if( pStack->pNext ){
            pStack->pNext->pPrev = pStack;
        }
        pTree->pStack = pStack;
        pNode->pStack = pStack;
        pTree->cb.flags |= HTML_STACK;
    } else {
      pNode->pStack = HtmlNodeParent(pNode)->pStack;
    }
    assert(pNode->pStack);
}

void
HtmlRestackNodes(pTree)
    HtmlTree *pTree;
{
    HtmlNodeStack *pStack;
    int zoffset = 0;

    if (0 == (pTree->cb.flags & HTML_STACK)) return;

    for (pStack = pTree->pStack; pStack; pStack = pStack->pNext) {
        int z = zoffset;
        HtmlNode *pNode = pStack->pNode;
        for ( ; pNode; pNode = HtmlNodeParent(pNode)) {
            if (pNode->pPropertyValues->eFloat != CSS_CONST_NONE) {
                z += 1;
            }
            if (pNode->pPropertyValues->ePosition != CSS_CONST_STATIC) {
                int iZIndex = pNode->pPropertyValues->iZIndex;
                if ( iZIndex>=0 ) {
                    z += (25 * (iZIndex + 1));
                } else {
                    z += (25 * iZIndex);
                }
            }
        }
        zoffset = MIN(zoffset, z);
        pStack->iBlockZ = z;
    }
    for (pStack = pTree->pStack; pStack; pStack = pStack->pNext) {
        pStack->iBlockZ -= zoffset;
        pStack->iInlineZ = pStack->iBlockZ + 5;
        assert(pStack->iBlockZ >= 0);
        assert(pStack->iInlineZ >= 0);
    }

    pTree->cb.flags &= (~HTML_STACK);
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
        int redrawmode = 0;
        HtmlComputedValues *pV = pNode->pPropertyValues;
        pNode->pPropertyValues = 0;
        HtmlDelStackingInfo(pTree, pNode);

        /* If the clientData was set to a non-zero value, then the 
         * stylesheet configuration has changed. In this case we need to
         * recalculate the nodes list of dynamic conditions.
         */
        if (trashDynamics) {
            HtmlCssFreeDynamics(pNode);
        }
    
        /* If there is a "style" attribute on this node, parse the attribute
         * value and put the resulting mini-stylesheet in pNode->pStyle. 
         *
         * We assume that if the pStyle attribute is not NULL, then this node
         * has been styled before. The stylesheet configuration may have
         * changed since then, so we have to recalculate pNode->pProperties,
         * but the "style" attribute is constant so pStyle is never invalid.
         */
        if (!pNode->pStyle) {
            zStyle = HtmlNodeAttr(pNode, "style");
            if (zStyle) {
                HtmlCssParseStyle(-1, zStyle, &pNode->pStyle);
            }
        }
    
        /* Recalculate the properties for this node */
        HtmlCssStyleSheetApply(pTree, pNode);
        HtmlComputedValuesRelease(pTree, pNode->pPreviousValues);
        pNode->pPreviousValues = pV;

        redrawmode = HtmlComputedValuesCompare(pNode->pPropertyValues, pV);

        /* Regenerate any :before and :after content */
        if (pNode->pBefore || pNode->pAfter) {
            HtmlCallbackLayout(pTree, pNode);
            HtmlNodeClearGenerated(pTree, pNode);
            redrawmode = 2;
        }
        HtmlCssStyleSheetGenerated(pTree, pNode);
        if (pNode->pBefore || pNode->pAfter) {
            redrawmode = 2;
        }

        if (!pV || redrawmode == 2) {
            HtmlCallbackLayout(pTree, pNode);
        } else if (redrawmode == 1 && !(pTree->cb.flags & HTML_LAYOUT)) {
            int x, y, w, h;
            HtmlWidgetNodeBox(pTree, pNode, &x, &y, &w, &h);
            HtmlCallbackDamage(pTree,x-pTree->iScrollX,y-pTree->iScrollY,w,h,0);
        }

        addStackingInfo(pTree, pNode);

        if (pNode->pBefore) {
            pNode->pBefore->pStack = pNode->pStack;
            pNode->pBefore->pParent = pNode;
            pNode->pBefore->iNode = -1;
        }
        if (pNode->pAfter) {
            pNode->pAfter->pStack = pNode->pStack;
            pNode->pAfter->pParent = pNode;
            pNode->pAfter->iNode = -1;
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

