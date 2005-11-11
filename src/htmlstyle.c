
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
static char rcsid[] = "$Id: htmlstyle.c,v 1.15 2005/11/11 09:05:43 danielk1977 Exp $";

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
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlStyleParse(pTree, interp, pStyleText, pId, pImportCmd)
    HtmlTree *pTree;
    Tcl_Interp *interp;
    Tcl_Obj *pStyleText;
    Tcl_Obj *pId;
    Tcl_Obj *pImportCmd;
{
    int stylesheet_origin = 0;
    Tcl_Obj *pStyleId = 0;
    CONST char *zId;

    /* Parse up the stylesheet id. It must begin with one of the strings
     * "agent", "user" or "author". After that it may contain any text.
     */
    zId = Tcl_GetString(pId);
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
    HtmlCssParse(pStyleText, stylesheet_origin, pStyleId, &pTree->pStyle);

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
static int 
styleNode(pTree, pNode)
    HtmlTree *pTree; 
    HtmlNode *pNode;
{
    CONST char *zStyle;      /* Value of "style" attribute for node */

    if (!HtmlNodeIsText(pNode)) {
	/* Release the old property values structure, if any. */
        HtmlComputedValuesRelease(pNode->pPropertyValues);
        pNode->pPropertyValues = 0;
    
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
    
        HtmlCssStyleSheetApply(pTree, pNode);
    }

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

