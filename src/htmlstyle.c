
/*
 * htmlstyle.c ---
 *
 *     This file applies the cascade algorithm using the stylesheet
 *     code in css.c to the tree built with code in htmltree.c
 *
 * TODO: Copyright.
 */
static char rcsid[] = "@(#) $Id:";

#include "html.h"
#include <assert.h>
#include <string.h>

/*
 * --------------------------------------------------------------------------
 * Begin CssNodeInterface declaration.
 */
static CONST char * 
xType(node)
    void *node;
{
    HtmlNode *pNode = (HtmlNode *)node;
    return HtmlMarkupName(pNode->pElement->base.type);
}
static CONST char * 
xAttr(node, zAttr)
    void *node;
    CONST char *zAttr;
{
    HtmlNode *pNode = (HtmlNode *)node;
    return HtmlMarkupArg(pNode->pElement, zAttr, 0);
}
static void * 
xParent(node)
    void *node;
{
    HtmlNode *pNode = (HtmlNode *)node;
    return 0;
}
static int 
xNumChild(node)
    void *node;
{
    HtmlNode *pNode = (HtmlNode *)node;
    return 0;
}
static void * 
xChild(node, iChild)
    void *node;
    int iChild;
{
    HtmlNode *pNode = (HtmlNode *)node;
    return 0;
}
static CONST char * 
xLang(node)
    void *node;
{
    HtmlNode *pNode = (HtmlNode *)node;
    return "en";
}
static int 
xParentIdx(node)
    void *node;
{
    HtmlNode *pNode = (HtmlNode *)node;
    return -1;
}
CssNodeInterface nodeinterface = {
    xType,
    xAttr,
    xParent,
    xNumChild,
    xChild,
    xLang,
    xParentIdx
};
/*
 * End of CssNodeInterface declaration.
 * --------------------------------------------------------------------------
 */

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeInterface --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
CONST CssNodeInterface * 
HtmlNodeInterface()
{
    return &nodeinterface;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlStyleParse --
 *
 *     Compile a stylesheet document from text and add it to the widget.
 *
 *     Tcl: $widget style parse STYLE-SHEET
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
HtmlStyleParse(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlWidget *p = (HtmlWidget *)clientData;

    assert( objc==4 );

    /* For now, if there is already a style-sheet, just delete it */
    HtmlCssStyleSheetFree(p->pStyle);
    HtmlCssParse(-1, Tcl_GetString(objv[3]), &p->pStyle);

    return TCL_OK;
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
int styleNode(p, pNode)
    HtmlWidget *p; 
    HtmlNode *pNode;
{
    CssProperties **ppProp;
    if( pNode->pProperties ){
        HtmlCssPropertiesFree(pNode->pProperties);
    }
    ppProp = &pNode->pProperties;
    HtmlCssStyleSheetApply(p->pStyle, HtmlNodeInterface(), pNode, ppProp);
    return 0;
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
HtmlStyleApply(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlWidget *p = (HtmlWidget *)clientData;
    HtmlWalkTree(p, styleNode);
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
int HtmlStyleSyntaxErrs(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlWidget *p = (HtmlWidget *)clientData;
    int nSyntaxErrs = 0;
    if( p->pStyle ){
        nSyntaxErrs = HtmlCssStyleSheetSyntaxErrs(p->pStyle);
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(nSyntaxErrs));
    return TCL_OK;
}
