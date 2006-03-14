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

static const char rcsid[] = "$Id: htmltree.c,v 1.53 2006/03/14 11:20:26 danielk1977 Exp $";

#include "html.h"
#include "swproc.h"
#include <assert.h>
#include <string.h>

/*
 *---------------------------------------------------------------------------
 *
 * moveToLeftSibling --
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
    pNewParent->apChildren = (HtmlNode **)HtmlRealloc(
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
    assert(HtmlNodeTagType(pNode) == Html_TABLE);
    int i;

    for (i = HtmlNodeNumChildren(pNode) - 1; i >= 0; i--) {
        HtmlNode *pChild = HtmlNodeChild(pNode, i);
        int tag = HtmlNodeTagType(pChild);

        if (tag == Html_TR) {
            /* Any child of a <tr> that is not a <td> or <th> is 
             * moved to become a left-hand sibling of the <table>.
             */
            int j;
            for (j = HtmlNodeNumChildren(pChild) - 1; j >= 0; j--) {
                HtmlNode *pGrandChild = HtmlNodeChild(pChild, j);
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

    for (i = 0; i < HtmlNodeNumChildren(pNode); i++) {
        HtmlNode *pChild = HtmlNodeChild(pNode, i);
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
            for (j = i + 1; j < HtmlNodeNumChildren(pNode); j++) {
                HtmlNode *pSibling = HtmlNodeChild(pNode, j);
                int tag2 = HtmlNodeTagType(pSibling);
                assert(tag2 == Html_TR || tag2 == Html_TD || tag2 == Html_TH);
                if (tag2 == Html_TR) break;
            }
            nMove = j - i;
            assert(nMove > 0);

            /* Create a token and link it into the token list just 
             * before it's first adopted child.
             */
            pRowToken = (HtmlToken *)HtmlClearAlloc(sizeof(HtmlToken));
            pRowToken->type = Html_TR;
            pRowToken->pNext = pChild->pToken;
            pRowToken->pPrev = pChild->pToken->pPrev;
            pChild->pToken->pPrev->pNext = pRowToken;
            pChild->pToken->pPrev = pRowToken;

            /* Create an HtmlNode for the new <tr> */
            pRowNode = (HtmlNode *)HtmlClearAlloc(sizeof(HtmlNode));
            pRowNode->pParent = pNode;
            pRowNode->pToken = pRowToken;
            pRowNode->nChild = nMove;
            pRowNode->apChildren = 
                (HtmlNode **)HtmlClearAlloc(sizeof(HtmlNode*) * nMove);
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
        for (pT = pToken; pT && pT->type == Html_Space; pT = pT->pNext);
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
clearReplacement(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    HtmlNodeReplacement *p = pNode->pReplacement;
    pNode->pReplacement = 0;
    if (p) {
        if (p->pDelete) {
            /* If there is a delete script, invoke it now. */
            int flags = TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL;
            Tcl_EvalObjEx(pTree->interp, p->pDelete, flags);
            Tcl_DecrRefCount(p->pDelete);
        }
        if (p->pReplace) {
            /* Cancel geometry management */
            if (p->win) {
                Tk_ManageGeometry(p->win, 0, 0);
            }
            Tcl_DecrRefCount(p->pReplace);
        }
        if (p->pConfigure) Tcl_DecrRefCount(p->pConfigure);
        HtmlFree((char *)p);
    }
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
        for(i=0; i<pNode->nChild; i++){
            freeNode(pTree, pNode->apChildren[i]);
        }
        HtmlComputedValuesRelease(pTree, pNode->pPropertyValues);
        HtmlComputedValuesRelease(pTree, pNode->pPreviousValues);
        HtmlCssPropertiesFree(pNode->pStyle);
        if (pNode->pNodeCmd) {
            Tcl_Obj *pCommand = pNode->pNodeCmd->pCommand;
            Tcl_DeleteCommand(pTree->interp, Tcl_GetString(pCommand));
            Tcl_DecrRefCount(pCommand);
            HtmlFree((char *)pNode->pNodeCmd);
            pNode->pNodeCmd = 0;
        }
        clearReplacement(pTree, pNode);
        HtmlFree((char *)pNode->apChildren);
        HtmlFree((char *)pNode);
    }
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
 * nodeAddChild --
 *
 *     Add a new child node to node pNode. pToken becomes the starting
 *     token for the new node. The value returned is the index of the new
 *     child. So the call:
 *
 *          HtmlNodeChild(pNode, nodeAddChild(pNode, pToken))
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
static int 
nodeAddChild(pNode, pToken)
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
    pNode->apChildren = (HtmlNode **)HtmlRealloc((char *)pNode->apChildren, n);

    pNew = (HtmlNode *)HtmlAlloc(sizeof(HtmlNode));
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
HtmlAddToken(pTree, pToken)
    HtmlTree *pTree;
    HtmlToken *pToken;
{
    HtmlNode *pCurrent = pTree->pCurrent;
    int type = pToken->type;

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
         * If the first thing we strike is not an <html> tag, then add one
         * artificially.
         */
        HtmlToken *pHtml = pToken;
        if (type != Html_HTML) {
            pHtml = (HtmlToken *)HtmlAlloc(sizeof(HtmlToken));
            memset(pHtml, 0, sizeof(HtmlToken));
            pHtml->type = Html_HTML;
            pHtml->pNext = pTree->pFirst;
            pTree->pFirst = pHtml;
            pTree->pLast = pHtml;
            if (pHtml->pNext) {
                pHtml->pNext->pPrev = pHtml;
            }
        }
        
        pCurrent = (HtmlNode *)HtmlAlloc(sizeof(HtmlNode));
        memset(pCurrent, 0, sizeof(HtmlNode));
        pCurrent->pToken = pHtml;
        pTree->pRoot = pCurrent;
        pTree->pCurrent = pCurrent;

        if (pHtml != pToken) {
            HtmlAddToken(pTree, pToken);
        }

    } else if (type != Html_HTML) {
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
            pCurrent = HtmlNodeChild(pCurrent, nodeAddChild(pCurrent, pToken));
            pCurrent->iNode = pTree->iNextNode++;
        }

        if (HtmlMarkupFlags(type) & HTMLTAG_EMPTY) {
            nodeHandlerCallbacks(pTree, pCurrent);
            pCurrent = HtmlNodeParent(pCurrent);
        }
        pTree->pCurrent = pCurrent;
    }
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
        xCallback(pTree, pNode, clientData);
        for (i = 0; i<pNode->nChild; i++) {
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

    for (p = pNode->pToken; p && p->type == Html_Space; p = p->pNext);
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

static void 
geomRequestProc(clientData, widget)
    ClientData clientData;
    Tk_Window widget;
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlCallbackSchedule(pTree, HTML_CALLBACK_LAYOUT);
}

static HtmlNode *
commonParent(pNodeA, pNodeB) 
    HtmlNode *pNodeA;
    HtmlNode *pNodeB;
{
    HtmlNode *pA;
    HtmlNode *pB;

    assert(pNodeB);

    for (pA = pNodeA; pA; pA = HtmlNodeParent(pA)) {
        for (pB = pNodeB; pB; pB = HtmlNodeParent(pB)) {
            if (pB == pA) return pA;
        }
    }

    assert(!pNodeA);
    return pNodeB;
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
 *         <node> right_sibling
 *         <node> tag
 *         <node> text
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
        "attr", "tag", "nChildren", "child", "text", 
        "parent", "prop", "replace", "right_sibling", 
        "dynamic", 0
    };
    enum NODE_enum {
        NODE_ATTR, NODE_TAG, NODE_NCHILDREN, NODE_CHILD, NODE_TEXT,
        NODE_PARENT, NODE_PROP, NODE_REPLACE, NODE_RIGHT_SIBLING,
        NODE_DYNAMIC
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
         * nodeHandle attr ??-default DEFAULT-VALUE? ATTR-NAME?
         */
        case NODE_ATTR: {
            char CONST *zAttr = 0;
            char *zAttrName = 0;
            char *zDefault = 0;

            switch (objc) {
                case 2:
                    break;
                case 3:
                    zAttrName = Tcl_GetString(objv[2]);
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
                "? ?-default DEFAULT-VALUE? ATTR-NAME?", 0);
            return TCL_ERROR;
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
        case NODE_NCHILDREN: {
            if (objc!=2) {
                Tcl_WrongNumArgs(interp, 2, objv, "");
                return TCL_ERROR;
            }
            Tcl_SetObjResult(interp, Tcl_NewIntObj(HtmlNodeNumChildren(pNode)));
            break;
        }
        case NODE_CHILD: {
            Tcl_Obj *pCmd;
            int n;
            if (objc!=3) {
                Tcl_WrongNumArgs(interp, 2, objv, "");
                return TCL_ERROR;
            }
            if (TCL_OK!=Tcl_GetIntFromObj(interp, objv[2], &n)) {
                return TCL_ERROR;
            }
            if (n>=HtmlNodeNumChildren(pNode) || n<0) {
                Tcl_SetResult(interp, "Parameter out of range", TCL_STATIC);
                return TCL_ERROR;
            }
            pCmd = HtmlNodeCommand(pTree, HtmlNodeChild(pNode, n));
            Tcl_SetObjResult(interp, pCmd);
            break;
        }

        /*
         * nodeHandle text ?-tokens?
         *
         */
        case NODE_TEXT: {
            HtmlToken *pT;
            Tcl_Obj *pRet;
            int tokens;

            if (
                (objc != 2 && objc != 3) ||
                (objc == 3 && strcmp(Tcl_GetString(objv[2]), "-tokens"))
            ) {
                Tcl_WrongNumArgs(interp, 2, objv, "?-tokens?");
                return TCL_ERROR;
            }

            tokens = ((objc == 3) ? 1 : 0);
            pRet = Tcl_NewObj();
            Tcl_IncrRefCount(pRet);
            for (
                pT = pNode->pToken;
                pT && (pT->type==Html_Space || pT->type==Html_Text);
                pT = pT->pNext
            ) {
                if (pT->type==Html_Text) {
                    if (tokens) {
                        Tcl_Obj *pObj = Tcl_NewStringObj("TEXT(", -1);
                        Tcl_AppendToObj(pObj, pT->x.zText, pT->count);
                        Tcl_AppendToObj(pObj, ")", -1);
                        Tcl_ListObjAppendElement(interp, pRet, pObj);
                    } else {
                        Tcl_AppendToObj(pRet, pT->x.zText, pT->count);
                    }
                } else {
                    if (tokens) {
                        if (pT->x.newline) {
                            Tcl_Obj *pObj = Tcl_NewStringObj("NEWLINE", -1);
                            Tcl_ListObjAppendElement(interp, pRet, pObj);
                        } else {
                            Tcl_Obj *pObj;
                            char zBuf[128];
                            sprintf(zBuf, "SPACE(%d)", pT->count);
                            pObj = Tcl_NewStringObj(zBuf, -1);
                            Tcl_ListObjAppendElement(interp, pRet, pObj);
                        }
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
            if (HtmlNodeProperties(interp, pN->pPropertyValues)) {
                return TCL_ERROR;
            }
            break;
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
                Tcl_Obj *aArgs[3];
                HtmlNodeReplacement *pReplace; /* New pNode->pReplacement */
                int nBytes;                    /* bytes allocated at pReplace */
                Tk_Window widget;              /* Replacement widget */
                Tk_Window win = pTree->win;    /* Application main window */

                SwprocConf aArgConf[4] = {
                    {SWPROC_ARG, "new-value", 0, 0},
                    {SWPROC_OPT, "configurecmd", 0, 0},
                    {SWPROC_OPT, "deletecmd", 0, 0},
                    {SWPROC_END, 0, 0, 0}
                };

                if (SwprocRt(interp, objc - 2, &objv[2], aArgConf, aArgs)) {
                    return TCL_ERROR;
                }

                /* Make sure the replacement object is a Tk window. Register
                 * Tkhtml as the geometry manager.
                 */
                widget = Tk_NameToWindow(interp, Tcl_GetString(aArgs[0]), win);
                if (!widget) {
                    return TCL_ERROR;
                } else {
                    static Tk_GeomMgr sManage = {
                        "Tkhtml",
                        geomRequestProc,
                        0
                    };
                    Tk_ManageGeometry(widget, &sManage, pTree);
                }

                nBytes = sizeof(HtmlNodeReplacement);
                pReplace = (HtmlNodeReplacement *) HtmlClearAlloc(nBytes);
                pReplace->pReplace = aArgs[0];
                pReplace->pConfigure = aArgs[1];
                pReplace->pDelete = aArgs[2];
                pReplace->win = widget;

		/* Free any existing replacement object and set
		 * pNode->pReplacement to point at the new structure. 
                 *
		 * Todo: We could just overwrite the existing values to deal
		 * with this case.
                 */
                clearReplacement(pTree, pNode);
                pNode->pReplacement = pReplace;

                /* Run the layout engine. */
                HtmlCallbackSchedule(pTree, HTML_CALLBACK_LAYOUT);
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
         * nodeHandle right_sibling
         *
         *     Return the right-sibling of the node, or an empty string
         *     if no such sibling exists.
         */
        case NODE_RIGHT_SIBLING: {
            HtmlNode *pSibling;
            pSibling = HtmlNodeRightSibling(pNode);
            if (pSibling) {
                Tcl_SetObjResult(interp, HtmlNodeCommand(pTree, pSibling));
            } 
            break;
        }

        /*
         * nodeHandle dynamic set|clear ?flag?
         *
         */
        case NODE_DYNAMIC: {
            struct DynamicFlag {
                const char *zName;
                Html_u8 flag;
            } flags[] = {
                {"active", HTML_DYNAMIC_ACTIVE}, 
                {"focus",  HTML_DYNAMIC_FOCUS}, 
                {"hover",  HTML_DYNAMIC_HOVER},
                {0, 0}
            };
            const char *zArg1 = (objc>2) ? Tcl_GetString(objv[2]) : 0;
            const char *zArg2 = (objc>3) ? Tcl_GetString(objv[3]) : 0;
            Tcl_Obj *pRet;
            int i;

            Html_u8 mask = 0;

            if (zArg2) {
                for (i = 0; flags[i].zName; i++) {
                    if (0 == strcmp(zArg2, flags[i].zName)) {
                        mask = flags[i].flag;
                    }
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

            pTree->cb.pDynamic = commonParent(pTree->cb.pDynamic, pNode);
            pTree->cb.isCssDynamic = 1;
            HtmlCallbackSchedule(pTree, HTML_CALLBACK_DYNAMIC);

            break;
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

    if (!pNodeCmd) {
        char zBuf[100];
        Tcl_Obj *pCmd;
        sprintf(zBuf, "::tkhtml::node%d", nodeNumber++);

        pCmd = Tcl_NewStringObj(zBuf, -1);
        Tcl_IncrRefCount(pCmd);
        Tcl_CreateObjCommand(pTree->interp, zBuf, nodeCommand, pNode, 0);
        pNodeCmd = (HtmlNodeCmd *)HtmlAlloc(sizeof(HtmlNodeCmd));
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
 *     allocated by HtmlFree(). This function is only used for debugging.
 *     Code to build string representations of nodes for other purposes
 *     should be done in Tcl using the node-command interface.
 *
 * Results:
 *     Pointer to string allocated by HtmlAlloc().
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
            pToken = pToken->pNext;
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

    /* Copy the string from the Tcl_Obj* to memory obtained via HtmlAlloc().
     * Then release the reference to the Tcl_Obj*.
     */
    Tcl_GetStringFromObj(pStr, &len);
    zStr = HtmlAlloc(len+1);
    strcpy(zStr, Tcl_GetString(pStr));
    Tcl_DecrRefCount(pStr);

    return zStr;
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

    /* Free the tree representation - pTree->pRoot */
    HtmlTreeFree(pTree);

    /* Free the token representation */
    for (pToken=pTree->pFirst; pToken; pToken = pToken->pNext) {
        HtmlFree((char *)pToken->pPrev);
    }
    HtmlFree((char *)pTree->pLast);
    pTree->pFirst = 0;
    pTree->pLast = 0;

    for (pToken=pTree->pTextFirst; pToken; pToken = pToken->pNext) {
        HtmlFree((char *)pToken->pPrev);
    }
    HtmlFree((char *)pTree->pTextLast);
    pTree->pTextFirst = 0;
    pTree->pTextLast = 0;

    /* Free the canvas representation */
    /* HtmlDrawDeleteControls(pTree, &pTree->canvas); */
    HtmlDrawCleanup(&pTree->canvas);
    memset(&pTree->canvas, 0, sizeof(HtmlCanvas));

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

    /* Deschedule any dynamic callback */
    pTree->cb.isCssDynamic = 0;
    pTree->cb.pDynamic = 0;

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

