/*
 * HtmlTree.c ---
 *
 *     This file implements the tree structure that can be used to access
 *     elements of an HTML document.
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

static const char rcsid[] = "$Id: htmltree.c,v 1.89 2006/09/15 07:29:53 danielk1977 Exp $";

#include "html.h"
#include "swproc.h"
#include <assert.h>
#include <string.h>

#define NODE_EXT_IGNOREFORMS 0x00000001

#define NODE_EXT_NUMCHILDREN 1
#define NODE_EXT_CHILD       2

struct ExtCbContext {
    HtmlNode *pParent;
    int flags;
    int eType;
    int n;
    ClientData retval;
};
typedef struct ExtCbContext ExtCbContext;

static int 
extCb(pDummy, pNode, clientData)
    HtmlTree *pDummy;
    HtmlNode *pNode;
    ClientData clientData;
{
    ExtCbContext *pContext = (ExtCbContext *)clientData;
    if (
        (pNode == pContext->pParent) || ( 
            (pContext->flags & NODE_EXT_IGNOREFORMS) && 
            (HtmlNodeTagType(pNode) == Html_FORM)
        )
    ) {
        return HTML_WALK_DESCEND;
    } else {
        switch (pContext->eType) {
            case NODE_EXT_NUMCHILDREN:
                pContext->retval = (ClientData)((int)(pContext->retval) + 1);
                break; 
            case NODE_EXT_CHILD:
                if (pContext->n == 0) {
                    pContext->retval = (ClientData)pNode;
                    return HTML_WALK_ABANDON;
                }
                pContext->n--;
                break; 
        }
        return HTML_WALK_DO_NOT_DESCEND;
    }
}

static int 
nodeNumChildrenExt(pNode, flags)
    HtmlNode *pNode;
    int flags;
{
    ExtCbContext sContext;
    sContext.pParent = pNode;
    sContext.flags = flags;
    sContext.eType = NODE_EXT_NUMCHILDREN;
    sContext.retval = 0;
    HtmlWalkTree(0, pNode, extCb, &sContext);
    return (int)sContext.retval;
}

static HtmlNode * 
nodeChildExt(pNode, n, flags)
    HtmlNode *pNode;
    int n;
    int flags;
{
    ExtCbContext sContext;
    sContext.pParent = pNode;
    sContext.flags = flags;
    sContext.eType = NODE_EXT_CHILD;
    sContext.retval = 0;
    sContext.n = n;
    HtmlWalkTree(0, pNode, extCb, &sContext);
    return (HtmlNode *)sContext.retval;
}

/*
 *---------------------------------------------------------------------------
 *
 * moveToLeftSibling --
 *
 *     This function moves pNewSibling from whereever it is in the document
 *     tree and inserts it as the left sibling of node pNode. For example, if
 *     this function is called when the document tree looks like this:
 *
 *         <div>
 *           <table id=pNode>
 *             <tr>
 *               <td>...</td>
 *               <p id=pNewSibling>...</p>
 *
 *     it would be modified to the following:
 *
 *         <div>
 *           <p id=pNewSibling>...</p>
 *           <table id=pNode>
 *             <tr>
 *               <td>...</td>
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Modifies tree structure.
 *
 *---------------------------------------------------------------------------
 */
static void
moveToLeftSibling(pNode, pNewSibling)
    HtmlNode *pNode;
    HtmlNode *pNewSibling;
{
    HtmlNode *pOldParent = HtmlNodeParent(pNewSibling);
    HtmlNode *pNewParent = HtmlNodeParent(pNode);
    int i;
    int found = 0;

    assert(pOldParent && pNewParent);

    /* Remove pNewSibling from it's old parent */
    for (i = 0; i < HtmlNodeNumChildren(pOldParent); i++) {
        if (found) {
            pOldParent->apChildren[i - 1] = pOldParent->apChildren[i];
        } else if (HtmlNodeChild(pOldParent, i) == pNewSibling) {
            found = 1;
        }
    }
    assert(found);
    pOldParent->nChild--;

    /* Insert it into the new parent */
    pNewParent->apChildren = (HtmlNode **)HtmlRealloc(0, 
            pNewParent->apChildren, 
            sizeof(HtmlNode *) * (pNewParent->nChild + 1)
    );
    for (found = 0, i = HtmlNodeNumChildren(pNewParent) - 1; i >= 0; i--) {
        HtmlNode *pChild = HtmlNodeChild(pNewParent, i);
        if (!found) {
            pNewParent->apChildren[i + 1] = pChild;
        }
        if (pChild == pNode) {
            found = 1;
            pNewParent->apChildren[i] = pNewSibling;
            pNewSibling->pParent = pNewParent;
        }
    }
    assert(found);
    pNewParent->nChild++;
}

/*
 *---------------------------------------------------------------------------
 *
 * reworkTableNode --
 *
 *     Node *pNode is a <table> element. This function modifies the tree
 *     rooted at pNode so that the layout engine can handle the table 
 *     correctly.
 *
 *     The precise way in which this function manipulates the tree structure
 *     is documented as part of the "support.html" page of the website 
 *     (auto-generated from the webpage/mksupportpage.html file.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Modifies tree structure.
 *
 *---------------------------------------------------------------------------
 */
static void
reworkTableNode(pNode)
    HtmlNode *pNode;
{
    int i;
    int flags = NODE_EXT_IGNOREFORMS;

    assert(HtmlNodeTagType(pNode) == Html_TABLE);

    for (i = nodeNumChildrenExt(pNode, flags) - 1; i >= 0; i--) {
        HtmlNode *pChild = nodeChildExt(pNode, i, flags);
        int tag = HtmlNodeTagType(pChild);

        if (tag == Html_TR) {
            /* Any child of a <tr> that is not a <td> or <th> is 
             * moved to become a left-hand sibling of the <table>.
             */
            int j;
            for (j = nodeNumChildrenExt(pChild, flags) - 1; j >= 0; j--) {
                HtmlNode *pGrandChild = nodeChildExt(pChild, j, flags);
                int tag = HtmlNodeTagType(pGrandChild);
                if (tag != Html_TD && tag != Html_TH) {
                    moveToLeftSibling(pNode, pGrandChild);
                }
            }
        } else if (tag != Html_TD && tag != Html_TH) {
            /* Any child of the <table> element apart from <tr>, <td>, <th>
             * is moved to become a left-hand sibling of the <table>.
             */
            moveToLeftSibling(pNode, pChild);
        }
    }

    for (i = 0; i < nodeNumChildrenExt(pNode, flags); i++) {
        HtmlNode *pChild = nodeChildExt(pNode, i, flags);
        int tag = HtmlNodeTagType(pChild);
        assert(tag == Html_TR || tag == Html_TD || tag == Html_TH);
        if (tag != Html_TR) {
            /* A <td> or <th> element as a child of a <table>. Insert a
             * <tr> element between them. The <tr> element becomes the
             * parent of this table-cell and any others in the tree directly 
             * to the right.
             */
            HtmlToken *pRowToken;
            HtmlNode *pRowNode;

            int nMove;
            int j;
            for (j = i + 1; j < nodeNumChildrenExt(pNode, flags); j++) {
                HtmlNode *pSibling = nodeChildExt(pNode, j, flags);
                int tag2 = HtmlNodeTagType(pSibling);
                assert(tag2 == Html_TR || tag2 == Html_TD || tag2 == Html_TH);
                if (tag2 == Html_TR) break;
            }
            nMove = j - i;
            assert(nMove > 0);

            /* Create a token and link it into the token list just 
             * before it's first adopted child.
             */
            pRowToken = (HtmlToken *)HtmlClearAlloc(0, sizeof(HtmlToken));
            pRowToken->type = Html_TR;
            pRowToken->pNextToken = pChild->pToken;

            /* Create an HtmlNode for the new <tr> */
            pRowNode = (HtmlNode *)HtmlClearAlloc(0, sizeof(HtmlNode));
            pRowNode->pParent = pNode;
            pRowNode->pToken = pRowToken;
            pRowNode->nChild = nMove;
            pRowNode->apChildren = 
                (HtmlNode **)HtmlClearAlloc(0, sizeof(HtmlNode*) * nMove);
            for (j = 0; j < nMove; j++) {
                pRowNode->apChildren[j] = pNode->apChildren[i + j];
                pRowNode->apChildren[j]->pParent = pRowNode;
            }

            pNode->apChildren[i] = pRowNode;
            for (j = i + nMove; j < pNode->nChild; j++) {
                pNode->apChildren[j - (nMove - 1)] = pNode->apChildren[j];
            }
            pNode->nChild -= (nMove - 1);

            i += (nMove - 1);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * isExplicitClose --
 *
 *     Return true if tag is the explicit closing tag for pNode.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int isExplicitClose(pNode, tag)
    HtmlNode *pNode;
    int tag;
{
    if (tag != Html_Text && tag != Html_Space) {
        int opentag = pNode->pToken->type;
        return (tag==(opentag+1));
    }
    return 0;
}


/*
 *---------------------------------------------------------------------------
 *
 * tokenAction --
 *
 *     Figure out the effect on the document tree of the token in pToken.
 *
 * Results:
 *     True if the token creates a new node, false if it does not (i.e. if it
 *     is a closing tag).
 *
 * Side effects:
 *     May modify pTree->pCurrent.
 *
 *---------------------------------------------------------------------------
 */
static int 
tokenAction(pTree, pToken, pNClose)
    HtmlTree *pTree;
    HtmlToken *pToken;
    int *pNClose;
{
    HtmlNode *pCurrent = pTree->pCurrent;
    int nClose = 0;
    int nLevel = 0;
    int rc = 0;
    int tag = pToken->type;
    int seenImplicit = 0;

    if (tag == Html_Space) {
        HtmlToken *pT;
        for (pT = pToken; pT && pT->type == Html_Space; pT = pT->pNextToken);
        if (pT && pT->type == Html_Text) {
            tag = Html_Text;
        }
    }

    /* If pToken is NULL, this means the end of the token list has been
     * reached. Close all nodes and set pTree->pCurrent to NULL.
     */ 
    if (!pToken) {
        HtmlNode *p;
        for (p = pCurrent; p; p = HtmlNodeParent(p)) {
            nClose++;
        }
        pCurrent = 0;
    }

    else {
        HtmlNode *p = pCurrent;
        while (p) {
            HtmlTokenMap *pMap = HtmlMarkup(HtmlNodeTagType(p));
            int a;
            int isExplicit = 0;
            if (isExplicitClose(p, tag)) {
                a = TAG_CLOSE;
                isExplicit = 1;
            } else if (pMap && pMap->xClose) {
                a = pMap->xClose(pTree, p, tag);
            } else {
                a = TAG_PARENT;
            }

            if (seenImplicit && a == TAG_PARENT) {
                a = TAG_OK;
            }
            assert(!seenImplicit || a != TAG_CLOSE);

            switch (a) {
                case TAG_CLOSE:
                    assert(!seenImplicit);
                    p = HtmlNodeParent(p);
                    if (p) {
                        pCurrent = p;
                        nLevel++;
                    } else {
                        pCurrent = pTree->pRoot;
                    }
                    nClose = nLevel;
                    break;
                case TAG_OK:
                    p = 0;
                    break;
                case TAG_IMPLICIT:
                    seenImplicit = 1;
                    assert(HtmlNodeNumChildren(p) > 0);
                    p = HtmlNodeChild(p, HtmlNodeNumChildren(p) - 1);
                    assert(p);
                    pCurrent = p;
                    break;
                case TAG_PARENT:
                    nLevel++;
                    p = HtmlNodeParent(p);
                    break;

                default: assert(!"Impossible");
            }
            if (isExplicit) {
                p = 0;
            }
        }

        assert(!HtmlNodeIsText(pCurrent) || tag==Html_Space || tag==Html_Text);

        if (
            !(HTMLTAG_END & HtmlMarkupFlags(tag)) && 
            !HtmlNodeIsText(pCurrent)
        ) {
            assert(pCurrent);
            rc = 1;
        }
    }

    assert(!seenImplicit || rc);
    
    pTree->pCurrent = pCurrent;
    *pNClose = nClose;
    return rc;
}

static void
geomRequestProcCb(clientData) 
    ClientData clientData;
{
    HtmlNode *pNode = (HtmlNode *)clientData;
    HtmlTree *pTree = pNode->pNodeCmd->pTree;
    HtmlCallbackLayout(pTree, pNode);
}

static void 
geomRequestProc(clientData, widget)
    ClientData clientData;
    Tk_Window widget;
{
    HtmlNode *pNode = (HtmlNode *)clientData;
    HtmlTree *pTree = pNode->pNodeCmd->pTree;
    if (!pTree->cb.inProgress) {
        HtmlCallbackLayout(pTree, pNode);
    } else {
        Tcl_DoWhenIdle(geomRequestProcCb, (ClientData)pNode);
    }
}

static void
clearReplacement(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    HtmlNodeReplacement *p = pNode->pReplacement;
    pNode->pReplacement = 0;
    if (p) {

        /* Cancel any idle callback scheduled by geomRequestProc() */
        Tcl_CancelIdleCall(geomRequestProcCb, (ClientData)pNode);

        /* If there is a delete script, invoke it now. */
        if (p->pDelete) {
            int flags = TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL;
            Tcl_EvalObjEx(pTree->interp, p->pDelete, flags);
        }

	/* Remove any entry from the HtmlTree.pMapped list. */
        if (p == pTree->pMapped) {
            pTree->pMapped = p->pNext;
        } else {
            HtmlNodeReplacement *pCur = pTree->pMapped; 
            while( pCur && pCur->pNext != p ) pCur = pCur->pNext;
            if (pCur) {
                pCur->pNext = p->pNext;
            }
        }

        /* Cancel geometry management */
        if (p->win) {
            if (Tk_IsMapped(p->win)) {
                Tk_UnmapWindow(p->win);
            }
            Tk_ManageGeometry(p->win, 0, 0);
        }

        /* Delete the Tcl_Obj's and the structure itself. */
        if (p->pDelete) Tcl_DecrRefCount(p->pDelete);
        if (p->pReplace) Tcl_DecrRefCount(p->pReplace);
        if (p->pConfigure) Tcl_DecrRefCount(p->pConfigure);
        HtmlFree(0, (char *)p);
    }
}

int 
HtmlNodeClearStyle(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if (pNode) {
        HtmlNodeClearGenerated(pTree, pNode);
        HtmlComputedValuesRelease(pTree, pNode->pPropertyValues);
        HtmlComputedValuesRelease(pTree, pNode->pPreviousValues);
        HtmlCssPropertiesFree(pNode->pStyle);
        HtmlCssFreeDynamics(pNode);
        pNode->pStyle = 0;
        pNode->pPropertyValues = 0;
        pNode->pPreviousValues = 0;
        pNode->pDynamic = 0;
        HtmlDelStackingInfo(pTree, pNode);
    }
    return 0;
}

int 
HtmlNodeDeleteCommand(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if (pNode->pNodeCmd) {
        Tcl_Obj *pCommand = pNode->pNodeCmd->pCommand;
        Tcl_DeleteCommand(pTree->interp, Tcl_GetString(pCommand));
        Tcl_DecrRefCount(pCommand);
        HtmlFree(0, (char *)pNode->pNodeCmd);
        pNode->pNodeCmd = 0;
    }
    return 0;
}


/*
 *---------------------------------------------------------------------------
 *
 * freeNode --
 *
 *     Free the memory allocated for pNode and all of it's children. If the
 *     node has attached style information, either from stylesheets or an
 *     Html style attribute, this is deleted here too.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     pNode and children are made invalid.
 *
 *---------------------------------------------------------------------------
 */
static void 
freeNode(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if( pNode ){
        int i;
        HtmlToken *pToken;

        /* Invalidate the cache of the parent node before deleting any
         * child nodes. This is because invalidating a cache may involve
         * deleting primitives that correspond to descendant nodes. In
         * general, primitives must be deleted before their owner nodes.
         */
        HtmlLayoutInvalidateCache(pTree, pNode);

        /* Free the token representation */
        pToken = pNode->pToken;
        if (HtmlNodeIsText(pNode)) {
            while (pToken) {
                char *t = (char *)pToken;
                pToken = pToken->pNextToken;
                HtmlFree(0, t);
                if (
                    pToken && 
                    pToken->type != Html_Text && 
                    pToken->type != Html_Space
                ) {
                    break;
                }
            }
        } else {
            HtmlFree(0, (char *)pToken);
        }


        /* Delete the parsed tokens for this element */

        /* Delete the descendant nodes. */
        for(i=0; i<pNode->nChild; i++){
            freeNode(pTree, pNode->apChildren[i]);
        }
        HtmlFree(0, (char *)pNode->apChildren);

        /* Delete the computed values caches. */
        HtmlNodeClearStyle(pTree, pNode);

        /* Delete the computed values caches. */
        HtmlDelScrollbars(pTree, pNode);

        /* And the compiled cache of the node's "style" attribute, if any. */
        HtmlCssPropertiesFree(pNode->pStyle);

        if (pNode->pOverride) {
            Tcl_DecrRefCount(pNode->pOverride);
            pNode->pOverride = 0;
        }

        HtmlNodeDeleteCommand(pTree, pNode);

        clearReplacement(pTree, pNode);
        HtmlCssFreeDynamics(pNode);
        HtmlTagCleanupNode(pNode);
        HtmlFree(0, (char *)pNode);
    }
}

int
HtmlNodeClearGenerated(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    freeNode(pTree, pNode->pBefore);
    freeNode(pTree, pNode->pAfter);
    pNode->pBefore = 0;
    pNode->pAfter = 0;
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTreeFree --
 *
 *     Delete the internal tree representation.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Deletes the tree stored in p->pTree, if any. p->pTree is set to 0.
 *
 *---------------------------------------------------------------------------
 */
void HtmlTreeFree(pTree)
    HtmlTree *pTree;
{
    if( pTree->pRoot ){
        freeNode(pTree, pTree->pRoot);
    }
    pTree->pRoot = 0;
    pTree->pCurrent = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeHandlerCallbacks --
 *
 *     This is called for every tree node by HtmlWalkTree() immediately
 *     after the document tree is constructed. It calls the node handler
 *     script for the node, if one exists.
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
nodeHandlerCallbacks(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    Tcl_HashEntry *pEntry;
    int tag;
    Tcl_Interp *interp = pTree->interp;

    /* If the node is a <table> element, do the special processing before
     * invoking any node-handler callback. Precisely why anyone would use
     * a node-handler callback on a <table> element I'm not clear on.
     */
    tag = HtmlNodeTagType(pNode);
    if (tag == Html_TABLE) {
      reworkTableNode(pNode);
    }

    pEntry = Tcl_FindHashEntry(&pTree->aNodeHandler, (char *)tag);
    if (pEntry) {
        Tcl_Obj *pEval;
        Tcl_Obj *pScript;
        Tcl_Obj *pNodeCmd;
        int rc;

        pScript = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
        pEval = Tcl_DuplicateObj(pScript);
        Tcl_IncrRefCount(pEval);

        pNodeCmd = HtmlNodeCommand(pTree, pNode); 
        Tcl_ListObjAppendElement(0, pEval, pNodeCmd);
        rc = Tcl_EvalObjEx(interp, pEval, TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL);
        if (rc != TCL_OK) {
            Tcl_BackgroundError(interp);
        }

        Tcl_DecrRefCount(pEval);
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlFinishNodeHandlers --
 *
 *     Execute any outstanding node-handler callbacks. This is used when
 *     the end of a document is reached - the EOF implicitly closes all
 *     open nodes. This function executes node-handler scripts for nodes
 *     closed in such a fashion.
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
HtmlFinishNodeHandlers(pTree)
    HtmlTree *pTree;
{
    HtmlNode *p;
    for (p = pTree->pCurrent ; p; p = HtmlNodeParent(p)) {
        nodeHandlerCallbacks(pTree, p);
    }
    pTree->pCurrent = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeAddChild --
 *
 *     Add a new child node to node pNode. pToken becomes the starting
 *     token for the new node. The value returned is the index of the new
 *     child. So the call:
 *
 *          HtmlNodeChild(pNode, HtmlNodeAddChild(pNode, pToken))
 *
 *     returns the new child node.
 *
 * Results:
 *     Index of the child added to pNode.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlNodeAddChild(pNode, pToken)
    HtmlNode *pNode;
    HtmlToken *pToken;
{
    int n;             /* Number of bytes to alloc for pNode->apChildren */
    int r;             /* Return value */
    HtmlNode *pNew;    /* New child node */

    assert(pNode);
    assert(pToken);
    
    r = pNode->nChild++;
    n = (r+1) * sizeof(HtmlNode*);
    pNode->apChildren = (HtmlNode **)HtmlRealloc(0, (char *)pNode->apChildren, n);

    pNew = (HtmlNode *)HtmlAlloc(0, sizeof(HtmlNode));
    memset(pNew, 0, sizeof(HtmlNode));
    pNew->pToken = pToken;
    pNew->pParent = pNode;
    pNode->apChildren[r] = pNew;

    assert(r < pNode->nChild);
    return r;
}

/*
 *---------------------------------------------------------------------------
 *
 * setNodeAttribute --
 *
 *     Set the value of an attribute on a node. This function is currently
 *     a bit inefficient, due to the way the HtmlToken structure is 
 *     allocated.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     Modifies the HtmlToken structure associated with the specified node.
 *
 *---------------------------------------------------------------------------
 */
static void
setNodeAttribute(pNode, zAttrName, zAttrVal)
    HtmlNode *pNode;
    const char *zAttrName;
    const char *zAttrVal;
{
    HtmlToken *p = pNode->pToken;
    HtmlToken *pNew;
    int nArgs = 2;
    int nBytes = strlen(zAttrName) + 1 + strlen(zAttrVal) + 1;
    int i;
    int n = 0;
    char *zSpace;

    for (i = 0; i < p->count; i += 2) {
        if (0 != strcmp(p->x.zArgs[i], zAttrName)) {
            nArgs += 2;
            nBytes += strlen(p->x.zArgs[i]) + 1;
            nBytes += strlen(p->x.zArgs[i + 1]) + 1;
        }
    }

    nBytes += sizeof(HtmlToken) + nArgs * sizeof(char *);
    pNew = (HtmlToken *)HtmlClearAlloc(0, nBytes);
    pNew->type = p->type;
    pNew->count = nArgs;
    pNew->x.zArgs = (char **)&pNew[1];
    zSpace = (char *)&pNew->x.zArgs[nArgs];

    for (i = 0; i < p->count; i += 2) {
        if (0 != strcmp(p->x.zArgs[i], zAttrName)) {
            pNew->x.zArgs[n] = zSpace;
            strcpy(zSpace, p->x.zArgs[i]);
            while (*zSpace != '\0') zSpace++; zSpace++;
            pNew->x.zArgs[n + 1] = zSpace;
            strcpy(zSpace, p->x.zArgs[i + 1]);
            while (*zSpace != '\0') zSpace++; zSpace++;
            n += 2;
        }
    }
    pNew->x.zArgs[n] = zSpace;
    strcpy(zSpace, zAttrName);
    while (*zSpace != '\0') zSpace++; zSpace++;
    pNew->x.zArgs[n + 1] = zSpace;
    strcpy(zSpace, zAttrVal);
    while (*zSpace != '\0') zSpace++; zSpace++;
    n += 2;

    HtmlFree(0, p);
    pNode->pToken = pNew;
   
    assert(n == nArgs);
}

static void
mergeAttributes(pNode, pToken)
    HtmlNode *pNode;
    HtmlToken *pToken;
{
    int nBytes = 0;
    int nArgs = 0;
    int i;
    int n;
    HtmlToken *p = pNode->pToken;
    HtmlToken *pNew;
    char *zSpace;

    for (i = 0; i < pToken->count; i += 2) {
        if (!HtmlNodeAttr(pNode, pToken->x.zArgs[i])) {
            nBytes += strlen(pToken->x.zArgs[i]) + 1;
            nBytes += strlen(pToken->x.zArgs[i + 1]) + 1;
            nArgs += 2;
        }else{
            pToken->x.zArgs[i] = 0;
        }
    }
   
    nArgs += p->count;
    for (i = 0; i < p->count; i += 2) {
        nBytes += strlen(p->x.zArgs[i]) + 1;
        nBytes += strlen(p->x.zArgs[i + 1]) + 1;
    }

    nBytes += sizeof(HtmlToken) + nArgs * sizeof(char *);
    pNew = (HtmlToken *)HtmlClearAlloc(0, nBytes);
    pNew->type = p->type;
    pNew->count = nArgs;
    pNew->x.zArgs = (char **)&pNew[1];
    zSpace = (char *)&pNew->x.zArgs[nArgs];

    for (i = 0; i < p->count; i += 2) {
        pNew->x.zArgs[i] = zSpace;
        strcpy(zSpace, p->x.zArgs[i]);
        while (*zSpace != '\0') zSpace++; zSpace++;
        pNew->x.zArgs[i + 1] = zSpace;
        strcpy(zSpace, p->x.zArgs[i + 1]);
        while (*zSpace != '\0') zSpace++; zSpace++;
    }

    n = p->count;
    for (i = 0; i < pToken->count; i += 2) {
        if (pToken->x.zArgs[i]) {
            pNew->x.zArgs[n] = zSpace;
            strcpy(zSpace, pToken->x.zArgs[i]);
            while (*zSpace != '\0') zSpace++; zSpace++;
            pNew->x.zArgs[n + 1] = zSpace;
            strcpy(zSpace, pToken->x.zArgs[i + 1]);
            while (*zSpace != '\0') zSpace++; zSpace++;
            n += 2;
        }
    }

    HtmlFree(0, p);
    HtmlFree(0, pToken);
    pNode->pToken = pNew;
}

static int
doParseHandler(pTree, eType, pNode, iOffset)
    HtmlTree *pTree;
    int eType;
    HtmlNode *pNode;
    int iOffset;
{
    int rc = TCL_OK;
    Tcl_HashEntry *pEntry;
    if (iOffset < 0) return TCL_OK;

    if (eType == Html_Space) {
        eType = Html_Text;
    }

    pEntry = Tcl_FindHashEntry(&pTree->aParseHandler, (char *)eType);
    if (pEntry) {
        Tcl_Obj *pScript;
        pScript = (Tcl_Obj *)Tcl_GetHashValue(pEntry);

        pScript = Tcl_DuplicateObj(pScript);
        Tcl_IncrRefCount(pScript);
        if (pNode) {
            Tcl_ListObjAppendElement(0, pScript, HtmlNodeCommand(pTree, pNode));
        } else {
            Tcl_ListObjAppendElement(0, pScript, Tcl_NewStringObj("", -1));
        }
        Tcl_ListObjAppendElement(0, pScript, Tcl_NewIntObj(iOffset));

        rc = Tcl_EvalObjEx(pTree->interp, pScript, TCL_EVAL_GLOBAL);
        Tcl_DecrRefCount(pScript);
    }

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlAddToken --
 *
 *     Update the tree structure with token pToken.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May modify the tree structure at HtmlTree.pRoot and
 *     HtmlTree.pCurrent.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlAddToken(pTree, pToken, iOffset)
    HtmlTree *pTree;
    HtmlToken *pToken;
    int iOffset;
{
    HtmlNode *pCurrent = pTree->pCurrent;
    HtmlNode *pHeadNode;
    int eType = pToken->type;

    /* If token pToken causes a node to be added to the tree, or the
     * attributes of an <html>, <body> or <head> element to be updated,
     * this variable is set to point to the node. At the end of this
     * function, it is used as an argument to any registered 
     * [$widget handler parse] callback script.
     */
    HtmlNode *pParsed = 0; 

    assert((pCurrent && pTree->pRoot) || !pCurrent);
    if (!pCurrent && pTree->pRoot) {
        pCurrent = pTree->pRoot;
    }

    /* Variable HtmlTree.pCurrent is only manipulated by this function (not
     * entirely true - it is also set to zero when the tree is deleted). It
     * stores the node currently being constructed. All things being equal,
     * if the next token parsed is an opening tag (i.e. "<strong>"), then
     * it will create a new node that becomes the right-most child of
     * pCurrent.
     *
     * From the point of view of building the tree, the token pToken may
     * fall into one of three categories:
     *
     *     1. A text token (a token of type Html_Text or Html_Space). If
     *        pCurrent is a text node, then nothing need be done. Otherwise,
     *        the token starts a new node as the right-most child of
     *        pCurrent.
     *
     *     2. An explicit closing tag (i.e. </strong>). This may close
     *        pCurrent and zero or more of it's ancestors (it also may close
     *        no tags at all)
     *
     *     3. An opening tag (i.e. <strong>). This may close pCurrent and
     *        zero or more of it's ancestors. It also creates a new node, as
     *        the right-most child of pCurrent or an ancestor.
     *
     * As well as the above three, the trivial case of an empty tree is
     * handled seperately.
     */

    if (!pCurrent) {
        /* If pCurrent is NULL, then this is the first token in the
         * document. If the document is well-formed, an <html> tag (Html
         * documents may have a DOCTYPE and other useless garbage in them,
         * but the tokenizer should ignore all that).
         *
         * But in these uncertain times you really can't trust anyone, so
         * Tkhtml automatically inserts the following structure at the root
         * of every document:
         *
         *    <html>
         *      <head>
         *      </head>
         *      <body>
         */
        HtmlToken *pHtml = (HtmlToken *)HtmlClearAlloc(0, sizeof(HtmlToken));
        HtmlToken *pHead = (HtmlToken *)HtmlClearAlloc(0, sizeof(HtmlToken));
        HtmlToken *pBody = (HtmlToken *)HtmlClearAlloc(0, sizeof(HtmlToken));

        pHtml->type = Html_HTML;
        pHead->type = Html_HEAD;
        pBody->type = Html_BODY;

        pCurrent = (HtmlNode *)HtmlClearAlloc(0, sizeof(HtmlNode));
        pCurrent->pToken = pHtml;
        pTree->pRoot = pCurrent;
        pTree->pCurrent = pCurrent;

        HtmlNodeAddChild(pCurrent, pHead);
        HtmlNodeAddChild(pCurrent, pBody);
        pCurrent = pTree->pRoot->apChildren[1];
        assert(HtmlNodeTagType(pCurrent) == Html_BODY);
    } 
    pHeadNode = pTree->pRoot->apChildren[0];

    if (pTree->isCdataInHead && eType != Html_Text && eType != Html_Space) {
        int nChild = HtmlNodeNumChildren(pHeadNode) - 1;
        HtmlNode *pTitle = HtmlNodeChild(pHeadNode, nChild);
        pTree->isCdataInHead = 0;
        nodeHandlerCallbacks(pTree, pTitle);
        return;   /* TODO: Should this be removed? */
    }

    switch (eType) {
        case Html_HTML:
            pParsed = pTree->pRoot;
            mergeAttributes(pParsed, pToken);
            break;
        case Html_HEAD:
            pParsed = pHeadNode;
            mergeAttributes(pParsed, pToken);
            break;
        case Html_BODY:
            pParsed = pTree->pRoot->apChildren[1];
            mergeAttributes(pParsed, pToken);
            break;

            /* Elements with content #CDATA for the document head. 
             *
	     * Todo: Technically, we should be worried about <script> and
	     * <style> elements in the document head too, but in practice it
	     * makes little difference where these wind up. <script> is
	     * a bit tricky as this can appear in either the <head> or <body>
	     * section.
             */
        case Html_TITLE: {
            int n = HtmlNodeAddChild(pHeadNode, pToken);
            HtmlNode *p = HtmlNodeChild(pHeadNode, n);
            pTree->isCdataInHead = 1;
            p->iNode = pTree->iNextNode++;
            pParsed = p;
            break;
        }

            /* Self-closing elements to add to the document head */
        case Html_META:
        case Html_LINK:
        case Html_BASE: {
            int n = HtmlNodeAddChild(pHeadNode, pToken);
            HtmlNode *p = HtmlNodeChild(pHeadNode, n);
            p->iNode = pTree->iNextNode++;
            nodeHandlerCallbacks(pTree, p);
            pParsed = p;
            break;
        }


        case Html_Text:
        case Html_Space:
            if (pTree->isCdataInHead) {
                int nChild = HtmlNodeNumChildren(pHeadNode) - 1;
                HtmlNode *pTitle = HtmlNodeChild(pHeadNode, nChild);
                HtmlNode *p = HtmlNodeChild(pTitle,
                    HtmlNodeAddChild(pTitle,pToken));
                p->iNode = pTree->iNextNode++;
                pTree->isCdataInHead = 0;
                nodeHandlerCallbacks(pTree, pTitle);
                pParsed = p;
            }else{
                int n = HtmlNodeAddChild(pCurrent,pToken);
                HtmlNode *p = HtmlNodeChild(pCurrent, n);
                p->iNode = pTree->iNextNode++;
                pParsed = p;
            }
            break;

        case Html_EndHTML:
        case Html_EndBODY:
        case Html_EndHEAD:
            HtmlFree(0, pToken);
            break;

        default: {
            int nClose = 0;
            int i;
            int r = tokenAction(pTree, pToken, &nClose);

            for (i = 0; i < nClose; i++) {
                nodeHandlerCallbacks(pTree, pCurrent);
                pCurrent = HtmlNodeParent(pCurrent);
            }

#ifndef NDEBUG
            {
                HtmlNode *pTmp = pCurrent;
                assert(r || pTmp == pTree->pCurrent);
                while (pTmp != pCurrent) {
                    assert(HtmlNodeNumChildren(pTmp) > 0);
                    pTmp = HtmlNodeChild(pTmp, HtmlNodeNumChildren(pTmp) - 1);
                }
            }
#endif

            pCurrent = pTree->pCurrent;
            if (r) {
                assert(!HtmlNodeIsText(pTree->pCurrent));
                pCurrent = HtmlNodeChild(pCurrent, 
                    HtmlNodeAddChild(pCurrent, pToken));
                pCurrent->iNode = pTree->iNextNode++;
                pParsed = pCurrent;

                if (HtmlMarkupFlags(eType) & HTMLTAG_EMPTY) {
                    nodeHandlerCallbacks(pTree, pCurrent);
                    pCurrent = HtmlNodeParent(pCurrent);
                }
            } else {
                HtmlFree(0, pToken);
            }
        }
    }

    doParseHandler(pTree, eType, pParsed, iOffset);

    pTree->pCurrent = pCurrent;
}

/*
 *---------------------------------------------------------------------------
 *
 * walkTree --
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
walkTree(pTree, xCallback, pNode, clientData)
    HtmlTree *pTree;
    int (*xCallback)(HtmlTree *, HtmlNode *, ClientData clientData);
    HtmlNode *pNode;
    ClientData clientData;
{
    int i;
    if( pNode ){
        int rc = xCallback(pTree, pNode, clientData);
        switch (rc) {
            case HTML_WALK_ABANDON:
                return 1;
            case HTML_WALK_DESCEND:
                break;
            case HTML_WALK_DO_NOT_DESCEND:
                return 0;
            default:
                    assert(!"Bad return value from HtmlWalkTree() callback");
        }

        for (i = 0; i < pNode->nChild; i++) {
            HtmlNode *pChild = pNode->apChildren[i];
            int rc = walkTree(pTree, xCallback, pChild, clientData);
            assert(HtmlNodeParent(pChild) == pNode);
            if (rc) return rc;
        }
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWalkTree --
 *
 *     Traverse the subset of document tree pTree rooted at pNode. If pNode is
 *     NULL the entire tree is traversed. This function does a pre-order or
 *     prefix traversal (each node is visited before it's children).
 *
 *     When a node is visited the supplied callback function is invoked. The
 *     callback function must return one of the following three hash
 *     defined values:
 *
 *         HTML_WALK_DESCEND
 *         HTML_WALK_DO_NOT_DESCEND
 *         HTML_WALK_ABANDON
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
HtmlWalkTree(pTree, pNode, xCallback, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int (*xCallback)(HtmlTree *, HtmlNode *, ClientData clientData);
    ClientData clientData;
{
    return walkTree(pTree, xCallback, pNode?pNode:pTree->pRoot, clientData);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeNumChildren --
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
int HtmlNodeNumChildren(pNode)
    HtmlNode *pNode;
{
    return pNode->nChild;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeChild --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode * 
HtmlNodeChild(pNode, n)
    HtmlNode *pNode;
    int n;
{
    if (!pNode || pNode->nChild<=n) return 0;
    return pNode->apChildren[n];
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeIsText --
 *
 *     Return non-zero if the node is a (possibly empty) text node, or zero
 *     otherwise.
 *
 * Results:
 *     Non-zero if the node is text, else zero.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlNodeIsText(pNode)
    HtmlNode *pNode;
{
    int type = HtmlNodeTagType(pNode);
    return (type==Html_Text || type==Html_Space);
}

int 
HtmlNodeIsWhitespace(pNode)
    HtmlNode *pNode;
{
    HtmlToken *p;
    if (!HtmlNodeIsText(pNode)) {
        return 0;
    }

    for (p = pNode->pToken; p && p->type == Html_Space; p = p->pNextToken);
    if (p && p->type == Html_Text) {
        return 0;
    }

    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeTagType --
 *
 *     Return the tag-type of the node, i.e. Html_P, Html_Text or
 *     Html_Space.
 *
 * Results:
 *     Integer tag type.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Html_u8 HtmlNodeTagType(pNode)
    HtmlNode *pNode;
{
    if (pNode && pNode->pToken) {
        return pNode->pToken->type;
    } 
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeTagName --
 *
 *     Return the name of the tag-type of the node, i.e. "p", "text" or
 *     "div".
 *
 * Results:
 *     Boolean.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
CONST char * HtmlNodeTagName(pNode)
    HtmlNode *pNode;
{
    if (pNode && pNode->pToken) {
        return HtmlMarkupName(pNode->pToken->type);
    } 
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeRightSibling --
 * 
 *     Get the right-hand sibling to a node, if it has one.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode *HtmlNodeRightSibling(pNode)
    HtmlNode *pNode;
{
    HtmlNode *pParent = pNode->pParent;
    if( pParent ){
        int i;
        for (i=0; i<pParent->nChild-1; i++) {
            if (pNode==pParent->apChildren[i]) {
                return pParent->apChildren[i+1];
            }
        }
        assert(pNode==pParent->apChildren[pParent->nChild-1]);
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeLeftSibling --
 * 
 *     Get the left-hand sibling to a node, if it has one.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode *HtmlNodeLeftSibling(pNode)
    HtmlNode *pNode;
{
    HtmlNode *pParent = pNode->pParent;
    if( pParent ){
        int i;
        for (i = 1; i < pParent->nChild; i++) {
            if (pNode == pParent->apChildren[i]) {
                return pParent->apChildren[i-1];
            }
        }
        assert(pNode==pParent->apChildren[0]);
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeParent --
 *
 *     Get the parent of the current node.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode *HtmlNodeParent(pNode)
    HtmlNode *pNode;
{
    return pNode?pNode->pParent:0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeAttr --
 *
 *     Return a pointer to the value of node attribute zAttr. Attributes
 *     are always represented as NULL-terminated strings.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
char CONST *HtmlNodeAttr(pNode, zAttr)
    HtmlNode *pNode; 
    char CONST *zAttr;
{
    if (pNode) {
        return HtmlMarkupArg(pNode->pToken, zAttr, 0);
    }
    return 0;
}

static int 
markWindowAsClipped(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    HtmlNodeReplacement *p = pNode->pReplacement;
    if (p) {
        p->clipped = 1;
    }

    return HTML_WALK_DESCEND;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeViewCmd --
 *
 *     This function implements the Tcl commands:
 *
 *         [nodeHandle yview] 
 *         [nodeHandle xview]
 *
 *     used to scroll boxes generated by tree elements with "overflow:auto"
 *     or "overflow:scroll". At present, the implementation of this is
 *     not very efficient.
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
nodeViewCmd(pNode, isVertical, objv, objc)
    HtmlNode *pNode;
    int isVertical;
    Tcl_Obj *CONST objv[];
    int objc;
{
    HtmlTree *pTree;
    int eType;       /* One of the TK_SCROLL_ symbols */
    double fraction;
    int count;

    int iNew;
    int iMax;
    int iSize;
    int iIncr;

    int x, y, w, h;

    if (!pNode->pScrollbar) {
        return TCL_ERROR;
    }

    pTree = pNode->pNodeCmd->pTree;
    if (isVertical) {
        iNew = pNode->pScrollbar->iVertical;
        iMax = pNode->pScrollbar->iVerticalMax;
        iSize = pNode->pScrollbar->iHeight;
        iIncr = pTree->options.yscrollincrement;
    } else {
        iNew = pNode->pScrollbar->iHorizontal;
        iMax = pNode->pScrollbar->iHorizontalMax;
        iSize = pNode->pScrollbar->iWidth;
        iIncr = pTree->options.xscrollincrement;
    }

    eType = Tk_GetScrollInfoObj(pTree->interp, objc, objv, &fraction, &count);

    switch (eType) {
        case TK_SCROLL_MOVETO:
            iNew = (int)((double)iMax * fraction);
            break;
        case TK_SCROLL_PAGES: /* TODO */
            iNew += count * (0.9 * iSize);
            break;
        case TK_SCROLL_UNITS: /* TODO */
            iNew += count * iIncr;
            break;
        case TK_SCROLL_ERROR:
            return TCL_ERROR;

        default: assert(!"Not possible");
    }

    iNew = MAX(0, iNew);
    iNew = MIN(iNew, iMax - iSize);
    if (isVertical) {
        pNode->pScrollbar->iVertical = iNew;
    } else {
        pNode->pScrollbar->iHorizontal = iNew;
    }

    HtmlNodeScrollbarDoCallback(pNode->pNodeCmd->pTree, pNode);
    HtmlWidgetNodeBox(pTree, pNode, &x, &y, &w, &h);
    HtmlCallbackDamage(pTree, x - pTree->iScrollX, y - pTree->iScrollY, w, h,0);
    pTree->cb.flags |= HTML_NODESCROLL;
    HtmlWalkTree(pTree, pNode, markWindowAsClipped, 0);

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeCommand --
 *
 *         <node> attr ?options? HTML-ATTRIBUTE-NAME
 *         <node> child CHILD-NUMBER 
 *         <node> nChildren 
 *         <node> parent
 *         <node> prop
 *         <node> replace ?options? ?NEW-VALUE?
 *         <node> tag
 *         <node> text
 *
 *         <node> override
 *
 *     This function is the implementation of the Tcl node handle command. The
 *     clientData passed to the command is a pointer to the HtmlNode structure
 *     for the document node. 
 *
 *     When this function is called, ((HtmlNode *)clientData)->pNodeCmd is 
 *     guaranteed to point to a valid HtmlNodeCmd structure.
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
nodeCommand(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST objv[];
{
    HtmlNode *pNode = (HtmlNode *)clientData;
    HtmlTree *pTree = pNode->pNodeCmd->pTree;
    int choice;

    static CONST char *NODE_strs[] = {
        "attr",                  /* Read/write node attributes */
        "children",              /* Return a list of the nodes child nodes */
        "dynamic",               /* Set/clear dynamic flags (i.e. :hover) */
        "override",              /* Read/write CSS property overrides */
        "parent",                /* Return the parent node */
        "prop",                  /* Query CSS property values */
        "property",              /* Query a single CSS property value */
        "replace",               /* Set/clear the node replacement object */
        "tag",                   /* Read/write the node's tag */
        "text",                  /* Read/write the node's text content */
        "xview",                 /* Scroll a scrollable node horizontally */
        "yview",                 /* Scroll a scrollable node vertically */
        0
    };
    enum NODE_enum {
        NODE_ATTR, NODE_CHILDREN, NODE_DYNAMIC, NODE_OVERRIDE,
        NODE_PARENT, NODE_PROP, NODE_PROPERTY, NODE_REPLACE, 
        NODE_TAG, NODE_TEXT, NODE_XVIEW, NODE_YVIEW
    };

    if (objc<2) {
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], NODE_strs, "option", 0, &choice) ){
        return TCL_ERROR;
    }

    switch ((enum NODE_enum)choice) {
        /*
         * nodeHandle attr ??-default DEFAULT-VALUE? ATTR-NAME? ?NEW-VALUE?
         */
        case NODE_ATTR: {
            char CONST *zAttr = 0;
            char *zAttrName = 0;
            char *zAttrVal = 0;
            char *zDefault = 0;

            switch (objc) {
                case 2:
                    break;
                case 3:
                    zAttrName = Tcl_GetString(objv[2]);
                    break;
                case 4:
                    zAttrName = Tcl_GetString(objv[2]);
                    zAttrVal = Tcl_GetString(objv[3]);
                    break;
                case 5:
                    if (strcmp(Tcl_GetString(objv[2]), "-default")) {
                        goto node_attr_usage;
                    }
                    zDefault = Tcl_GetString(objv[3]);
                    zAttrName = Tcl_GetString(objv[4]);
                    break;
                default:
                    goto node_attr_usage;
            }

            /* If there are values for both zAttrName and zAttrVal, then
             * set the value of the attribute to the string pointed to by 
             * zAttrVal. After doing this, run the code for an attribute
             * query, so that the new attribute value is returned.
             */
            if (zAttrName && zAttrVal) {
                assert(!zDefault);
                setNodeAttribute(pNode, zAttrName, zAttrVal);
                HtmlCallbackRestyle(pTree, pNode);
            }

            if (zAttrName) {
                zAttr = HtmlNodeAttr(pNode, zAttrName);
                zAttr = (zAttr ? zAttr : zDefault);
                if (zAttr==0) {
                    Tcl_AppendResult(interp, "No such attr: ", zAttrName, 0);
                    return TCL_ERROR;
                }
                Tcl_SetResult(interp, (char *)zAttr, TCL_VOLATILE);
            } else 

            if (!HtmlNodeIsText(pNode)) {
                int i;
                HtmlToken *pToken = pNode->pToken;
                Tcl_Obj *p = Tcl_NewObj();
                for (i = 2; i < pToken->count; i++) {
                    Tcl_Obj *pArg = Tcl_NewStringObj(pToken->x.zArgs[i], -1);
                    Tcl_ListObjAppendElement(interp, p, pArg);
                }
                Tcl_SetObjResult(interp, p);
            }
            break;

node_attr_usage:
            Tcl_ResetResult(interp);
            Tcl_AppendResult(interp, "Usage: ",
                Tcl_GetString(objv[0]), " ",
                Tcl_GetString(objv[1]), " ",
                "? ?-default DEFAULT-VALUE? ATTR-NAME ?NEW-VAL??", 0);
            return TCL_ERROR;
        }

        /*
         * nodeHandle children
         *
	 *     Return a list of node handles for all children of nodeHandle.
	 *     The leftmost child node becomes element 0 of the list, the
	 *     second leftmost element 1, and so on.
         */
        case NODE_CHILDREN: {
            if (objc == 2) {
                int i;
                Tcl_Obj *pRes = Tcl_NewObj();
                for (i = 0; i < HtmlNodeNumChildren(pNode); i++) {
                    HtmlNode *pChild = HtmlNodeChild(pNode, i);
                    Tcl_Obj *pCmd = HtmlNodeCommand(pTree, pChild);
                    Tcl_ListObjAppendElement(0, pRes, pCmd);
                }
                Tcl_SetObjResult(interp, pRes);
            } else {
                Tcl_WrongNumArgs(interp, 2, objv, "");
                return TCL_ERROR;
            }
            break;
        }

        case NODE_TAG: {
            char CONST *zTag;
            if (objc!=2) {
                Tcl_WrongNumArgs(interp, 2, objv, "");
                return TCL_ERROR;
            }
            zTag = HtmlMarkupName(HtmlNodeTagType(pNode));
            Tcl_SetResult(interp, (char *)zTag, TCL_VOLATILE);
            break;
        }

        /*
         * nodeHandle text ?-tokens || -pre?
         *
         */
        case NODE_TEXT: {
            HtmlToken *pT;
            Tcl_Obj *pRet;
            int tokens;
            int pre;
            char *z3 = 0;
            if (objc == 3) {
                z3 = Tcl_GetString(objv[2]);
            }

            if (
                (objc != 2 && objc != 3) ||
                (objc == 3 && strcmp(z3, "-tokens") && strcmp(z3, "-pre"))
            ) {
                Tcl_WrongNumArgs(interp, 2, objv, "?-tokens || -pre?");
                return TCL_ERROR;
            }

            tokens = ((objc == 3 && z3[1]=='t') ? 1 : 0);
            pre =    ((objc == 3 && z3[1]=='p') ? 1 : 0);
            pRet = Tcl_NewObj();
            Tcl_IncrRefCount(pRet);
            for (
                pT = pNode->pToken;
                pT && (pT->type==Html_Space || pT->type==Html_Text);
                pT = pT->pNextToken
            ) {
                if (pT->type==Html_Text) {
                    if (tokens) {
                        Tcl_Obj *pObj = Tcl_NewObj();
                        Tcl_ListObjAppendElement(0, pObj, 
                                Tcl_NewStringObj("text", -1)
                        );
                        Tcl_ListObjAppendElement(0, pObj, 
                                Tcl_NewStringObj(pT->x.zText, pT->count)
                        );
                        Tcl_ListObjAppendElement(interp, pRet, pObj);
                    } else {
                        Tcl_AppendToObj(pRet, pT->x.zText, pT->count);
                    }
                } else {
                    if (tokens) {
                        if (pT->x.newline) {
                            Tcl_Obj *pObj = Tcl_NewStringObj("newline", -1);
                            Tcl_ListObjAppendElement(interp, pRet, pObj);
                        } else {
                            Tcl_Obj *pObj;
                            char zBuf[128];
                            sprintf(zBuf, "space %d", pT->count);
                            pObj = Tcl_NewStringObj(zBuf, -1);
                            Tcl_ListObjAppendElement(interp, pRet, pObj);
                        }
                    } else if (pre) {
                        char *zWhite = "\n";
                        char nWhite = 1;
                        if (0 == pT->x.newline) {
                            zWhite = "                                        ";
                            assert(strlen(zWhite) == 40);
                            for (nWhite = pT->count; nWhite > 40; nWhite -= 40){
                                Tcl_AppendToObj(pRet, zWhite, 40);
                            }
                        }
                        Tcl_AppendToObj(pRet, zWhite, nWhite);
                    } else {
                        Tcl_AppendToObj(pRet, " ", 1);
                    }
                }
            }

            Tcl_SetObjResult(interp, pRet);
            Tcl_DecrRefCount(pRet);
            break;
        }

        case NODE_PARENT: {
            HtmlNode *pParent;
            pParent = HtmlNodeParent(pNode);
            if (pParent) {
                Tcl_SetObjResult(interp, HtmlNodeCommand(pTree, pParent));
            } 
            break;
        }

        /*
         * nodeHandle prop ?PSEUDO-ELEMENT?
         *
         *     Argument may be "after" or "before".
         */
        case NODE_PROP: {
            HtmlNode *pN = pNode;
            if (HtmlNodeIsText(pN)) {
                pN = HtmlNodeParent(pNode);
            }
            HtmlCallbackForce(pTree);
            if (0 == pN->pPropertyValues) {
                Tcl_ResetResult(interp);
                Tcl_AppendResult(interp,"Computed values cannot be obtained",0);
                return TCL_ERROR;
            }

            if (objc == 3) {
                const char *zPseudo = Tcl_GetString(objv[2]);
                if (strcmp(zPseudo, "after") == 0 && pN->pAfter) {
                    HtmlNodeProperties(interp, pN->pAfter->pPropertyValues);
                } else if (strcmp(zPseudo, "before") == 0 && pN->pBefore) {
                    HtmlNodeProperties(interp, pN->pBefore->pPropertyValues);
                } else {
                    Tcl_ResetResult(interp);
                    Tcl_AppendResult(interp, "No such pseudo-element: ", 0);
                    Tcl_AppendResult(interp, zPseudo, 0);
                    return TCL_ERROR;
                }
            } else {
                HtmlNodeProperties(interp, pN->pPropertyValues);
            }

            break;
        }

        /*
         * nodeHandle property PROPERTY-NAME
         *
         *     Return the calculated value of a node's CSS property. If the
         *     node is a text node, return the value of the property as
         *     assigned to the parent node.
         */
        case NODE_PROPERTY: {
            HtmlNode *pN = pNode;
            if (HtmlNodeIsText(pN)) {
                pN = HtmlNodeParent(pNode);
            }
            if (objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "PROPERTY-NAME");
                return TCL_ERROR;
            }
            HtmlCallbackForce(pTree);
            return HtmlNodeGetProperty(interp, objv[2], pN->pPropertyValues);
        }

        /*
         * nodeHandle replace ?new-value? ?options?
         *
         *     supported options are:
         *
         *         -configurecmd       <script>
         *         -deletecmd          <script>
         */
        case NODE_REPLACE: {
            if (objc > 2) {
                Tcl_Obj *aArgs[4];
                HtmlNodeReplacement *pReplace = 0; /* New pNode->pReplacement */
                int nBytes;                  /* bytes allocated at pReplace */
                Tk_Window widget;            /* Replacement widget */
                Tk_Window mainwin = Tk_MainWindow(pTree->interp);

                const char *zWin = 0;        /* Replacement window name */

                SwprocConf aArgConf[] = {
                    {SWPROC_ARG, "new-value", 0, 0},      /* aArgs[0] */
                    {SWPROC_OPT, "configurecmd", 0, 0},   /* aArgs[1] */
                    {SWPROC_OPT, "deletecmd", 0, 0},      /* aArgs[2] */
                    {SWPROC_OPT, "stylecmd", 0, 0},       /* aArgs[3] */
                    {SWPROC_END, 0, 0, 0}
                };
                if (SwprocRt(interp, objc - 2, &objv[2], aArgConf, aArgs)) {
                    return TCL_ERROR;
                }

                zWin = Tcl_GetString(aArgs[0]);

                if (zWin[0]) {
		    /* If the replacement object is a Tk window,
                     * register Tkhtml as the geometry manager.
                     */
                    widget = Tk_NameToWindow(interp, zWin, mainwin);
                    if (widget) {
                        static Tk_GeomMgr sManage = {
                            "Tkhtml",
                            geomRequestProc,
                            0
                        };
                        Tk_ManageGeometry(widget, &sManage, pNode);
                    }

                    nBytes = sizeof(HtmlNodeReplacement);
                    pReplace = (HtmlNodeReplacement *)HtmlClearAlloc(0, nBytes);
                    pReplace->pReplace = aArgs[0];
                    pReplace->pConfigure = aArgs[1];
                    pReplace->pDelete = aArgs[2];
                    pReplace->pStyle = aArgs[3];
                    pReplace->win = widget;
                }

		/* Free any existing replacement object and set
		 * pNode->pReplacement to point at the new structure. 
                 */
                clearReplacement(pTree, pNode);
                pNode->pReplacement = pReplace;

                /* Run the layout engine. */
                HtmlCallbackLayout(pTree, pNode);
            }

            /* The result of this command is the name of the current
             * replacement object (or an empty string).
             */
            if (pNode->pReplacement) {
                assert(pNode->pReplacement->pReplace);
                Tcl_SetObjResult(interp, pNode->pReplacement->pReplace);
            }
            break;
        }

        /*
         * nodeHandle dynamic set|clear ?flag?
         * nodeHandle dynamic conditions
         *
	 *     Note that the [nodeHandle dynamic conditions] command is for
	 *     debugging only. It is not documented in the man page.
         */
        case NODE_DYNAMIC: {
            struct DynamicFlag {
                const char *zName;
                Html_u8 flag;
            } flags[] = {
                {"active",  HTML_DYNAMIC_ACTIVE}, 
                {"focus",   HTML_DYNAMIC_FOCUS}, 
                {"hover",   HTML_DYNAMIC_HOVER},
                {"link",    HTML_DYNAMIC_LINK},
                {"visited", HTML_DYNAMIC_VISITED},
                {0, 0}
            };
            const char *zArg1 = (objc>2) ? Tcl_GetString(objv[2]) : 0;
            const char *zArg2 = (objc>3) ? Tcl_GetString(objv[3]) : 0;
            Tcl_Obj *pRet;
            int i;
            Html_u8 mask = 0;

            if (zArg1 && 0 == strcmp(zArg1, "conditions")) {
                HtmlCallbackForce(pTree);
                return HtmlCssTclNodeDynamics(interp, pNode);
            }

            if (zArg2) {
                for (i = 0; flags[i].zName; i++) {
                    if (0 == strcmp(zArg2, flags[i].zName)) {
                        mask = flags[i].flag;
                    }
                }
                if (!mask) {
                    Tcl_AppendResult(interp, 
                        "Unsupported dynamic CSS flag: ", zArg2, 0);
                    return TCL_ERROR;
                }
            }

            if ( 
                !zArg1 || 
                (strcmp(zArg1, "set") && strcmp(zArg1, "clear")) ||
                (zArg2 && !mask)
            ) {
                Tcl_WrongNumArgs(interp, 2, objv, "set|clear ?flag?");
                return TCL_ERROR;
            }

            if (*zArg1 == 's') {
                pNode->flags |= mask;
            } else {
                pNode->flags &= ~(mask?mask:0xFF);
            }

            pRet = Tcl_NewObj();
            for (i = 0; flags[i].zName; i++) {
                if (pNode->flags & flags[i].flag) {
                    Tcl_Obj *pNew = Tcl_NewStringObj(flags[i].zName, -1);
                    Tcl_ListObjAppendElement(0, pRet, pNew);
                }
            }
            Tcl_SetObjResult(interp, pRet);

            HtmlCallbackDynamic(pTree, pNode);
            break;
        }

        /*
         * nodeHandle override ?new-value?
         *
         *     Get/set the override list.
         */
        case NODE_OVERRIDE: {
            if (objc != 2 && objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "?new-value?");
                return TCL_ERROR;
            }

            if (objc == 3) {
                if (pNode->pOverride) {
                    Tcl_DecrRefCount(pNode->pOverride);
                }
                pNode->pOverride = objv[2];
                Tcl_IncrRefCount(pNode->pOverride);
            }

            Tcl_ResetResult(interp);
            if (pNode->pOverride) {
                Tcl_SetObjResult(interp, pNode->pOverride);
            }
            HtmlCallbackRestyle(pTree, pNode);
            return TCL_OK;
        }

        case NODE_XVIEW: {
            return nodeViewCmd(pNode, 0, objv, objc);
        }
        case NODE_YVIEW: {
            return nodeViewCmd(pNode, 1, objv, objc);
        }

        default:
            assert(!"Impossible!");
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeCommand --
 *
 *     Return a Tcl object containing the name of the Tcl command used to
 *     access pNode. If the command does not already exist it is created.
 *
 *     The Tcl_Obj * returned is always a pointer to pNode->pCommand.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj *
HtmlNodeCommand(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    static int nodeNumber = 0;
    HtmlNodeCmd *pNodeCmd = pNode->pNodeCmd;

    if (pNode->iNode < 0) {
        return 0;
    }

    if (!pNodeCmd) {
        char zBuf[100];
        Tcl_Obj *pCmd;
        sprintf(zBuf, "::tkhtml::node%d", nodeNumber++);

        pCmd = Tcl_NewStringObj(zBuf, -1);
        Tcl_IncrRefCount(pCmd);
        Tcl_CreateObjCommand(pTree->interp, zBuf, nodeCommand, pNode, 0);
        pNodeCmd = (HtmlNodeCmd *)HtmlAlloc(0, sizeof(HtmlNodeCmd));
        pNodeCmd->pCommand = pCmd;
        pNodeCmd->pTree = pTree;
        pNode->pNodeCmd = pNodeCmd;
    }

    return pNodeCmd->pCommand;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeToString --
 *
 *     Return a human-readable string representation of pNode in memory
 *     allocated by HtmlFree(0, ). This function is only used for debugging.
 *     Code to build string representations of nodes for other purposes
 *     should be done in Tcl using the node-command interface.
 *
 * Results:
 *     Pointer to string allocated by HtmlAlloc(0, ).
 *
 * Side effects:
 *     Allocates memory. Since this function is usually called from a 
 *     debugger, this memory is unlikely to get freed.
 *
 *---------------------------------------------------------------------------
 */
char * 
HtmlNodeToString(pNode)
    HtmlNode *pNode;
{
    int len;
    char *zStr;
    int tag;

    Tcl_Obj *pStr = Tcl_NewObj();
    Tcl_IncrRefCount(pStr);

    tag = HtmlNodeTagType(pNode);

    if (tag==Html_Text || tag==Html_Space) {
        HtmlToken *pToken;
        pToken = pNode->pToken;

        Tcl_AppendToObj(pStr, "\"", -1);
        while (pToken && (pToken->type==Html_Text||pToken->type==Html_Space)) {
            if (pToken->type==Html_Space) {
                int i;
                for (i=0; i<(pToken->count - (pToken->x.newline?1:0)); i++) {
                    Tcl_AppendToObj(pStr, " ", 1);
                }
                if (pToken->x.newline) {
                    Tcl_AppendToObj(pStr, "<nl>", 4);
                }
            } else {
                Tcl_AppendToObj(pStr, pToken->x.zText, pToken->count);
            }
            pToken = pToken->pNextToken;
        }
        Tcl_AppendToObj(pStr, "\"", -1);

    } else {
        int i;
        HtmlToken *pToken = pNode->pToken;
        Tcl_AppendToObj(pStr, "<", -1);
        Tcl_AppendToObj(pStr, HtmlMarkupName(tag), -1);
        for (i = 2; i < pToken->count; i += 2) {
            Tcl_AppendToObj(pStr, " ", -1);
            Tcl_AppendToObj(pStr, pToken->x.zArgs[i], -1);
            Tcl_AppendToObj(pStr, "=\"", -1);
            Tcl_AppendToObj(pStr, pToken->x.zArgs[i+1], -1);
            Tcl_AppendToObj(pStr, "\"", -1);
        }
        Tcl_AppendToObj(pStr, ">", -1);
    }

    /* Copy the string from the Tcl_Obj* to memory obtained via HtmlAlloc(0, ).
     * Then release the reference to the Tcl_Obj*.
     */
    Tcl_GetStringFromObj(pStr, &len);
    zStr = HtmlAlloc(0, len+1);
    strcpy(zStr, Tcl_GetString(pStr));
    Tcl_DecrRefCount(pStr);

    return zStr;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeScrollbarDoCallback --
 *
 *     If node pNode is scrollable, invoke the [$scrollbar set] command
 *     for each of it's scrollbar widgets.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Invokes 0-2 [$scrollbar set] scripts.
 *
 *---------------------------------------------------------------------------
 */
int HtmlNodeScrollbarDoCallback(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    HtmlNodeScrollbars *p;
    p = pNode->pScrollbar;

    if (p) {
        char zTmp[256];
        if (p->vertical.win) {
            snprintf(zTmp, 255, "%s set %f %f", 
                Tcl_GetString(p->vertical.pReplace), 
                (double)p->iVertical / (double)p->iVerticalMax,
                (double)(p->iVertical + p->iHeight) / (double)p->iVerticalMax
            );
            zTmp[255] = '\0';
            Tcl_Eval(pTree->interp, zTmp);
        }
        if (p->horizontal.win) {
            snprintf(zTmp, 255, "%s set %f %f", 
                Tcl_GetString(p->horizontal.pReplace), 
                (double)p->iHorizontal / (double)p->iHorizontalMax,
                (double)(p->iHorizontal + p->iWidth) / (double)p->iHorizontalMax
            );
            zTmp[255] = '\0';
            Tcl_Eval(pTree->interp, zTmp);
        }
    }

    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlTreeClear --
 *
 *     Completely reset the widgets internal structures - for example when
 *     loading a new document.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Deletes internal document representation.
 *
 *---------------------------------------------------------------------------
 */
int HtmlTreeClear(pTree)
    HtmlTree *pTree;
{
    HtmlToken *pToken;
    HtmlToken *pPrev = 0;

    /* Free the canvas representation */
    HtmlDrawCleanup(pTree, &pTree->canvas);
    memset(&pTree->canvas, 0, sizeof(HtmlCanvas));

    /* Free the tree representation - pTree->pRoot */
    HtmlTreeFree(pTree);

    /* Free any parsed text tokens that have not been added to the 
     * tree structure.  */
    pPrev = 0;
    for (pToken=pTree->pTextFirst; pToken; pToken = pToken->pNextToken) {
        HtmlFree(0, (char *)pPrev);
        pPrev = pToken;
    }
    HtmlFree(0, (char *)pTree->pTextLast);
    pTree->pTextFirst = 0;
    pTree->pTextLast = 0;

    /* Free the formatted text, if any (HtmlTree.pText) */
    HtmlTextInvalidate(pTree);

    /* Free the plain text representation */
    if (pTree->pDocument) {
        Tcl_DecrRefCount(pTree->pDocument);
    }
    pTree->nParsed = 0;
    pTree->pDocument = 0;
    pTree->iCol = 0;

    /* Free the stylesheets */
    HtmlCssStyleSheetFree(pTree->pStyle);
    pTree->pStyle = 0;

    /* Set the scroll position to top-left and clear the selection */
    pTree->iScrollX = 0;
    pTree->iScrollY = 0;

    /* Clear the selection */
    pTree->pFromNode = 0;
    pTree->pToNode = 0;
    pTree->iFromIndex = 0;
    pTree->iToIndex = 0;

    /* Deschedule any dynamic or style callback */
    pTree->cb.pDynamic = 0;
    pTree->cb.pRestyle = 0;
    pTree->cb.flags &= ~(HTML_DYNAMIC|HTML_RESTYLE);

    pTree->iNextNode = 0;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeGetPointer --
 *
 *     String argument zCmd is the name of a node command created for
 *     some node of tree pTree. Find the corresponding HtmlNode pointer
 *     and return it. If zCmd is not the name of a node command, leave
 *     an error in pTree->interp and return NULL.
 *
 * Results:
 *     Pointer to node object associated with Tcl command zCmd, or NULL.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode *
HtmlNodeGetPointer(pTree, zCmd)
    HtmlTree *pTree;
    char CONST *zCmd;
{
    Tcl_Interp *interp = pTree->interp;
    Tcl_CmdInfo info;
    int rc;

    rc = Tcl_GetCommandInfo(interp, zCmd, &info);
    if (rc == 0 || info.objProc != nodeCommand){ 
        Tcl_AppendResult(interp, "no such node: ", zCmd, 0);
        return 0;
    }
    return (HtmlNode *)info.objClientData;
}

