
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
 *---------------------------------------------------------------------------
 *
 * HtmlStyleParse --
 *
 *     Compile a stylesheet document from text and add it to the widget.
 *
 *     Tcl: $widget style parse STYLE-SHEET-ID STYLE-SHEET
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
    int stylesheet_origin;
    Tcl_Obj *pStyleId = 0;
    CONST char *zId;
    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc!=5) {
        Tcl_WrongNumArgs(interp, 2, objv, "STYLE-SHEET-ID STYLE-SHEET");
        return TCL_ERROR;
    }

    /* Parse up the stylesheet id. It must begin with one of the strings
     * "agent", "user" or "author". After that it may contain any text.
     */
    zId = Tcl_GetString(objv[3]);
    if (0==strncmp("agent", zId, 5)) {
        stylesheet_origin = CSS_ORIGIN_AGENT;
        pStyleId = Tcl_NewStringObj(&zId[5], -1);
    }
    else if (0==strncmp("user", zId, 4)) {
        stylesheet_origin = CSS_ORIGIN_USER;
        pStyleId = Tcl_NewStringObj(&zId[4], -1);
    }
    else if (0==strncmp("author", zId, 5)) {
        stylesheet_origin = CSS_ORIGIN_AUTHOR;
        pStyleId = Tcl_NewStringObj(&zId[6], -1);
    }
    if (!pStyleId) {
        Tcl_AppendResult(interp, "Bad style-sheet-id: ", zId, 0);
        return TCL_ERROR;
    }
    Tcl_IncrRefCount(pStyleId);

    /* If there is already a stylesheet in pTree->pStyle, then this call will
     * parse the stylesheet text in objv[3] and append rules to the
     * existing stylesheet. If p->pStyle is NULL, then a new stylesheet is
     * created. Within Tkhtml, each document only ever has a single
     * stylesheet object, possibly created by combining text from multiple
     * stylesheet documents.
     */
    HtmlCssParse(objv[4], stylesheet_origin, pStyleId, &pTree->pStyle);

    Tcl_DecrRefCount(pStyleId);
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
int styleNode(pTree, pNode)
    HtmlTree *pTree; 
    HtmlNode *pNode;
{
    CssProperties **ppProp;
    CssProperty *pReplace;
    CONST char *zStyle;      /* Value of "style" attribute for node */

    if( pNode->pProperties ){
        HtmlCssPropertiesFree(pNode->pProperties);
        pNode->pProperties = 0;
    }
    ppProp = &pNode->pProperties;
    HtmlCssStyleSheetApply(pTree->pStyle, pNode, ppProp);

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

    /* Delete the property cache for the node, if one exists. */
    HtmlDeletePropertyCache(pNode->pPropCache);
    pNode->pPropCache = 0;

    return TCL_OK;
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
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlWalkTree(pTree, styleNode);
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
    HtmlTree *pTree = (HtmlTree *)clientData;
    int nSyntaxErrs = 0;
    if( pTree->pStyle ){
        nSyntaxErrs = HtmlCssStyleSheetSyntaxErrs(pTree->pStyle);
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(nSyntaxErrs));
    return TCL_OK;
}

