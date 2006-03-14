
#include "html.h"
#include "cssInt.h"

/*
 * How CSS selectors are implemented:
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
    CssDynamic *pNew = (CssDynamic *)HtmlClearAlloc(sizeof(CssDynamic));
#if 0
printf("Attach dynamic selector\n");
#endif
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
        HtmlFree(pTmp);
    }
    pNode->pDynamic = 0;
}


int 
checkDynamicCb(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    CssDynamic *p;
    for (p = pNode->pDynamic; p; p = p->pNext) {
        int res = HtmlCssSelectorTest(p->pSelector, pNode, 0) ? 1 : 0; 
        if (res != p->isSet) {
            HtmlRestyleNode(pTree, pNode);
            HtmlLayoutPaintNode(pTree, pNode);
        }
        p->isSet = res;
    }
    return 0;
}

void
HtmlCssCheckDynamic(pTree)
    HtmlTree *pTree;
{
    if (pTree->cb.isCssDynamic) {
        HtmlWalkTree(pTree, pTree->cb.pDynamic, checkDynamicCb, 0);
    }
    pTree->cb.isCssDynamic = 0;
    pTree->cb.pDynamic = 0;
}

