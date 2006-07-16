
#include "html.h"
#include "cssInt.h"

static const char rcsid[] = "$Id: cssdynamic.c,v 1.8 2006/07/16 10:53:14 danielk1977 Exp $";

/*
 * How dynamic CSS selectors are implemented:
 *
 *     The implementation of dynamic CSS selectors serves two purposes.
 *     Firstly, they are a feature in and of themselves. Secondly, they
 *     exercise the same dynamic-update code that an external scripting 
 *     implementation someday might.
 *
 *     A "dynamic selector", according to Tkhtml, is any selector that
 *     includes an :active, :focus, or :hover pseudo class.
 */

struct CssDynamic {
    int isSet;                /* True when the condition is set */
    CssSelector *pSelector;   /* The selector for this condition */
    CssDynamic *pNext;
};

void
HtmlCssAddDynamic(pNode, pSelector, isSet)
    HtmlNode *pNode;
    CssSelector *pSelector;
    int isSet;
{
    CssDynamic *pNew;
    for (pNew = pNode->pDynamic; pNew ; pNew = pNew->pNext) {
        if (pNew->pSelector == pSelector) return;
    }
    pNew = 0;

    pNew = (CssDynamic *)HtmlClearAlloc(0, sizeof(CssDynamic));
    pNew->isSet = (isSet ? 1 : 0);
    pNew->pSelector = pSelector;
    pNew->pNext = pNode->pDynamic;
    pNode->pDynamic = pNew;
}

void
HtmlCssFreeDynamics(pNode)
    HtmlNode *pNode;
{
    CssDynamic *p = pNode->pDynamic;
    while (p) {
        CssDynamic *pTmp = p;
        p = p->pNext;
        HtmlFree(0, pTmp);
    }
    pNode->pDynamic = 0;
}


static int 
checkDynamicCb(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    CssDynamic *p;
    for (p = pNode->pDynamic; p; p = p->pNext) {
        int res = HtmlCssSelectorTest(p->pSelector, pNode, 0) ? 1 : 0; 
        if (res != p->isSet) {
            HtmlCallbackRestyle(pTree, pNode);
        }
        p->isSet = res;
    }
    return HTML_WALK_DESCEND;
}

void
HtmlCssCheckDynamic(pTree)
    HtmlTree *pTree;
{
    if (pTree->cb.pDynamic) {
        HtmlNode *pParent = HtmlNodeParent(pTree->cb.pDynamic);
        if (pParent) {
            int i;
            for (i = 0; HtmlNodeChild(pParent, i) != pTree->cb.pDynamic; i++);
            for ( ; HtmlNodeChild(pParent, i); i++) {
                HtmlWalkTree(pTree,HtmlNodeChild(pParent,i),checkDynamicCb,0);
            }
        } else {
            HtmlWalkTree(pTree, pTree->cb.pDynamic, checkDynamicCb, 0);
        }
        pTree->cb.pDynamic = 0;
    }
}

int
HtmlCssTclNodeDynamics(interp, pNode)
    Tcl_Interp *interp;
    HtmlNode *pNode;
{
    CssDynamic *p;
    Tcl_Obj *pRet = Tcl_NewObj();
    for (p = pNode->pDynamic; p ; p = p->pNext) {
        Tcl_Obj *pOther = Tcl_NewObj();
        HtmlCssSelectorToString(p->pSelector, pOther);
        Tcl_ListObjAppendElement(0, pRet, pOther);
    }
    Tcl_SetObjResult(interp, pRet);
    return TCL_OK;
}

